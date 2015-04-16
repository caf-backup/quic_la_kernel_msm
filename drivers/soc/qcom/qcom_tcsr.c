/*
 * Copyright (c) 2014, The Linux foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License rev 2 and
 * only rev 2 as published by the free Software foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or fITNESS fOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define TCSR_USB_PORT_SEL	0xb0
#define TCSR_USB_HSPHY_CONFIG	0xC

#define TCSR_ESS_INTERFACE_SEL_OFFSET   0x0
#define TCSR_ESS_INTERFACE_SEL_MASK     0xf

static int tcsr_probe(struct platform_device *pdev)
{
	struct resource *res;
	const struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	u32 val;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	if (!of_property_read_u32(node, "qcom,usb-ctrl-select", &val)) {
		dev_err(&pdev->dev, "setting usb port select = %d\n", val);
		writel(val, base + TCSR_USB_PORT_SEL);
	}

	if (!of_property_read_u32(node, "qcom,usb-hsphy-mode-select", &val)) {
		dev_info(&pdev->dev, "setting usb hs phy mode select = %x\n", val);
		writel(val, base + TCSR_USB_HSPHY_CONFIG);
	}

	if (!of_property_read_u32(node, "qcom,ess-interface-select", &val)) {
		u32 tmp = 0;
		dev_info(&pdev->dev, "setting ess interface select = %x\n", val);
		tmp = readl(base + TCSR_ESS_INTERFACE_SEL_OFFSET);
		tmp = tmp & (~TCSR_ESS_INTERFACE_SEL_MASK);
		tmp = tmp | (val&TCSR_ESS_INTERFACE_SEL_MASK);
		writel(tmp, base + TCSR_ESS_INTERFACE_SEL_OFFSET);
        }

	return 0;
}

static const struct of_device_id tcsr_dt_match[] = {
	{ .compatible = "qcom,tcsr", },
	{ },
};

MODULE_DEVICE_TABLE(of, tcsr_dt_match);

static struct platform_driver tcsr_driver = {
	.driver = {
		.name		= "tcsr",
		.owner		= THIS_MODULE,
		.of_match_table	= tcsr_dt_match,
	},
	.probe = tcsr_probe,
};

module_platform_driver(tcsr_driver);

MODULE_AUTHOR("Andy Gross <agross@codeaurora.org>");
MODULE_DESCRIPTION("QCOM TCSR driver");
MODULE_LICENSE("GPL v2");
