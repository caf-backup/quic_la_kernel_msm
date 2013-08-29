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

#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/control.h>
#include <linux/platform_device.h>

static struct snd_soc_dai_driver lpass_codec_dai = {
	.name = "ipq-codec-dai",
	.capture = {
		.stream_name = "lpass-cpature",
		.rate_max = 48000,
		.rate_min = 8000,
		.rates = SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000 |
			SNDRV_PCM_RATE_22050 |
			SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S8,
		.channels_min = 1,
		.channels_max = 1,
	},
	.playback = {
		.stream_name = "lpass-playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_22050 |
			SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000 |
			SNDRV_PCM_RATE_88200 |
			SNDRV_PCM_RATE_96000 |
			SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S8 |
			SNDRV_PCM_FMTBIT_S16 |
			SNDRV_PCM_FMTBIT_S24 |
			SNDRV_PCM_FMTBIT_S32,
	},
};

static int lpass_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	return -ENOTSUPP;
}

static const struct snd_kcontrol_new lpass_vol_ctrl  = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master playback volume",
	.access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			SNDRV_CTL_ELEM_ACCESS_READWRITE),
	.info = lpass_info,
};

static const struct snd_soc_codec_driver lpass_codec = {
	.controls = &lpass_vol_ctrl,
	.num_controls = 1,
};

static int lpass_codec_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
			&lpass_codec, &lpass_codec_dai, 1);
}

static int lpass_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver lpass_codec_driver = {
	.probe	= lpass_codec_probe,
	.remove = lpass_codec_remove,
	.driver = {
		.name = "ipq-lpass-codec",
		.owner = THIS_MODULE,
	},
};

struct platform_device *device;
static int __init codec_init(void)
{
	platform_driver_register(&lpass_codec_driver);
	return 0;
}

static void __exit codec_exit(void)
{
	platform_driver_unregister(&lpass_codec_driver);

}
module_init(codec_init);
module_exit(codec_exit);
MODULE_DESCRIPTION("IPQ LPASS Codec Driver");
MODULE_LICENSE("GPL v2");
