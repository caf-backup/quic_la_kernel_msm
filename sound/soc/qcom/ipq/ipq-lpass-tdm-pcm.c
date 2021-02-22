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


/*
 * Global Constant Definitions
 */
void __iomem *ipq_lpass_lpaif_base;

/*
 * Static Variable Definitions
 */
static atomic_t data_avail;
static atomic_t data_at;
static struct lpass_dma_buffer *rx_dma_buffer;
static struct lpass_dma_buffer *tx_dma_buffer;
static struct platform_device *pcm_pdev;
static spinlock_t pcm_lock;
static struct pcm_context context;
static struct clk *pcm_clk;
static enum ipq_hw_type ipq_hw;

static uint32_t rx_size_count;
struct ipq_lpass_pcm_params *pcm_params;

static DECLARE_WAIT_QUEUE_HEAD(pcm_q);
/*
 * FUNCTION: pcm_rx_irq_handler
 *
 * DESCRIPTION: pcm rx dma irq handler
 *
 * RETURN VALUE: none
 */
static irqreturn_t ipq_lpass_pcm_rx_irq_handler(int intrsrc, void *data)
{
	uint32_t dma_at;
	uint32_t rx_size;
	atomic_set(&data_at, dma_at);
	atomic_set(&data_avail, 1);

	wake_up_interruptible(&pcm_q);

	return IRQ_HANDLED;
}

/*
 * FUNCTION: pcm_tx_irq_handler
 *
 * DESCRIPTION: pcm dma tx irq handler
 *
 * RETURN VALUE: none
 */
static irqreturn_t ipq_lpass_pcm_tx_irq_handler(int intrsrc, void *data)
{
	/* do nothing for now, done in rx irq handler */
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
	uint32_t count;
	int8_t idx, num_active_slots_cnt = 0;

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

	if ((PCM_SAMPLE_RATE_8K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_16K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_24K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_32K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_48K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_96K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_192K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_384K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_22_05K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_44_1K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_88_2K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_176_4K != tdm_cfg_ptr->sample_rate)&&
		(PCM_SAMPLE_RATE_352_8K != tdm_cfg_ptr->sample_rate)){
		 pr_err("%s: Invalid PCM sampling rate %d.\n",
			__func__, params->sample_rate);
		return -EINVAL;
	}

	/* slot width supported is 16 , 24 & 32 */
	if (!((params->slot_width == 16) ||
			(params->slot_width == 24) ||
			(params->slot_width == 32))) {
		pr_err("%s: Invalid slot_width %d.\n",
				__func__, params->bit_width);
		return -EINVAL;
	}

	if (params->slot_width < params->bit_width){
		 pr_err("%s: Invalid slot_width %d.\n",
				 __func__, params->bit_width);
		return -EINVAL;
	}

	if ((params->num_channels > IPQ_PCM_MAX_CHANNEL_CNT) ||
			&& (params->num_channels & 0x1)){
		pr_err("%s: Invalid channel %d.\n",
				 __func__, params->num_channels);
		return -EINVAL;
	}

	if (params->nslots_per_frame >
			AFE_DRIVER_TDM_MAX_SLOTS_PER_FRAME){
		pr_err("%s: Invalid nslots per frame %d.\n",
				__func__, params->nslots_per_frame);
		return -EINVAL;
	}

	if (512 < (params->nslots_per_frame * params->slot_width)){
		pr_err("%s: Invalid nbits per frame %d.\n",
				 __func__,
				params->nslots_per_frame * params->slot_width );
		return -EINVAL;
	}

	config->intf_id = params->intf_id;

/*
 * Validate if active slot_mask is set same as number of channels
 */
	for (idx = 0; idx < MAX_TDM_SLOTS; idx++){
		if (params->slot_mask & (0x00000001 << idx)){
			num_active_slots_cnt++;
		}
	}
	if (num_active_slots_cnt != params->num_channels){
		pr_err("%s: Nums of channels & Active channels mismatch.\n",
				 __func__);
		return -EINVAL;
	}
	//Parse slot mask
	config->slot_mask = params->slot_mask;

	config->num_channels = params->num_channels;
	config->bit_width = params->bit_width;
	config->data_format = params->data_format;
	config->sample_rate = params->sample_rate;
	// Parse the slot per frame
	config->nslots_per_frame = params->nslots_per_frame;
	// Parse the slot width
	config->slot_width = params->slot_width;

	config->bytes_per_channel = (params->bit_width > 16) ? 4 : 2;

	// Parse the sync mode : short sync or long sync
	switch(params->sync_mode){
	case PCM_TDM_LONG_SYNC_MODE:
		config->sync_type = TDM_LONG_SYNC_TYPE;
	break;
	case PCM_TDM_SHORT_SYNC_SLOT_MODE:
		config->sync_type = TDM_SLOT_SYNC_TYPE;
	break;
	case PCM_TDM_SHORT_SYNC_BIT_MODE:
		config->sync_type = TDM_SHORT_SYNC_TYPE;
	break;
	default:
		pr_err("%s: Invalid sync mode.\n",
				 __func__,);
		return -EINVAL;
	}

	// Parse the sync src: internal or external
	switch (params->sync_src){
	case PCM_TDM_SYNC_SRC_EXTERNAL:
		config->sync_src = TDM_MODE_SLAVE;
	break;
	case PCM_TDM_SYNC_SRC_INTERNAL:
		config->sync_src = TDM_MODE_MASTER;
	break;
	default:
		pr_err("%s: Invalid sync source.\n",
				 __func__,);
		return -EINVAL;
	}

	// Parse the OE enable flag
	switch(params->ctrl_data_out_enable){
	case PCM_TDM_CTRL_DATA_OE_ENABLE:
		config->ctrl_data_oe = TDM_CTRL_DATA_OE_ENABLE;
	break;
	case PCM_TDM_CTRL_DATA_OE_DISABLE:
		config->ctrl_data_oe = TDM_CTRL_DATA_OE_DISABLE;
	break;
	default:
		pr_err("%s: Invalid ctrl data OE enable.\n",
				 __func__,);
		return -EINVAL;
	}

	// Parse the invert sync pulse flag
	switch(params->ctrl_invert_sync_pulse){
	case PCM_TDM_SYNC_INVERT:
		config->ctrl_invert_sync_pulse = TDM_LONG_SYNC_INVERT;
	break;
	case PCM_TDM_SYNC_NORMAL:
		config->ctrl_invert_sync_pulse = TDM_LONG_SYNC_NORMAL;
	break;
	default:
		pr_err("%s: Invalid control invert sync mode.\n",
				 __func__,);
		return -EINVAL;
	}

	// Parse the sync data delay
	switch(params->ctrl_sync_data_delay){
	case PCM_TDM_DATA_DELAY_0_BCLK_CYCLE:
		config->ctrl_sync_data_delay = TDM_DATA_DELAY_0_CYCLE;
	break;
	case PCM_TDM_DATA_DELAY_1_BCLK_CYCLE:
		config->ctrl_sync_data_delay = TDM_DATA_DELAY_1_CYCLE;
	break;
	case PCM_TDM_DATA_DELAY_2_BCLK_CYCLE:
		config->ctrl_sync_data_delay = TDM_DATA_DELAY_2_CYCLE;
	break;
	default:
		pr_err("%s: Invalid control sync delay mode.\n",
				 __func__,);
		return -EINVAL;
	}

/*
 * Determining the qformat shift factor based on bit width.
 * We use this shift factor for 24bit
 */
	if ((16 == config->bit_width) || (32 == config->bit_width))
		config->q_format_shift_factor = 0;
	}
	else {
		config->q_format_shift_factor = QFORMAT_SHIFT_FACTOR;
	}
	return 0;
}

static uint32_t ipq_lpass_get_watermark(struct ipq_lpass_pcm_params *params)
{
	uint32_t water_mark = DEAFULT_PCM_WATERMARK;
	uint32_t dma_buf_size_in_bytes_per_intr;
	uint32_t dma_buf_size_in_dword_per_intr;

/*
 * check if burst mode can be enabled
 */
	dma_buf_size_in_bytes_per_intr =
		params->bytes_per_channel*
			params->int_samples_per_period * params->num_channels;

	dma_buf_size_in_dword_per_intr = dma_buf_size_in_bytes_per_intr >> 2;

	if((DEAFULT_PCM_WATERMARK >= dma_buf_size_in_dword_per_intr) ||
		(dma_buf_size_in_dword_per_intr & 0x3)  ){
		water_mark = 1;
	}

	return water_mark;
}

static uint32_t ipq_lpass_pcm_get_num_dma_buffer(uint32_t bytes_per_channel,
			uint32_t int_samples_per_period, uint32_t num_channels)
{
	uint32_t num_dma_buffer = DEFAULT_PCM_DMA_BUFFERS;
	uint32_t dma_buf_size_in_bytes_per_intr;
	uint32_t circ_buf_length;

	dma_buf_size_in_bytes_per_intr = bytes_per_channel*
						int_samples_per_period *
						p_dev_state->num_channels;

/*
 * twice the length of each buffer - ping-pong
 */
	circ_buf_length = num_dma_buffer * dma_buf_size_in_bytes_per_intr;

	while((circ_buf_length & PCM_DMA_BUFFER_16BYTE_ALIGNMENT) &&
			(num_dma_buffer < MAX_PCM_DMA_BUFFERS)){
	/*
	 * Double the number of DMA buffers
	 */
	num_dma_buffer <<= 1 ;

	/*
	 * Double the DMA buf size
	 */
	circ_buf_length =dma_buf_size_in_bytes_per_intr * num_dma_buffer;
	}

	if(num_dma_buffer > MAX_PCM_DMA_BUFFERS){
		num_dma_buffer = 0;
	}

	return num_dma_buffer;

}

static void ipq_lpass_pcm_get_dma_buffer_size(struct lpass_dma_buffer *dma_buffer)
{

  uint32_t buffer_size;

  buffer_size =  dma_buffer->int_samples_per_period *
			dma_buffer->num_channels* dma_buffer->bytes_per_channel;

/*
 * set the period count in double words
 */
  dma_buffer->period_count_in_word32 = buffer_size / sizeof(uint32_t);

/*
 * total buffer size for all DMA buffers
 */
  dma_buffer->dma_buffer_size = buffer_size * dma_buffer->num_buffers;

}

static ipq_lpass_dma_config_init(struct lpass_dma_buffer *buffer)
{
	uint32_t wps_count;
	struct lpass_dma_config dma_config
/*
 * wps count is the number of 32 bit words per sample element
 * (sample element = bytes per sample * number of channels)
 * if the number of 32 bits words per sample element is less than 1 Dword,
 * then set it to 1.
 */
	wps_count =
		(buffer->num_channels * buffer->bytes_per_sample) >> 2;
	if(0 == wps_count){
		wps_count = 1;
	}

/*
 * init and fill the dma config args.
 * calculate burst size depending on the period length.
 * Period count should be a multiple of burst size
 */
	if(0 == (buffer->period_count_in_word32 &
			DMA_BUFFER_MGR_16_BYTE_ALIGNMENT)){
		dma_config.burst_size = 16;
	} else if(0 == (buffer->period_count_in_word32 &
			DMA_BUFFER_MGR_8_BYTE_ALIGNMENT)){
		dma_config.burst_size = 8;
	} else if(0 == (buffer->period_count_in_word32 &
			DMA_BUFFER_MGR_4_BYTE_ALIGNMENT)){
		dma_config.burst_size = 4;
	} else {
		dma_config.burst_size = 1;
	}

/*
 * configure the DMA channel
 */
	dma_config.buffer_len = buffer->dma_buffer_size/sizeof(uint32_t);
	dma_config.buffer_start_addr = buffer->dma_buffer_phy_addr_ptr;
	dma_config.dma_int_per_cnt = buffer->period_count_in_word32;
	dma_config.wps_count = wps_count;
	dma_config.watermark = buffer->watermark;
	dma_config.ifconfig = buffer->ifconfig;

	ipq_lpass_disable_dma_channel(ipq_lpass_lpaif_base,
						buffer->idx, bufer->dir);
	ipq_lpass_config_dma_channel(&ma_config, bufer->dir, buffer->idx);

	ipq_lpass_dma_clear_interrupt(ipq_lpass_lpaif_base, bufer->dir,
						buffer->idx, 0);

	ipq_lpass_dma_enable_interrupt(ipq_lpass_lpaif_base, bufer->dir,
						buffer->idx, 0);

	ipq_lpass_enable_dma_channel(ipq_lpass_lpaif_base,
						buffer->idx, bufer->dir);
}
/*
 * FUNCTION: ipq_lpass_pcm_init
 *
 * DESCRIPTION: initializes PCM interface and MBOX interface
 *
 * RETURN VALUE: error if any
 */
int ipq_lpass_pcm_init(struct ipq_lpass_pcm_params *params)
{
	struct ipq_lpass_pcm_config config;
	int ret;
	uint32_t single_buf_size;
	uint32_t clk_rate;
	uint32_t i;
	uint32_t temp_lpm_base;
	uint32_t reg_val;
	uint32_t watermark;
	uint32_t nums_buff;
	uint32_t int_samples_per_period;
	uint32_t bytes_per_sample;

	pcm_params = params;
	temp_lpm_base = IPQ_LPASS_LPM_BASE;
	ret = ipq_lpass_pcm_validate_params(params, &config);
	if (ret)
		return ret;

	atomic_set(&data_avail, 0);
	atomic_set(&data_at, 0);

	int_samples_per_period = params->sample_rate / 1000;
	bytes_per_sample = BYTES_PER_CHANNEL(params->bit_width);
#if 0
	clk_rate = params->slot_count * params->bit_width
					* params->rate * PCM_MULT_FACTOR;
#endif

	watermark = ipq_lpass_get_watermark(params);
	nums_buff = ipq_lpass_pcm_get_num_dma_buffer(params);
/*
 * DMA Rx buffer
 */
	rx_dma_buffer->idx = 1;
	rx_dma_buffer->dir = LPASS_HW_DMA_SINK;
	rx_dma_buffer->bytes_per_sample = bytes_per_sample;
	rx_dma_buffer->int_samples_per_period = int_samples_per_period;
	rx_dma_buffer->num_buffers = num_buffers;
	rx_dma_buffer->num_channel = params->num_channels;
	ipq_lpass_pcm_get_dma_buffer_size(rx_dma_buffer);
	rx_dma_buffer->dma_buffer_phy_addr_ptr = temp_lpm_base;
	rx_dma_buffer->ifconfig_dma_control = SECONDARY
	temp_lpm_base += dma_buffer_size;
/*
 * DMA Tx buffer
 */
	tx_dma_buffer->idx = 0;
	tx_dma_buffer->dir = LPASS_HW_DMA_SOURCE;
	tx_dma_buffer->bytes_per_sample = bytes_per_sample;
	tx_dma_buffer->int_samples_per_period = int_samples_per_period;
	tx_dma_buffer->num_buffers = num_buffers;
	tx_dma_buffer->num_channel = params->num_channels;
	ipq_lpass_pcm_get_dma_buffer_size(tx_dma_buffer);
	rx_dma_buffer->dma_buffer_phy_addr_ptr = temp_lpm_base;
	rx_dma_buffer->ifconfig_dma_control = PRIMARY
/*
 * TDM/PCM , Primary PCM support only RX mode
 * Secondary PCM support only TX mode
 */
	ipq_lpass_pcm_reset(ipq_lpass_lpaif_base,
				1, LPASS_HW_DMA_SINK);

	ipq_lpass_pcm_reset_release(ipq_lpass_lpaif_base,
▹       ▹       ▹       ▹       1, LPASS_HW_DMA_SINK);

	ipq_lpass_pcm_config(config, ipq_lpass_lpaif_base,
				1, LPASS_HW_DMA_SINK);

	ipq_lpass_pcm_reset(ipq_lpass_lpaif_base,
				0, LPASS_HW_DMA_SOURCE);

	ipq_lpass_pcm_reset_release(ipq_lpass_lpaif_base,$
▹       ▹       ▹       ▹       0, LPASS_HW_DMA_SOURCE);

	ipq_lpass_pcm_config(config, ipq_lpass_lpaif_base,
				0, LPASS_HW_DMA_SOURCE);

	ipq_lpass_dma_config_init(rx_dma_buffer)
	ipq_lpass_dma_config_init(tx_dma_buffer)

	return ret;
}
EXPORT_SYMBOL(ipq_lpass_pcm_init);

/*
 * FUNCTION: ipq_lpass_pcm_data
 *
 * DESCRIPTION: calculate the free tx buffer and full rx buffers for use by the
 *		upper layer
 *
 * RETURN VALUE: returns the rx and tx buffer pointers and the size to fill or
 *		read
 */
uint32_t ipq_lpass_pcm_data(uint8_t **rx_buf, uint8_t **tx_buf)
{
	unsigned long flag;
	uint32_t offset;
	uint32_t size;

	wait_event_interruptible(pcm_q, atomic_read(&data_avail) != 0);

	atomic_set(&data_avail, 0);
	offset = (rx_dma_buffer->single_buf_size) * atomic_read(&data_at);

	spin_lock_irqsave(&pcm_lock, flag);
	*rx_buf = rx_dma_buffer->area + offset;
	*tx_buf = tx_dma_buffer->area + offset;
	spin_unlock_irqrestore(&pcm_lock, flag);

	size = rx_dma_buffer->single_buf_size;

	return size;
}
EXPORT_SYMBOL(ipq_lpass_pcm_data);

/*
 * FUNCTION: ipq_lpass_pcm_done
 *
 * DESCRIPTION: this api tells the PCM that the upper layer has finished
 *		updating the Tx buffer
 *
 * RETURN VALUE: none
 */
void ipq_lpass_pcm_done(void)
{
	atomic_set(&data_avail, 0);
}
EXPORT_SYMBOL(ipq_lpass_pcm_done);


void ipq_lpass_pcm_send_event(void)
{
	atomic_set(&data_avail, 1);
	wake_up_interruptible(&pcm_q);
}
EXPORT_SYMBOL(ipq_lpass_pcm_send_event);

/*
 * FUNCTION: ipq_lpass_pcm_deinit
 *
 * DESCRIPTION: deinitialization api, clean up everything
 *
 * RETURN VALUE: none
 */
void ipq_lpass_pcm_deinit(struct ipq_lpass_pcm_params *params)
{
}
EXPORT_SYMBOL(ipq_lpass_pcm_deinit);

static const struct of_device_id qca_raw_match_table[] = {
	{ .compatible = "qca,ipq5018-pcm", .data = (void *)IPQ5018 },
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
	uint32_t tx_channel;
	uint32_t rx_channel;
	uint32_t single_buf_size_max;
	struct resource *res;
	struct device_node *np = NULL;
	const struct of_device_id *match;
	int ret;
	bool adss_init = false;

	match = of_match_device(qca_raw_match_table, &pdev->dev);
	if (!match)
		return -ENODEV;

	ipq_hw = (enum ipq_hw_type)match->data;

	if (!pdev)
		return -EINVAL;

	pcm_pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ipq_lpass_lpaif_base = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(adss_pcm_base))
		return PTR_ERR(adss_pcm_base);
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

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
	ipq_lpass_pcm_deinit(pcm_params);

	devm_clk_put(&pdev->dev, pcm_clk);
	kfree(rx_dma_buffer);
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
