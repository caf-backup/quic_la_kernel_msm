/*
 * qca_85xx_sw.c: QCA 85xx switch driver
 *
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/qca_85xx_sw.h>
#include <linux/debugfs.h>
#include <linux/phy.h>

 /* 85xx chipset specific data */
struct qca_85xx_sw_chip {
	int (*hw_init)(struct qca_85xx_sw_platform_data *pdata);
	const struct qca_85xx_sw_mib_desc *mib_decs;
	uint32_t num_mibs;
	uint32_t max_qsgmii_if;
	uint32_t max_sgmii_plus_if;
	uint32_t max_ge_if;
};

struct qca_85xx_sw_priv {
	uint32_t (*read)(uint32_t reg_addr);
	void (*write)(uint32_t reg_addr, uint32_t val);
	struct mii_bus *mdio_bus;
	struct net_device *eth_dev;
	const struct qca_85xx_sw_chip *chip;
	uint32_t reg_val;			/* Hold the value of the
						   previous register read op */
	uint32_t reg_addr;			/* Previous register */
	struct dentry *top_dentry;		/* Top dentry */
	struct dentry *write_dentry;		/* write-reg file dentry */
	struct dentry *read_dentry;		/* read-reg file dentry */
};

struct qca_85xx_sw_priv *priv;

 /* Configure a QSGMII interface */
static int qca_85xx_sw_init_qsgmii_port (struct qca_85xx_sw_qsgmii_cfg *qsgmii_cfg)
{
	uint32_t val, ch0_val, sd_clk_sel_val;
	uint32_t ch0_ctrl_addr, ch1_ctrl_addr, ch2_ctrl_addr, ch3_ctrl_addr;
	bool part_b = false;
	int group_num;

	if (qsgmii_cfg->port_base > (priv->chip->max_qsgmii_if * 4))
		return -EINVAL;

	switch (qsgmii_cfg->port_base) {
		case QCA_85XX_SW_PORT_1:
			group_num = 0;
			break;
		case QCA_85XX_SW_PORT_9:
			group_num = 1;
			break;
		case QCA_85XX_SW_PORT_17:
			group_num = 2;
			break;
		case QCA_85XX_SW_PORT_5:
			group_num = 0;
			part_b = true;
			break;
		case QCA_85XX_SW_PORT_13:
			group_num = 1;
			part_b = true;
			break;
		case QCA_85XX_SW_PORT_21:
			group_num = 2;
			part_b = true;
			break;
		default:
			return -EINVAL;
	}

	if (part_b) {
		ch0_ctrl_addr = QSGMII_5_CTRL0(group_num);
		ch1_ctrl_addr = QSGMII_6_CTRL0(group_num);
		ch2_ctrl_addr = QSGMII_7_CTRL0(group_num);
		ch3_ctrl_addr = QSGMII_8_CTRL0(group_num);
	} else {
		ch0_ctrl_addr = QSGMII_1_CTRL0(group_num);
		ch1_ctrl_addr = QSGMII_2_CTRL0(group_num);
		ch2_ctrl_addr = QSGMII_3_CTRL0(group_num);
		ch3_ctrl_addr = QSGMII_4_CTRL0(group_num);
	}

	val = priv->read(ch0_ctrl_addr);
	if (qsgmii_cfg->is_speed_forced == false)
		val &= ~(QSGMII_CTRL_FORCE_SPEED | QSGMII_CTRL_FORCE_LINK_UP);
	else {
		val |= QSGMII_CTRL_FORCE_SPEED | QSGMII_CTRL_FORCE_LINK_UP;
		if (qsgmii_cfg->forced_speed == SPEED_1000)
			val |= QSGMII_CTRL_FORCE_SPEED_1000 | QSGMII_CTRL_FORCE_DUPLEX_FULL;
		else if (qsgmii_cfg->forced_speed == SPEED_100)
			val |= QSGMII_CTRL_FORCE_SPEED_100;
		else if (qsgmii_cfg->forced_speed == SPEED_10)
			val |= QSGMII_CTRL_FORCE_SPEED_10;
		else
			return -EINVAL;

		if (qsgmii_cfg->forced_speed != SPEED_1000) {
			if (qsgmii_cfg->forced_duplex == DUPLEX_FULL)
				val |= QSGMII_CTRL_FORCE_DUPLEX_FULL;
			else
				val &= ~QSGMII_CTRL_FORCE_DUPLEX_FULL;
		}
	}

	/* Force the speed if necessary, set the SGMII mode, link up bit */
	val = val & (~(QSGMII_CTRL_QSGMII_MODE_MAC | QSGMII_CTRL_QSGMII_MODE_PHY));
	ch0_val = val | QSGMII_CTRL_QSGMII_MODE_MAC;

	priv->write(ch0_ctrl_addr, ch0_val);
	priv->write(ch1_ctrl_addr, val);
	priv->write(ch2_ctrl_addr, val);
	priv->write(ch3_ctrl_addr, val);

	/* Turn off auto-neg in port_status_cfg */
	val = priv->read(port_status_cfg(qsgmii_cfg->port_base));

	if (qsgmii_cfg->is_speed_forced == false) {
		/* Enable  Auto-negotiation */
		val |= PORT_STATUS_AUTONEG_EN;
	} else {
		/* Disable Auto-negotiation */
		val &= ~PORT_STATUS_AUTONEG_EN;
		val |= (PORT_STATUS_RXMAC_EN | PORT_STATUS_TXMAC_EN);

		/* Force the speed and duplex as configured */
		if (qsgmii_cfg->forced_speed == SPEED_1000)
			val |= PORT_STATUS_FORCE_SPEED_1000 | PORT_STATUS_FORCE_DUPLEX_FULL;
		else if (qsgmii_cfg->forced_speed == SPEED_100)
			val |= PORT_STATUS_FORCE_SPEED_100;
		else if (qsgmii_cfg->forced_speed == SPEED_10)
			val |= PORT_STATUS_FORCE_SPEED_10;
		else
			return -EINVAL;

		if (qsgmii_cfg->forced_speed != SPEED_1000) {
			if (qsgmii_cfg->forced_duplex == DUPLEX_FULL)
				val |= PORT_STATUS_FORCE_DUPLEX_FULL;
			else
				val &= ~PORT_STATUS_FORCE_DUPLEX_FULL;
		}
	}

	priv->write(port_status_cfg(qsgmii_cfg->port_base), val);
	priv->write(port_status_cfg(qsgmii_cfg->port_base + 1), val);
	priv->write(port_status_cfg(qsgmii_cfg->port_base + 2), val);
	priv->write(port_status_cfg(qsgmii_cfg->port_base + 3), val);

	/* Set QSGMII mode in GLOBAL_CTRL_1 */
	val = priv->read(GLOBAL_CTRL_1);
	val &= ~(GLOBAL_CTRL_1_MAC1_SGMII_EN << (qsgmii_cfg->port_base/4));
	priv->write(GLOBAL_CTRL_1, val);

	 /* Enable SD clock selects for the QSGMII port */
	sd_clk_sel_val = priv->read(SD_CLK_SEL);
	sd_clk_sel_val |= (SD_CLK_SEL_QSGMII_1_RX << (qsgmii_cfg->port_base/4));
	priv->write(SD_CLK_SEL, sd_clk_sel_val);

	return 0;
}

/* Configure a SGMII/SGMII+ interface */
static int qca_85xx_sw_init_sgmii_port (enum qca_85xx_sw_gmac_port port, struct qca_85xx_sw_sgmii_cfg *sgmii_cfg)
{
	uint32_t val, sgmii_ctrl0_reg, sgmii_ctrl0_val, sd_clk_sel_val;

	/* Set the port to either SGMII or SGMII+ mode in GLOBAL_CTRL_1 */
	val = priv->read(GLOBAL_CTRL_1);

	sd_clk_sel_val = priv->read(SD_CLK_SEL);
	switch (port) {
		case QCA_85XX_SW_PORT_26:
			sgmii_ctrl0_reg = SGMII_CTRL0_PORT26;
			sd_clk_sel_val |= SD_CLK_SEL_SGMII_PORT26_RX;
			if (sgmii_cfg->port_mode == QCA_85XX_SW_PORT_MODE_SGMII_PLUS) {
				val |= GLOBAL_CTRL_1_PORT_26_SGMII_PLUS_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_26_SGMII_EN);
			} else {
				val |= GLOBAL_CTRL_1_PORT_26_SGMII_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_26_SGMII_PLUS_EN);
			}
			break;
		case QCA_85XX_SW_PORT_27:
			sd_clk_sel_val |= SD_CLK_SEL_SGMII_PORT27_RX;
			sgmii_ctrl0_reg = SGMII_CTRL0_PORT27;
			if (sgmii_cfg->port_mode == QCA_85XX_SW_PORT_MODE_SGMII_PLUS) {
				val |= GLOBAL_CTRL_1_PORT_27_SGMII_PLUS_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_27_SGMII_EN);
			} else {
				val |= GLOBAL_CTRL_1_PORT_27_SGMII_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_27_SGMII_PLUS_EN);
			}
			break;
		case QCA_85XX_SW_PORT_28:
			sd_clk_sel_val |= SD_CLK_SEL_SGMII_PORT28_RX;
			sgmii_ctrl0_reg = SGMII_CTRL0_PORT28;
			if (sgmii_cfg->port_mode == QCA_85XX_SW_PORT_MODE_SGMII_PLUS) {
				val |= GLOBAL_CTRL_1_PORT_28_SGMII_PLUS_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_28_SGMII_EN);
			} else {
				val |= GLOBAL_CTRL_1_PORT_28_SGMII_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_28_SGMII_PLUS_EN);
			}
			break;
		case QCA_85XX_SW_PORT_29:
			sd_clk_sel_val |= SD_CLK_SEL_SGMII_PORT29_RX;
			sgmii_ctrl0_reg = SGMII_CTRL0_PORT29;
			if (sgmii_cfg->port_mode == QCA_85XX_SW_PORT_MODE_SGMII_PLUS) {
				val |= GLOBAL_CTRL_1_PORT_29_SGMII_PLUS_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_29_SGMII_EN);
			} else {
				val |= GLOBAL_CTRL_1_PORT_29_SGMII_EN;
				val &= ~(GLOBAL_CTRL_1_PORT_29_SGMII_PLUS_EN);
			}
			break;
		default:
			return -EINVAL;
	}

	priv->write(GLOBAL_CTRL_1, val);

	/* Enable SGMII Rx clock select */
	priv->write(SD_CLK_SEL, sd_clk_sel_val);

	/* Set the MAC-mode settings in SGMII_CTRL0 register */
	sgmii_ctrl0_val = priv->read(sgmii_ctrl0_reg);

	/* Place the SGMII mode into MAC mode for SGMII ports and 1000Base-X for SGMII+ */
	if (sgmii_cfg->port_mode != QCA_85XX_SW_PORT_MODE_SGMII_PLUS)
		sgmii_ctrl0_val |= SGMII_CTRL0_SGMII_MODE_MAC;
	else
		sgmii_ctrl0_val &= ~(SGMII_CTRL0_SGMII_MODE_PHY | SGMII_CTRL0_SGMII_MODE_MAC);

	val = priv->read(port_status_cfg(port));

	/* Settings to force speed if configured */
	if (sgmii_cfg->is_speed_forced == false)
		val |= PORT_STATUS_AUTONEG_EN;
	else {
		/* Disable Auto-negotiation */
		val &= ~(PORT_STATUS_AUTONEG_EN | PORT_STATUS_AUTO_FLOW_CTRL_EN
				| PORT_STATUS_TX_HALF_FLOW_EN | PORT_STATUS_RX_FLOW_EN
				| PORT_STATUS_TX_FLOW_EN);
		val |= (PORT_STATUS_RXMAC_EN | PORT_STATUS_TXMAC_EN);

		if (sgmii_cfg->port_mode != QCA_85XX_SW_PORT_MODE_SGMII_PLUS)
			sgmii_ctrl0_val |= SGMII_CTRL0_FORCE_MODE_EN;

		/* Force the speed and duplex as configured */
		switch (sgmii_cfg->forced_speed) {
			case SPEED_1000:
			case SPEED_2500:
				val |= PORT_STATUS_FORCE_SPEED_1000 | PORT_STATUS_FORCE_DUPLEX_FULL;
				sgmii_ctrl0_val |= SGMII_CTRL0_FORCE_SPEED_1000 | SGMII_CTRL0_FORCE_DUPLEX_FULL;
				break;
			case SPEED_100:
				val |= PORT_STATUS_FORCE_SPEED_100;
				sgmii_ctrl0_val |= SGMII_CTRL0_FORCE_SPEED_100;
				break;
			case SPEED_10:
				val |= PORT_STATUS_FORCE_SPEED_10;
				sgmii_ctrl0_val |= SGMII_CTRL0_FORCE_SPEED_10;
				break;
			default:
				return -EINVAL;
		}

		if ((sgmii_cfg->forced_speed != SPEED_1000) && (sgmii_cfg->forced_speed != SPEED_2500)) {
			if (sgmii_cfg->forced_duplex == DUPLEX_FULL)
				val |= PORT_STATUS_FORCE_DUPLEX_FULL;
			else {
				val &= ~PORT_STATUS_FORCE_DUPLEX_FULL;
				sgmii_ctrl0_val &= ~SGMII_CTRL0_FORCE_DUPLEX_FULL;
			}
		}
	}

	priv->write(port_status_cfg(port), val);
	priv->write(sgmii_ctrl0_reg, sgmii_ctrl0_val);

	/* Serdes control settings */
	if (sgmii_cfg->port_mode == QCA_85XX_SW_PORT_MODE_SGMII_PLUS) {
		priv->write(XAUI_SGMII_SERDES13_CTRL0, 0x01e2023e);
		priv->write(XAUI_SGMII_SERDES13_CTRL1, 0x4c93ff0);
	}

	return 0;
}

/* Initialize the 8511 chipset */
static int qca8511_hw_init(struct qca_85xx_sw_platform_data *pdata)
{
	if (pdata->qsgmii_cfg.port_mode == QCA_85XX_SW_PORT_MODE_QSGMII)
		if (qca_85xx_sw_init_qsgmii_port(&pdata->qsgmii_cfg) != 0)
			return -1;

	if (pdata->port_24_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if (qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_24, &pdata->port_24_cfg) != 0)
			return -1;

	if (pdata->port_25_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if (qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_25, &pdata->port_25_cfg) != 0)
			return -1;

	if (pdata->port_26_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if (qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_26, &pdata->port_26_cfg) != 0)
			return -1;

	if (pdata->port_27_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if (qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_27, &pdata->port_27_cfg) != 0)
			return -1;

	if (pdata->port_28_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if (qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_28, &pdata->port_28_cfg) != 0)
			return -1;

	if (pdata->port_29_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if (qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_29, &pdata->port_29_cfg) != 0)
			return -1;

	return 0;
}

struct qca_85xx_sw_chip qca8511_chip = {
	.hw_init = qca8511_hw_init,
	.max_qsgmii_if = 2,
	.max_sgmii_plus_if = 4,
	.max_ge_if = 8,
};

/* Switch register read routine */
static uint32_t qca_85xx_sw_mii_read(uint32_t reg_addr)
{
	struct mii_bus *bus = priv->mdio_bus;
	uint32_t reg_word_addr;
	uint32_t phy_addr, tmp_val, reg_val;
	uint16_t phy_val;
	uint8_t phy_reg;

	/* Change reg_addr to 32-bit aligned */
	reg_word_addr = (reg_addr & 0xfffffffc);

	/* Configure register high address */
	phy_addr = 0x18 | (reg_word_addr >> 29);
	phy_reg = (reg_word_addr & 0x1f000000) >> 24;

	/* Bit 23-8 of reg address */
	phy_val = (uint16_t) ((reg_word_addr >> 8) & 0xffff);
	mdiobus_write(bus, phy_addr, phy_reg, phy_val);

	/*
	 * For some registers such as MIBs, since it is read/clear,
	 * we should read the lower 16-bit register then the higher one
	 */

	/*
	 * Read register in lower address
	 * Bit 7-5 of reg address
	 */
	phy_addr = 0x10 | ((reg_word_addr >> 5) & 0x7);

	/*  Bit 4-0 of reg address */
	phy_reg = (uint8_t) (reg_word_addr & 0x1f);
	reg_val = (uint32_t) mdiobus_read(bus, phy_addr, phy_reg);

	/*
	 * Read register in higher address
	 * Bit 7-5 of reg address
	 */

	phy_addr = 0x10 | ((reg_word_addr >> 5) & 0x7);

	/* Bit4-0 of reg address */
	phy_reg = (uint8_t) ((reg_word_addr & 0x1f) | 0x2);
	tmp_val = (uint32_t) mdiobus_read(bus, phy_addr, phy_reg);
	reg_val |= (tmp_val << 16);

	return reg_val;
}

/* Switch register write routine */
static void qca_85xx_sw_mii_write(uint32_t reg_addr, uint32_t reg_val)
{
	struct mii_bus *bus = priv->mdio_bus;
	uint32_t reg_word_addr;
	uint32_t phy_addr;
	uint16_t phy_val;
	uint8_t phy_reg;

	/*
	 * Change reg_addr to 16-bit word address,
	 * 32-bit aligned
	 */
	reg_word_addr = (reg_addr & 0xfffffffc);

	/* configure register high address */
	phy_addr = 0x18 | (reg_word_addr >> 29);
	phy_reg = (reg_word_addr & 0x1f000000) >> 24;

	/* Bit23-8 of reg address */
	phy_val = (uint16_t) ((reg_word_addr >> 8) & 0xffff);
	mdiobus_write(bus, phy_addr, phy_reg, phy_val);

	/*
	 * For some registers such as ARL and VLAN, since they include BUSY bit
	 * in lower address, we should write the higher 16-bit register then the
	 * lower one
	 */

	/*
	 * write register in higher address
	 *  bit7-5 of reg address
	 */
	phy_addr = 0x10 | ((reg_word_addr >> 5) & 0x7);

	/* bit4-0 of reg address */
	phy_reg = (uint8_t) (reg_word_addr & 0x1f);

	/* lowest 16bit data */
	phy_val = (uint16_t) (reg_val & 0xffff);
	mdiobus_write(bus, phy_addr, phy_reg, phy_val);

	/*
	 * write register in lower address
	 * bit7-5 of reg address
	 */
	phy_addr = 0x10 | ((reg_word_addr >> 5) & 0x7);

	/* Bit4-0 of reg address */
	phy_reg = (uint8_t) ((reg_word_addr & 0x1f) | 0x2);

	/* Highest 16bit data */
	phy_val = (uint16_t) ((reg_val >> 16) & 0xffff);
	mdiobus_write(bus, phy_addr, phy_reg, phy_val);
}

/* Check for a valid range of switch register */
static bool qca_85xx_sw_check_valid_reg(uint32_t reg_addr)
{
	return true;
}

/* read from debug-fs file */
static ssize_t qca_85xx_sw_read_reg_get(struct file *fp, char __user *ubuf,
				   size_t sz, loff_t *ppos)
{
	char lbuf[40];
	int bytes_read;

	if (!priv)
		return -EFAULT;

	snprintf(lbuf, sizeof(lbuf), "0x%x: 0x%x\n", priv->reg_addr, priv->reg_val);

	bytes_read = simple_read_from_buffer(ubuf, sz, ppos, lbuf, strlen(lbuf));

	return bytes_read;
}

/* Write into the file and read back the switch register */
static ssize_t qca_85xx_sw_read_reg_set(struct file *fp, const char __user *ubuf,
				   size_t sz, loff_t *ppos)
{
	char lbuf[32];
	size_t lbuf_size;
	char *curr_ptr = lbuf;
	unsigned long reg_addr = 0;
	uint32_t reg_val = 0;
	bool is_reabable = false;

	if (!priv)
		return -EFAULT;

	lbuf_size = min(sz, (sizeof(lbuf) - 1));

	if (copy_from_user(lbuf, ubuf, lbuf_size)) {
		pr_debug("Failed in copy_from_user\n");
		return -EFAULT;
	}

	lbuf[lbuf_size] = 0;

	while ((*curr_ptr == ' ') && (curr_ptr < &lbuf[lbuf_size]))
		curr_ptr++;

	if (curr_ptr == &lbuf[lbuf_size])
		return -EINVAL;

	if (strict_strtoul(curr_ptr, 16, &reg_addr)) {
		pr_debug("Invalid register address input\n");
		return -EINVAL;
	}

	/* Read into PHY reg and store value in previous reg read */
	is_reabable = qca_85xx_sw_check_valid_reg(reg_addr);
	if (is_reabable) {
		priv->reg_addr = reg_addr;
		reg_val = qca_85xx_sw_mii_read(reg_addr);
		priv->reg_val = reg_val;
		return lbuf_size;
	}

	pr_debug("Non-readable register address input\n");
	return -EINVAL;
}

/* Debug-fs function to write a switch register */
static ssize_t qca_85xx_sw_write_reg_set(struct file *fp, const char __user *ubuf,
				    size_t sz, loff_t *ppos)
{
	char lbuf[32];
	size_t lbuf_size;
	char *curr_ptr = lbuf;
	unsigned long reg_addr = 0;
	unsigned long reg_val = 0;
	bool is_writeable = false;

	if (!priv)
		return -EFAULT;

	lbuf_size = min(sz, (sizeof(lbuf) - 1));

	if (copy_from_user(lbuf, ubuf, lbuf_size)) {
		pr_debug("Failed in copy_from_user\n");
		return -EFAULT;
	}

	lbuf[lbuf_size] = 0;

	while ((*curr_ptr == ' ') && (curr_ptr < &lbuf[lbuf_size]))
		curr_ptr++;

	if (curr_ptr == &lbuf[lbuf_size])
		return -EINVAL;

	reg_addr = simple_strtoul(curr_ptr, &curr_ptr, 16);

	while ((*curr_ptr == ' ') && (curr_ptr < &lbuf[lbuf_size]))
		curr_ptr++;

	if (curr_ptr == &lbuf[lbuf_size])
		return -EINVAL;

	if (strict_strtoul(curr_ptr, 16, &reg_val)) {
		pr_debug("Invalid register address input\n");
		return -EINVAL;
	}

	/* Check for a valid range of register before writing */
	is_writeable = qca_85xx_sw_check_valid_reg(reg_addr);
	if (is_writeable) {
		qca_85xx_sw_mii_write(reg_addr, reg_val);
		return lbuf_size;
	}

	pr_debug("Non-writable register address input\n");
	return -EINVAL;
}

static const struct file_operations qca_85xx_sw_read_reg_ops = { \
	.open = simple_open, \
	.read = qca_85xx_sw_read_reg_get, \
	.write = qca_85xx_sw_read_reg_set, \
	.llseek = no_llseek, \
};

static const struct file_operations qca_85xx_sw_write_reg_ops = { \
	.open = simple_open, \
	.write = qca_85xx_sw_write_reg_set, \
	.llseek = no_llseek, \
};

/* Create debug-fs qca_85xx_sw dir and files */
static int qca_85xx_sw_init_debugfs_entries(void)
{
	priv->top_dentry = debugfs_create_dir("qca-85xx-sw", NULL);
	if (priv->top_dentry == NULL) {
		pr_debug("Failed to create qca-85xx-sw directory in debugfs\n");
		return -1;
	}

	priv->write_dentry = debugfs_create_file("write-reg", 0400,
						priv->top_dentry,
						priv, &qca_85xx_sw_write_reg_ops);

	if (unlikely(priv->write_dentry == NULL)) {
		pr_debug("Failed to create qca-85xx-sw/write-reg file in debugfs\n");
		debugfs_remove_recursive(priv->top_dentry);
		return -1;
	}

	priv->read_dentry = debugfs_create_file("read-reg", 0400,
						priv->top_dentry,
						priv, &qca_85xx_sw_read_reg_ops);

	if (unlikely(priv->read_dentry == NULL)) {
		pr_debug("Failed to create qca-85xx-sw/read-reg file in debugfs\n");
		debugfs_remove_recursive(priv->top_dentry);
		return -1;
	}

	priv->reg_val = 0;
	priv->reg_addr = 0;

	return 0;
}

static int __devinit qca_85xx_sw_probe(struct platform_device *pdev)
{
	struct device *miidev;
	uint8_t busid[MII_BUS_ID_SIZE];
	struct mii_bus *mdio_bus;
	struct qca_85xx_sw_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	if (!pdata)
		return -ENODEV;

	priv = vzalloc(sizeof(struct qca_85xx_sw_priv));
	if (priv == NULL) {
		pr_debug("Could not allocate private memory for 85xx driver\n");
		return -1;
	}

	if (pdata->chip_id == QCA_85XX_SW_ID_QCA8511) {
		pr_debug("Initializing 8511 switch chipset\n");
		priv->chip = &qca8511_chip;
		priv->read = qca_85xx_sw_mii_read;
		priv->write = qca_85xx_sw_mii_write;
	} else {
		pr_debug("Unsupported 85xx switch chipset\n");
		vfree(priv);
		return -1;
	}

	/* Find the MDIO bus to use */
	snprintf(busid, MII_BUS_ID_SIZE, "%s.%d", pdata->mdio_bus_name, pdata->mdio_bus_id);

	miidev = bus_find_device_by_name(&platform_bus_type, NULL, busid);
	if (!miidev) {
		pr_debug("mdio bus '%s':: cannot find bus device\n", busid);
		vfree(priv);
		return -1;
	}

	mdio_bus = dev_get_drvdata(miidev);
	if (!mdio_bus) {
		pr_debug("mdio bus '%s':: cannot find mdio bus pointer from device\n", busid);
		vfree(priv);
		return -1;
	}

	priv->mdio_bus = mdio_bus;

	/* Find the ethernet device we are bound to */
	priv->eth_dev = dev_get_by_name(&init_net, pdata->eth_dev_name);
	if (!priv->eth_dev) {
		pr_debug("Could not find ethernet device from device name %s\n", pdata->eth_dev_name);
		vfree(priv);
		return -1;
	}

	/* Initialize the debug-fs entries */
	ret = qca_85xx_sw_init_debugfs_entries();
	if (ret < 0) {
		pr_debug("Could not initialize debugfs entries\n");
		dev_put(priv->eth_dev);
		vfree(priv);
		return ret;
	}

	if (priv->chip->hw_init(pdata) != 0) {
		pr_debug("Error initializing 85xx chipset\n");
		if (likely(priv->top_dentry != NULL))
			debugfs_remove_recursive(priv->top_dentry);
		dev_put(priv->eth_dev);
		vfree(priv);
		return ret;
	}

	pr_debug("85xx switch initialization complete\n");

	return 0;
}

static int __devexit qca_85xx_sw_remove(struct platform_device *pdev)
{
	/* Remove debugfs tree */
	if (likely(priv->top_dentry != NULL))
		debugfs_remove_recursive(priv->top_dentry);

	dev_put(priv->eth_dev);

	vfree(priv);

	return 0;
}

static struct platform_driver qca_85xx_sw_driver = {
	.driver = {
		.name = "qca_85xx_sw",
		.owner = THIS_MODULE,
	},
	.probe = qca_85xx_sw_probe,
	.remove = __devexit_p(qca_85xx_sw_remove),
};

int __init qca_85xx_sw_init(void)
{
	platform_driver_register(&qca_85xx_sw_driver);

	return 0;
}

void __exit qca_85xx_sw_exit(void)
{
	platform_driver_unregister(&qca_85xx_sw_driver);
}

module_init(qca_85xx_sw_init);
module_exit(qca_85xx_sw_exit);
MODULE_LICENSE("GPL");
