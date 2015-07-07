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
#include <linux/i2c.h>

#include "ipq40xx-adss.h"

#define IPQ40XX_ADAPTER_INDEX	0
static struct i2c_client *ipq40xx_i2c_client;

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
	{
		.name = "qca-spdif-codec-dai",
		.playback = {
			.stream_name = "qca-spdif-playback",
			.channels_min = CH_STEREO,
			.channels_max = CH_STEREO,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S24 |
				SNDRV_PCM_FMTBIT_S32,
		},
		.capture = {
			.stream_name = "qca-spdif-capture",
			.channels_min = CH_STEREO,
			.channels_max = CH_STEREO,
			.rates = SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16 |
				SNDRV_PCM_FMTBIT_S24 |
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

static int ipq40xx_codec_i2c_probe(struct i2c_client *i2c,
					const struct i2c_device_id *id)
{
	int ret;

	ret = snd_soc_register_codec(&i2c->dev,
			&ipq40xx_codec, ipq40xx_codec_dais,
			ARRAY_SIZE(ipq40xx_codec_dais));
	if (ret < 0) {
		pr_err("\nsnd_soc_register_codec failed (%d)\n", ret);
	}

	return ret;
}

static int ipq40xx_codec_i2c_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id ipq40xx_codec_of_match[] = {
	{ .compatible = "qca,ipq40xx-codec" },
	{},
};

static const struct i2c_device_id ipq40xx_codec_i2c_id[] = {
	{ "qca_codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ipq40xx_codec_i2c_id);

static struct i2c_driver ipq40xx_codec_i2c_driver = {
	.driver = {
		.name = "qca_codec",
		.owner = THIS_MODULE,
		.of_match_table = ipq40xx_codec_of_match,
	},
	.probe = ipq40xx_codec_i2c_probe,
	.remove = ipq40xx_codec_i2c_remove,
	.id_table = ipq40xx_codec_i2c_id,
};

static struct i2c_board_info ipq40xx_codec_i2c_info = {
	.type = "qca_codec",
	.addr = 0x10,
};

static int ipq40xx_codec_init(void)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	adapter = i2c_get_adapter(IPQ40XX_ADAPTER_INDEX);
	if (adapter == NULL) {
		pr_err("%s: %d: I2C failed to get adapter", __func__, __LINE__);
		ret = -ENODEV;
		goto out_exit;
	}

	client = i2c_new_device(adapter, &ipq40xx_codec_i2c_info);
	if (client == NULL) {
		pr_err("%s: %d: Failed to add I2C device", __func__, __LINE__);
		ret = -ENODEV;
		goto out_put_adapter;
	}
	ipq40xx_i2c_client = client;

	ret = i2c_add_driver(&ipq40xx_codec_i2c_driver);
	if (ret < 0) {
		pr_err("%s: %d: Failed to add I2C driver", __func__, __LINE__);
		goto unregister_client;
	}

	i2c_put_adapter(adapter);

	return 0;

unregister_client:
	i2c_unregister_device(client);
	ipq40xx_i2c_client = NULL;
out_put_adapter:
	i2c_put_adapter(adapter);
out_exit:
	return ret;
}
module_init(ipq40xx_codec_init);

static void ipq40xx_codec_exit(void)
{
	i2c_del_driver(&ipq40xx_codec_i2c_driver);
	if (ipq40xx_i2c_client)
		i2c_unregister_device(ipq40xx_i2c_client);
}
module_exit(ipq40xx_codec_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("IPQ40xx Codec Driver");
