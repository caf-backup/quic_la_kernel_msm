/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/pcm_params.h>

#include "ipq40xx-pcm.h"
#include "ipq40xx-adss.h"

static struct snd_pcm_hardware ipq40xx_pcm_hardware_playback = {
	.info			=	SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_BLOCK_TRANSFER |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_INTERLEAVED |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME,
	.formats		=	SNDRV_PCM_FMTBIT_S16 |
					SNDRV_PCM_FMTBIT_S32,
	.rates			=	SNDRV_PCM_RATE_32000 |
					SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000 |
					SNDRV_PCM_RATE_96000,
	.rate_min		=	FREQ_32000,
	.rate_max		=	FREQ_96000,
	.channels_min		=	CH_STEREO,
	.channels_max		=	CH_STEREO,
	.buffer_bytes_max	=	IPQ40xx_I2S_BUFF_SIZE,
	.period_bytes_max	=	IPQ40xx_I2S_BUFF_SIZE / 2,
	.period_bytes_min	=	IPQ40xx_I2S_PERIOD_BYTES_MIN,
	.periods_min		=	IPQ40xx_I2S_NO_OF_PERIODS,
	.periods_max		=	IPQ40xx_I2S_NO_OF_PERIODS,
	.fifo_size		=	0,
};

static int ipq40xx_pcm_preallocate_dma_buffer(struct snd_pcm *pcm,
						int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		size = ipq40xx_pcm_hardware_playback.buffer_bytes_max;
	else
		return -EINVAL;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev, size,
					&buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;

	return 0;
}

static irqreturn_t ipq40xx_pcm_irq(int intrsrc, void *data)
{
	uint32_t processed_size;

	struct snd_pcm_substream *substream = data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv =
		(struct ipq40xx_pcm_rt_priv *)runtime->private_data;

	/* Store the last played buffer in the runtime priv struct */
	pcm_rtpriv->last_played =
		ipq40xx_mbox_get_last_played(pcm_rtpriv->channel);

	/* Set the OWN bits */
	processed_size = ipq40xx_mbox_get_elapsed_size(pcm_rtpriv->channel);

	if (processed_size > pcm_rtpriv->period_size)
		snd_printd("Processed more than one period bytes : %d\n",
							processed_size);

	snd_pcm_period_elapsed(substream);

	if (pcm_rtpriv->last_played == NULL) {
		snd_printd("BUG: ISR called but no played buf found\n");
		goto ack;
	}

ack:
	return IRQ_HANDLED;
}

static snd_pcm_uframes_t ipq40xx_pcm_i2s2_pointer(
				struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv;
	snd_pcm_uframes_t ret;

	pcm_rtpriv = runtime->private_data;

	if (pcm_rtpriv->last_played == NULL)
		ret = 0;
	else
		ret = (pcm_rtpriv->last_played->BufPtr -
				(runtime->dma_addr & 0xFFFFFFF));
	ret = bytes_to_frames(runtime, ret);
	return ret;
}

static int ipq40xx_pcm_i2s2_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_coherent(substream->pcm->card->dev, vma,
		runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
}

static int ipq40xx_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}


static int ipq40xx_pcm_i2s2_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv;

	uint32_t ret;

	pcm_rtpriv = runtime->private_data;

	ret = ipq40xx_mbox_dma_prepare(pcm_rtpriv->channel);
	if (ret) {
		pr_err("%s: %d: Error in dma prepare : channel : %d\n",
				__func__, __LINE__, pcm_rtpriv->channel);
		ipq40xx_mbox_dma_release(pcm_rtpriv->channel);
		return ret;
	}

	/* Set the ownership bits */
	ipq40xx_mbox_get_elapsed_size(pcm_rtpriv->channel);

	pcm_rtpriv->last_played = NULL;

	return ret;
}

static int ipq40xx_pcm_i2s2_close(struct snd_pcm_substream *substream)
{
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv;
	uint32_t ret;

	pcm_rtpriv = substream->runtime->private_data;
	ret = ipq40xx_mbox_dma_release(pcm_rtpriv->channel);
	if (ret)
		pr_err("%s: %d: Error in dma release\n", __func__, __LINE__);
	kfree(pcm_rtpriv);

	return 0;
}

static int ipq40xx_pcm_i2s2_trigger(struct snd_pcm_substream *substream,
								int cmd)
{
	int ret;
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv =
				substream->runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable the I2S Stereo block for operation */
		ipq40xx_stereo_config_enable(ENABLE,
					get_stereo_id(substream, I2S2));

		ret = ipq40xx_mbox_dma_start(pcm_rtpriv->channel);
		if (ret) {
			pr_err("%s: %d: Error in dma start\n",
				__func__, __LINE__);
			ipq40xx_mbox_dma_release(pcm_rtpriv->channel);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable the I2S Stereo block */
		ipq40xx_stereo_config_enable(DISABLE,
					get_stereo_id(substream, I2S2));

		ret = ipq40xx_mbox_dma_stop(pcm_rtpriv->channel);
		if (ret) {
			pr_err("%s: %d: Error in dma stop\n",
				__func__, __LINE__);
			ipq40xx_mbox_dma_release(pcm_rtpriv->channel);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int ipq40xx_pcm_i2s2_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv;
	int ret;
	unsigned int period_size, sample_size, sample_rate, frames, channels;

	pr_debug("%s %d\n", __func__, __LINE__);

	pcm_rtpriv = runtime->private_data;

	ret = ipq40xx_mbox_form_ring(pcm_rtpriv->channel,
			substream->dma_buffer.addr,
			params_period_bytes(hw_params),
			params_buffer_bytes(hw_params));
	if (ret) {
		pr_err("%s: %d: Error dma form ring\n", __func__, __LINE__);
		ipq40xx_mbox_dma_release(pcm_rtpriv->channel);
		return ret;
	}

	period_size = params_period_bytes(hw_params);
	sample_size = snd_pcm_format_size(params_format(hw_params), 1);
	sample_rate = params_rate(hw_params);
	channels = params_channels(hw_params);
	frames = period_size / (sample_size * channels);

	pcm_rtpriv->period_size = params_period_bytes(hw_params);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = params_buffer_bytes(hw_params);
	return ret;
}

static int ipq40xx_pcm_i2s2_open(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ipq40xx_pcm_rt_priv *pcm_rtpriv;

	pr_debug("%s %d\n", __func__, __LINE__);

	pcm_rtpriv = kmalloc(sizeof(struct ipq40xx_pcm_rt_priv), GFP_KERNEL);
	if (!pcm_rtpriv)
		return -ENOMEM;
	snd_printd("%s: 0x%xB allocated at 0x%08x\n",
			__func__, sizeof(*pcm_rtpriv), (u32) pcm_rtpriv);
	pcm_rtpriv->last_played = NULL;
	pcm_rtpriv->dev = substream->pcm->card->dev;
	pcm_rtpriv->channel = get_mbox_id(substream, I2S2);
	substream->runtime->private_data = pcm_rtpriv;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		runtime->dma_bytes =
			ipq40xx_pcm_hardware_playback.buffer_bytes_max;
		snd_soc_set_runtime_hwparams(substream,
				&ipq40xx_pcm_hardware_playback);
	} else {
		pr_err("%s: Invalid stream\n", __func__);
		ret = -EINVAL;
		goto error;
	}
	ret = ipq40xx_mbox_dma_init(pcm_rtpriv->dev,
		pcm_rtpriv->channel, ipq40xx_pcm_irq, substream);
	if (ret) {
		pr_err("%s: %d: Error initializing dma\n",
					__func__, __LINE__);
		goto error;
	}

	ret = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		pr_err("%s: snd_pcm_hw_constraint_integer failed\n", __func__);
		goto error;
	}

	return 0;
error:
	kfree(pcm_rtpriv);
	return ret;
}

static struct snd_pcm_ops ipq40xx_asoc_pcm_i2s2_ops = {
	.open		= ipq40xx_pcm_i2s2_open,
	.hw_params	= ipq40xx_pcm_i2s2_hw_params,
	.hw_free	= ipq40xx_pcm_hw_free,
	.trigger	= ipq40xx_pcm_i2s2_trigger,
	.ioctl		= snd_pcm_lib_ioctl,
	.close		= ipq40xx_pcm_i2s2_close,
	.prepare	= ipq40xx_pcm_i2s2_prepare,
	.mmap		= ipq40xx_pcm_i2s2_mmap,
	.pointer	= ipq40xx_pcm_i2s2_pointer,
};

static void ipq40xx_asoc_pcm_i2s2_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream = 0;

	substream = pcm->streams[stream].substream;
	buf = &substream->dma_buffer;
	if (buf->area) {
		dma_free_coherent(pcm->card->dev, buf->bytes,
					buf->area, buf->addr);
	}
	buf->area = NULL;
}

static int ipq40xx_asoc_pcm_i2s2_new(struct snd_soc_pcm_runtime *prtd)
{
	struct snd_card *card = prtd->card->snd_card;
	struct snd_pcm *pcm = prtd->pcm;

	int ret;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &card->dev->coherent_dma_mask;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = ipq40xx_pcm_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_PLAYBACK);

		if (ret) {
			pr_err("%s: %d: Error allocating dma buf\n",
						__func__, __LINE__);
			return -ENOMEM;
		}
	}

	return ret;
}

static struct snd_soc_platform_driver ipq40xx_asoc_pcm_i2s2_platform = {
	.ops		= &ipq40xx_asoc_pcm_i2s2_ops,
	.pcm_new	= ipq40xx_asoc_pcm_i2s2_new,
	.pcm_free	= ipq40xx_asoc_pcm_i2s2_free,
};

static const struct of_device_id ipq40xx_pcm_i2s2_id_table[] = {
	{ .compatible = "qca,ipq40xx-pcm-i2s2" },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, ipq40xx_pcm_i2s2_id_table);

static int ipq40xx_pcm_i2s2_driver_probe(struct platform_device *pdev)
{
	int ret;
	pr_debug("%s %d\n", __func__, __LINE__);
	ret = snd_soc_register_platform(&pdev->dev,
			&ipq40xx_asoc_pcm_i2s2_platform);
	if (ret)
		dev_err(&pdev->dev, "%s: Failed to register i2s2 pcm device\n",
								__func__);
	return ret;
}

static int ipq40xx_pcm_i2s2_driver_remove(struct platform_device *pdev)
{
	pr_debug("%s %d\n", __func__, __LINE__);
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static struct platform_driver ipq40xx_pcm_i2s2_driver = {
	.probe = ipq40xx_pcm_i2s2_driver_probe,
	.remove = ipq40xx_pcm_i2s2_driver_remove,
	.driver = {
		.name = "qca-pcm-i2s2",
		.owner = THIS_MODULE,
		.of_match_table = ipq40xx_pcm_i2s2_id_table,
	},
};

module_platform_driver(ipq40xx_pcm_i2s2_driver);

MODULE_ALIAS("platform:qca-pcm-i2s2");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("IPQ40xx PCM I2S Platform Driver");
