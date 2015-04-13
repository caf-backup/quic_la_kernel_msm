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

#ifndef _QCA961X_MBOX_H_
#define _QCA961X_MBOX_H_

#include <linux/sound.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include "qca961x-adss.h"

#define ADSS_MBOX_REG_BASE			(0x7700000 + 0x6000)
#define ADSS_MBOX_RANGE				(0xFA000)
#define ADSS_MBOX_SPDIF_IRQ			(163 + 32)
#define ADSS_MBOX0_IRQ				(156 + 32)
#define ADSS_MBOX1_IRQ				(157 + 32)
#define ADSS_MBOX2_IRQ				(158 + 32)
#define ADSS_MBOX3_IRQ				(159 + 32)

enum {
	ADSS_MBOX_SPDIF_CHANNEL,
	ADSS_MBOX_STEREO0_CHANNEL,
	ADSS_MBOX_STEREO1_CHANNEL,
	ADSS_MBOX_STEREO2_CHANNEL,
	ADSS_MBOX_STEREO3_CHANNEL,
	ADSS_MBOX_NR_CHANNELS
};

extern spinlock_t qca961x_mbox_lock;
extern struct qca961x_mbox_rt_priv *mbox_rtime[ADSS_MBOX_NR_CHANNELS];

struct qca961x_mbox_desc {

	unsigned int	length 	: 12,	/* bit 11-00 */
			size	: 12,	/* bit 23-12 */
			vuc	: 1,	/* bit 24 */
			ei	: 1,	/* bit 25 */
			rsvd1	: 4,	/* bit 29-26 */
			EOM	: 1,	/* bit 30 */
			OWN	: 1, 	/* bit 31 */
			BufPtr	: 28,   /* bit 27-00 */
			rsvd2	:  4,   /* bit 31-28 */
			NextPtr	: 28,   /* bit 27-00 */
			rsvd3	:  4;   /* bit 31-28 */

	unsigned int vuc_dword[38];

};

struct qca961x_mbox_rt_priv {
	/* Desc array in virtual space */
	struct qca961x_mbox_desc *dma_virt_head;

	/* Desc array for DMA */
	dma_addr_t dma_phys_head;
	struct device *dev;
	unsigned int ndescs;
	int channel;
	int irq_no;
	int direction;
	irq_handler_t callback;
	volatile void __iomem *mbox_reg_base;
	void *dai_priv;
};

/* Replaces struct ath_i2s_softc */
struct qca961x_pcm_pltfm_priv {
	struct snd_pcm_substream *playback;
	struct snd_pcm_substream *capture;
};

/* platform data */
extern struct snd_soc_platform_driver qca961x_soc_platform;

int qca961x_mbox_fifo_reset(int channel);
int qca961x_mbox_dma_start(int channel);
int qca961x_mbox_dma_stop(int channel);
int qca961x_mbox_dma_prepare(int channel);
int qca961x_mbox_form_ring(int channel, dma_addr_t baseaddr, int period_bytes, int bufsize);
int qca961x_mbox_dma_release(int channel);
int qca961x_mbox_dma_init(struct device *dev, int channel, int direction,
	irq_handler_t callback, void *private_data);

static inline int qca961x_mbox_interrupt_enable(int channel, unsigned int mask)
{
	volatile void __iomem *mbox_reg;
	unsigned int val;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;

	val = readl(mbox_reg + ADSS_MBOXn_MBOX_INT_ENABLE_REG);
	val |= mask;
	writel(val, mbox_reg + ADSS_MBOXn_MBOX_INT_ENABLE_REG);

	return 0;
}

static inline int qca961x_mbox_interrupt_disable(int channel, unsigned int mask)
{
	volatile void __iomem *mbox_reg;
	unsigned int val;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;

	val = readl(mbox_reg + ADSS_MBOXn_MBOX_INT_ENABLE_REG);
	val &= ~mask;
	writel(val, mbox_reg + ADSS_MBOXn_MBOX_INT_ENABLE_REG);

	return 0;
}

static inline int qca961x_mbox_interrupt_ack(int channel, unsigned int mask)
{
	volatile void __iomem *mbox_reg;
	unsigned int val;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	mbox_reg = mbox_rtime[channel]->mbox_reg_base;

	val = readl(mbox_reg + ADSS_MBOXn_MBOX_INT_STATUS_REG);
	val &= ~mask;
	writel(val, mbox_reg + ADSS_MBOXn_MBOX_INT_STATUS_REG);

	return 0;
}

static inline unsigned int qca961x_mbox_get_elapsed_size(unsigned int channel)
{
	struct qca961x_mbox_desc *desc;
	unsigned int i, size_played = 0;
	unsigned long flags;

	if (!mbox_rtime[channel])
		return size_played;

	desc = mbox_rtime[channel]->dma_virt_head;
	spin_lock_irqsave(&qca961x_mbox_lock, flags);

	for (i = 0; i < mbox_rtime[channel]->ndescs; i++) {
		if (desc->OWN == 0) {
			desc->OWN = 1;
			desc->ei = 1;
			size_played += desc->size;
		}
		desc += 1;
	}

	spin_unlock_irqrestore(&qca961x_mbox_lock, flags);
	return size_played;
}

static inline int qca961x_mbox_clear_own_bits(unsigned int channel)
{
	struct qca961x_mbox_desc *desc;
	unsigned int i;
	unsigned long flags;

	if (!mbox_rtime[channel])
		return -ENOMEM;

	desc = mbox_rtime[channel]->dma_virt_head;

	spin_lock_irqsave(&qca961x_mbox_lock, flags);
	for (i = 0; i < mbox_rtime[channel]->ndescs; i++) {
		if (desc->OWN == 1)
			desc->OWN = 0;

		desc += 1;
	}
	spin_unlock_irqrestore(&qca961x_mbox_lock, flags);

	return 0;
}

static inline struct qca961x_mbox_desc
	*qca961x_mbox_get_last_played(unsigned int channel)
{
	struct qca961x_mbox_desc *desc, *prev;
	unsigned int ndescs, i;
	unsigned long flags;

	if (!mbox_rtime[channel])
		return NULL;

	ndescs = mbox_rtime[channel]->ndescs;
	/* Point to the last desc */
	prev = &mbox_rtime[channel]->dma_virt_head[ndescs - 1];

	/* Point to the first desc */
	desc = &mbox_rtime[channel]->dma_virt_head[0];

	spin_lock_irqsave(&qca961x_mbox_lock, flags);
	for (i = 0; i < ndescs; i++) {
		if (desc->OWN == 1 && prev->OWN == 0) {
			spin_unlock_irqrestore(&qca961x_mbox_lock, flags);
			return desc;
		}
		prev = desc;
		desc += 1;
	}
	spin_unlock_irqrestore(&qca961x_mbox_lock, flags);

	/* If we didn't find the last played buffer, return NULL */
	return NULL;
}
#endif /* _QCA961X_MBOX_H_ */
