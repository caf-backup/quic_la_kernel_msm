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
 */

#ifndef _IPQ_LPASS_TDM_PCM_H
#define _IPQ_LPASS_TDM_PCM_H

#define DEFAULT_PCM_DMA_BUFFERS			2
#define MAX_PCM_DMA_BUFFERS			4
#define PCM_DMA_BUFFER_16BYTE_ALIGNMENT		0xF
#define TDM_DMA_BUFFER_OFFSET_ALIGNMENT		0x1F
#define IPQ_PCM_MAX_SLOTS			4
#define DEAFULT_PCM_WATERMARK			8
#define IPQ_LPASS_MAX_LPM_BLK			4
#define IPQ_PCM_SAMPLES_PER_10MS(rate) ((rate / 1000) * 10)
#define BYTES_PER_CHANNEL(a_bit_width)	(a_bit_width > 16) ? 4 : 2
#define DMA_CHANNEL_ID(a_port_id)	(a_port_id - 1)

#define IPQ_PCM_BYTES_PER_SAMPLE_MAX		4
#define IPQ_PCM_MAX_CHANNEL_CNT			32
#define IPQ_PCM_MAX_SLOTS_PER_FRAME		32

/** Short (one-bit) Synchronization mode. */
#define PCM_TDM_SHORT_SYNC_BIT_MODE		0

/** Long Synchronization mode. */
#define PCM_TDM_LONG_SYNC_MODE			1

/** Short (one-slot) Synchronization mode. */
#define PCM_TDM_SHORT_SYNC_SLOT_MODE		2

/** Synchronization source is external. */
#define PCM_TDM_SYNC_SRC_EXTERNAL		0

/** Synchronization source is internal. */
#define PCM_TDM_SYNC_SRC_INTERNAL		1

/** Disable sharing of the data-out signal. */
#define PCM_TDM_CTRL_DATA_OE_DISABLE		0

/** Enable sharing of the data-out signal. */
#define PCM_TDM_CTRL_DATA_OE_ENABLE		1

/** Normal synchronization. */
#define PCM_TDM_SYNC_NORMAL			0

/** Invert the synchronization. */
#define PCM_TDM_SYNC_INVERT			1

#define PCM_32BIT_FORMAT			31

#define PCM_16BIT_FORMAT			15

#define 32BIT_PCM_FORMAT			27

#define SHIFT_FACTOR		(PCM_32BIT_FORMAT - 32BIT_PCM_FORMAT)

/*
 * TDM codec can be in either aux or pcm modes.
 * PCM mode is also known as short
 * sync mode & AUX mode is also known as long sync mode
 */
enum ipq_lpass_tdm_sync
{
  TDM_SHORT_SYNC_TYPE = 0,
  TDM_LONG_SYNC_TYPE,
  TDM_SLOT_SYNC_TYPE,
};

/*
 * TDM long sync can be inverted.
 */
enum ipq_lpass_tdm_longsyncinfo
{
  TDM_LONG_SYNC_NORMAL = 0,
  TDM_LONG_SYNC_INVERT
};

/*
 * TDM data delay relative to sync.
 * This is hardware value..
 */
enum ipq_lpass_tdm_syncdelayinfo
{
  TDM_DATA_DELAY_0_CYCLE = 2,
  TDM_DATA_DELAY_1_CYCLE = 1,
  TDM_DATA_DELAY_2_CYCLE = 0
};

/** Zero-bit clock cycle synchronization data delay. */
#define PCM_TDM_DATA_DELAY_0_BCLK_CYCLE		0

/** One-bit clock cycle synchronization data delay. */
#define PCM_TDM_DATA_DELAY_1_BCLK_CYCLE		1

/** Two-bit clock cycle synchronization data delay. @newpage */
#define PCM_TDM_DATA_DELAY_2_BCLK_CYCLE		2

#define PCM_SAMPLE_RATE_8K			8000
#define PCM_SAMPLE_RATE_11_025K			11025
#define PCM_SAMPLE_RATE_12K			12000
#define PCM_SAMPLE_RATE_16K			16000
#define PCM_SAMPLE_RATE_22_05K			22050
#define PCM_SAMPLE_RATE_24K			24000
#define PCM_SAMPLE_RATE_32K			32000
#define PCM_SAMPLE_RATE_44_1K			44100
#define PCM_SAMPLE_RATE_88_2K			88200
#define PCM_SAMPLE_RATE_48K			48000
#define PCM_SAMPLE_RATE_96K			96000
#define PCM_SAMPLE_RATE_176_4K			176400
#define PCM_SAMPLE_RATE_192K			192000
#define PCM_SAMPLE_RATE_352_8K			352800
#define PCM_SAMPLE_RATE_384K			384000

struct lpass_dma_buffer {
	uint16_t idx;
	uint16_t dir;
	uint32_t watermark;
	uint32_t no_of_buff;
	uint32_t lpm_mem_location;
	uint32_t size_max;
	uint32_t single_buf_size;
};

struct lpass_lpm_block {
	uint16_t block_id;
	uint16_t allocated;
	uint16_t size;
	uint32_t base;
};
struct lpass_lpm_info {
	struct lpass_lpm_block[IPQ_LPASS_MAX_LPM_BLK]
	uint32_t base;
	uint32_t size;
};

struct lpm_mem_info{
	uint16_t block_mask;
	uint16_t size;
	uint32_t address;
};

struct ipq_lpass_pcm_params {
	uint32_t num_channels;
	uint32_t sample_rate;
	uint32_t bit_width;
	uint16_t data_format;
	uint16_t sync_mode;
	uint16_t sync_src;
	uint16_t nslots_per_frame;
	uint16_t ctrl_data_out_enable;
	uint16_t ctrl_invert_sync_pulse;
	uint16_t ctrl_sync_data_delay;
	uint16_t slot_width;
	uint32_t slot_mask;
};


int ipq_lpass_pcm_init(struct ipq_lpass_pcm_params *params);
void ipq_lpass_pcm_deinit(struct ipq_lpass_pcm_params *params);
uint32_t ipq_lpass_pcm_data(uint8_t **rx_buf, uint8_t **tx_buf);
void ipq_lpass_pcm_done(void);
void ipq_lpass_pcm_send_event(void);

#endif /*_IPQ_LPASS_TDM_PCM_H*/
