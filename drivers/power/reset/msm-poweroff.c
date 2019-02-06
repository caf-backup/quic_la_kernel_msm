/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/pm.h>

#define QCOM_SET_DLOAD_MODE 0x10
static void __iomem *msm_ps_hold;
static struct regmap *tcsr_regmap;
static unsigned int dload_mode_offset;
static int do_always_reboot;
unsigned int *readvalue;

static int do_msm_restart(struct notifier_block *nb, unsigned long action,
			   void *data)
{
	pr_err("In %s @ %d\n", __func__, __LINE__);
	regmap_read(tcsr_regmap, dload_mode_offset, readvalue);
	pr_err("Download mode cookie is %s\n cookie val is %d\n",
		(*readvalue != 0 ? "yes" : "No"), *readvalue);
	msm_trigger_wdog_bite();
	mdelay(10000);

	return NOTIFY_DONE;
}

static int do_msm_reboot(struct notifier_block *nb, unsigned long action,
			   void *data)
{
	pr_err("In %s @ %d\n", __func__, __LINE__);

	regmap_read(tcsr_regmap, dload_mode_offset, readvalue);
	pr_err("Download mode cookie is %s\n cookie val is %d\n",
		(*readvalue != 0 ? "yes" : "No"), *readvalue);
	regmap_write(tcsr_regmap, dload_mode_offset, 0x0);
	regmap_read(tcsr_regmap, dload_mode_offset, readvalue);
	pr_err("Download mode cookie is %s\n cookie val is %d\n",
		(*readvalue != 0 ? "yes" : "No"), *readvalue);

	writel(0, msm_ps_hold);
	mdelay(10);

	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call  = do_msm_restart,
};

static struct notifier_block reboot_blk = {
	.notifier_call = do_msm_reboot,
};

static int msm_restart_probe(struct platform_device *pdev)
{
	int ret;
	struct of_phandle_args args;
	struct device *dev = &pdev->dev;
	struct resource *mem;

	readvalue = devm_kzalloc(&pdev->dev,
						sizeof(*readvalue), GFP_KERNEL);
	ret = of_parse_phandle_with_fixed_args
				(dev->of_node, "qcom,dload-mode",
				1, 0, &args);
	if (ret < 0)
		return ret;

	tcsr_regmap = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	if (IS_ERR(tcsr_regmap))
		return PTR_ERR(tcsr_regmap);

	dload_mode_offset = args.args[0];

	/* Enable download mode by writing the cookie */
	regmap_write(tcsr_regmap, dload_mode_offset,
		     QCOM_SET_DLOAD_MODE);

	regmap_read(tcsr_regmap,
			dload_mode_offset, readvalue);
	pr_err("Download mode cookie is %s\n cookie val is %d\n",
		(*readvalue != 0 ? "yes" : "No"), *readvalue);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	msm_ps_hold = devm_ioremap_resource(dev, mem);
	if (IS_ERR(msm_ps_hold))
		return PTR_ERR(msm_ps_hold);

	register_reboot_notifier(&reboot_blk);
	if (do_always_reboot != 1)
		atomic_notifier_chain_register(
				&panic_notifier_list, &panic_blk);
	else
		atomic_notifier_chain_register(
				&panic_notifier_list, &reboot_blk);

	return 0;
}

static void msm_restart_shutdown(struct platform_device *pdev)
{
	/* Clean shutdown, disable download mode to allow normal restart */
	regmap_write(tcsr_regmap, dload_mode_offset, 0x0);
}

static const struct of_device_id of_msm_restart_match[] = {
	{ .compatible = "qcom,pshold", },
	{},
};
MODULE_DEVICE_TABLE(of, of_msm_restart_match);

static struct platform_driver msm_restart_driver = {
	.probe = msm_restart_probe,
	.driver = {
		.name = "msm-restart",
		.of_match_table = of_match_ptr(of_msm_restart_match),
	},
	.shutdown = msm_restart_shutdown,
};

static int __init msm_restart_init(void)
{
	return platform_driver_register(&msm_restart_driver);
}
device_initcall(msm_restart_init);
