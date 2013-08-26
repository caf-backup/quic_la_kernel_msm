/* Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#ifndef _IPQ_PCM_H
#define _IPQ_PCM_H

enum ipq_stream_channels {
	IPQ_CHANNEL_MONO = 1,
	IPQ_CHANNELS_STEREO = 2,
	IPQ_CHANNELS_4 = 4,
	IPQ_CHANNELS_6 = 6,
	IPQ_CHANNELS_8 = 8,
};

struct ipq_pcm_stream_t {
	uint8_t pcm_prepare_start;
	uint32_t period_index;
	struct snd_pcm_substream *substream;
};

struct ipq_dml_t {
	uint8_t	dml_dma_started;
	dma_addr_t dml_start_addr;
	dma_addr_t dml_src_addr;
	dma_addr_t dml_dst_addr;
	ssize_t dml_transfer_size;
};

struct ipq_lpa_if_t {
	uint8_t lpa_if_dma_start;
	uint8_t dma_ch;
};

struct ipq_lpass_runtime_data_t {
	struct ipq_pcm_stream_t pcm_stream_info;
	struct ipq_dml_t dml_info;
	struct ipq_lpa_if_t lpaif_info;
};

enum dma_intf_wr_ch {
	MIN_DMA_WR_CH = 5,
	PCM0_DMA_WR_CH = 5,
	PCM1_DMA_WR_CH,
	MAX_DMA_WR_CH = 8,
};

enum dma_intf_rd_ch {
	MIN_DMA_RD_CH = 0,
	MI2S_DMA_RD_CH = 0,
	PCM0_DMA_RD_CH,
	PCM1_DMA_RD_CH,
	MAX_DMA_RD_CH = 4,
};
#endif /*_IPQ_PCM_H */
