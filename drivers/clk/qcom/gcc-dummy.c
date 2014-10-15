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

struct clk_dummy {
	struct clk_hw hw;
};

static struct clk_dummy dummy = {
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

struct clk *of_dummy_get(struct of_phandle_args *clkspec, void *data)
{
	return clk;
};

static int gcc_dummy_probe(struct platform_device *pdev)
{
	int ret;

	clk = clk_register_fixed_rate(&pdev->dev, "xo", NULL, CLK_IS_ROOT,
				      19200000);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	clk = devm_clk_register(&pdev->dev, &dummy.hw);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	ret = of_clk_add_provider(pdev->dev.of_node, of_dummy_get, NULL);
	if (ret)
		return -ENOMEM;

	dev_dbg(&pdev->dev, "Registered dummy clock provider\n");
	return ret;
}

static int gcc_dummy_remove(struct platform_device *pdev)
{
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
