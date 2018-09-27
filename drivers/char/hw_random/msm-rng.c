/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interconnect.h>

/* Device specific register offsets */
#define PRNG_DATA_OUT		0x0000
#define PRNG_STATUS		0x0004
#define PRNG_LFSR_CFG		0x0100
#define PRNG_CONFIG		0x0104

/* Device specific register masks and config values */
#define PRNG_LFSR_CFG_MASK	0x0000ffff
#define PRNG_LFSR_CFG_CLOCKS	0x0000dddd
#define PRNG_CONFIG_HW_ENABLE	BIT(1)
#define PRNG_STATUS_DATA_AVAIL	BIT(0)

#define MAX_HW_FIFO_DEPTH	16
#define MAX_HW_FIFO_SIZE	(MAX_HW_FIFO_DEPTH * 4)
#define WORD_SZ			4
#define PRNG_RETRY_COUNT	5
#define PRNG_RETRY_DELAY_USEC	440

struct msm_rng {
	void __iomem *base;
	struct clk *clk;
	struct hwrng hwrng;
	uint32_t qrng_perf_client;
	struct mutex rng_lock;
	bool skip_qrng_config;
	struct icc_path *path;
	uint32_t min_bw;
	uint32_t max_bw;
};

#define to_msm_rng(p)	container_of(p, struct msm_rng, hwrng)

static int msm_rng_enable(struct hwrng *hwrng, int enable)
{
	struct msm_rng *rng = to_msm_rng(hwrng);
	u32 val;
	int ret;

	/*
	 * Configuration of these registers happen on secure side
	 * on latest platforms hence skipping this part.
	 */
	if (rng->skip_qrng_config)
		return 0;

	icc_set(rng->path, 0, rng->max_bw);
	ret = clk_prepare_enable(rng->clk);
	if (ret)
		return ret;

	if (enable) {
		/* Enable PRNG only if it is not already enabled */
		val = readl_relaxed(rng->base + PRNG_CONFIG);
		if (val & PRNG_CONFIG_HW_ENABLE)
			goto already_enabled;

		val = readl_relaxed(rng->base + PRNG_LFSR_CFG);
		val &= ~PRNG_LFSR_CFG_MASK;
		val |= PRNG_LFSR_CFG_CLOCKS;
		writel(val, rng->base + PRNG_LFSR_CFG);

		val = readl_relaxed(rng->base + PRNG_CONFIG);
		val |= PRNG_CONFIG_HW_ENABLE;
		writel(val, rng->base + PRNG_CONFIG);
	} else {
		val = readl_relaxed(rng->base + PRNG_CONFIG);
		val &= ~PRNG_CONFIG_HW_ENABLE;
		writel(val, rng->base + PRNG_CONFIG);
	}

already_enabled:
	clk_disable_unprepare(rng->clk);
	icc_set(rng->path, 0, rng->min_bw);
	return 0;
}

static int msm_rng_read(struct hwrng *hwrng, void *data, size_t max, bool wait)
{
	struct msm_rng *rng = to_msm_rng(hwrng);
	size_t currsize = 0;
	u32 *retdata = data;
	uint32_t failed = 0;
	size_t maxsize;
	int ret;
	u32 val;

	/* calculate max size bytes to transfer back to caller */
	maxsize = min_t(size_t, MAX_HW_FIFO_SIZE, max);

	mutex_lock(&rng->rng_lock);
	icc_set(rng->path, 0, rng->max_bw);
	ret = clk_prepare_enable(rng->clk);
	if (ret)
		return ret;

	/* read random data from hardware */
	do {
		val = readl_relaxed(rng->base + PRNG_STATUS);
		if (!(val & PRNG_STATUS_DATA_AVAIL)) {
			if (failed++ == PRNG_RETRY_COUNT) {
				if (currsize == 0)
					pr_err("Data not available\n");
				break;
			}
			udelay(PRNG_RETRY_DELAY_USEC);
		} else {
			val = readl_relaxed(rng->base + PRNG_DATA_OUT);
			if (!val)
				break;

			*retdata++ = val;
			currsize += WORD_SZ;

			/* make sure we stay on 32bit boundary */
			if ((maxsize - currsize) < WORD_SZ)
				break;
		}
	} while (currsize < maxsize);

	clk_disable_unprepare(rng->clk);
	icc_set(rng->path, 0, rng->min_bw);
	mutex_unlock(&rng->rng_lock);

	return currsize;
}

static int msm_rng_init(struct hwrng *hwrng)
{
	return msm_rng_enable(hwrng, 1);
}

static void msm_rng_cleanup(struct hwrng *hwrng)
{
	msm_rng_enable(hwrng, 0);
}

static int msm_rng_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_rng *rng;
	int ret;

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	platform_set_drvdata(pdev, rng);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rng->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rng->base))
		return PTR_ERR(rng->base);

	rng->clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(rng->clk))
		return PTR_ERR(rng->clk);

	if (pdev->dev.of_node) {
		rng->skip_qrng_config = of_property_read_bool(pdev->dev.of_node,
				"qcom,no-qrng-config");
		if (of_property_read_u32(pdev->dev.of_node,
				"qcom,bandwidth-min", &rng->min_bw)) {
			pr_err("failed to get min bandwidth\n");
			return -EINVAL;
		}
		if (of_property_read_u32(pdev->dev.of_node,
				"qcom,bandwidth-max", &rng->max_bw)) {
			pr_err("failed to get max bandwidth\n");
			return -EINVAL;
		}
	}

	rng->path = of_icc_get(&pdev->dev, "cpu");
	if (IS_ERR(rng->path))
		return PTR_ERR(rng->path);

	mutex_init(&rng->rng_lock);
	rng->hwrng.name = KBUILD_MODNAME,
	rng->hwrng.init = msm_rng_init,
	rng->hwrng.cleanup = msm_rng_cleanup,
	rng->hwrng.read = msm_rng_read,

	ret = devm_hwrng_register(&pdev->dev, &rng->hwrng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id msm_rng_of_match[] = {
	{ .compatible = "qcom,prng", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_rng_of_match);

static struct platform_driver msm_rng_driver = {
	.probe = msm_rng_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = of_match_ptr(msm_rng_of_match),
	}
};
module_platform_driver(msm_rng_driver);

MODULE_ALIAS("platform:" KBUILD_MODNAME);
MODULE_AUTHOR("The Linux Foundation");
MODULE_DESCRIPTION("Qualcomm MSM random number generator driver");
MODULE_LICENSE("GPL v2");
