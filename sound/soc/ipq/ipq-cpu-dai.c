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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio.h>
#include <sound/pcm_params.h>
#include <mach/clk.h>
#include "ipq-pcm.h"
#include "ipq-lpaif.h"
#include "ipq806x.h"
#include "ipq-spdif.h"

static int ipq_lpass_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *dai)
{
	return 0;
}

static int ipq_lpass_spdif_prepare(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	return 0;
}
static int ipq_lpass_spdif_startup(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	ipq_spdif_onetime_cfg();
	return 0;
}

static void ipq_lpass_spdif_shutdown(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
}

static int ipq_lpass_spdif_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	return 0;
}

static int ipq_lpass_mi2s_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *dai)
{
	int ret = 0;
	uint32_t bit_width = params_format(params);
	uint32_t channels = params_channels(params);

	ret = ipq_cfg_mi2s_hwparams_bit_width(bit_width, LPA_IF_MI2S);
	if (ret)
		return -EINVAL;

	ret = ipq_cfg_mi2s_hwparams_channels(channels, LPA_IF_MI2S);
	if (ret)
		return -EINVAL;

	return ret;
}

static int ipq_lpass_mi2s_prepare(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	return 0;
}
static int ipq_lpass_mi2s_startup(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	ipq_cfg_i2s_spkr(0, 0, LPA_IF_MI2S);
	return 0;
}

static void ipq_lpass_mi2s_shutdown(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	ipq_cfg_i2s_spkr(0, 0, LPA_IF_MI2S);
}

static int ipq_lpass_mi2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	return 0;
}

static int ipq_lpass_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params,
					struct snd_soc_dai *dai)
{
	uint32_t stream;
	uint32_t bit_width = params_format(params);

	stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 1 : 0;

	if (bit_width == SNDRV_PCM_FORMAT_S8 ||
	bit_width == SNDRV_PCM_FORMAT_S16)
		ipq_cfg_pcm_width(bit_width, stream);
	else
		dev_dbg(dai->dev, "Format Not supported %s: %d\n",
						__func__, __LINE__);

	ipq_cfg_pcm_rate(params_rate(params));
	return 0;
}

static int ipq_lpass_pcm_prepare(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	return 0;
}
static int ipq_lpass_pcm_startup(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	/*
	 * Put the PCM instance to Reset
	 */
	ipq_cfg_pcm_reset(1);
	/*
	 * Sync source external
	 */
	ipq_cfg_pcm_sync_src(1);
	/*
	 * Configuring Slot0 for Tx and Rx
	 */
	ipq_cfg_pcm_slot(0, 0);
	ipq_cfg_pcm_slot(0, 1);

	return 0;
}

static void ipq_lpass_pcm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
}

static int ipq_lpass_pcm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->dev, "%s:%d\n", __func__, __LINE__);
	return 0;
}


static struct snd_soc_dai_ops ipq_lpass_pcm_ops = {
	.startup	= ipq_lpass_pcm_startup,
	.prepare	= ipq_lpass_pcm_prepare,
	.hw_params	= ipq_lpass_pcm_hw_params,
	.shutdown	= ipq_lpass_pcm_shutdown,
	.set_fmt	= ipq_lpass_pcm_set_fmt,
};

static struct snd_soc_dai_ops ipq_lpass_mi2s_ops = {
	.startup	= ipq_lpass_mi2s_startup,
	.prepare	= ipq_lpass_mi2s_prepare,
	.hw_params	= ipq_lpass_mi2s_hw_params,
	.shutdown	= ipq_lpass_mi2s_shutdown,
	.set_fmt	= ipq_lpass_mi2s_set_fmt,
};

static struct snd_soc_dai_ops ipq_lpass_spdif_ops = {
	.startup	= ipq_lpass_spdif_startup,
	.prepare	= ipq_lpass_spdif_prepare,
	.hw_params	= ipq_lpass_spdif_hw_params,
	.shutdown	= ipq_lpass_spdif_shutdown,
	.set_fmt	= ipq_lpass_spdif_set_fmt,
};


static struct snd_soc_dai_driver ipq_cpu_dais[] = {
	{
		.playback = {
			.rates		= SNDRV_PCM_RATE_8000 |
					SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000,
			.formats	= SNDRV_PCM_FMTBIT_S8 |
					SNDRV_PCM_FMTBIT_S16,
			.channels_min	= 1,
			.channels_max	= 2,
			.rate_min	= 8000,
			.rate_max	= 16000,
		},
		.capture = {
			.rates		= SNDRV_PCM_RATE_8000 |
					SNDRV_PCM_RATE_16000,
			.formats	= SNDRV_PCM_FMTBIT_S8 |
					SNDRV_PCM_FMTBIT_S16,
			.channels_min	= 1,
			.channels_max	= 2,
			.rate_min	= 8000,
			.rate_max	= 16000,
		},
		.ops = &ipq_lpass_pcm_ops,
		.name = "ipq-pcm-dai"
	},
	{
		.playback = {
			.rates		= SNDRV_PCM_RATE_8000 |
					SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_32000 |
					SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000 |
					SNDRV_PCM_RATE_96000 |
					SNDRV_PCM_RATE_192000,
			.formats	= SNDRV_PCM_FMTBIT_S16 |
					SNDRV_PCM_FMTBIT_S24 |
					SNDRV_PCM_FMTBIT_S32,
			.channels_min	= 1,
			.channels_max	= 8,
			.rate_min	= 8000,
			.rate_max	= 192000,
		},
		.ops    = &ipq_lpass_mi2s_ops,
		.name = "ipq-mi2s-dai"
	},
	{
		.playback = {
			.rates		= SNDRV_PCM_RATE_8000 |
					SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_32000 |
					SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000 |
					SNDRV_PCM_RATE_96000,
			.formats	= SNDRV_PCM_FMTBIT_S16 |
					SNDRV_PCM_FMTBIT_S24,
			.channels_min	= 1,
			.channels_max	= 8,
			.rate_min	= 8000,
			.rate_max	= 192000,
		},
		.ops    = &ipq_lpass_spdif_ops,
		.name = "ipq-spdif-dai"
	},
};

static __devinit int ipq_lpass_dai_probe(struct platform_device *pdev)
{
	int ret;
	ret = snd_soc_register_dais(&pdev->dev, ipq_cpu_dais,
					ARRAY_SIZE(ipq_cpu_dais));
	if (ret)
		dev_err(&pdev->dev,
			"%s: error registering soc dais\n", __func__);

	return ret;
}

static __devexit int ipq_lpass_dai_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(ipq_cpu_dais));
	return 0;
}

static struct platform_driver ipq_lpass_dai_driver = {
	.probe	= ipq_lpass_dai_probe,
	.remove	= ipq_lpass_dai_remove,
	.driver	= {
		.name	= "ipq-cpu-dai",
		.owner	= THIS_MODULE,
	},
};

static int __init ipq_lpass_dai_init(void)
{
	int ret;
	ret = platform_driver_register(&ipq_lpass_dai_driver);

	if (ret)
		pr_err("%s: %d:cpu dai registration failed\n",
					__func__, __LINE__);

	return ret;
}

static void __exit ipq_lpass_dai_exit(void)
{
	platform_driver_unregister(&ipq_lpass_dai_driver);
}

module_init(ipq_lpass_dai_init);
module_exit(ipq_lpass_dai_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPQ806x CPU DAI DRIVER");
