/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/notifier.h>
#include <linux/mfd/syscon.h>
#include <uapi/linux/major.h>
#include <linux/completion.h>
#include <linux/ipc_logging.h>
#include "bt.h"

static void *bt_ipc_alloc_lmsg(struct bt_descriptor *btDesc, uint32_t len,
		struct ipc_aux_ptr *aux_ptr, uint8_t *is_lbuf_full)
{
	uint8_t idx;
	uint8_t blks;
	uint8_t blks_consumed;
	struct bt_mem *btmem = &btDesc->btmem;
	struct device *dev = &btDesc->pdev->dev;

	if (btmem->tx_ctxt->lring_buf == NULL) {
		dev_err(dev, "no long message buffer not initialized\n");
		return ERR_PTR(-ENODEV);
	}

	blks = GET_NO_OF_BLOCKS(len, TX);

	if (!btmem->lmsg_ctxt.lmsg_free_cnt ||
			(blks > btmem->lmsg_ctxt.lmsg_free_cnt)) {
		dev_err(dev, "no long message buffer left\n");
		return ERR_PTR(-ENOMEM);
	}

	idx = btmem->lmsg_ctxt.widx;

	if ((btmem->lmsg_ctxt.widx + blks) > btmem->tx_ctxt->lmsg_buf_cnt) {
		blks_consumed = btmem->tx_ctxt->lmsg_buf_cnt - idx;
		aux_ptr->len = len - (blks_consumed * IPC_TX_LBUF_SZ);
		aux_ptr->buf = TO_APPS_ADDR(btmem->tx_ctxt->lring_buf);
	}

	btmem->lmsg_ctxt.widx = (btmem->lmsg_ctxt.widx + blks) %
		btmem->tx_ctxt->lmsg_buf_cnt;

	btmem->lmsg_ctxt.lmsg_free_cnt -= blks;

	if (btmem->lmsg_ctxt.lmsg_free_cnt <=
			((btmem->tx_ctxt->lmsg_buf_cnt * 20) / 100))
		*is_lbuf_full = 1;

	return (TO_APPS_ADDR(btmem->tx_ctxt->lring_buf) +
			(idx * IPC_TX_LBUF_SZ));
}

static struct ring_buffer_info *bt_ipc_get_tx_rbuf(struct bt_descriptor *btDesc,
		uint8_t *is_sbuf_full)
{
	uint8_t idx;
	struct ring_buffer_info *rinfo;
	struct bt_mem *btmem = &btDesc->btmem;

	for (rinfo = &(btmem->tx_ctxt->sring_buf_info);	rinfo != NULL;
		rinfo = TO_APPS_ADDR(btmem->tx_ctxt->sring_buf_info.next)) {
		idx = (rinfo->widx + 1) % (btmem->tx_ctxt->smsg_buf_cnt);

		if (idx != rinfo->tidx) {
			btmem->lmsg_ctxt.smsg_free_cnt--;

			if (btmem->lmsg_ctxt.smsg_free_cnt <=
				((btmem->tx_ctxt->smsg_buf_cnt * 20) / 100))
				*is_sbuf_full = 1;

			return rinfo;
		}
	}

	return ERR_PTR(-ENOMEM);
}

static int bt_ipc_send_msg(struct bt_descriptor *btDesc, uint16_t msg_hdr,
		const uint8_t *pData, uint16_t len)
{
	int ret = 0;
	struct bt_mem *btmem = &btDesc->btmem;
	struct device *dev = &btDesc->pdev->dev;
	struct ring_buffer_info *rinfo;
	struct ring_buffer *rbuf;
	uint8_t is_lbuf_full = 0;
	uint8_t is_sbuf_full = 0;
	struct ipc_aux_ptr aux_ptr;

	rinfo = bt_ipc_get_tx_rbuf(btDesc, &is_sbuf_full);
	if (rinfo == NULL) {
		dev_err(dev, "no tx buffer left\n");
		ret = PTR_ERR(rinfo);
		return ret;
	}

	rbuf = &((struct ring_buffer *)(TO_APPS_ADDR(
						rinfo->rbuf)))[rinfo->widx];
	rbuf->msg_hdr = msg_hdr;
	rbuf->len = len;

	if (len > IPC_MSG_PLD_SZ) {
		rbuf->msg_hdr = rbuf->msg_hdr | IPC_LMSG_MASK;

		aux_ptr.len = 0;
		aux_ptr.buf = NULL;

		rbuf->payload.lmsg_data = bt_ipc_alloc_lmsg(btDesc, len,
				&aux_ptr, &is_lbuf_full);

		if (IS_ERR(rbuf->payload.lmsg_data)) {
			dev_err(dev, "long message buffer allocation failed\n");
			ret = PTR_ERR(rbuf->payload.lmsg_data);
			return ret;
		}

		memcpy_toio(rbuf->payload.lmsg_data, pData,
				(len - aux_ptr.len));

		if (aux_ptr.buf)
			memcpy_toio(aux_ptr.buf, (pData + (len - aux_ptr.len)),
					aux_ptr.len);

		rbuf->payload.lmsg_data = TO_BT_ADDR(rbuf->payload.lmsg_data);
	} else {
		memcpy_toio(rbuf->payload.smsg_data, pData, len);
	}

	if (is_sbuf_full || is_lbuf_full)
		rbuf->msg_hdr = rbuf->msg_hdr | IPC_RACK_MASK;

	rinfo->widx = (rinfo->widx + 1) % btmem->tx_ctxt->smsg_buf_cnt;

	regmap_write(btDesc->ipc.regmap, btDesc->ipc.offset,
			BIT(btDesc->ipc.bit));

	return ret;
}

static
void bt_ipc_free_lmsg(struct bt_descriptor *btDesc, void *lmsg, uint16_t len)
{
	uint8_t idx;
	uint8_t blks;
	struct bt_mem *btmem = &btDesc->btmem;

	idx = GET_TX_INDEX_FROM_BUF((uint8_t *)lmsg);

	if (idx != btmem->lmsg_ctxt.ridx)
		return;

	blks = GET_NO_OF_BLOCKS(len, TX);

	btmem->lmsg_ctxt.ridx  = (btmem->lmsg_ctxt.ridx  + blks) %
		btmem->tx_ctxt->lmsg_buf_cnt;

	btmem->lmsg_ctxt.lmsg_free_cnt += blks;
}

static void bt_ipc_cust_msg(struct bt_descriptor *btDesc, uint8_t msgid)
{
	struct device *dev = &btDesc->pdev->dev;
	uint16_t msg_hdr = 0;
	int ret;

	if (unlikely(!atomic_read(&btDesc->state))) {
		dev_err(dev, "BT IPC not initialized, no message sent\n");
		return;
	}

	msg_hdr |= msgid;

	switch (msgid) {
	case IPC_CMD_IPC_STOP:
		atomic_set(&btDesc->state, 0);
		msg_hdr |= IPC_RACK_MASK;
		dev_info(dev, "BT IPC Stopped, gracefully stopping APSS IPC\n");
		break;
	case IPC_CMD_SWITCH_TO_UART:
		dev_info(dev, "Configured UART, Swithing BT to debug mode\n");
		break;
	case IPC_CMD_PREPARE_DUMP:
		dev_info(dev, "IPQ crashed, inform BT to prepare dump\n");
		break;
	case IPC_CMD_COLLECT_DUMP:
		dev_info(dev, "BT Crashed, gracefully stopping IPC\n");
		return;
	default:
		dev_err(dev, "invalid custom message\n");
		return;
	}

	ret = bt_ipc_send_msg(btDesc, msg_hdr, NULL, 0);
	if (ret)
		dev_err(dev, "err: sending message\n");
}

static bool bt_ipc_process_peer_msgs(struct bt_descriptor *btDesc,
		struct ring_buffer_info *rinfo, uint8_t *pRxMsgCount)
{
	struct bt_mem *btmem = &btDesc->btmem;
	struct ring_buffer *rbuf;
	uint8_t ridx, lbuf_idx;
	uint8_t blks_consumed;
	struct ipc_aux_ptr aux_ptr;
	enum ipc_pkt_type pktType = IPC_CUST_PKT;
	bool ackReqd = false;
	uint8_t *rxbuf = NULL;
	unsigned char *buf;

	ridx = rinfo->ridx;

	while (ridx != rinfo->widx) {
		memset(&aux_ptr, 0, sizeof(struct ipc_aux_ptr));

		rbuf = &((struct ring_buffer *)(TO_APPS_ADDR(
				btmem->rx_ctxt->sring_buf_info.rbuf)))[ridx];

		if (IS_LONG_MSG(rbuf->msg_hdr)) {
			rxbuf = TO_APPS_ADDR(rbuf->payload.lmsg_data);

			if (IS_RX_MEM_NON_CONTIGIOUS(rbuf->payload.lmsg_data,
								rbuf->len)) {

				lbuf_idx = GET_RX_INDEX_FROM_BUF(rxbuf);

				blks_consumed = btmem->rx_ctxt->lmsg_buf_cnt -
					lbuf_idx;
				aux_ptr.len = rbuf->len -
					(blks_consumed * IPC_RX_LBUF_SZ);
				aux_ptr.buf =
					TO_APPS_ADDR(btmem->rx_ctxt->lring_buf);
			}
		} else {
			rxbuf = rbuf->payload.smsg_data;
		}

		if (IS_REQ_ACK(rbuf->msg_hdr))
			ackReqd = true;

		pktType = IPC_GET_PKT_TYPE(rbuf->msg_hdr);

		switch (pktType) {
		case IPC_HCI_PKT:
			buf = kzalloc(rbuf->len, GFP_ATOMIC);
			if (!buf)
				return -ENOMEM;

			memcpy_fromio(buf, rxbuf, (rbuf->len - aux_ptr.len));

			if (aux_ptr.buf)
				memcpy_fromio(buf + (rbuf->len - aux_ptr.len),
						aux_ptr.buf, aux_ptr.len);

			btDesc->recvmsg_cb(btDesc, buf, rbuf->len);
			kfree(buf);
			break;
		case IPC_CUST_PKT:
			bt_ipc_cust_msg(btDesc, IPC_GET_MSG_ID(rbuf->msg_hdr));
			break;
		case IPC_AUDIO_PKT:
			break;
		default:
			break;
		}

		ridx = (ridx + 1) % rinfo->ring_buf_cnt;
	}

	rinfo->ridx = ridx;

	return ackReqd;
}

static void bt_ipc_process_ack(struct bt_descriptor *btDesc)
{
	struct ring_buffer_info *rinfo;
	struct bt_mem *btmem = &btDesc->btmem;

	for (rinfo = &btmem->tx_ctxt->sring_buf_info; rinfo != NULL;
			rinfo = rinfo->next) {
		uint8_t tidx = rinfo->tidx;
		struct ring_buffer *rbuf = (struct ring_buffer *)
			TO_APPS_ADDR(rinfo->rbuf);

		while (tidx != rinfo->ridx) {
			if (IS_LONG_MSG(rbuf[tidx].msg_hdr)) {
				bt_ipc_free_lmsg(btDesc,
						rbuf[tidx].payload.lmsg_data,
						rbuf[tidx].len);
			}

			tidx = (tidx + 1) % btmem->tx_ctxt->smsg_buf_cnt;
			btmem->lmsg_ctxt.smsg_free_cnt++;
		}

		rinfo->tidx = tidx;
	}
}

static
int bt_ipc_sendmsg(struct bt_descriptor *btDesc, unsigned char *buf, int len)
{
	int ret;
	uint16_t msg_hdr = 0x100;
	struct device *dev = &btDesc->pdev->dev;
	unsigned long flags;

	if (unlikely(!atomic_read(&btDesc->state))) {
		dev_err(dev, "BT IPC not initialized, no message sent\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&btDesc->lock, flags);

	ret = bt_ipc_send_msg(btDesc, msg_hdr, (uint8_t *)buf, (uint16_t)len);
	if (ret)
		dev_err(dev, "err: sending message\n");

	spin_unlock_irqrestore(&btDesc->lock, flags);

	return ret;
}

static irqreturn_t bt_ipc_irq_handler(int irq, void *data)
{
	unsigned long flags;
	struct ring_buffer_info *rinfo;
	struct bt_descriptor *btDesc = data;
	struct bt_mem *btmem = &btDesc->btmem;
	bool ackReqd = false;

	disable_irq_nosync(btDesc->ipc.irq);
	spin_lock_irqsave(&btDesc->lock, flags);

	if (unlikely(!atomic_read(&btDesc->state))) {
		btmem->rx_ctxt = (struct context_info *)
			(btDesc->btmem.virt + 0xe000);
		btmem->tx_ctxt = (struct context_info *)((void *)
			btmem->rx_ctxt + btmem->rx_ctxt->TotalMemorySize);

		btmem->lmsg_ctxt.widx = 0;
		btmem->lmsg_ctxt.ridx = 0;
		btmem->lmsg_ctxt.smsg_free_cnt = btmem->tx_ctxt->smsg_buf_cnt;
		btmem->lmsg_ctxt.lmsg_free_cnt = btmem->tx_ctxt->lmsg_buf_cnt;

		atomic_set(&btDesc->state, 1);
	}

	bt_ipc_process_ack(btDesc);

	for (rinfo = &(btmem->rx_ctxt->sring_buf_info); rinfo != NULL;
			rinfo = btmem->rx_ctxt->sring_buf_info.next) {
		if (bt_ipc_process_peer_msgs(btDesc, rinfo,
					&btmem->rx_ctxt->smsg_buf_cnt)) {
			ackReqd = true;
		}
	}

	if (ackReqd) {
		regmap_write(btDesc->ipc.regmap, btDesc->ipc.offset,
				BIT(btDesc->ipc.bit));
	}

	if (btDesc->debug_en)
		bt_ipc_cust_msg(btDesc, IPC_CMD_SWITCH_TO_UART);

	spin_unlock_irqrestore(&btDesc->lock, flags);
	enable_irq(btDesc->ipc.irq);

	return IRQ_HANDLED;
}

static
int ipc_panic_handler(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct bt_descriptor *btDesc = container_of(nb, struct bt_descriptor,
								panic_nb);
	bt_ipc_cust_msg(btDesc, IPC_CMD_PREPARE_DUMP);

	return NOTIFY_DONE;
}

int bt_ipc_init(struct bt_descriptor *btDesc)
{
	int ret;
	struct bt_ipc *ipc = &btDesc->ipc;
	struct device *dev = &btDesc->pdev->dev;

	spin_lock_init(&btDesc->lock);

	ret = devm_request_threaded_irq(dev, ipc->irq, NULL, bt_ipc_irq_handler,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT, "bt_ipc_irq", btDesc);

	if (ret) {
		dev_err(dev, "error registering irq[%d] ret = %d\n",
				ipc->irq, ret);
		goto irq_err;
	}

	btDesc->panic_nb.notifier_call = ipc_panic_handler;

	ret = atomic_notifier_chain_register(&panic_notifier_list,
							&btDesc->panic_nb);
	if (ret)
		goto panic_nb_err;

	btDesc->sendmsg_cb = bt_ipc_sendmsg;

	return 0;

panic_nb_err:
	devm_free_irq(dev, ipc->irq, btDesc);
irq_err:
	return ret;
}
EXPORT_SYMBOL(bt_ipc_init);

void bt_ipc_deinit(struct bt_descriptor *btDesc)
{
	struct bt_ipc *ipc = &btDesc->ipc;
	struct device *dev = &btDesc->pdev->dev;

	atomic_notifier_chain_unregister(&panic_notifier_list,
							&btDesc->panic_nb);
	devm_free_irq(dev, ipc->irq, btDesc);
}
EXPORT_SYMBOL(bt_ipc_deinit);
