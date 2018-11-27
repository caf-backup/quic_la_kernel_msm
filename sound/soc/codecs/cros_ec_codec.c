// SPDX-License-Identifier: GPL-2.0
/*
 * cros_ec_codec - Driver for Chrome OS Embedded Controller codec.
 *
 * Copyright 2018 Google, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This driver uses the cros-ec interface to communicate with the Chrome OS
 * EC about audio data.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mfd/cros_ec.h>
#include <linux/mfd/cros_ec_commands.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/platform_device.h>

#include "cros_ec_codec.h"

#define MAX_GAIN 43

#define DRV_NAME "cros-ec-codec"

static const DECLARE_TLV_DB_SCALE(ec_mic_gain_tlv, 0, 100, 0);

static int ec_command_testing(struct cros_ec_codec_data *codec_data,
			      int version, int command,
			      uint8_t *outdata, int outsize, uint8_t *indata,
			      int insize)
{
	struct cros_ec_device *ec_device = codec_data->ec_device;
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(insize, outsize), GFP_KERNEL);
	if (!msg) {
		dev_err(codec_data->dev,
			"%s, Failed to allocate ec command message\n",
			__func__);
		return -ENOMEM;
	}

	msg->version = version;
	msg->command = command;
	msg->outsize = outsize;
	msg->insize = insize;

	if (outsize)
		memcpy(msg->data, outdata, outsize);

	ret = cros_ec_cmd_xfer_status(ec_device, msg);
	if (ret > 0 && insize)
		memcpy(indata, msg->data, insize);

	kfree(msg);
	return ret;
}

static int get_ec_mic_gain_testing(struct cros_ec_codec_data *codec_data,
			   uint8_t *left, uint8_t *right)
{
	struct ec_param_codec_i2s param;
	struct ec_response_codec_gain resp;
	int ret;

	dev_dbg(codec_data->dev, "%s get mic gain\n", __func__);

	param.cmd = EC_CODEC_GET_GAIN;

	ret = ec_command_testing(codec_data, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 (uint8_t *)&resp, sizeof(resp));
	if (ret < 0) {
		dev_err(codec_data->dev, "I2S get gain command returned 0x%x\n",
			 ret);
		return -EINVAL;
	}

	*left = resp.left;
	*right = resp.right;

	dev_dbg(codec_data->dev, "%s get mic gain %u, %u\n", __func__, *left, *right);

	return 0;
}

static int set_ec_mic_gain_testing(struct cros_ec_codec_data *codec_data,
				   uint8_t left, uint8_t right)
{
	struct ec_param_codec_i2s param;
	int ret;

	dev_dbg(codec_data->dev, "%s set mic gain to %u, %u\n",
		__func__, left, right);

	param.cmd = EC_CODEC_SET_GAIN;
	param.gain.left = left;
	param.gain.right = right;

	ret = ec_command_testing(codec_data, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 NULL, 0);
	if (ret < 0) {
		dev_err(codec_data->dev, "I2S set gain command returned 0x%x\n",
			ret);
		return -EINVAL;
	}

	return 0;
}

static int test_ec_mic_gain(struct cros_ec_codec_data *codec_data)
{
	uint8_t left, right;
	set_ec_mic_gain_testing(codec_data, 30, 43);
	get_ec_mic_gain_testing(codec_data, &left, &right);
	return 0;
}

/*
 * Wrapper for EC command.
 */
static int ec_command(struct snd_soc_component *component, int version, int command,
		      uint8_t *outdata, int outsize, uint8_t *indata,
		      int insize)
{
	struct cros_ec_codec_data *codec_data = snd_soc_component_get_drvdata(
			component);
	struct cros_ec_device *ec_device = codec_data->ec_device;
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(insize, outsize), GFP_KERNEL);
	if (!msg) {
		dev_err(component->dev,
			"%s, Failed to allocate ec command message\n",
			__func__);
		return -ENOMEM;
	}

	msg->version = version;
	msg->command = command;
	msg->outsize = outsize;
	msg->insize = insize;

	if (outsize)
		memcpy(msg->data, outdata, outsize);

	ret = cros_ec_cmd_xfer_status(ec_device, msg);
	if (ret > 0 && insize)
		memcpy(indata, msg->data, insize);

	kfree(msg);
	return ret;
}

static int set_i2s_config(struct snd_soc_component *component,
			  enum ec_i2s_config i2s_config)
{
	struct ec_param_codec_i2s param;
	int ret;

	dev_dbg(component->dev, "%s set I2S format to %u\n", __func__, i2s_config);

	param.cmd = EC_CODEC_I2S_SET_CONFIG;
	param.i2s_config = i2s_config;

	ret = ec_command(component, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 NULL, 0);
	if (ret < 0) {
		dev_err(component->dev,
			"set I2S format to %u command returned 0x%x\n",
			i2s_config, ret);
		return -EINVAL;
	}
	return 0;
}

/*
 * TODO(cychiang): Add function to support set_tdm_slot ops.
 * The function needs to set TDM mode on EC codec
 * using EC_CODEC_I2S_SET_TDM_CONFIG to adjust delay from
 * SYNC to MSB of left and right channels.
 */

static int cros_ec_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	enum ec_i2s_config i2s_config;

	dev_dbg(component->dev, "%s enter\n", __func__);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		i2s_config = EC_DAI_FMT_I2S;
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		i2s_config = EC_DAI_FMT_RIGHT_J;
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		i2s_config = EC_DAI_FMT_LEFT_J;
		break;

	case SND_SOC_DAIFMT_DSP_A:
		i2s_config = EC_DAI_FMT_PCM_A;
		break;

	case SND_SOC_DAIFMT_DSP_B:
		i2s_config = EC_DAI_FMT_PCM_B;
		break;

	default:
		return -EINVAL;
	}

	set_i2s_config(component, i2s_config);

	dev_dbg(component->dev, "%s set I2S DAI format\n", __func__);

	return 0;
}
#if 0
static int set_i2s_sample_depth(struct snd_soc_component *component,
				enum ec_sample_depth_value depth)
{
	struct ec_param_codec_i2s param;
	int ret;

	dev_dbg(component->dev, "%s set depth to %u\n", __func__, depth);

	param.cmd = EC_CODEC_SET_SAMPLE_DEPTH;
	param.depth = depth;

	ret = ec_command(component, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 NULL, 0);
	if (ret < 0) {
		dev_err(component->dev, "I2S sample depth %u returned 0x%x\n",
			depth, ret);
		return -EINVAL;
	}
	return 0;
}
#endif
static int cros_ec_i2s_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
#if 0
	struct snd_soc_component *component = dai->component;
	int frame_size;
	unsigned int rate;
	int ret;

	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(component->dev, "Unsupported frame size: %d\n", frame_size);
		return -EINVAL;
	}

	rate = params_rate(params);
	if (rate != 48000) {
		dev_err(component->dev, "Unsupported rate\n");
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		ret = set_i2s_sample_depth(component, EC_CODEC_SAMPLE_DEPTH_16);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ret = set_i2s_sample_depth(component, EC_CODEC_SAMPLE_DEPTH_24);
		break;
	default:
		return -EINVAL;
	}
#endif
	return 0;
}

static const struct snd_soc_dai_ops cros_ec_i2s_dai_ops = {
	.hw_params = cros_ec_i2s_hw_params,
	.set_fmt = cros_ec_i2s_set_dai_fmt,
};

/*
 * TODO(cychiang) Add DAI driver for WoV recording.
 */
struct snd_soc_dai_driver cros_ec_dai[] = {
	{
		.name = "cros_ec_codec I2S",
		.id = 0,
		.capture = {
			.stream_name = "I2S Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cros_ec_i2s_dai_ops,
	}
};

static int get_ec_mic_gain(struct snd_soc_component *component,
			   uint8_t *left, uint8_t *right)
{
#if 0
	struct ec_param_codec_i2s param;
	struct ec_response_codec_gain resp;
	int ret;


	dev_dbg(component->dev, "%s get mic gain\n", __func__);

	param.cmd = EC_CODEC_GET_GAIN;

	ret = ec_command(component, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 (uint8_t *)&resp, sizeof(resp));
	if (ret < 0) {
		dev_err(component->dev, "I2S get gain command returned 0x%x\n",
			 ret);
		return -EINVAL;
	}

	*left = resp.left;
	*right = resp.right;

	dev_dbg(component->dev, "%s get mic gain %u, %u\n", __func__, *left, *right);
#endif
	return 0;
}

static int mic_gain_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	uint8_t left, right;
	int ret;

	ret = get_ec_mic_gain(component, &left, &right);

	if (ret)
		return ret;

	ucontrol->value.integer.value[0] = left;
	ucontrol->value.integer.value[1] = right;

	return 0;
}

static int set_ec_mic_gain(struct snd_soc_component *component,
			   uint8_t left, uint8_t right)
{
#if 0
	struct ec_param_codec_i2s param;
	int ret;

	dev_dbg(component->dev, "%s set mic gain to %u, %u\n",
		__func__, left, right);

	param.cmd = EC_CODEC_SET_GAIN;
	param.gain.left = left;
	param.gain.right = right;

	ret = ec_command(component, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 NULL, 0);
	if (ret < 0) {
		dev_err(component->dev, "I2S set gain command returned 0x%x\n",
			ret);
		return -EINVAL;
	}
#endif
	return 0;
}

static int mic_gain_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	int left = ucontrol->value.integer.value[0];
	int right = ucontrol->value.integer.value[1];
	int ret;

	if (left > MAX_GAIN || right > MAX_GAIN)
		return -EINVAL;

	ret = set_ec_mic_gain(component, (uint8_t)left, (uint8_t)right);

	if (ret)
		return ret;

	return 0;
}

static const struct snd_kcontrol_new cros_ec_snd_controls[] = {
	SOC_DOUBLE_EXT_TLV("EC Mic Gain", SND_SOC_NOPM, SND_SOC_NOPM, 0, 43, 0,
		       mic_gain_get, mic_gain_put, ec_mic_gain_tlv)
};

static int enable_i2s(struct snd_soc_component *component, int enable)
{
	struct ec_param_codec_i2s param;
	int ret;

	dev_dbg(component->dev, "%s set i2s to %u\n", __func__, enable);

	param.cmd = EC_CODEC_I2S_ENABLE;
	param.i2s_enable = enable;

	ret = ec_command(component, 0, EC_CMD_CODEC_I2S,
			 (uint8_t *)&param, sizeof(param),
			 NULL, 0);
	if (ret < 0) {
		dev_err(component->dev, "I2S enable %d command returned 0x%x\n",
			 enable, ret);
		return -EINVAL;
	}
	return 0;
}

static int cros_ec_i2s_enable_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	dev_dbg(component->dev, "%s enter\n", __func__);

	switch(event) {

	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(component->dev, "%s got SND_SOC_DAPM_PRE_PMU event\n", __func__);
		return enable_i2s(component, 1);

	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(component->dev, "%s got SND_SOC_DAPM_PRE_PMD event\n", __func__);
		return enable_i2s(component, 0);
	}

	return 0;
}

/*
 * The goal of this DAPM route is to turn on/off I2S using EC
 * host command when capture stream is added / removed.
 */
static const struct snd_soc_dapm_widget cros_ec_dapm_widgets[] = {
	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMIC"),

        /* Control EC to enable/disable I2S. */
	SND_SOC_DAPM_SUPPLY("I2S Enable", SND_SOC_NOPM,
		 0, 0, cros_ec_i2s_enable_event,
		 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_AIF_OUT("I2STX", "I2S Capture", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route cros_ec_dapm_routes[] = {
	{ "I2STX", NULL, "DMIC" },
	{ "I2STX", NULL, "I2S Enable" },
};


static struct snd_soc_component_driver cros_ec_component_driver = {
	.controls		= cros_ec_snd_controls,
	.num_controls		= ARRAY_SIZE(cros_ec_snd_controls),
	.dapm_widgets		= cros_ec_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(cros_ec_dapm_widgets),
	.dapm_routes		= cros_ec_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(cros_ec_dapm_routes),
};

/*
 * Platform device and platform driver fro cros-ec-codec.
 */
static int cros_ec_codec_platform_probe(struct platform_device *pd)
{
	struct device *dev = &pd->dev;
	struct cros_ec_device *ec_device = dev_get_drvdata(pd->dev.parent);
	struct cros_ec_codec_data *codec_data;
	int rc;

	dev_dbg(dev, "%s start\n", __func__);

	/*
	 * If the parent ec device has not been probed yet, defer the probe of
	 * this keyboard/button driver until later.
	 */
	if (ec_device == NULL) {
		dev_dbg(dev, "No EC device found yet.\n");
		return -EPROBE_DEFER;
	}

	codec_data = devm_kzalloc(dev, sizeof(struct cros_ec_codec_data),
				  GFP_KERNEL);
	if (!codec_data)
		return -ENOMEM;

	codec_data->dev = dev;
	codec_data->ec_device = ec_device;

	platform_set_drvdata(pd, codec_data);

	rc = snd_soc_register_component(dev, &cros_ec_component_driver,
				        cros_ec_dai, ARRAY_SIZE(cros_ec_dai));

	/* Test EC command */
	test_ec_mic_gain(codec_data);

	dev_dbg(dev, "%s done\n", __func__);

	return 0;
}

static int cros_ec_codec_platform_remove(struct platform_device *pd)
{
	struct device *dev = &pd->dev;
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int cros_ec_codec_platform_resume(struct device *dev)
{
	struct cros_ec_codec_data *codec_data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s start\n", __func__);

	if (!codec_data)
		return 0;

	codec_data->suspended = false;

	dev_dbg(dev, "%s done\n", __func__);
	return 0;
}

static int cros_ec_codec_platform_suspend(struct device *dev)
{
	struct cros_ec_codec_data *codec_data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s start\n", __func__);

	codec_data->suspended = true;

	dev_dbg(dev, "%s done\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id cros_ec_codec_of_match[] = {
	{ .compatible = "google,cros-ec-codec" },
	{},
};
MODULE_DEVICE_TABLE(of, cros_ec_codec_of_match);
#endif

static SIMPLE_DEV_PM_OPS(cros_ec_codec_platform_pm_ops,
	cros_ec_codec_platform_suspend,
	cros_ec_codec_platform_resume);

static struct platform_driver cros_ec_codec_platform_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &cros_ec_codec_platform_pm_ops,
		.of_match_table = of_match_ptr(cros_ec_codec_of_match),
	},
	.probe = cros_ec_codec_platform_probe,
	.remove = cros_ec_codec_platform_remove,
};

module_platform_driver(cros_ec_codec_platform_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Chrome EC codec driver");
MODULE_ALIAS("platform:" DRV_NAME);
