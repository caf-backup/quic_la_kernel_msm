/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <linux/reset-controller.h>
#include <dt-bindings/reset/qcom,gcc-qca961x.h>
#include <dt-bindings/clock/qcom,gcc-dummy.h>

#include "common.h"
#include "clk-regmap.h"
#include "reset.h"

struct clk *clk;

static int clk_dummy_is_enabled(struct clk_hw *hw)
{
	return 1;
};

static int clk_dummy_enable(struct clk_hw *hw)
{
	return 0;
};

static void clk_dummy_disable(struct clk_hw *hw)
{
	return;
};

static u8 clk_dummy_get_parent(struct clk_hw *hw)
{
	return 0;
};

static int clk_dummy_set_parent(struct clk_hw *hw, u8 index)
{
	return 0;
};

static int clk_dummy_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	return 0;
};

static long clk_dummy_determine_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *p_rate, struct clk **p)
{
	return rate;
};

static unsigned long clk_dummy_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	return parent_rate;
};

const struct clk_ops clk_dummy_ops = {
	.is_enabled = clk_dummy_is_enabled,
	.enable = clk_dummy_enable,
	.disable = clk_dummy_disable,
	.get_parent = clk_dummy_get_parent,
	.set_parent = clk_dummy_set_parent,
	.set_rate = clk_dummy_set_rate,
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_determine_rate,
};

static struct clk_regmap dummy = {
	.hw.init = &(struct clk_init_data){
		.name = "dummy_clk_src",
		.parent_names = (const char *[]){ "xo"},
		.num_parents = 1,
		.ops = &clk_dummy_ops,
	},
};

static const struct of_device_id gcc_dummy_match_table[] = {
	{ .compatible = "qcom,gcc-dummy" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_dummy_match_table);

static struct clk_regmap *gcc_qca961x_clks[] = {
	[GCC_DUMMY_CLK] = &dummy,
};

static const struct qcom_reset_map gcc_qca961x_resets[] = {
	[WIFI0_CPU_INIT_RESET] = { 0x1f008, 5 },
	[WIFI0_RADIO_SRIF_RESET] = { 0x1f008, 4 },
	[WIFI0_RADIO_WARM_RESET] = { 0x1f008, 3 },
	[WIFI0_RADIO_COLD_RESET] = { 0x1f008, 2 },
	[WIFI0_CORE_WARM_RESET] = { 0x1f008, 1 },
	[WIFI0_CORE_COLD_RESET] = { 0x1f008, 0 },
	[WIFI1_CPU_INIT_RESET] = { 0x20008, 5 },
	[WIFI1_RADIO_SRIF_RESET] = { 0x20008, 4 },
	[WIFI1_RADIO_WARM_RESET] = { 0x20008, 3 },
	[WIFI1_RADIO_COLD_RESET] = { 0x20008, 2 },
	[WIFI1_CORE_WARM_RESET] = { 0x20008, 1 },
	[WIFI1_CORE_COLD_RESET] = { 0x20008, 0 },
};

static const struct regmap_config gcc_qca961x_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0xffffc,
	.fast_io	= true,
};

static const struct qcom_cc_desc gcc_qca961x_desc = {
	.config = &gcc_qca961x_regmap_config,
	.clks = gcc_qca961x_clks,
	.num_clks = ARRAY_SIZE(gcc_qca961x_clks),
	.resets = gcc_qca961x_resets,
	.num_resets = ARRAY_SIZE(gcc_qca961x_resets),
};

static int gcc_dummy_probe(struct platform_device *pdev)
{
	int ret;

	clk = clk_register_fixed_rate(&pdev->dev, "xo", NULL, CLK_IS_ROOT,
				      19200000);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	ret = qcom_cc_probe(pdev, &gcc_qca961x_desc);

	dev_dbg(&pdev->dev, "Registered dummy clock provider\n");
	return ret;
}

static int gcc_dummy_remove(struct platform_device *pdev)
{
	qcom_cc_remove(pdev);
	return 0;
}

static struct platform_driver gcc_dummy_driver = {
	.probe		= gcc_dummy_probe,
	.remove		= gcc_dummy_remove,
	.driver		= {
		.name	= "gcc-dummy",
		.owner	= THIS_MODULE,
		.of_match_table = gcc_dummy_match_table,
	},
};

static int __init gcc_dummy_init(void)
{
	return platform_driver_register(&gcc_dummy_driver);
}
core_initcall(gcc_dummy_init);

static void __exit gcc_dummy_exit(void)
{
	platform_driver_unregister(&gcc_dummy_driver);
}
module_exit(gcc_dummy_exit);

MODULE_DESCRIPTION("QCOM GCC Dummy Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gcc-dummy");
