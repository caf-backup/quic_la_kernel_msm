/* Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/io.h>
#include "ipq806x.h"

static struct clk *lpass_ahbex_clk;
static struct clk *lpass_ahbix_clk;
struct lpass_clk_baseinfo lpass_clk_base;
EXPORT_SYMBOL_GPL(lpass_clk_base);

static struct request_gpio mi2s_gpios[] = {
	{
		.gpio_no = GPIO_MI2S_WS,
		.gpio_name = "MI2S_WS",
	},
	{
		.gpio_no = GPIO_MI2S_SCK,
		.gpio_name = "GPIO_MI2S_SCK",
	},
	{
		.gpio_no = GPIO_MI2S_SD3,
		.gpio_name = "GPIO_MI2S_SD3",
	},
	{
		.gpio_no = GPIO_MI2S_SD2,
		.gpio_name = "GPIO_MI2S_SD2",
	},
	{
		.gpio_no = GPIO_MI2S_SD1,
		.gpio_name = "GPIO_MI2S_SD1",
	},
	{
		.gpio_no = GPIO_MI2S_SD0,
		.gpio_name = "GPIO_MI2S_SD0",
	},
	{
		.gpio_no = GPIO_MI2S_MCLK,
		.gpio_name = "GPIO_MI2S_MCLK",
	},
};

static struct request_gpio pcm_gpios[] = {
	{
		.gpio_no = GPIO_PCM_DOUT,
		.gpio_name = "GPIO_PCM_DOUT",
	},
	{
		.gpio_no = GPIO_PCM_DIN,
		.gpio_name = "GPIO_PCM_DIN",
	},
	{
		.gpio_no = GPIO_PCM_SYNC,
		.gpio_name = "GPIO_PCM_SYNC",
	},
	{
		.gpio_no = GPIO_PCM_CLK,
		.gpio_name = "GPIO_PCM_CLK",
	},
};

static int ipq806x_cfg_mi2s_gpios(void)
{
	int i;
	int ret;
	int j;

	for (i = 0; i < ARRAY_SIZE(mi2s_gpios); i++) {
		ret = gpio_request(mi2s_gpios[i].gpio_no,
					mi2s_gpios[i].gpio_name);
		if (ret) {
			pr_err("%s Failed to request gpio %d\n",
					__func__, mi2s_gpios[i].gpio_no);

			for (j = i; j >= 0; j--)
				gpio_free(mi2s_gpios[i].gpio_no);

			goto err;
		}
	}
err:
	return ret;
}

static int ipq806x_cfg_pcm_gpios(void)
{
	int i;
	int ret;
	int j;

	for (i = 0; i < ARRAY_SIZE(pcm_gpios); i++) {
		ret = gpio_request(pcm_gpios[i].gpio_no,
					pcm_gpios[i].gpio_name);
		if (ret) {
			pr_err("%s failed failed to request gpio %d\n",
					__func__, pcm_gpios[i].gpio_no);
			goto err;
		}
		if (pcm_gpios[i].gpio_no != GPIO_PCM_DIN) {
			ret = gpio_direction_output(pcm_gpios[i].gpio_no, 0x2);
			if (ret) {
				pr_err("%s err in setting dir for gpio :%d\n",
					__func__, pcm_gpios[i].gpio_no);
				goto err;
			}
		}
	}

	return ret;

err:
	for (j = i; j >= 0; j--)
		gpio_free(pcm_gpios[i].gpio_no);

	return ret;
}

static int ipq806x_cfg_spdif_gpios(void)
{
	int ret;
	ret = gpio_request(GPIO_SPDIF, "GPIO_SPDIF");
	if (ret) {
		pr_err("%s error in requesting spdif gpio %d\n",
					__func__, GPIO_SPDIF);
		goto err;
	}

	ret = gpio_direction_output(GPIO_SPDIF, 0x2);
	if (ret) {
		pr_err("%s err in setting the dir for %d\n",
					__func__, GPIO_SPDIF);
		goto err;
	}

	return ret;

err:
	gpio_free(GPIO_SPDIF);
	return ret;
}

static void ipq806x_mi2s_free_gpios(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(mi2s_gpios); i++)
		gpio_free(mi2s_gpios[i].gpio_no);
}

static void ipq806x_pcm_free_gpios(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pcm_gpios); i++)
		gpio_free(pcm_gpios[i].gpio_no);
}

static void ipq806x_spdif_free_gpios(void)
{
	gpio_free(GPIO_SPDIF);
}

static struct snd_soc_dai_link ipq_snd_dai[] = {
	/* Front end DAI Links */
	{
		.name		= "IPQ806x Media1",
		.stream_name	= "MultiMedia1",
		/* Front End DAI Name */
		.cpu_dai_name	= "ipq-mi2s-dai",
		/* Platform Driver Name */
		.platform_name	= "ipq-pcm-mi2s",
		/* Codec DAI Name */
		.codec_dai_name	= "ipq-mi2s-codec-dai",
		/*Codec Driver Name */
		.codec_name	= "ipq-lpass-codec",
	},
	{
		.name		= "IPQ806x Media2",
		.stream_name	= "MultiMedia2",
		.cpu_dai_name	= "ipq-spdif-dai",
		.platform_name	= "ipq-pcm-spdif",
		.codec_dai_name	= "ipq-spdif-codec-dai",
		.codec_name	= "ipq-lpass-codec",
	},
	{
		.name		= "IPQ806x VoIP",
		.stream_name	= "CS-Voice",
		.cpu_dai_name	= "ipq-pcm-dai",
		.platform_name	= "ipq-pcm-voip",
		.codec_dai_name	= "ipq-pcm-codec-dai",
		.codec_name	= "ipq-lpass-codec",
	},
};

static struct snd_soc_card snd_soc_card_ipq = {
	.name		= "ipq806x_snd_card",
	.dai_link	= ipq_snd_dai,
	.num_links	= ARRAY_SIZE(ipq_snd_dai),
};

static int ipq_lpass_clk_probe(struct platform_device *pdev)
{
	uint32_t ret;
	struct resource *clk_res;

	clk_res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ipq-lpass-clk");

	if (!clk_res) {
		dev_err(&pdev->dev, "%s: %d: mem resource for lpass failed\n",
							__func__, __LINE__);
		return -ENOMEM;
	}

	lpass_clk_base.base = ioremap(
		clk_res->start, (clk_res->end - clk_res->start) + 1);

	if (!lpass_clk_base.base) {
		dev_err(&pdev->dev, "%s: %d: Error in lpass clk mem remap\n",
							__func__, __LINE__);
		return -ENOMEM;
	}

	lpass_ahbex_clk = clk_get(&pdev->dev, "ahbex_clk");
	if (IS_ERR(lpass_ahbex_clk)) {
		dev_err(&pdev->dev, "%s: %d: Error in getting ahbex_clk\n",
							__func__, __LINE__);
		ret = PTR_ERR(lpass_ahbex_clk);
		goto err;
	}
	ret = clk_prepare_enable(lpass_ahbex_clk);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "%s: %d: Error in enabling ahbex_clk\n",
							__func__, __LINE__);
		goto err;
	}

	lpass_ahbix_clk = clk_get(&pdev->dev, "ahbix_clk");
	if (IS_ERR(lpass_ahbix_clk)) {
		dev_err(&pdev->dev, "%s: %d: Error in getting ahbix_clk\n",
							__func__, __LINE__);
		ret = PTR_ERR(lpass_ahbix_clk);
		goto err;
	}
	clk_set_rate(lpass_ahbix_clk, 131072);
	ret = clk_prepare_enable(lpass_ahbix_clk);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "%s: %d: Error in enabling ahbix_clk\n",
							__func__, __LINE__);
		goto err;
	}

	return 0;
err:
	iounmap(lpass_clk_base.base);
	return ret;

}

static int ipq_lpass_clk_remove(struct platform_device *pdev)
{
	pr_debug("%s %d\n", __func__, __LINE__);
	if (lpass_ahbex_clk) {
		clk_disable_unprepare(lpass_ahbex_clk);
		clk_put(lpass_ahbex_clk);
		lpass_ahbex_clk = NULL;
	}

	if (lpass_ahbix_clk) {
		clk_disable_unprepare(lpass_ahbix_clk);
		clk_put(lpass_ahbix_clk);
		lpass_ahbix_clk = NULL;
	}
	iounmap(lpass_clk_base.base);
	return 0;
}

static struct platform_driver ipq_lpass_clk = {
	.driver = {
		.name   = "ipq-lpass-clk",
		.owner  = THIS_MODULE,
	},
	.probe  = ipq_lpass_clk_probe,
	.remove = ipq_lpass_clk_remove,
};

static struct platform_device *ipq_snd_device;
static int __init ipq806x_lpass_init(void)
{
	int ret;

	ipq_snd_device = platform_device_alloc("soc-audio", -1);
	if (!ipq_snd_device) {
		pr_err("%s: snd device allocation failed\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(ipq_snd_device, &snd_soc_card_ipq);
	ret = platform_device_add(ipq_snd_device);
	if (ret) {
		pr_err("%s: snd device add failed\n", __func__);
		goto err_add;
	}

	ret = platform_driver_register(&ipq_lpass_clk);
	if (ret) {
		pr_err("%s: %d: clk registration failed\n",
					__func__, __LINE__);
		goto err_reg;
	}

	ret = ipq806x_cfg_mi2s_gpios();
	if (ret) {
		pr_err("%s: %d: mi2s gpios configuration failed\n",
					__func__, __LINE__);
		goto err_reg;
	}

	ret = ipq806x_cfg_pcm_gpios();
	if (ret) {
		pr_err("%s: %d: pcm gpios configuration failed\n",
					__func__, __LINE__);
		goto err_reg;
	}

	ret = ipq806x_cfg_spdif_gpios();
	if (ret) {
		pr_err("%s: %d: spdif gpios configuration failed\n",
					__func__, __LINE__);
		goto err_reg;
	}

	return ret;

err_reg:
	platform_driver_unregister(&ipq_lpass_clk);
err_add:
	platform_device_del(ipq_snd_device);
	platform_device_put(ipq_snd_device);
	return ret;
}

static void __exit ipq806x_lpass_exit(void)
{
	ipq806x_mi2s_free_gpios();
	ipq806x_pcm_free_gpios();
	ipq806x_spdif_free_gpios();

	platform_driver_unregister(&ipq_lpass_clk);
	platform_device_unregister(ipq_snd_device);
}


module_init(ipq806x_lpass_init);
module_exit(ipq806x_lpass_exit);

MODULE_DESCRIPTION("ALSA SoC IPQ806x Machine Driver");
MODULE_LICENSE("GPL v2");
