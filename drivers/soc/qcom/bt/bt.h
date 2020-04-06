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

#ifndef _BT_H
#define _BT_H

#include <linux/tty.h>
#include <linux/reset.h>
#include <linux/serial.h>
#include <linux/kthread.h>
#include <linux/tty_flip.h>
#include <linux/remoteproc.h>
#include <linux/serial_core.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>

#define	PAS_ID	0xC
#define	CMD_ID	0x14
#define BT_M0_WARM_RST_ORIDE	0x0
#define BT_M0_WARM_RST		0x4

#define IOCTL_IPC_BOOT		0xBE

#define IPC_RX_LBUF_SZ		0x0108
#define IPC_TX_LBUF_SZ		0x0108

#define	TO_APPS_ADDR(a)		(btmem->virt + (int)(uintptr_t)a)
#define	TO_BT_ADDR(a)		(void *)(a - btmem->virt)

#define IPC_MSG_HDR_SZ		(4u)
#define IPC_MSG_PLD_SZ		(40u)
#define IPC_TOTAL_MSG_SZ	(IPC_MSG_HDR_SZ + IPC_MSG_PLD_SZ)

#define IPC_LMSG_MASK		(0x8000u)
#define IPC_RACK_MASK		(0x4000u)
#define IPC_PKT_TYPE_MASK	(0x0300u)
#define IPC_MSG_ID_MASK		(0x00FFu)

#define IPC_LMSG_TYPE		((uint16_t) IPC_LMSG_MASK)
#define IPC_SMSG_TYPE		((uint16_t) 0x0000u)
#define IPC_REQ_ACK		((uint16_t) IPC_RACK_MASK)
#define IPC_NO_ACK		((uint16_t) 0x0000u)
#define IPC_PKT_TYPE_CUST	((uint16_t) 0x0000u)
#define IPC_PKT_TYPE_HCI	((uint16_t) 0x0100u)
#define IPC_PKT_TYPE_AUDIO	((uint16_t) 0x0200u)
#define IPC_PKT_TYPE_RFU	(IPC_PKT_TYPE_MASK)

#define IPC_LMSG_SHIFT		(15u)
#define IPC_RACK_SHIFT		(14u)
#define IPC_PKT_TYPE_SHIFT	(8u)

#define	GET_NO_OF_BLOCKS(a, b) ((a + IPC_##b##_LBUF_SZ - 1) / IPC_##b##_LBUF_SZ)

#define GET_RX_INDEX_FROM_BUF(x) \
	((x - btmem->rx_ctxt->lring_buf) / IPC_RX_LBUF_SZ)

#define GET_TX_INDEX_FROM_BUF(x) \
	((x - btmem->tx_ctxt->lring_buf) / IPC_TX_LBUF_SZ)

#define IS_RX_MEM_NON_CONTIGIOUS(pBuf, len)		\
	(((int)(uintptr_t)pBuf + len) >			\
	((int)(uintptr_t)btmem->tx_ctxt->lring_buf +	\
	(IPC_RX_LBUF_SZ * btmem->tx_ctxt->lmsg_buf_cnt)))

/** Message header format.
 *
 *         ---------------------------------------------------------------
 * BitPos |    15    | 14 | 13 | 12 | 11 | 10 |  9  |  8  |    7 - 0     |
 *         ---------------------------------------------------------------
 * Field  | Long Msg |rAck|        RFU        |  PktType  |    msgID     |
 *         ---------------------------------------------------------------
 *
 * - Long Msg   :
 *
 * - reqAck     : This is interpreted by receiver for sending acknowledegement
 *                to sender i.e. send a ack IPC interrupt if set.
 *                Use @ref IS_REQ_ACK or @ref IS_NO_ACK
 *                to determine ack is requested or not.
 *
 * - RFU        : Reserved for future use.
 *
 * - pktType    :
 *
 * - msgID      : Contains unique message ID within a Category.
 *                Use @ref IPC_GET_MSG_ID to get message ID.
 */
#define IPC_ConstructMsgHeader(msgID, reqAck, pktType, longMsg)	\
	(((uint8_t) longMsg << IPC_LMSG_SHIFT) |		\
	((uint8_t) reqAck << IPC_RACK_SHIFT) |			\
	((uint16_t) pktType << IPC_PKT_TYPE_SHIFT) | msgID)

#define IPC_GET_PKT_TYPE(hdr)	\
	((enum ipc_pkt_type)((hdr & IPC_PKT_TYPE_MASK) >> IPC_PKT_TYPE_SHIFT))

#define IS_LONG_MSG(hdr)	((hdr & IPC_LMSG_MASK) == IPC_LMSG_TYPE)
#define IS_SHORT_MSG(hdr)	((hdr & IPC_LMSG_MASK) == IPC_SMSG_TYPE)

#define IS_REQ_ACK(hdr)		((hdr & IPC_RACK_MASK) == IPC_REQ_ACK)
#define IS_NO_ACK(hdr)		((hdr & IPC_RACK_MASK) == IPC_NO_ACK)

#define IS_HCI_PKT(hdr)		((hdr & IPC_PKT_TYPE_MASK) == IPC_PKT_TYPE_HCI)
#define IS_CUST_PKT(hdr)	((hdr & IPC_PKT_TYPE_MASK) == IPC_PKT_TYPE_CUST)

#define IPC_GET_MSG_ID(hdr)	((uint8_t)(hdr & IPC_MSG_ID_MASK))

#define IPC_CMD_IPC_STOP	(0x01)
#define IPC_CMD_SWITCH_TO_UART	(0x02)
#define IPC_CMD_PREPARE_DUMP	(0x03)
#define IPC_CMD_COLLECT_DUMP	(0x04)

/*-------------------------------------------------------------------------
 * Type Declarations
 * ------------------------------------------------------------------------
 */

enum ipc_pkt_type {
	IPC_CUST_PKT,
	IPC_HCI_PKT,
	IPC_AUDIO_PKT,
	IPC_PKT_MAX
};

struct long_msg_info {
	uint16_t smsg_free_cnt;
	uint16_t lmsg_free_cnt;
	uint8_t ridx;
	uint8_t widx;
};

struct ipc_aux_ptr {
	uint32_t len;
	uint8_t *buf;
};

struct ring_buffer {
	uint16_t msg_hdr;
	uint16_t len;

	union {
		uint8_t  smsg_data[IPC_MSG_PLD_SZ];
		void  *lmsg_data;
	} payload;
};

struct ring_buffer_info {
	struct ring_buffer *rbuf;
	uint8_t ring_buf_cnt;
	uint8_t ridx;
	uint8_t widx;
	uint8_t tidx;
	struct ring_buffer_info *next;
};

struct context_info {
	uint16_t TotalMemorySize;
	uint8_t lmsg_buf_cnt;
	uint8_t smsg_buf_cnt;
	struct ring_buffer_info sring_buf_info;
	uint8_t *sring_buf;
	uint8_t *lring_buf;
	uint8_t *reserved;
};


struct bt_mem {
	phys_addr_t phys;
	phys_addr_t reloc;
	void __iomem *virt;
	size_t size;
	struct context_info *tx_ctxt;
	struct context_info *rx_ctxt;
	struct long_msg_info lmsg_ctxt;
};

struct bt_ipc {
	struct regmap *regmap;
	int offset;
	int bit;
	int irq;
};

struct bt_descriptor {
	struct tty_driver *tty_drv;
	struct tty_port tty_port;
	struct reserved_mem *rmem;
	const char *fw_name;
	struct platform_device *pdev;
	void __iomem *warm_reset;
	struct bt_ipc ipc;
	struct bt_mem btmem;
	struct reset_control *btss_reset;
	struct reset_control *btss_clk_ctl;
	struct dentry *dbgfs;
	struct platform_device *rproc_pdev;
	int (*sendmsg_cb)(struct bt_descriptor *, unsigned char *, int);
	void (*recvmsg_cb)(struct bt_descriptor *, unsigned char *, int);
	spinlock_t lock;
	atomic_t state;
	struct notifier_block panic_nb;
	struct pinctrl *pinctrl;
	bool debug_en;
	bool nosecure;
};

extern int bt_ipc_init(struct bt_descriptor *btDesc);
extern void bt_ipc_deinit(struct bt_descriptor *btDesc);
extern u32 rproc_elf_get_boot_addr(struct rproc *rproc,
					const struct firmware *fw);
#endif /* _BT_H */
