/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <sound/pcm.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/spinlock.h>
#include <linux/of_device.h>

#include "ipq-lpass.h"

static struct lpass_lpm_info lpm_info;

static void __iomem *sg_ipq_lpass_base;

static lpass_info_t *ipq_lpass_cfgs;

static lpass_info_t ipq50xx_devconfig =
{
	LPAIF,
	LPASS_HW_DMA_TYPE_DEFAULT,
	{
		TDM,
		2,
	},
	2,
	2
};

static void ipq-lpass-reg-update(void __iomem *register_addr, uint32_t mask,
					uint32_t value, bool fIsWriteonly)
{
	uint32_t temp = 0;

	if (fIsWriteonly == false){
		temp = readl(register_addr);
	}
	mask = ~mask;
	/*
	 * Zero-out the fields that will be udpated
	 */
	temp = temp & mask;
	/*
	 * Update with new values
	 */
	temp = temp | value;
	/*
	 * write new value to HW reg
	 */
	writel(temp, register_addr);
}

void ipq_lpass_pcm_reset(void __iomem *lpaif_base,
				uint32_t pcm_index, uint32_t dir)
{
	uint32_t mask,field_value;

	if(TDM_SINK == dir){
/*
 * Reset transmit path
 */
	field_value = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_TX_ENABLE_FVAL <<
				HWIO_LPASS_LPAIF_PCM_CTLa_RESET_TX_SHFT;
	mask = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_TX_BMSK;
	} else {
/*
 * field_value receive path
 */
	field_value = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_ENABLE_FVAL <<
				HWIO_LPASS_LPAIF_PCM_CTLa_RESET_RX_SHFT;
	mask = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_RX_BMSK;
	}
	ipq-lpass-reg-update(
		HWIO_LPASS_LPAIF_PCM_CTLa_ADDR(lpaif_base, pcm_index),
		mask, field_value, 0);
}
EXPORT_SYMBOL(ipq_lpass_pcm_reset);

void ipq_lpass_pcm_reset_release(void __iomem *lpaif_base, uint32_t pcm_index,
						uint32_t dir)
{
	uint32_t mask,field_value;

	if(TDM_SINK == dir){
/*
 * reset release trasmit path
 */
	field_value = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_TX_DISABLE_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_RESET_TX_SHFT;
	mask = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_TX_BMSK;
	} else {
/*
 * reset release receive path
 */
	field_value = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_RX_DISABLE_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_RESET_RX_SHFT;
	mask = HWIO_LPASS_LPAIF_PCM_CTLa_RESET_RX_BMSK;
	}

	ipq-lpass-reg-update(
		HWIO_LPASS_LPAIF_PCM_CTLa_ADDR(lpaif_base, pcm_index),
		mask, field_value, 0);
}
EXPORT_SYMBOL(ipq_lpass_pcm_reset_release);

void ipq_lpass_pcm_config(struct ipq_lpass_pcm_config *configPtr,
				void __iomem *lpaif_base, uint32_t pcm_index,
				uint32_t dir)
{
	uint32_t mask, fieldValue;
	uint32_t regOffset;
	uint32_t nbitsPerFrame;

/*
 * First step: Collect info for LPASS_LPAIF_LPAIF_PCM_CTLa
 */
	fieldValue = 0;

/*
 * Setup the mask for sync src, (short,long) sync type, oe mode
*/
	mask =  HWIO_LPASS_LPAIF_PCM_CTLa_SYNC_SRC_BMSK |
			HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_BMSK |
			HWIO_LPASS_LPAIF_PCM_CTLa_CTRL_DATA_OE_BMSK;
/*
 * Setup the sync src: internal or external
*/
	switch (configPtr->sync_src){
	case TDM_MODE_MASTER:
		fieldValue |=
			(HWIO_LPASS_LPAIF_PCM_CTLa_SYNC_SRC_INTERNAL_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_SYNC_SRC_SHFT);
	break;
	case TDM_MODE_SLAVE:
		fieldValue |=
			(HWIO_LPASS_LPAIF_PCM_CTLa_SYNC_SRC_EXTERNAL_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_SYNC_SRC_SHFT);
	break;
	default:
	break;
	}

/*
 * Setup the sync type, short / long
*/
	switch(configPtr->sync_type){
	case TDM_SHORT_SYNC_TYPE:
		fieldValue |= (HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_PCM_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_SHFT);
	break;
	case TDM_LONG_SYNC_TYPE:
		fieldValue |= (HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_AUX_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_SHFT);
	break;
	case TDM_SLOT_SYNC_TYPE:
		fieldValue |= (HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_PCM_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_AUX_MODE_SHFT);
		fieldValue |=
			(HWIO_LPASS_LPAIF_PCM_CTLa_ONE_SLOT_SYNC_EN_ENABLE_FVAL <<
			HWIO_LPASS_LPAIF_PCM_CTLa_ONE_SLOT_SYNC_EN_SHFT);
	break;
	default:
	break;
	}

/*
 * Setup the OE
 */
	switch (configPtr->ctrl_data_oe){
	case TDM_CTRL_DATA_OE_DISABLE:
		fieldValue |= (TDM_CTRL_DATA_OE_DISABLE <<
				HWIO_LPASS_LPAIF_PCM_CTLa_CTRL_DATA_OE_SHFT);
	break;
	case TDM_CTRL_DATA_OE_ENABLE:
		fieldValue |= (TDM_CTRL_DATA_OE_ENABLE <<
				HWIO_LPASS_LPAIF_PCM_CTLa_CTRL_DATA_OE_SHFT);
	break;
	default:
	break;
	}

	regOffset = HWIO_LPASS_LPAIF_PCM_CTLa_OFFS(pcm_index);
	ipq-lpass-reg-update(lpaif_base + regOffset, mask, fieldValue, 0);
/*
 * End First step: Update LPASS_LPAIF_LPAIF_PCM_CTLa
 * Seoncd phase: LPASS_LPAIF_LPAIF_PCM_TDM_CTL_a
 * Setup enable-tdm, sync-delay, rate, enable diff
 */
	mask = HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_TDM_BMSK |
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_BMSK |
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_RATE_BMSK |
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_DIFF_SAMPLE_WIDTH_BMSK;
/*
 * Enable TDM mode
 */
	fieldValue  = HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_TDM_ENABLE_FVAL
		<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_TDM_SHFT;
/*
 * Figure out sync delay, delay 1 is the typical use case
 */
	switch(configPtr->sync_data_delay){
	case TDM_DATA_DELAY_0_CYCLE:
		fieldValue |=
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_DELAY_0_CYCLE_FVAL <<
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_SHFT;
	break;
	case TDM_DATA_DELAY_1_CYCLE:
	default:
		fieldValue |=
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_DELAY_1_CYCLE_FVAL <<
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_SHFT;
	break;
	case TDM_DATA_DELAY_2_CYCLE:
		fieldValue |=
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_DELAY_2_CYCLE_FVAL <<
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_SYNC_DELAY_SHFT;
	break;
	}
/*
 * Figure out the rate = slot x slot-width..  rate should be limited
 */
	nbitsPerFrame = configPtr->nslots_per_frame * configPtr->slot_width;

	fieldValue |= (nbitsPerFrame - 1) <<
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_RATE_SHFT;
/*
 * Based on slot-width, enable or disable DIFF support
 */
	if(configPtr->bit_width != configPtr->slot_width){
		fieldValue |=
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_DIFF_SAMPLE_WIDTH_ENABLE_FVAL
		<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_DIFF_SAMPLE_WIDTH_SHFT;
	} else {
		fieldValue |=
		HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_DIFF_SAMPLE_WIDTH_DISABLE_FVAL
		<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_ENABLE_DIFF_SAMPLE_WIDTH_SHFT;
	}
/*
 * Invert sync or not only for LONG SYNC mode ??
 * Setup the width regardless of DIFF mode.
 */
	if (TDM_SINK == dir){
		mask |= HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_INV_TPCM_SYNC_BMSK;
		fieldValue |= configPtr->sync_invert
			<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_INV_TPCM_SYNC_SHFT;

		mask |= HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_TPCM_WIDTH_BMSK;
		fieldValue |= (configPtr->bit_width - 1)
			<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_TPCM_WIDTH_SHFT;
	} else {
		mask |= HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_INV_RPCM_SYNC_BMSK;
		fieldValue |= configPtr->sync_invert
			<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_INV_RPCM_SYNC_SHFT;

		mask |= HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_RPCM_WIDTH_BMSK;
		fieldValue |= (configPtr->bit_width - 1)
			<< HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_TDM_RPCM_WIDTH_SHFT;
	}
	regOffset = HWIO_LPASS_LPAIF_PCM_TDM_CTL_a_OFFS(pcm_index);
	ipq-lpass-reg-update(lpaif_base + regOffset, mask, fieldValue, 0);
/*
 * End seoncd phase: LPASS_LPAIF_LPAIF_PCM_TDM_CTL_a
 * Phase 3: Setup LPASS_LPAIF_LPAIF_PCM_TDM_SAMPLE_WIDTH_a if DIFF mode
 */
	if(configPtr->bit_width != configPtr->slot_width){
		if(TDM_SINK == dir){
			mask = HWIO_LPASS_LPAIF_PCM_TDM_SAMPLE_WIDTH_a_TDM_TPCM_SAMPLE_WIDTH_BMSK;
			fieldValue = (configPtr->slot_width - 1)
				<< HWIO_LPASS_LPAIF_PCM_TDM_SAMPLE_WIDTH_a_TDM_TPCM_SAMPLE_WIDTH_SHFT;
		} else {
			mask = HWIO_LPASS_LPAIF_PCM_TDM_SAMPLE_WIDTH_a_TDM_RPCM_SAMPLE_WIDTH_BMSK;
			fieldValue = (configPtr->slot_width - 1)
				<< HWIO_LPASS_LPAIF_PCM_TDM_SAMPLE_WIDTH_a_TDM_RPCM_SAMPLE_WIDTH_SHFT;
		}
	regOffset = HWIO_LPASS_LPAIF_PCM_TDM_SAMPLE_WIDTH_a_OFFS(pcm_index);
	ipq-lpass-reg-update(lpaif_base + regOffset, mask, fieldValue, 0);
	}
/*
 * End Phase 3: LPASS_LPAIF_LPAIF_PCM_TDM_SAMPLE_WIDTH_a if DIFF mode
 * Phase 4: slot mask, LPASS_LPAIF_LPAIF_PCM_TPCM_SLOT_NUM_a or
 * LPASS_LPAIF_LPAIF_PCM_RPCM_SLOT_NUM_a
 */
	if(TDM_SINK == dir){
		regOffset = HWIO_LPASS_LPAIF_PCM_TPCM_SLOT_NUM_a_OFFS(pcm_index);
		mask = HWIO_LPASS_LPAIF_PCM_TPCM_SLOT_NUM_a_RMSK;
	} else {
		regOffset = HWIO_LPASS_LPAIF_PCM_RPCM_SLOT_NUM_a_OFFS(pcm_index);
		mask = HWIO_LPASS_LPAIF_PCM_RPCM_SLOT_NUM_a_RMSK;
	}
	fieldValue = configPtr->slot_mask;
	ipq-lpass-reg-update(lpaif_base + regOffset, mask, fieldValue, 0);
/*
 * End Phase 4: slot mask
 */
}
EXPORT_SYMBOL(ipq_lpass_pcm_config);

void ipq_lpass_pcm_enable(void __iomem *lpaif_base,
				uint32_t pcm_index, uint32_t dir)
{
	uint32_t mask,field_value;

	if(TDM_SINK == dir){
/*
 * enable transmit path
 */
		field_value = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_TX_ENABLE_FVAL <<
				HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_TX_SHFT;
		mask = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_TX_BMSK;
	} else {
/*
 * enable receive path
 */
		field_value = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_ENABLE_FVAL <<
				HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_SHFT;
		mask = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_BMSK;
	}

	ipq-lpass-reg-update(
		HWIO_LPASS_LPAIF_PCM_CTLa_ADDR(lpaif_base, pcm_index),
		mask, field_value, 0); ///< update register
}

EXPORT_SYMBOL(ipq_lpass_pcm_enable);

void ipq_lpass_pcm_disable(void __iomem *lpaif_base,
					uint32_t pcm_index, uint32_t dir)
{
	uint32_t mask,field_value;

	if(TDM_SINK == dir){
/*
 * disable transmit path
 */
		field_value = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_TX_DISABLE_FVAL <<
				HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_TX_SHFT;
		mask = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_TX_BMSK;
	} else {
/*
 * disable receive path
 */
		field_value = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_DISABLE_FVAL <<
				HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_SHFT;
		mask = HWIO_LPASS_LPAIF_PCM_CTLa_ENABLE_RX_BMSK;
	}

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_PCM_CTLa_ADDR(lpaif_base, pcm_index),
		mask, field_value, 0); ///< update register
}
EXPORT_SYMBOL(ipq_lpass_pcm_disable);

void ipq_lpass_enable_dma_channel(void __iomem *lpaif_base,
						uint32_t dma_idx,
						uint32_t dma_dir)
	{
	uint32_t mask, field_value, offset;

	if(LPASS_HW_DMA_SINK == dma_dir){
		mask = HWIO_LPASS_LPAIF_RDDMA_CTLa_ENABLE_ON_FVAL;
		field_value =
			HWIO_LPASS_LPAIF_RDDMA_CTLa_ENABLE_BMSK <<
			HWIO_LPASS_LPAIF_RDDMA_CTLa_ENABLE_SHFT;
		offset = HWIO_LPASS_LPAIF_RDDMA_CTLa_OFFS(dma_idx);
	} else {
		mask = HWIO_LPASS_LPAIF_WRDMA_CTLa_ENABLE_ON_FVAL;
		field_value =
			HWIO_LPASS_LPAIF_WRDMA_CTLa_ENABLE_BMSK <<
			HWIO_LPASS_LPAIF_WRDMA_CTLa_ENABLE_SHFT;
		offset = HWIO_LPASS_LPAIF_WRDMA_CTLa_OFFS(dma_idx);
	}
	ipq-lpass-reg-update(lpaif_base + offset, mask, field_value, 0);
}
EXPORT_SYMBOL(ipq_lpass_enable_dma_channel);

void ipq_lpass_dma_disable_dma_channel(void __iomem *lpaif_base,
						uint32_t dma_idx,
						uint32_t dma_dir)
{
	uint32_t mask, field_value, offset;

	if(LPASS_HW_DMA_SINK == dma_dir){
		mask = HWIO_LPASS_LPAIF_RDDMA_CTLa_ENABLE_ON_FVAL |
			HWIO_LPASS_LPAIF_RDDMA_CTLa_AUDIO_INTF_BMSK;
		field_value = (HWIO_LPASS_LPAIF_RDDMA_CTLa_ENABLE_OFF_FVAL <<
					HWIO_LPASS_LPAIF_RDDMA_CTLa_ENABLE_SHFT)|
			(HWIO_LPASS_LPAIF_RDDMA_CTLa_AUDIO_INTF_NONE_FVAL <<
				HWIO_LPASS_LPAIF_RDDMA_CTLa_AUDIO_INTF_SHFT);
		offset = HWIO_LPASS_LPAIF_RDDMA_CTLa_OFFS(dma_idx);
	} else {
		mask = HWIO_LPASS_LPAIF_WRDMA_CTLa_ENABLE_ON_FVAL |
				HWIO_LPASS_LPAIF_WRDMA_CTLa_AUDIO_INTF_BMSK;
		field_value = (HWIO_LPASS_LPAIF_WRDMA_CTLa_ENABLE_OFF_FVAL <<
					HWIO_LPASS_LPAIF_WRDMA_CTLa_ENABLE_SHFT)|
		(HWIO_LPASS_LPAIF_WRDMA_CTLa_AUDIO_INTF_NONE_FVAL <<
			HWIO_LPASS_LPAIF_WRDMA_CTLa_AUDIO_INTF_SHFT);
		offset = HWIO_LPASS_LPAIF_WRDMA_CTLa_OFFS(dma_idx);
	}
	ipq-lpass-reg-update(lpaif_base + offset, mask,field_value,0);
}
EXPORT_SYMBOL(ipq_lpass_dma_disable_dma_channel);

void ipq_lpass_config_dma_channel(struct lpass_dma *config,
					uint32_t dma_dir, uint32_t dma_idx)
{
	if(LPASS_HW_DMA_SINK == dma_dir){
		ipq_lpass_dma_config_dma_channel_sink(config, dma_idx);
	} else {
		ipq_lpass_dma_config_dma_channel_source(config, dma_idx);
	}
}
EXPORT_SYMBOL(ipq_lpass_config_dma_channel);

static void ipq_lpass_dma_clear_interrupt(void __iomem *lpaif_base,
						uint32_t dma_dir,
						uint32_t dma_idx,
						uint32_t dma_intr_idx)
{
	uint32 mask;
	uint32 clear_mask = 0;

	if(LPASS_HW_DMA_SINK == dma_dir){
		switch (dma_idx){
	/*
	 * For RDDMA <=4, use IRQ_CLEAR macros
	*/
		case 0:
			clear_mask = (HWIO_LPASS_LPAIF_IRQ_CLEARa_PER_RDDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_UNDR_RDDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_ERR_RDDMA_CH0_BMSK);
		break;
		case 1:
			clear_mask = (HWIO_LPASS_LPAIF_IRQ_CLEARa_PER_RDDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_UNDR_RDDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_ERR_RDDMA_CH1_BMSK);
		break;
		default:
		break;
		}
	} else {
		switch (dma_idx){
	/*
	 * For WRDMA <= 3, use IRQ_CLEAR macros
	 */
		case 0:
			clear_mask = (HWIO_LPASS_LPAIF_IRQ_CLEARa_PER_WRDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_OVR_WRDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_ERR_WRDMA_CH0_BMSK);
		break;
		case 1:
			clear_mask = (HWIO_LPASS_LPAIF_IRQ_CLEARa_PER_WRDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_OVR_WRDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_CLEARa_ERR_WRDMA_CH1_BMSK);
		break;
		default:
		break;
		}
	}

	mask = 0x0;

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_IRQ_CLEARa_ADDR(
				lpaif_base, dma_intr_idx),
				mask, clear_mask, 1);
}
EXPORT_SYMBOL(ipq_lpass_dma_clear_interrupt);

void ipq_lpass_dma_disable_interrupt(void __iomem *lpaif_base,
						uint32_t dma_dir,
						uint32_t dma_idx,
						uint32_t dma_intr_idx)
{
	uint32_t disable_mask=0;


	if(LPASS_HW_DMA_SINK == dma_dir){
		switch (dma_idx){
	/*
	 * For RDDMA <=4, use IRQ_EN macros
	 */
		case 0:
			disable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_RDDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_UNDR_RDDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_RDDMA_CH0_BMSK);
		break;
		case 1:
			disable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_RDDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_UNDR_RDDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_RDDMA_CH1_BMSK);
		break;
		default:
		break;
		}
	} else {
		switch (dma_idx){
		case 0:
			disable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_WRDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_OVR_WRDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_WRDMA_CH0_BMSK);
		break;
		case 1:
			disable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_WRDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_OVR_WRDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_WRDMA_CH1_BMSK);
		break;
		default:
		break;
		}
	}

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_IRQ_ENa_ADDR(
				lpaif_base, dma_intr_idx),
				disable_mask, 0x0, 0);
}
EXPORT_SYMBOL(ipq_lpass_dma_disable_interrupt);

void ipq_lpass_dma_enable_interrupt(void __iomem *lpaif_base,
						uint32_t dma_dir,
						uint32_t dma_idx,
						uint32_t dma_intr_idx)
{
	uint32_t enable_mask = 0;

	if(LPASS_HW_DMA_SINK == dma_dir){
		switch (dma_idx){
	/*
	 * For RDDMA <=4, use IRQ_EN macros
	 */
		case 0:
			enable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_RDDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_UNDR_RDDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_RDDMA_CH0_BMSK);
		break;
		case 1:
			enable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_RDDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_UNDR_RDDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_RDDMA_CH1_BMSK);
		break;
		default:
		break;
		}
	} else {
		switch (dma_idx){
		case 0:
			enable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_WRDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_OVR_WRDMA_CH0_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_WRDMA_CH0_BMSK);
		break;
		case 1:
			enable_mask = (HWIO_LPASS_LPAIF_IRQ_ENa_PER_WRDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_OVR_WRDMA_CH1_BMSK
				| HWIO_LPASS_LPAIF_IRQ_ENa_ERR_WRDMA_CH1_BMSK);
		break;
		default:
		break;
		}
	}

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_IRQ_ENa_ADDR(
				lpaif_base, dma_intr_idx),
				enable_mask,enable_mask,0);
}
EXPORT_SYMBOL(ipq_lpass_dma_enable_interrupt);

void ipq_lpass_dma_get_dma_int_stc(void __iomem *lpaif_base,
					uint32_t dma_dir, uint32_t dma_idx)
{
	uint32  lsb, msb1, msb2;

/*
 * Whenever DMA interrupt happens,
 * the DMA STC registers latch on to the STC value (from AV-timer)
 * at that instant.
 */

	if(LPASS_HW_DMA_SINK == dma_dir){
		msb1 = readl(lpaif_base + HWIO_LPASS_LPAIF_RDDMA_STC_MSBa_OFFS(dma_idx));
		lsb = readl(lpaif_base + HWIO_LPASS_LPAIF_RDDMA_STC_LSBa_OFFS(dma_idx));
		msb2 = readl(lpaif_base + HWIO_LPASS_LPAIF_RDDMA_STC_MSBa_OFFS(dma_idx));
		if(msb1 != msb2){
			lsb = readl(lpaif_base +
				HWIO_LPASS_LPAIF_RDDMA_STC_LSBa_OFFS(dma_idx));
		}
		lsb = (lsb & HWIO_LPASS_LPAIF_RDDMA_STC_LSBa_STC_LSB_BMSK) >>
				HWIO_LPASS_LPAIF_RDDMA_STC_LSBa_STC_LSB_SHFT;
		msb2 = (msb2 & HWIO_LPASS_LPAIF_RDDMA_STC_MSBa_STC_MSB_BMSK) >>
				HWIO_LPASS_LPAIF_RDDMA_STC_MSBa_STC_MSB_SHFT;
		*p_stc = ((uint64)msb2 << 32) | lsb;
	} else {
		msb1 = readl(lpaif_base + HWIO_LPASS_LPAIF_WRDMA_STC_MSBa_OFFS(dma_idx));
		lsb = readl(lpaif_base + HWIO_LPASS_LPAIF_WRDMA_STC_LSBa_OFFS(dma_idx));
		msb2 = readl(lpaif_base + HWIO_LPASS_LPAIF_WRDMA_STC_MSBa_OFFS(dma_idx));
		if(msb1 != msb2) {
			lsb = readl(lpaif_base +
				HWIO_LPASS_LPAIF_WRDMA_STC_LSBa_OFFS(dma_idx));
		}
		lsb = (lsb & HWIO_LPASS_LPAIF_WRDMA_STC_LSBa_STC_LSB_BMSK) >>
				HWIO_LPASS_LPAIF_WRDMA_STC_LSBa_STC_LSB_SHFT;
		msb2 = (msb2 & HWIO_LPASS_LPAIF_RDDMA_STC_MSBa_STC_MSB_BMSK) >>
				HWIO_LPASS_LPAIF_RDDMA_STC_MSBa_STC_MSB_SHFT;
		*p_stc = ((uint64)msb2 << 32) | lsb;
	}
}
EXPORT_SYMBOL(ipq_lpass_dma_get_dma_int_stc);

void ipq_lpass_dma_get_dma_fifo_count(void __iomem *lpaif_base,
					uint32_t dma_dir, uint32_t dma_idx)
{
	uint32_t fifo_count = 0;
	if(LPASS_HW_DMA_SINK == dma_dir){
		fifo_count = readl(lpaif_base +
				HWIO_LPASS_LPAIF_RDDMA_PERa_OFFS(dma_idx));
		*fifo_cnt_ptr = (fifo_count &
			HWIO_LPASS_LPAIF_RDDMA_PERa_PERIOD_FIFO_BMSK) >>
				HWIO_LPASS_LPAIF_RDDMA_PERa_PERIOD_FIFO_SHFT;
	} else {
		fifo_count = readl(lpaif_base +
				HWIO_LPASS_LPAIF_WRDMA_PERa_OFFS(dma_idx));
		*fifo_cnt_ptr = (fifo_count &
			HWIO_LPASS_LPAIF_WRDMA_PERa_PERIOD_FIFO_BMSK) >>
				HWIO_LPASS_LPAIF_WRDMA_PERa_PERIOD_FIFO_SHFT;
	}
}
EXPORT_SYMBOL(ipq_lpass_dma_get_dma_fifo_count);

static void ipq_lpass_dma_config_channel_sink(struct lpass_dma *config,
							uint32_t dma_idx)
{
	uint32_t mask,field_value;

/*
 * Update the base address of the DMA buffer
 */
	mask = HWIO_LPASS_LPAIF_RDDMA_BASEa_BASE_ADDR_BMSK;
	field_value = ((uint32)config->buffer_start_addr >>
			HWIO_LPASS_LPAIF_RDDMA_BASEa_BASE_ADDR_SHFT)
				<< HWIO_LPASS_LPAIF_RDDMA_BASEa_BASE_ADDR_SHFT;
/*
 * READ DMA
 */
	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_RDDMA_BASEa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);


/*
 * Update the buffer length of the DMA buffer.
 */
	mask = HWIO_LPASS_LPAIF_RDDMA_BUFF_LENa_LENGTH_BMSK;
	field_value = (config->buffer_len-1) ;


	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_RDDMA_BUFF_LENa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);

/*
 * Update the interrupt sampler period
 */
	mask = HWIO_LPASS_LPAIF_RDDMA_PER_LENa_LENGTH_BMSK;
	field_value = (config->dma_int_per_cnt-1) <<
				HWIO_LPASS_LPAIF_RDDMA_PER_LENa_LENGTH_SHFT;


	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_RDDMA_PER_LENa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);

/*
 * Update the audio interface and fifo Watermark in AUDIO_DMA_CTLa(channel)
 * register. Obtain these values from the input configuration structure
 */
	mask = HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_BMSK |
	HWIO_LPASS_LPAIF_RDDMA_CTLa_AUDIO_INTF_BMSK |
	HWIO_LPASS_LPAIF_RDDMA_CTLa_FIFO_WATERMRK_BMSK |
	HWIO_LPASS_LPAIF_RDDMA_CTLa_WPSCNT_BMSK;


	field_value = (config->ifconfig) <<
				HWIO_LPASS_LPAIF_RDDMA_CTLa_AUDIO_INTF_SHFT;

	field_value |=((config->watermark-1) <<
				HWIO_LPASS_LPAIF_RDDMA_CTLa_FIFO_WATERMRK_SHFT);

	if((HWIO_LPASS_LPAIF_RDDMA_CTLa_FIFO_WATERMRK_ENUM_8_FVAL + 1) <
		config->dma_int_per_cnt){
	/*
	 * enable BURST basing on burst_size
	 */
		switch(config->burst_size){
		case 4:
		case 8:
		case 16:
			field_value |= (HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_INCR4_FVAL <<
					HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_SHFT);
		break;
		case 1:
			field_value |= (HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_SINGLE_FVAL <<
					HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_SHFT);
		break;
		default:
		break;
		}
	} else {
		field_value |= (HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_SINGLE_FVAL <<
					HWIO_LPASS_LPAIF_RDDMA_CTLa_BURST_EN_SHFT);
	}

	field_value |=  ((dma_config_ptr->wps_count - 1) <<
				HWIO_LPASS_LPAIF_RDDMA_CTLa_WPSCNT_SHFT);

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_RDDMA_CTLa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);

}

void ipq_lpass_dma_config_channel_source(struct lpass_dma *config,
                                                        uint32_t dma_idx)
{
	uint32_t mask,field_value;
/*
 * Update the base address of the DMA buffer
 */
	mask = HWIO_LPASS_LPAIF_WRDMA_BASEa_BASE_ADDR_BMSK;
	field_value = ((uint32)config->buffer_start_addr >>
			HWIO_LPASS_LPAIF_WRDMA_BASEa_BASE_ADDR_SHFT)
				<< HWIO_LPASS_LPAIF_WRDMA_BASEa_BASE_ADDR_SHFT;

/*
 * WRITE DMA
 */
	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_WRDMA_BASEa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);

/*
 * Update the buffer length of the DMA buffer.
 */
	mask = HWIO_LPASS_LPAIF_WRDMA_BUFF_LENa_LENGTH_BMSK;
	field_value = (config->buffer_len-1);

/*
 * WRDMA
 */
	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_WRDMA_BUFF_LENa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);
/*
 * Update the interrupt sampler period
 */
	mask = HWIO_LPASS_LPAIF_WRDMA_PER_LENa_LENGTH_BMSK;
	field_value=(config->dma_int_per_cnt-1) <<
			HWIO_LPASS_LPAIF_WRDMA_PER_LENa_LENGTH_SHFT;

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_WRDMA_PER_LENa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);

/*
 * Update the audio interface and fifo Watermark in AUDIO_DMA_CTLa(channel)
 * register. Obtain these values from the input configuration structure
 */
	mask = HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_BMSK |
	HWIO_LPASS_LPAIF_WRDMA_CTLa_AUDIO_INTF_BMSK |
	HWIO_LPASS_LPAIF_WRDMA_CTLa_FIFO_WATERMRK_BMSK |
	HWIO_LPASS_LPAIF_WRDMA_CTLa_WPSCNT_BMSK;

	field_value = (config->ifconfig) <<
			HWIO_LPASS_LPAIF_WRDMA_CTLa_AUDIO_INTF_SHFT;

	field_value |=((config->watermark-1) <<
			HWIO_LPASS_LPAIF_WRDMA_CTLa_FIFO_WATERMRK_SHFT );

	if((HWIO_LPASS_LPAIF_WRDMA_CTLa_FIFO_WATERMRK_ENUM_8_FVAL + 1) <
		config->dma_int_per_cnt){
	/*
	 * enable BURST basing on burst_size
	 */
		switch(config->burst_size){
		case 4:
		case 8:
		case 16:
			field_value |= (HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_INCR4_FVAL <<
					HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_SHFT);
		break;
		case 1:
			field_value |= (HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_SINGLE_FVAL <<
					HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_SHFT);
		break;
		default:
		break;
		}
	} else {
		field_value |= (HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_SINGLE_FVAL <<
					HWIO_LPASS_LPAIF_WRDMA_CTLa_BURST_EN_SHFT);
	}

	field_value |=  ((config->wps_count - 1) <<
					HWIO_LPASS_LPAIF_WRDMA_CTLa_WPSCNT_SHFT);

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_WRDMA_CTLa_ADDR(
			config->lpaif_base,config->idx), mask, field_value, 0);
}

void ipq_lpass_dma_read_interrupt_status(void __iomem *lpaif_base,
						uint32_t dma_intr_idx,
						uint32_t *status)
{
	uint32_t read_status;
	read_status = readl(HWIO_LPASS_LPAIF_IRQ_STATa_ADDR(
					lpaif_base + dma_intr_idx));
	*status = read_status
}
EXPORT_SYMBOL(ipq_lpass_dma_read_interrupt_status);

void ipq_lpass_dma_clear_interrupt(void __iomem *lpaif_base,
						uint32_t dma_intr_idx,
						uint32_t value)
{
	uint32_t mask;

	mask = value;

	ipq-lpass-reg-update(HWIO_LPASS_LPAIF_IRQ_CLEARa_ADDR(
			lpaif_base, dma_intr_idx), mask, value, 1);
}
EXPORT_SYMBOL(ipq_lpass_dma_clear_interrupt);

void ipq_lpass_dma_reset(void __iomem *lpaif_base,
                                                uint32_t dma_idx,
                                                uint32_t dma_dir)
{
	uint32_t mask,field_value;

	if(LPASS_HW_DMA_SINK == dma_dir){
		mask = HWIO_LPASS_LPAIF_RDDMA_CTLa_RMSK;
		field_value =  HWIO_LPASS_LPAIF_RDDMA_CTLa_POR;
		ipq-lpass-reg-update(HWIO_LPASS_LPAIF_RDDMA_CTLa_ADDR(
			lpaif_base, dma_idx, mask, field_value, 0);

	} else {
		mask = HWIO_LPASS_LPAIF_WRDMA_CTLa_RMSK;
		field_value = HWIO_LPASS_LPAIF_WRDMA_CTLa_POR;
		ipq-lpass-reg-update(HWIO_LPASS_LPAIF_WRDMA_CTLa_ADDR(
			lpaif_base, dma_idx, mask, field_value, 0);
	}
}
EXPORT_SYMBOL(ipq_lpass_dma_reset);

static const struct of_device_id ipq_audio_lpass_id_table[] = {
	{ .compatible = "qca,ipq50xx-audio-lpass", .data = &ipq50xx_devconfig},
	{},
};
MODULE_DEVICE_TABLE(of, ipq_audio_lpass_id_table);

static int ipq_audio_lpass_probe(struct platform_device *pdev)
{
	struct resource *res;
	const struct of_device_id *match;

	match = of_match_device(ipq_audio_lpass_id_table, &pdev->dev);
	if (!match)
		return -ENODEV;

	ipq_lpass_cfgs = (lpass_info_t *)match->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sg_ipq_lpass_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sg_ipq_lpass_base))
		return PTR_ERR(lpass_audio_local_base);

	return 0;
}

static int ipq_audio_lpass_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver ipq_audio_lpass_driver = {
	.probe = ipq_audio_lpass_probe,
	.remove = ipq_audio_lpass_remove,
	.driver = {
		.name = "ipq-lpass",
		.of_match_table = ipq_audio_lpass_id_table,
	},
};

module_platform_driver(ipq_audio_lpass_driver);

MODULE_ALIAS("platform:ipq-lpass");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("IPQ Audio subsytem driver");
