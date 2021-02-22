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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/of_device.h>

#include "ipq-lpass.h"
#include "ipq-lpass-tdm-pcm.h"

#define DEFAULT_CLK_RATE		2048000

/*
 * Global Constant Definitions
 */
void __iomem *ipq_lpass_lpaif_base;
void __iomem *ipq_lpass_lpm_base;

/*
 * Static Variable Definitions
 */
static atomic_t data_avail;
static atomic_t tx_add;
static atomic_t rx_add;
static struct lpass_dma_buffer *rx_dma_buffer;
static struct lpass_dma_buffer *tx_dma_buffer;
static struct platform_device *pcm_pdev;
static spinlock_t pcm_lock;
static struct ipq_lpass_pcm_params *pcm_params;

static DECLARE_WAIT_QUEUE_HEAD(pcm_q);

static uint32_t ipq_lpass_pcm_get_dataptr(struct lpass_dma_buffer *buffer,
							uint32_t addr)
{
	uint32_t dataptr;
	uint32_t last_addr;
	uint32_t size;

	last_addr = buffer->dma_last_curr_addr;
	dataptr= buffer->dma_last_curr_addr;
	buffer->dma_last_curr_addr = addr;

	if(addr < last_addr){
		size = ((buffer->dma_base_address + buffer->dma_buffer_size) -
				last_addr);
	buffer->dma_last_curr_addr = buffer->dma_base_address;
	} else {
		size = addr - last_addr;
	}

	buffer->size_done = size;

	return dataptr;
}

/*
 * FUNCTION: ipq_lpass_pcm_playback_handler
 *
 * DESCRIPTION: pcm rx dma irq handler
 *
 * RETURN VALUE: none
 */
static irqreturn_t ipq_lpass_pcm_playback_handler(int intrsrc, void *data)
{
	uint32_t status;
	uint32_t txcurr_addr;
	struct lpass_dma_buffer *buffer = (struct lpass_dma_buffer *)data;

	ipq_lpass_dma_read_interrupt_status(ipq_lpass_lpaif_base,
						buffer->intr_id, &status);

	ipq_lpass_dma_get_curr_addr(ipq_lpass_lpaif_base, buffer->idx,
					buffer->dir, &txcurr_addr);

	ipq_lpass_dma_clear_interrupt(ipq_lpass_lpaif_base,
					buffer->intr_id, status);

	txcurr_addr = ipq_lpass_pcm_get_dataptr(buffer, txcurr_addr);

	atomic_set(&tx_add, txcurr_addr);

	return IRQ_HANDLED;
}

/*
 * FUNCTION: ipq_lpass_pcm_capture_handler
 *
 * DESCRIPTION: pcm dma tx irq handler
 *
 * RETURN VALUE: none
 */
static irqreturn_t ipq_lpass_pcm_capture_handler(int intrsrc, void *data)
{
	uint32_t status;
	uint32_t rxcurr_addr;
	struct lpass_dma_buffer *buffer = (struct lpass_dma_buffer *)data;

	ipq_lpass_dma_read_interrupt_status(ipq_lpass_lpaif_base,
						buffer->intr_id, &status);
	status &= (0x38000);

	if (status & (0x8000)) {
		ipq_lpass_dma_get_curr_addr(ipq_lpass_lpaif_base,
						buffer->idx,
						buffer->dir,
						&rxcurr_addr);

		rxcurr_addr = ipq_lpass_pcm_get_dataptr(buffer, rxcurr_addr);

		atomic_set(&rx_add, rxcurr_addr);

		atomic_set(&data_avail, 1);
	}
	wake_up_interruptible(&pcm_q);

	ipq_lpass_dma_clear_interrupt(ipq_lpass_lpaif_base,
					buffer->intr_id, status);
	return IRQ_HANDLED;
}

/*
 * FUNCTION: ipq_lpass_pcm_validate_params
 *
 * DESCRIPTION: validates the input parameters
 *
 * RETURN VALUE: error if any
 */
uint32_t ipq_lpass_pcm_validate_params(struct ipq_lpass_pcm_params *params,
					struct ipq_lpass_pcm_config *config)
{
	int32_t i, slot_mask;

	memset(config, 0, sizeof(*config));

	if (!params) {
		pr_err("%s: Invalid Params.\n", __func__);
		return -EINVAL;
	}

	/* Bit width supported is 16 , 24 & 32 */
	if (!((params->bit_width == 16) ||
			(params->bit_width == 24) ||
			(params->bit_width == 32))) {
		pr_err("%s: Invalid Bitwidth %d.\n",
				__func__, params->bit_width);
		return -EINVAL;
	}

	if ((PCM_SAMPLE_RATE_8K != params->rate)&&
		(PCM_SAMPLE_RATE_16K != params->rate)&&
		(PCM_SAMPLE_RATE_24K != params->rate)&&
		(PCM_SAMPLE_RATE_32K != params->rate)&&
		(PCM_SAMPLE_RATE_48K != params->rate)&&
		(PCM_SAMPLE_RATE_96K != params->rate)&&
		(PCM_SAMPLE_RATE_192K != params->rate)&&
		(PCM_SAMPLE_RATE_384K != params->rate)&&
		(PCM_SAMPLE_RATE_22_05K != params->rate)&&
		(PCM_SAMPLE_RATE_44_1K != params->rate)&&
		(PCM_SAMPLE_RATE_88_2K != params->rate)&&
		(PCM_SAMPLE_RATE_176_4K != params->rate)&&
		(PCM_SAMPLE_RATE_352_8K != params->rate)){
		 pr_err("%s: Invalid PCM sampling rate %d.\n",
			__func__, params->rate);
		return -EINVAL;
	}

	if (params->slot_count >
			IPQ_PCM_MAX_SLOTS_PER_FRAME){
		pr_err("%s: Invalid nslots per frame %d.\n",
				__func__, params->slot_count);
		return -EINVAL;
	}

	if (512 < (params->slot_count * params->bit_width)){
		pr_err("%s: Invalid nbits per frame %d.\n",
				 __func__,
				params->slot_count * params->bit_width );
		return -EINVAL;
	}

	slot_mask = 0;

	for (i = 0; i < params->active_slot_count; ++i) {
		slot_mask |= (1 << params->tx_slots[i]);
	}

	if (slot_mask == 0) {
		pr_err("%s: Invalid active slot %d.\n",
			 __func__,
			params->active_slot_count);
		return -EINVAL;
	}

	config->slot_mask = slot_mask;
	config->bit_width = params->bit_width;
	config->slot_count = params->slot_count;
	config->slot_width = params->bit_width;
	config->sync_type = TDM_SHORT_SYNC_TYPE;
	config->ctrl_data_oe = TDM_CTRL_DATA_OE_ENABLE;
	config->invert_sync = TDM_LONG_SYNC_NORMAL;
	config->sync_delay = TDM_DATA_DELAY_0_CYCLE;

	pr_info("PCM init bitwidth(%d) slot count (%d) slot mask (0x%x)\n",
			params->bit_width, params->slot_count, slot_mask);

	return 0;
}

static uint32_t ipq_lpass_get_watermark(struct lpass_dma_buffer *buffer)
{
	uint32_t water_mark = DEAFULT_PCM_WATERMARK;
	uint32_t bytes_per_intr;
	uint32_t dword_per_intr;

	bytes_per_intr = buffer->bytes_per_channel *
				buffer->int_samples_per_period *
				buffer->num_channels;

	dword_per_intr = bytes_per_intr >> 2;

	if((DEAFULT_PCM_WATERMARK >= dword_per_intr) ||
		(dword_per_intr & 0x3)  ){
		water_mark = 1;
	}

	return water_mark;
}

static void ipq_lpass_dma_config_init(struct lpass_dma_buffer *buffer)
{
	uint32_t wps_count;
	struct lpass_dma_config dma_config;

	wps_count =
		(buffer->num_channels * buffer->bytes_per_sample) >> 2;
	if(0 == wps_count){
		wps_count = 1;
	}

	if(0 == (buffer->period_count_in_word32 &
			PCM_DMA_BUFFER_16BYTE_ALIGNMENT)){
		dma_config.burst_size = 16;
	} else if(0 == (buffer->period_count_in_word32 &
			PCM_DMA_BUFFER_8BYTE_ALIGNMENT)){
		dma_config.burst_size = 8;
	} else if(0 == (buffer->period_count_in_word32 &
			PCM_DMA_BUFFER_4BYTE_ALIGNMENT)){
		dma_config.burst_size = 4;
	} else {
		dma_config.burst_size = 1;
	}

	dma_config.buffer_len = buffer->dma_buffer_size/sizeof(uint32_t);
	dma_config.buffer_start_addr = buffer->dma_base_address;
	dma_config.dma_int_per_cnt = buffer->period_count_in_word32;
	dma_config.wps_count = wps_count;
	dma_config.watermark = DEAFULT_PCM_WATERMARK;
	dma_config.ifconfig = buffer->ifconfig;
	dma_config.idx = buffer->idx;
	dma_config.burst8_en = 1;
	dma_config.burst16_en = 0;
	dma_config.dir =  buffer->dir;
	dma_config.lpaif_base =  ipq_lpass_lpaif_base;

	ipq_lpass_disable_dma_channel(ipq_lpass_lpaif_base,
					buffer->idx, buffer->dir);

	ipq_lpass_config_dma_channel(&dma_config);

	ipq_lpass_enable_dma_channel(ipq_lpass_lpaif_base,
					buffer->idx, buffer->dir);
}

static void ipq_lpass_dma_enable(struct lpass_dma_buffer *buffer)
{
	ipq_lpass_dma_clear_interrupt_config(ipq_lpass_lpaif_base, buffer->dir,
						buffer->idx, buffer->intr_id);

	ipq_lpass_dma_enable_interrupt(ipq_lpass_lpaif_base, buffer->dir,
						buffer->idx, buffer->intr_id);
}

static void ipq_lpass_prepare_dma_buffer(struct lpass_dma_buffer *dma_buffer)
{
	uint32_t actual_buffer_size;
	uint32_t frame;

	actual_buffer_size =  dma_buffer->int_samples_per_period *
				dma_buffer->num_channels *
				dma_buffer->bytes_per_sample;

/*
 * set buffer_per interrupt
 */
	frame = LPASS_BUFFER_SIZE / actual_buffer_size;
	dma_buffer->single_buf_size = LPASS_BUFFER_SIZE;
	dma_buffer->frame = frame;

/*
 * set the period count in double words
 */
	dma_buffer->period_count_in_word32 =
				dma_buffer->single_buf_size / sizeof(uint32_t);
/*
 * total buffer size for all DMA buffers
 */
	dma_buffer->dma_buffer_size = LPASS_DMA_BUFFER_SIZE;
}

static void ipq_lpass_fill_tx_data(uint32_t *p)
{
	uint i,j = 0;

	for (i = 0; i < LPASS_DMA_BUFFER_SIZE / 4; ++i)
		p[j++] = i;
}

static void __iomem *ipq_lpass_phy_virt_lpm(uint32_t phy_addr)
{
	return (ipq_lpass_lpm_base + (phy_addr - IPQ_LPASS_LPM_BASE));
}

static int ipq_lpass_setup_bit_clock(uint32_t clk_rate)
{
/*
 * set clock rate for PRI & SEC
 * PRI is slave mode and seondary is master
 */
	ipq_lpass_lpaif_muxsetup(INTERFACE_PRIMARY, TDM_MODE_SLAVE);

	if (ipq_lpass_set_clk_rate(INTERFACE_SECONDARY, clk_rate) != 0){
		pr_err("%s: Bit clk set Failed \n",
			__func__);
		return -EINVAL;
	} else {
		ipq_lpass_lpaif_muxsetup(INTERFACE_SECONDARY,
						TDM_MODE_MASTER);
	}

	return 0;

}

/*
 * FUNCTION: ipq_lpass_pcm_init
 *
 * DESCRIPTION: initializes PCM interface and MBOX interface
 *
 * RETURN VALUE: error if any
 */
int ipq_pcm_init(struct ipq_lpass_pcm_params *params)
{
	struct ipq_lpass_pcm_config config;
	int ret;
	uint32_t clk_rate;
	uint32_t temp_lpm_base;
	uint32_t int_samples_per_period;
	uint32_t bytes_per_sample;

	if ((rx_dma_buffer == NULL ) || (tx_dma_buffer == NULL))
		return -EPERM;

	ret = ipq_lpass_pcm_validate_params(params, &config);
	if (ret)
		return ret;

	memset(rx_dma_buffer, 0, sizeof(*rx_dma_buffer));
	memset(tx_dma_buffer, 0, sizeof(*tx_dma_buffer));
	atomic_set(&data_avail, 0);
	atomic_set(&tx_add, 0);
	atomic_set(&rx_add, 0);

	pcm_params = params;
	temp_lpm_base = IPQ_LPASS_LPM_BASE;
	int_samples_per_period = params->rate / 1000;
	bytes_per_sample = BYTES_PER_CHANNEL(params->bit_width);
	clk_rate = params->bit_width * params->rate * params->slot_count;
	ret = ipq_lpass_setup_bit_clock(clk_rate);
	if (ret)
		return ret;
/*
 * DMA Rx buffer
 */
	rx_dma_buffer->idx = DMA_CHANNEL0;
	rx_dma_buffer->dir = LPASS_HW_DMA_SOURCE;
	rx_dma_buffer->bytes_per_sample = bytes_per_sample;
	rx_dma_buffer->int_samples_per_period = int_samples_per_period;
	rx_dma_buffer->num_channels = params->active_slot_count;
	ipq_lpass_prepare_dma_buffer(rx_dma_buffer);
	rx_dma_buffer->dma_base_address = temp_lpm_base;
	rx_dma_buffer->dma_last_curr_addr = temp_lpm_base;
	rx_dma_buffer->size_done = 0;
	rx_dma_buffer->watermark = ipq_lpass_get_watermark(rx_dma_buffer);
	rx_dma_buffer->ifconfig = INTERFACE_PRIMARY;
	rx_dma_buffer->intr_id = INTERRUPT_CHANNEL0;
	temp_lpm_base += LPASS_DMA_BUFFER_SIZE;
/*
 * DMA Tx buffer
 */
	tx_dma_buffer->idx = DMA_CHANNEL1;
	tx_dma_buffer->dir = LPASS_HW_DMA_SINK;
	tx_dma_buffer->bytes_per_sample = bytes_per_sample;
	tx_dma_buffer->int_samples_per_period = int_samples_per_period;
	tx_dma_buffer->num_channels = params->active_slot_count;
	ipq_lpass_prepare_dma_buffer(tx_dma_buffer);
	tx_dma_buffer->dma_base_address = temp_lpm_base;
	tx_dma_buffer->dma_last_curr_addr = temp_lpm_base;
	tx_dma_buffer->size_done = 0;
	tx_dma_buffer->watermark = ipq_lpass_get_watermark(tx_dma_buffer);
	tx_dma_buffer->ifconfig = INTERFACE_SECONDARY;
	tx_dma_buffer->intr_id = INTERRUPT_CHANNEL1;

	ipq_lpass_fill_tx_data((uint32_t *)ipq_lpass_phy_virt_lpm(
					tx_dma_buffer->dma_base_address));
/*
 * TDM/PCM , Primary PCM support only RX mode
 * Secondary PCM support only TX mode
 */

	ipq_lpass_dma_config_init(rx_dma_buffer);
	ipq_lpass_dma_config_init(tx_dma_buffer);
	ipq_lpass_pcm_reset(ipq_lpass_lpaif_base,
				SECONDARY, LPASS_HW_DMA_SINK);
	ipq_lpass_pcm_reset(ipq_lpass_lpaif_base,
				PRIMARY, LPASS_HW_DMA_SOURCE);
/*
 * Tx mode support in Sconday interface.
 * configure secondary as TDM master mode
 */
	config.sync_src = TDM_MODE_MASTER;
	ipq_lpass_pcm_config(&config, ipq_lpass_lpaif_base,
				SECONDARY, LPASS_HW_DMA_SINK);
/*
 * Rx mode support in Primary interface
 * configure primary as TDM slave mode
 */
	config.sync_src = TDM_MODE_SLAVE;
	ipq_lpass_pcm_config(&config, ipq_lpass_lpaif_base,
				PRIMARY, LPASS_HW_DMA_SOURCE);

	ipq_lpass_pcm_reset_release(ipq_lpass_lpaif_base,
				SECONDARY, LPASS_HW_DMA_SINK);
	ipq_lpass_pcm_reset_release(ipq_lpass_lpaif_base,
				PRIMARY, LPASS_HW_DMA_SOURCE);

	ipq_lpass_dma_enable(rx_dma_buffer);
	ipq_lpass_dma_enable(tx_dma_buffer);

	ipq_lpass_pcm_enable(ipq_lpass_lpaif_base,
				PRIMARY, LPASS_HW_DMA_SOURCE);
	ipq_lpass_pcm_enable(ipq_lpass_lpaif_base,
				SECONDARY, LPASS_HW_DMA_SINK);

	return ret;
}
EXPORT_SYMBOL(ipq_pcm_init);

/*
 * FUNCTION: ipq_lpass_pcm_data
 *
 * DESCRIPTION: calculate the free tx buffer and full rx buffers for use by the
 *		upper layer
 *
 * RETURN VALUE: returns the rx and tx buffer pointers and the size to fill or
 *		read
 */
uint32_t ipq_pcm_data(uint8_t **rx_buf, uint8_t **tx_buf)
{
	unsigned long flag;
	uint32_t size;
	uint32_t txcurr_addr = 0;
	uint32_t rxcurr_addr = 0;

	wait_event_interruptible(pcm_q, atomic_read(&data_avail) != 0);

	atomic_set(&data_avail, 0);
	txcurr_addr = atomic_read(&tx_add);
	rxcurr_addr = atomic_read(&rx_add);

	spin_lock_irqsave(&pcm_lock, flag);
	*rx_buf = (uint8_t *)ipq_lpass_phy_virt_lpm(rxcurr_addr);
	*tx_buf = (uint8_t *)ipq_lpass_phy_virt_lpm(txcurr_addr);
	spin_unlock_irqrestore(&pcm_lock, flag);
	size = rx_dma_buffer->size_done;

	return size;
}
EXPORT_SYMBOL(ipq_pcm_data);

/*
 * FUNCTION: ipq_lpass_pcm_done
 *
 * DESCRIPTION: this api tells the PCM that the upper layer has finished
 *		updating the Tx buffer
 *
 * RETURN VALUE: none
 */
void ipq_pcm_done(void)
{
	atomic_set(&data_avail, 0);
}
EXPORT_SYMBOL(ipq_pcm_done);

void ipq_pcm_send_event(void)
{
	atomic_set(&data_avail, 1);
	wake_up_interruptible(&pcm_q);
}
EXPORT_SYMBOL(ipq_pcm_send_event);

static void ipq_lpass_pcm_dma_deinit(struct lpass_dma_buffer *buffer)
{
	ipq_lpass_dma_clear_interrupt_config(ipq_lpass_lpaif_base, buffer->dir,
						buffer->idx, buffer->intr_id);

	ipq_lpass_dma_disable_interrupt(ipq_lpass_lpaif_base, buffer->dir,
						buffer->idx, buffer->intr_id);

	ipq_lpass_disable_dma_channel(ipq_lpass_lpaif_base, buffer->idx,
						buffer->dir);

	ipq_lpass_pcm_disable(ipq_lpass_lpaif_base, buffer->ifconfig,
					buffer->dir);
}
/*
 * FUNCTION: ipq_lpass_pcm_deinit
 *
 * DESCRIPTION: deinitialization api, clean up everything
 *
 * RETURN VALUE: none
 */
void ipq_pcm_deinit(struct ipq_lpass_pcm_params *params)
{
	ipq_lpass_pcm_dma_deinit(rx_dma_buffer);
	ipq_lpass_pcm_dma_deinit(tx_dma_buffer);
}
EXPORT_SYMBOL(ipq_pcm_deinit);

static const struct of_device_id qca_raw_match_table[] = {
	{ .compatible = "qca,ipq5018-lpass-pcm" },
	{},
};

/*
 * FUNCTION: ipq_lpass_pcm_driver_probe
 *
 * DESCRIPTION: very basic one time activities
 *
 * RETURN VALUE: error if any
 */
static int ipq_lpass_pcm_driver_probe(struct platform_device *pdev)
{
	uint32_t tx_irq;
	uint32_t rx_irq;
	struct resource *res;
	const struct of_device_id *match;
	int ret;

	match = of_match_device(qca_raw_match_table, &pdev->dev);
	if (!match)
		return -ENODEV;
	if (!pdev)
		return -EINVAL;

	pcm_pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ipq_lpass_lpaif_base = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(ipq_lpass_lpaif_base))
		return PTR_ERR(ipq_lpass_lpaif_base);

	ipq_lpass_lpm_base = ioremap_nocache(IPQ_LPASS_LPM_BASE,
				IPQ_LPASS_LPM_SIZE);

	if (IS_ERR(ipq_lpass_lpm_base))
		return PTR_ERR(ipq_lpass_lpm_base);

	pr_info("%s : Lpaif version : 0x%x\n",
			 __func__,readl(ipq_lpass_lpaif_base));

	/* Allocate memory for rx  and tx instance */
	rx_dma_buffer = kzalloc(sizeof(struct lpass_dma_buffer), GFP_KERNEL);
	if (rx_dma_buffer == NULL) {
		pr_err("%s: Error in allocating mem for rx_dma_buffer\n",
				__func__);
		return -ENOMEM;
	}

	tx_dma_buffer = kzalloc(sizeof(struct lpass_dma_buffer), GFP_KERNEL);
	if (tx_dma_buffer == NULL) {
		pr_err("%s: Error in allocating mem for tx_dma_buffer\n",
				__func__);
		kfree(rx_dma_buffer);
		return -ENOMEM;
	}
/*
 * Set interrupt
 */
	tx_irq = platform_get_irq_byname(pdev, "tx");
	if (tx_irq < 0) {
		dev_err(&pdev->dev, "Failed to get tx_irq by name (%d)\n",
				tx_irq);
	} else {
		ret = devm_request_irq(&pdev->dev,
					tx_irq,
					ipq_lpass_pcm_playback_handler,
					0,
					"ipq-lpass_tx",
					tx_dma_buffer);
		if (ret) {
			dev_err(&pdev->dev,
				"request_irq(tx) failed with ret: %d\n", ret);
		return ret;
		}
	}

	rx_irq = platform_get_irq_byname(pdev, "rx");
	if (rx_irq < 0) {
		dev_err(&pdev->dev, "Failed to get rx_irq by name (%d)\n",
				rx_irq);
	} else {
		ret = devm_request_irq(&pdev->dev,
					rx_irq,
					ipq_lpass_pcm_capture_handler,
					0,
					"ipq-lpass_rx",
					rx_dma_buffer);
		if (ret) {
			dev_err(&pdev->dev,
				"request_irq(rx) failed with ret: %d\n", ret);
		return ret;
		}
	}
/*
 * setup default bit clock to 2.048MHZ
 */
	ipq_lpass_setup_bit_clock(DEFAULT_CLK_RATE);

	spin_lock_init(&pcm_lock);

	return 0;
}

/*
 * FUNCTION: ipq_lpass_pcm_driver_remove
 *
 * DESCRIPTION: clean up
 *
 * RETURN VALUE: error if any
 */
static int ipq_lpass_pcm_driver_remove(struct platform_device *pdev)
{

	ipq_pcm_deinit(pcm_params);

	if (rx_dma_buffer)
		kfree(rx_dma_buffer);

	if (tx_dma_buffer)
		kfree(tx_dma_buffer);

	return 0;
}

/*
 * DESCRIPTION OF PCM RAW MODULE
 */

#define DRIVER_NAME "ipq_lpass_pcm_raw"

static struct platform_driver ipq_lpass_pcm_raw_driver = {
	.probe		= ipq_lpass_pcm_driver_probe,
	.remove		= ipq_lpass_pcm_driver_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.of_match_table = qca_raw_match_table,
	},
};

module_platform_driver(ipq_lpass_pcm_raw_driver);

MODULE_ALIAS(DRIVER_NAME);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("QCA RAW PCM VoIP Platform Driver");
