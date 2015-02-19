/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dmapool.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>


#include "qca961x-mbox.h"

static unsigned char ioremap_cnt;
volatile void __iomem *mbox_base;
spinlock_t qca961x_mbox_lock;

struct qca961x_mbox_rt_priv *mbox_rtime[ADSS_MBOX_NR_CHANNELS];

unsigned int mbox_irqs[ADSS_MBOX_NR_CHANNELS] = {
	[ADSS_MBOX_SPDIF_CHANNEL]	= ADSS_MBOX_SPDIF_IRQ,
	[ADSS_MBOX_STEREO0_CHANNEL]	= ADSS_MBOX0_IRQ,
	[ADSS_MBOX_STEREO1_CHANNEL]	= ADSS_MBOX1_IRQ,
	[ADSS_MBOX_STEREO2_CHANNEL]	= ADSS_MBOX2_IRQ,
	[ADSS_MBOX_STEREO3_CHANNEL]	= ADSS_MBOX3_IRQ
};

int qca961x_mbox_fifo_reset(int channel)
{
	volatile void __iomem *mbox_reg;
	unsigned int mask = MBOX_FIFO_RESET_TX_INIT |
				MBOX_FIFO_RESET_RX_INIT;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;
	writel(mask, mbox_reg + ADSS_MBOXn_MBOX_FIFO_RESET_REG);

	return 0;
}
EXPORT_SYMBOL(qca961x_mbox_fifo_reset);

int qca961x_mbox_dma_start(int channel)
{
	volatile void __iomem *mbox_reg;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;

	switch (mbox_rtime[channel]->direction) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		writel(ADSS_MBOXn_DMA_RX_CONTROL_START,
			mbox_reg + ADSS_MBOXn_MBOXn_DMA_RX_CONTROL_REG);
		break;

	case SNDRV_PCM_STREAM_CAPTURE:
		writel(ADSS_MBOXn_DMA_TX_CONTROL_START,
			mbox_reg + ADSS_MBOXn_MBOXn_DMA_TX_CONTROL_REG);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(qca961x_mbox_dma_start);

int qca961x_mbox_dma_stop(int channel)
{
	volatile void __iomem *mbox_reg;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;

	switch (mbox_rtime[channel]->direction) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		writel(ADSS_MBOXn_DMA_RX_CONTROL_STOP,
			mbox_reg + ADSS_MBOXn_MBOXn_DMA_RX_CONTROL_REG);
		break;

	case SNDRV_PCM_STREAM_CAPTURE:
		writel(ADSS_MBOXn_DMA_TX_CONTROL_STOP,
			mbox_reg + ADSS_MBOXn_MBOXn_DMA_TX_CONTROL_REG);
		break;
	}

	/* Delay for the dynamically calculated max time based on
	sample size, channel, sample rate + margin to ensure that the
	DMA engine will be truly idle. */

	mdelay(10);

	return 0;

}
EXPORT_SYMBOL(qca961x_mbox_dma_stop);

int qca961x_mbox_dma_prepare(int channel)
{
	struct qca961x_mbox_desc *desc;
	unsigned int val;
	int err;
	volatile void __iomem *mbox_reg;
	dma_addr_t phys_addr;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;

	val = readl(mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);
	val |= MBOX_DMA_POLICY_SW_RESET;
	writel(val, mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);

	mdelay(10);

	val &= ~(MBOX_DMA_POLICY_SW_RESET);
	writel(val, mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);

	desc = mbox_rtime[channel]->dma_virt_head;
	phys_addr = mbox_rtime[channel]->dma_phys_head;


	if (mbox_rtime[channel]->direction == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Request the DMA channel to the controller */

		val |= MBOX_DMA_POLICY_RX_INT_TYPE;
		writel(val, mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);


		/* The direction is indicated from the DMA engine perspective
		 * i.e. we'll be using the RX registers for Playback and
		 * the TX registers for capture */
		writel((readl(mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG) |
			ADSS_MBOX_DMA_POLICY_SRAM_AC(phys_addr)),
				mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);
		writel(phys_addr & 0xfffffff,
			mbox_reg + ADSS_MBOXn_MBOXn_DMA_RX_DESCRIPTOR_BASE_REG);

		err = qca961x_mbox_interrupt_enable(channel,
				MBOX_INT_ENABLE_RX_DMA_COMPLETE);
	} else {

		val |= MBOX_DMA_POLICY_TX_INT_TYPE |
			ADSS_MBOX_DMA_POLICY_TX_FIFO_THRESHOLD(6);

		writel(val, mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);

		writel((readl(mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG) |
			ADSS_MBOX_DMA_POLICY_SRAM_AC(phys_addr)),
				mbox_reg + ADSS_MBOXn_MBOX_DMA_POLICY_REG);

		writel(phys_addr & 0xfffffff,
			mbox_reg + ADSS_MBOXn_MBOXn_DMA_TX_DESCRIPTOR_BASE_REG);


		err = qca961x_mbox_interrupt_enable(channel,
				MBOX_INT_ENABLE_TX_DMA_COMPLETE);
	}

	return err;
}
EXPORT_SYMBOL(qca961x_mbox_dma_prepare);

int qca961x_mbox_form_ring(int channel, dma_addr_t baseaddr, int period_bytes,
				int bufsize)
{
	struct qca961x_mbox_desc *desc, *_desc_p;
	dma_addr_t desc_p;
	unsigned int i, ndescs;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	ndescs = (bufsize + (period_bytes - 1)) / period_bytes;

	desc = dma_alloc_coherent(mbox_rtime[channel]->dev,
				(ndescs * sizeof(struct qca961x_mbox_desc)),
				&desc_p, GFP_KERNEL);
	if (!desc) {
		printk("Mem alloc failed for MBOX RX DMA desc \n");
		return -ENOMEM;
	}

	memset(desc, 0, ndescs * sizeof(struct qca961x_mbox_desc));
	mbox_rtime[channel]->ndescs = ndescs;
	mbox_rtime[channel]->dma_virt_head = desc;
	mbox_rtime[channel]->dma_phys_head = desc_p;
	_desc_p = (struct qca961x_mbox_desc *)desc_p;

	for (i = 0; i < ndescs; i++) {

		desc->OWN = 1;
		desc->ei = 1;
		desc->rsvd1 = desc->rsvd2 = desc->rsvd3 = desc->EOM = 0;
		desc->BufPtr = (baseaddr & 0xfffffff);
		desc->NextPtr =
			(dma_addr_t)(&_desc_p[(i + 1) % ndescs]) & 0xfffffff;
		desc->size = period_bytes;
		desc->length = desc->size;
		baseaddr += period_bytes;
		desc += 1;
	}

	desc--;
	if (bufsize % period_bytes)
		desc->size = bufsize % period_bytes;
	else
		desc->size = period_bytes;

	return 0;
}
EXPORT_SYMBOL(qca961x_mbox_form_ring);

int qca961x_mbox_dma_release(int channel)
{
	unsigned long flags;

	spin_lock_irqsave(&qca961x_mbox_lock, flags);

	if (!mbox_rtime[channel]) {
		/* Handling an error condition where same channel
		 * is been forced to release more than once */
		spin_unlock_irqrestore(&qca961x_mbox_lock, flags);
		return -EINVAL;
	}

	dma_free_coherent(mbox_rtime[channel]->dev,
		mbox_rtime[channel]->ndescs * sizeof(struct qca961x_mbox_desc),
		mbox_rtime[channel]->dma_virt_head,
		mbox_rtime[channel]->dma_phys_head);

	free_irq(mbox_rtime[channel]->irq_no, mbox_rtime[channel]);
	kfree(mbox_rtime[channel]);
	mbox_rtime[channel] = NULL;

	if (--ioremap_cnt == 0)
		iounmap(mbox_base);

	spin_unlock_irqrestore(&qca961x_mbox_lock, flags);

	return 0;
}
EXPORT_SYMBOL(qca961x_mbox_dma_release);

static irqreturn_t qca961x_mbox_dma_irq(int irq, void *dev_id)
{
	unsigned int status;
	struct qca961x_mbox_rt_priv *curr_rtime =
				(struct qca961x_mbox_rt_priv *)dev_id;

	status = readl(curr_rtime->mbox_reg_base +
			ADSS_MBOXn_MBOX_INT_STATUS_REG);

	switch (curr_rtime->direction) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		if (status & MBOX_INT_STATUS_RX_DMA_COMPLETE) {
			qca961x_mbox_interrupt_ack(curr_rtime->channel,
					MBOX_INT_STATUS_RX_DMA_COMPLETE);

			if (curr_rtime->callback)
				curr_rtime->callback(irq, curr_rtime->dai_priv);

		} else if ((status & MBOX_INT_STATUS_RX_UNDERFLOW) ||
				(status & MBOX_INT_STATUS_RX_FIFO_UNDERFLOW))
			printk("Play back underrun\n");

		break;

	case SNDRV_PCM_STREAM_CAPTURE:
		if (status & MBOX_INT_STATUS_TX_DMA_COMPLETE) {
			qca961x_mbox_interrupt_ack(curr_rtime->channel,
					MBOX_INT_STATUS_TX_DMA_COMPLETE);

			if (curr_rtime->callback)
				curr_rtime->callback(irq, curr_rtime->dai_priv);

		} else if ((status & MBOX_INT_STATUS_TX_OVERFLOW) ||
				(status & MBOX_INT_STATUS_TX_FIFO_OVERFLOW))
			printk("Capture overrun\n");
		break;
	}

	return IRQ_HANDLED;
}

int qca961x_mbox_dma_init(struct device *dev, int channel, int direction,
	irq_handler_t callback, void *private_data)
{
	int err;
	unsigned long flags;

	if (channel > ADSS_MBOX_NR_CHANNELS)
		return -EINVAL;

	spin_lock_irqsave(&qca961x_mbox_lock, flags);

	if (++ioremap_cnt == 1)
		mbox_base = ioremap_nocache(ADSS_MBOX_REG_BASE,
						ADSS_MBOX_RANGE);
	if (!mbox_rtime[channel]) {
		mbox_rtime[channel] =
			kzalloc(sizeof(struct qca961x_mbox_priv_t *),
				GFP_KERNEL);
	} else {
		/* Handling an error condition where same channel
		 * is been requested more than once */
		spin_unlock_irqrestore(&qca961x_mbox_lock, flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&qca961x_mbox_lock, flags);

	if (!mbox_rtime[channel] && !mbox_base)
		return -ENOMEM;

	mbox_rtime[channel]->channel = channel;
	mbox_rtime[channel]->irq_no = mbox_irqs[channel];
	mbox_rtime[channel]->dai_priv = private_data;
	mbox_rtime[channel]->callback = callback;
	mbox_rtime[channel]->mbox_reg_base = mbox_base + (channel * 0x2000);
	mbox_rtime[channel]->direction = direction;
	mbox_rtime[channel]->dev = dev;

	err = request_irq(mbox_rtime[channel]->irq_no, qca961x_mbox_dma_irq, 0,
				"qca961x-mbox", mbox_rtime[channel]);

	if (err)
		return err;


	return 0;
}
EXPORT_SYMBOL(qca961x_mbox_dma_init);
