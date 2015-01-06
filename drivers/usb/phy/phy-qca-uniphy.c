/* Copyright (c) 2015, Code Aurora Forum. All rights reserved.
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
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <linux/reset.h>
#include <linux/of_device.h>
/**
 *  USB Hardware registers
 */
#define QSRATCH_BASE		0x08af8800
#define USB30_SS_PHY_CTRL	0x00000030
#define LANE0_PWR_PRESENT	(0x01 << 0x18)

#define MDIO_CTRL_0_REG		0x40
#define MDIO_CTRL_1_REG		0x44
#define MDIO_CTRL_2_REG		0x48
#define MDIO_CTRL_3_REG		0x4C
#define MDIO_CTRL_4_REG		0x50

#define MDIO_USB_PHY_ID		(0x0 << 13)
#define MDC_MODE			(0x1 << 12)
#define MDIO_CLAUSE_22		(0x0 << 8)
#define MDIO_CLAUSE_45		(0x1 << 8)
#define MDIO_USB_CLK_DIV	(0xF)

#define MDIO_MMD_ID			(0x1)

#define MDIO_ACCESS_BUSY	(0x1 << 16)
#define MDIO_ACCESS_START	(0x1 << 8)
#define MDIO_ACCESS_WRITE	(0x1)
#define MDIO_ACCESS_READ	(0x0)

#define MDIO_TIMEOUT_STATIC	1000

struct qca_uni_ss_phy {
	struct usb_phy phy;
	struct device *dev;

	void __iomem *base;
	void __iomem *qscratch_base;

	struct reset_control *por_rst;

	unsigned int host;
	unsigned int emulation;
};

#define	phy_to_dw_phy(x)	container_of((x), struct qca_uni_ss_phy, phy)

/**
 * Write register
 *
 * @base - PHY base virtual address.
 * @offset - register offset.
 */
static u32 qca_uni_ss_read(void __iomem *base, u32 offset)
{
	u32 value;
	value = readl_relaxed(base + offset);
	return value;
}

/**
 * Write register
 *
 * @base - PHY base virtual address.
 * @offset - register offset.
 * @val - value to write.
 */
static void qca_uni_ss_write(void __iomem *base, u32 offset, u32 val)
{
	writel(val, base + offset);
}

/**
 * Write register and read back masked value to confirm it is written
 *
 * @base - PHY base virtual address.
 * @offset - register offset.
 * @mask - register bitmask specifying what should be updated
 * @val - value to write.
 */
static void qca_uni_ss_write_readback(void __iomem *base, u32 offset,
		const u32 mask, u32 val)
{
	u32 write_val, tmp = readl(base + offset);

	tmp &= ~mask;       /* retain other bits */
	write_val = tmp | val;

	writel(write_val, base + offset);

	/* Read back to see if val was written */
	tmp = readl(base + offset);
	tmp &= mask;        /* clear other bits */

	if (tmp != val)
		pr_err("write: %x to UNI PHY: %x FAILED\n", val, offset);
}

int mdio_wait(void __iomem *base)
{
	unsigned int mdio_access;
	unsigned int timeout = MDIO_TIMEOUT_STATIC;

	do {
		mdio_access = qca_uni_ss_read(base, MDIO_CTRL_4_REG);
		if (!timeout--)
			return -EFAULT;
	} while (mdio_access & MDIO_ACCESS_BUSY);

	return 0;
}

int mdio_mii_read(void __iomem *base, unsigned char regAddr, unsigned short *data)
{
	unsigned short mdio_ctl_0 = (MDIO_USB_PHY_ID | MDC_MODE | MDIO_CLAUSE_22
								| MDIO_USB_CLK_DIV);
	unsigned int regVal;

	qca_uni_ss_write(base, MDIO_CTRL_0_REG, mdio_ctl_0);
	qca_uni_ss_write(base, MDIO_CTRL_1_REG, regAddr);

	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_READ);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_READ| MDIO_ACCESS_START);

	//wait for access busy to be cleared
	if(mdio_wait(base)) {
		pr_err("%s MDIO Access Busy Timeout %x\n", __func__, regAddr);
		return -EFAULT;
	}

	regVal =  qca_uni_ss_read(base, MDIO_CTRL_3_REG);
	*data = (unsigned short)regVal;
	return 0;
}

int mdio_mii_write(void __iomem *base, unsigned char regAddr, unsigned short data)
{
	unsigned short mdio_ctl_0 = (MDIO_USB_PHY_ID | MDC_MODE | MDIO_CLAUSE_22
								| MDIO_USB_CLK_DIV);

	qca_uni_ss_write(base, MDIO_CTRL_0_REG, mdio_ctl_0);
	qca_uni_ss_write(base, MDIO_CTRL_1_REG, regAddr);
	qca_uni_ss_write(base, MDIO_CTRL_2_REG, data);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE| MDIO_ACCESS_START);

	//wait for access busy to be cleared
	if(mdio_wait(base)) {
		pr_err("%s MDIO Access Busy Timeout %x\n", __func__, regAddr);
		return -EFAULT;
	}

	return 0;
}

int mdio_mmd_read(void __iomem *base, unsigned short regAddr, unsigned short *data)
{
	unsigned short mdio_ctl_0 = (MDIO_USB_PHY_ID | MDC_MODE | MDIO_CLAUSE_45
								| MDIO_USB_CLK_DIV);
	unsigned int regVal;

	qca_uni_ss_write(base, MDIO_CTRL_0_REG, mdio_ctl_0);
	qca_uni_ss_write(base, MDIO_CTRL_1_REG, MDIO_MMD_ID);
	qca_uni_ss_write(base, MDIO_CTRL_2_REG, regAddr);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE| MDIO_ACCESS_START);

	//wait for access busy to be cleared
	if(mdio_wait(base)) {
		pr_err("%s MDIO Access Busy Timeout %x\n", __func__, regAddr);
		return -EFAULT;
	}

	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_READ);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_READ| MDIO_ACCESS_START);

	//wait for access busy to be cleared
	if(mdio_wait(base)) {
		pr_err("%s MDIO Access Busy Timeout %x\n", __func__, regAddr);
		return -EFAULT;
	}

	regVal =  qca_uni_ss_read(base, MDIO_CTRL_3_REG);
	*data = (unsigned short)regVal;

	return 0;
}

int mdio_mmd_write(void __iomem *base, unsigned short regAddr, unsigned short data)
{

	unsigned short mdio_ctl_0 = (MDIO_USB_PHY_ID | MDC_MODE | MDIO_CLAUSE_45
								| MDIO_USB_CLK_DIV);

	qca_uni_ss_write(base, MDIO_CTRL_0_REG, mdio_ctl_0);
	qca_uni_ss_write(base, MDIO_CTRL_1_REG, MDIO_MMD_ID);
	qca_uni_ss_write(base, MDIO_CTRL_2_REG, regAddr);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE| MDIO_ACCESS_START);

	//wait for access busy to be cleared
	if(mdio_wait(base)) {
		pr_err("%s MDIO Access Busy Timeout %x\n", __func__, regAddr);
		return -EFAULT;
	}

	qca_uni_ss_write(base, MDIO_CTRL_2_REG, data);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE);
	qca_uni_ss_write(base, MDIO_CTRL_4_REG, MDIO_ACCESS_WRITE| MDIO_ACCESS_START);

	//wait for access busy to be cleared
	if(mdio_wait(base)) {
		pr_err("%s MDIO Access Busy Timeout %x\n", __func__, regAddr);
		return -EFAULT;
	}

	return 0;
}

static void qca_uni_ss_phy_shutdown(struct usb_phy *x)
{
	struct qca_uni_ss_phy *phy = phy_to_dw_phy(x);

	/* assert SS PHY POR reset */
	reset_control_assert(phy->por_rst);
}

static int qca_uni_ss_phy_set_vbus(struct usb_phy *x, int on)
{
	struct qca_uni_ss_phy  *phy = phy_to_dw_phy(x);

	if (on)
	{
		qca_uni_ss_write_readback(phy->qscratch_base, USB30_SS_PHY_CTRL,
				LANE0_PWR_PRESENT, LANE0_PWR_PRESENT);
	}
	else
	{
		qca_uni_ss_write_readback(phy->qscratch_base, USB30_SS_PHY_CTRL,
				LANE0_PWR_PRESENT, 0x0);
	}
	return 0;
}

static int qca_uni_ss_phy_init(struct usb_phy *x)
{
	struct qca_uni_ss_phy *phy = phy_to_dw_phy(x);

	/* assert SS PHY POR reset */
	reset_control_assert(phy->por_rst);

	udelay(100);

	if (phy->emulation)
	{
		mdio_mii_write(phy->base, 0x1, 0x8017);
		mdio_mii_write(phy->base, 0xb, 0x300d);
		mdio_mmd_write(phy->base, 0x2d, 0x681a);
		mdio_mmd_write(phy->base, 0x7d, 0x8);
		mdio_mmd_write(phy->base, 0x7f, 0x5ed5);
		mdio_mmd_write(phy->base, 0x87, 0xaa0a);
		mdio_mmd_write(phy->base, 0x4, 0x0802);
		mdio_mmd_write(phy->base, 0x8, 0x0280);
		mdio_mmd_write(phy->base, 0x9, 0x8854);
		mdio_mmd_write(phy->base, 0xa, 0x2815);
		mdio_mmd_write(phy->base, 0xb, 0x2120);
		mdio_mmd_write(phy->base, 0xb, 0x0120);
		mdio_mmd_write(phy->base, 0xc, 0x0480);
		mdio_mmd_write(phy->base, 0x13, 0x8000);
		mdio_mmd_write(phy->base, 0x7c, 0x82);
	}
	else
	{

	}

	/* deassert SS PHY POR reset */
	reset_control_deassert(phy->por_rst);

	if(!phy->host)
		qca_uni_ss_phy_set_vbus(x, 1);

	return 0;
}

static int qca_uni_ss_get_resources(struct platform_device *pdev,
		struct qca_uni_ss_phy *phy)
{
	struct resource *res;
	struct device_node *np = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy->base = devm_ioremap_resource(phy->dev, res);
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	phy->qscratch_base = devm_ioremap_resource(phy->dev, res);
	if (IS_ERR(phy->qscratch_base))
		return PTR_ERR(phy->qscratch_base);

	phy->por_rst = devm_reset_control_get(phy->dev, "por");
	if (IS_ERR(phy->por_rst))
		return PTR_ERR(phy->por_rst);

	np = of_node_get(pdev->dev.of_node);
	if ( of_property_read_u32(np, "qca,host", &phy->host)
			|| of_property_read_u32(np, "qca,emulation", &phy->emulation)) {
		pr_err("%s: error reading critical device node properties\n", np->name);
		return -EFAULT;
	}

	return 0;
}

static int qca_dummy_ss_phy_init(struct usb_phy *x)
{
	return 0;
}

static void qca_dummy_ss_phy_shutdown(struct usb_phy *x)
{
}

static int qca_uni_ss_remove(struct platform_device *pdev)
{
	struct qca_uni_ss_phy *phy = platform_get_drvdata(pdev);

	usb_remove_phy(&phy->phy);
	return 0;
}

static const struct of_device_id qca_uni_ss_id_table[] = {
	{ .compatible = "qca,uni-ssphy" },
	{ .compatible = "qca,dummy-ssphy"},
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, qca_uni_ss_id_table);

static int qca_uni_ss_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	struct qca_uni_ss_phy  *phy;
	int ret;

	match = of_match_device(qca_uni_ss_id_table, &pdev->dev);
	if (!match)
		return -ENODEV;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	platform_set_drvdata(pdev, phy);
	phy->dev = &pdev->dev;

	if (of_device_is_compatible(np, "qca,dummy-ssphy"))
	{
		phy->phy.dev        = phy->dev;
		phy->phy.label      = "qca-dummy-ssphy";
		phy->phy.init       = qca_dummy_ss_phy_init;
		phy->phy.shutdown   = qca_dummy_ss_phy_shutdown;
		phy->phy.type       = USB_PHY_TYPE_USB3;

		ret = usb_add_phy_dev(&phy->phy);
		return ret;
	}

	ret = qca_uni_ss_get_resources(pdev, phy);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request resources: %d\n", ret);
		return ret;
	}

	phy->phy.dev        = phy->dev;
	phy->phy.label      = "qca-uni-ssphy";
	phy->phy.init       = qca_uni_ss_phy_init;
	phy->phy.shutdown   = qca_uni_ss_phy_shutdown;
	phy->phy.type       = USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	return ret;
}

static struct platform_driver qca_uni_ss_driver = {
	.probe		= qca_uni_ss_probe,
	.remove		= qca_uni_ss_remove,
	.driver		= {
		.name	= "qca-uni-ssphy",
		.owner	= THIS_MODULE,
		.of_match_table = qca_uni_ss_id_table,
	},
};

module_platform_driver(qca_uni_ss_driver);

MODULE_ALIAS("platform:qca-uni-ssphy");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("USB3 QCA UNI SSPHY driver");
