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

#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/control.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "ipq40xx-adss.h"

static struct snd_soc_dai_driver ipq40xx_codec_dais[] = {
	{
		.name = "qca-i2s-codec-dai",
		.playback = {
			.stream_name = "qca-i2s-playback",
			.channels_min = CH_STEREO,
			.channels_max = CH_STEREO,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S32,
		},
		.capture = {
			.stream_name = "qca-i2s-capture",
			.channels_min = CH_STEREO,
			.channels_max = CH_STEREO,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S32,
		},
	},
	{
		.name = "qca-tdm-codec-dai",
		.playback = {
			.stream_name = "qca-tdm-playback",
			.channels_min = CH_STEREO,
			.channels_max = CH_7_1,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S32,
		},
		.capture = {
			.stream_name = "qca-tdm-capture",
			.channels_min = CH_STEREO,
			.channels_max = CH_7_1,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S32,
		},
	},
	{
		.name = "qca-i2s1-codec-dai",
		.playback = {
			.stream_name = "qca-i2s1-playback",
			.channels_min = CH_STEREO,
			.channels_max = CH_STEREO,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S32,
		},
	},
	{
		.name = "qca-i2s2-codec-dai",
		.playback = {
			.stream_name = "qca-i2s2-playback",
			.channels_min = CH_STEREO,
			.channels_max = CH_STEREO,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S32,
		},
	},
};

static int ipq40xx_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	return -ENOTSUPP;
}

static const struct snd_kcontrol_new vol_ctrl  = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "playback volume",
	.access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ |
		SNDRV_CTL_ELEM_ACCESS_READWRITE),
	.info = ipq40xx_info,
};

static const struct snd_soc_codec_driver ipq40xx_codec = {
	.num_controls = 0,
};

static const struct of_device_id ipq40xx_codec_id_table[] = {
	{ .compatible = "qca,ipq40xx-codec" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, ipq40xx_codec_id_table);

static int ipq40xx_codec_probe(struct platform_device *pdev)
{
	int ret;

	ret = snd_soc_register_codec(&pdev->dev,
			&ipq40xx_codec, ipq40xx_codec_dais,
			ARRAY_SIZE(ipq40xx_codec_dais));
	if (ret < 0) {
		pr_err("\nsnd_soc_register_codec failed \n");
	}
	return ret;
}

static int ipq40xx_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

struct platform_driver ipq40xx_codec_driver = {
	.probe = ipq40xx_codec_probe,
	.remove = ipq40xx_codec_remove,
	.driver = {
		.name = "50.qca-codec",
		.owner = THIS_MODULE,
		.of_match_table = ipq40xx_codec_id_table,
	},
};

module_platform_driver(ipq40xx_codec_driver);

MODULE_ALIAS("platform:qca-codec");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("IPQ40xx Codec Driver");
