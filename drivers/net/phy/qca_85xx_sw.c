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
#include "qca_85xx_sw_regdef.h"

 /* 85xx chipset specific data */
struct qca_85xx_sw_chip {
	int (*hw_init)(struct qca_85xx_sw_platform_data *pdata);
	const struct qca_85xx_sw_mib_desc *mib_decs;
	uint32_t num_mibs;
	uint32_t max_qsgmii_if;
	uint32_t max_sgmii_plus_if;
	uint32_t max_ge_if;
};

/* 85xx private data */
struct qca_85xx_sw_priv {
	uint32_t (*read)(uint32_t reg_addr);
	void (*write)(uint32_t reg_addr, uint32_t val);
	struct mii_bus *mdio_bus;
	struct net_device *eth_dev;
	const struct qca_85xx_sw_chip *chip;

	uint32_t reg_val;		/* Hold the result of the previous
					   debug-fs based register read op */

	uint32_t reg_addr;		/* Hold the register address for the
					   previous debug-fs based register
					   read op */
	uint32_t sgmii_plus_link_speed;	/* SGMII+ if link speed */
	uint32_t sgmii_plus_duplex;	/* SFMII+ if link duplex */
	uint32_t sgmii_plus_port_num;	/* SGMII+ if port number */
	struct dentry *top_dentry;	/* Top dentry */
	struct dentry *write_dentry;	/* write-reg file dentry */
	struct dentry *read_dentry;	/* read-reg file dentry */
};

struct qca_85xx_sw_priv *priv;

/*
 * Identify the new link speed and duplex setting for the SGMII+ if link, and
 * and force the SGMII+ speed/duplex accordingly
 */
static void qca_85xx_sw_sgmii_plus_if_change_speed(struct net_device *dev)
{
	uint32_t val, sgmii_ctrl0_val;
	int curr_speed = dev->phydev->speed;
	int curr_duplex = dev->phydev->duplex;

	if (curr_speed == priv->sgmii_plus_link_speed &&
	    curr_duplex == priv->sgmii_plus_duplex)
		return;

	switch (curr_speed) {
	case SPEED_2500:
		/* Set port 27 into SGMII+ Mode(2.5 Gig speed) */
		val = priv->read(GLOBAL_CTRL_1);
		val |= GLOBAL_CTRL_1_PORT_27_SGMII_PLUS_EN;
		val &= ~(GLOBAL_CTRL_1_PORT_27_SGMII_EN);
		priv->write(GLOBAL_CTRL_1, val);
		break;
	case SPEED_1000:
	case SPEED_100:
		if (priv->sgmii_plus_link_speed != SPEED_1000 &&
			priv->sgmii_plus_link_speed != SPEED_100) {
			/* Set port 27 into SGMII Mode(100/1000Mbps speed) */
			val = priv->read(GLOBAL_CTRL_1);
			val |= GLOBAL_CTRL_1_PORT_27_SGMII_EN;
			val &= ~(GLOBAL_CTRL_1_PORT_27_SGMII_PLUS_EN);
			priv->write(GLOBAL_CTRL_1, val);
		}
		break;
	default:
		priv->sgmii_plus_link_speed = SPEED_UNKNOWN;
		return;
	}

	sgmii_ctrl0_val = priv->read(SGMII_CTRL0_PORT27);
	val = priv->read(port_status_cfg(priv->sgmii_plus_port_num));

	/* Clear the previous link speed and duplex setting */
	if (priv->sgmii_plus_link_speed == SPEED_100 ||
	    priv->sgmii_plus_link_speed == SPEED_1000 ||
	    priv->sgmii_plus_link_speed == SPEED_UNKNOWN) {
		sgmii_ctrl0_val &= ~(SGMII_CTRL0_SGMII_MODE_MAC |
				     SGMII_CTRL0_FORCE_SPEED_100 |
				     SGMII_CTRL0_FORCE_SPEED_1000 |
				     SGMII_CTRL0_FORCE_DUPLEX_FULL);

		val &= ~(PORT_STATUS_FORCE_SPEED_100 |
			 PORT_STATUS_FORCE_SPEED_1000 |
			 PORT_STATUS_FORCE_DUPLEX_FULL);

	}

	if(curr_speed == SPEED_1000 || curr_speed == SPEED_2500) {
		/* Force the 1000Mbps link speed and duplex setting */
		val |= PORT_STATUS_FORCE_SPEED_1000 |
		       PORT_STATUS_FORCE_DUPLEX_FULL;
		sgmii_ctrl0_val |= SGMII_CTRL0_FORCE_SPEED_1000 |
				   SGMII_CTRL0_FORCE_DUPLEX_FULL;
	} else {
		/* Force the 100Mbps link speed and duplex setting */
		sgmii_ctrl0_val |= SGMII_CTRL0_SGMII_MODE_MAC |
				   SGMII_CTRL0_FORCE_SPEED_100;

		val |=  PORT_STATUS_FORCE_SPEED_100;

		if (curr_duplex) {
			/* Set 100Mbps Full Duplex */
			sgmii_ctrl0_val |= SGMII_CTRL0_FORCE_DUPLEX_FULL;
			val |=  PORT_STATUS_FORCE_DUPLEX_FULL;
		}
		else {
			/* Set 100Mbps Half Duplex */
			sgmii_ctrl0_val &= ~SGMII_CTRL0_FORCE_DUPLEX_FULL;
			val &= ~PORT_STATUS_FORCE_DUPLEX_FULL;
		}
	}
	priv->write(port_status_cfg(priv->sgmii_plus_port_num), val);
	priv->write(SGMII_CTRL0_PORT27, sgmii_ctrl0_val);
	priv->sgmii_plus_link_speed = curr_speed;
	priv->sgmii_plus_duplex = curr_duplex;
}

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
	val = priv->read(port_status_cfg(qsgmii_cfg->port_base + 1));

	if (qsgmii_cfg->is_speed_forced == false) {
		/* Enable  Auto-negotiation */
		val |= PORT_STATUS_AUTONEG_EN;
	} else {
		/* Disable Auto-negotiation */
		val &= ~(PORT_STATUS_AUTONEG_EN | PORT_STATUS_AUTO_FLOW_CTRL_EN
				| PORT_STATUS_TX_HALF_FLOW_EN | PORT_STATUS_RX_FLOW_EN
				| PORT_STATUS_TX_FLOW_EN);
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

	priv->write(port_status_cfg(qsgmii_cfg->port_base + 1), val);
	priv->write(port_status_cfg(qsgmii_cfg->port_base + 2), val);
	priv->write(port_status_cfg(qsgmii_cfg->port_base + 3), val);
	priv->write(port_status_cfg(qsgmii_cfg->port_base + 4), val);

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
		priv->sgmii_plus_port_num = port;
	}

	return 0;
}

/* Configure a trunk (Link-Aggregated) interface */
static int qca_85xx_sw_trunk_enable (struct qca_85xx_sw_trunk_cfg *trunk_cfg)
{
	uint32_t val = 0, i = 0, shift_val = 0;
	uint32_t trunk_table_entry_no = 0;
	uint32_t trunk_table_entry_addr, table_entry_val = 0;
	uint32_t port_bit_map = 0;
	uint32_t next_port_num = 0;

	if (trunk_cfg->trunk_id > (QCA_85XX_MAX_TRUNKS - 1))
		return -EINVAL;

	/* Enable trunk failover */
	val = priv->read(L2_MISC_CTRL);
	priv->write(L2_MISC_CTRL, val | FAILOVER_EN);

	/* Set the trunk hash key selection method */
	priv->write(TRUNK_HASH_KEY_SEL, trunk_cfg->trunk_hash_policy);

	/* Enable the trunk slave ports */
	for (i = QCA_85XX_SW_PORT_1; i <= QCA_85XX_SW_PORT_29; i++) {

		if ((i > priv->chip->max_ge_if) && (i < QCA_85XX_SW_PORT_26))
			continue;

		if (trunk_cfg->trunk_ports_bit_map & (1 << i)) {
			priv->write(port_trunk_cfg(i), (PORT_TRUNK_EN | trunk_cfg->trunk_id));
		}
	}

	/* Set the trunk member ports in the TRUNK_GROUP_MEM table */
	port_bit_map = trunk_cfg->trunk_ports_bit_map;

	/*
	 * The TRUNK_GROUP_MEM table has 8 member slots all of which need to be
	 * filled. So, cycle through the trunk port members list until we have
	 * filled all the 8 port entries of the table entry.
	 */
	while (trunk_table_entry_no < QCA_85XX_MAX_TRUNKS) {
		/* Get the next port number */
		next_port_num = ffs(port_bit_map) - 1;
		port_bit_map &= ~(1 << next_port_num);
		shift_val = (trunk_table_entry_no * TRUNK_MEM_PORT_ID_SHIFT_LEN) % 32;

		/* Configure the table entries */
		switch(trunk_table_entry_no) {
		case 0:
		case 1:
		case 2:
			trunk_table_entry_addr = trunk_group_mem_0(trunk_cfg->trunk_id);
			table_entry_val = priv->read(trunk_table_entry_addr);
			priv->write(trunk_table_entry_addr, table_entry_val | (next_port_num << shift_val));
			break;
		case 3:
		case 4:
			trunk_table_entry_addr = trunk_group_mem_1(trunk_cfg->trunk_id);
			table_entry_val = priv->read(trunk_table_entry_addr);
			priv->write(trunk_table_entry_addr, table_entry_val | (next_port_num << shift_val));
			break;
		case 5:
			trunk_table_entry_addr = trunk_group_mem_1(trunk_cfg->trunk_id);
			table_entry_val = priv->read(trunk_table_entry_addr);
			priv->write(trunk_table_entry_addr, table_entry_val | ((next_port_num & 0xF) << shift_val));

			if (next_port_num > 0xF) {
				trunk_table_entry_addr = trunk_group_mem_2(trunk_cfg->trunk_id);
				table_entry_val = priv->read(trunk_table_entry_addr);
				priv->write(trunk_table_entry_addr, table_entry_val | ((next_port_num & 0x30) >> 4));
			}

			break;
		case 6:
		case 7:
			trunk_table_entry_addr = trunk_group_mem_2(trunk_cfg->trunk_id);
			table_entry_val = priv->read(trunk_table_entry_addr);
			priv->write(trunk_table_entry_addr, table_entry_val | (next_port_num << shift_val));
			break;
		}

		trunk_table_entry_no++;

		if (port_bit_map == 0)
			port_bit_map = trunk_cfg->trunk_ports_bit_map;
	}

	/* Enable the trunk slave ports bit map */
	priv->write(trunk_local_port_mem(trunk_cfg->trunk_id), trunk_cfg->trunk_ports_bit_map);

	return 0;
}

/* Enable VLAN filters and STP forwarding */
void qca_85xx_sw_enable_port_defaults(void)
{
	uint32_t stp_grp_reg_val_0 = 0, stp_grp_reg_val_1 = 0;
	uint32_t port_vlan_fltr = 0;
	int i;

	/* Enable STP forwarding, and enable VLAN filters */
	for (i = QCA_85XX_SW_PORT_1; i <= QCA_85XX_SW_PORT_29; i++) {
		if (i < 16)
			stp_grp_reg_val_0 |= (PORT_FORWARDING << (i*2));
		else
			stp_grp_reg_val_1 |= (PORT_FORWARDING << ((i-16) * 2));

		port_vlan_fltr &=  ~(1 << i);
	}

	priv->write(spanning_tree_group_reg_0(0), stp_grp_reg_val_0);
	priv->write(spanning_tree_group_reg_1(0), stp_grp_reg_val_1);
	priv->write(PORT_VLAN_FLTR, port_vlan_fltr);
}

/* Configure a Port VLAN group */
int qca_85xx_port_vlan_group_enable(uint16_t vlan_id, uint32_t port_bit_map)
{
	int next_port_num = 0, pbm;
	uint16_t in_vlan_entry_num;
	uint32_t in_vlan_w0_addr, in_vlan_w1_addr, in_vlan_w2_addr;
	uint32_t in_vlan_w0_val = 0, in_vlan_w1_val = 0, in_vlan_w2_val = 0;
	uint32_t vlan_op_val = 0, vlan_op_data_0_val = 0, vlan_op_data_1_val = 0;
	uint32_t port_eg_vlan_ctrl_addr, port_eg_vlan_ctrl_val = 0;
	uint32_t eg_vlan_tag_addr, eg_vlan_tag_val = 0;

	if (vlan_id > QCA_85XX_VLAN_ID_MAX)
		return -EINVAL;

	/* Program IN_VLAN_TRANSLATION table */
	in_vlan_entry_num = IN_VLAN_ENTRY_LOWEST_PRIO;
	pbm = port_bit_map;

	/*
	 * Cycle through the port members list and update the IN_VLAN_TRANSLATION
	 * table entry for each port
	 */
	while (pbm) {
		next_port_num = ffs(pbm) - 1;
		pbm &= ~(1 << next_port_num);

		in_vlan_w0_addr = in_vlan_table_word_0(next_port_num, in_vlan_entry_num);
		in_vlan_w1_addr = in_vlan_table_word_1(next_port_num, in_vlan_entry_num);
		in_vlan_w2_addr = in_vlan_table_word_2(next_port_num, in_vlan_entry_num);

		/* Write table entry word 0 */
		in_vlan_w0_val |= (CVID_SEARCH_KEY_VALUE_UNTAGGED << CKEY_VID_SHIFT_LEN);
		priv->write(in_vlan_w0_addr, in_vlan_w0_val);

		/* Write table entry word 1 */
		in_vlan_w1_val |= (vlan_id << XLT_CVID_SHIFT_LEN);
		in_vlan_w1_val |= (XLT_CVID_EN | CKEY_VID_INCL);
		priv->write(in_vlan_w1_addr, in_vlan_w1_val);

		/* Write table entry word 2 */
		in_vlan_w2_val |= (IN_VLAN_TABLE_ENTRY_VALID | SA_LRN_EN | (CVLAN_ASSIGN_ALL_FRAMES << CKEY_FMT_SHIFT_LEN));
		priv->write(in_vlan_w2_addr, in_vlan_w2_val);
	}

	/* Program VLAN table entry */
	vlan_op_data_0_val = port_bit_map;
	priv->write(VLAN_OP_DATA_0, vlan_op_data_0_val);

	vlan_op_data_1_val |= (LRN_EN | VLAN_TABLE_ENTRY_VALID);
	priv->write(VLAN_OP_DATA_1, vlan_op_data_1_val);

	vlan_op_val |= (vlan_id | (VLAN_OP_TYPE_ADD_ONE << VLAN_OP_SHIFT_VAL));
	priv->write(VLAN_OPERATION, vlan_op_val);
	vlan_op_val |= VLAN_OP_START;
	priv->write(VLAN_OPERATION, vlan_op_val);

	usleep_range(1000, 10000);

	if (priv->read(VLAN_OPERATION) & VLAN_BUSY) {
		printk(KERN_ERR "qca_85xx_sw: VLAN entry add operation timed out\n");
		return -ETIMEDOUT;
	}

	/* Program Egress VLAN table */
	eg_vlan_tag_addr = eg_vlan_tag(vlan_id);
	eg_vlan_tag_val |= EG_VLAN_TAG_TABLE_ENTRY_VALID;
	priv->write(eg_vlan_tag_addr, eg_vlan_tag_val);

	port_eg_vlan_ctrl_val |= (PORT_TAG_CONTROL_VID_LOOKUP_VID
				  | PORT_EG_VLAN_CTRL_RESERVED
				  | (PORT_EG_VLAN_TYPE_MODE_UNTAGGED
					  << PORT_EG_VLAN_TYPE_MODE_SHIFT_LEN));
	pbm = port_bit_map;
	while (pbm) {
		next_port_num = ffs(pbm) - 1;
		pbm &= ~(1 << next_port_num);
		port_eg_vlan_ctrl_addr = port_eg_vlan_ctrl(next_port_num);
		priv->write(port_eg_vlan_ctrl_addr, port_eg_vlan_ctrl_val);
	}

	return 0;
}

/* Initialize the 8511 chipset */
static int qca8511_hw_init(struct qca_85xx_sw_platform_data *pdata)
{
	uint32_t val = 0;
	int ret = 0;

	/* Issue software reset and restore registers to default values */
	val = priv->read(GLOBAL_CTRL_0);
	val |= (GLOBAL_CTRL_0_REG_RST_EN | GLOBAL_CTRL_0_SOFT_RST);
	priv->write(GLOBAL_CTRL_0, val);

	usleep_range(1000, 10000);

	/* Is the reset complete? */
	if (priv->read(GLOBAL_CTRL_0) & GLOBAL_CTRL_0_SOFT_RST) {
		printk(KERN_ERR "qca_85xx_sw: software reset timed out\n");
		return -ETIMEDOUT;
	}

	/* Configuring QSGMII interface */
	if (pdata->qsgmii_cfg.port_mode == QCA_85XX_SW_PORT_MODE_QSGMII) {
		if ((ret = qca_85xx_sw_init_qsgmii_port(&pdata->qsgmii_cfg)) != 0)
			return ret;
	}

	/* Configure SGMII/SGMII+ interfaces */
	if (pdata->port_24_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if ((ret = qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_24, &pdata->port_24_cfg)) != 0)
			return ret;

	if (pdata->port_25_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if ((ret = qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_25, &pdata->port_25_cfg)) != 0)
			return ret;

	if (pdata->port_26_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if ((ret = qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_26, &pdata->port_26_cfg)) != 0)
			return ret;

	if (pdata->port_27_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if ((ret = qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_27, &pdata->port_27_cfg)) != 0)
			return ret;

	if (pdata->port_28_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if ((ret = qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_28, &pdata->port_28_cfg)) != 0)
			return ret;

	if (pdata->port_29_cfg.port_mode != QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED)
		if ((ret = qca_85xx_sw_init_sgmii_port(QCA_85XX_SW_PORT_29, &pdata->port_29_cfg)) != 0)
			return ret;

	/* Configure trunk (Link aggregated) interfaces */
	if (pdata->trunk_cfg.is_trunk_enabled == true)
		if ((ret = qca_85xx_sw_trunk_enable(&pdata->trunk_cfg)) != 0)
			return ret;

	/* Configure port VLANs */
	qca_85xx_sw_enable_port_defaults();

	/*
	 * TODO: Enable swconfig support to configure port VLANs,
	 * to dynamically enable the following port grouping
	 */

	/* Enable port-vlan group 1 for ports 1, 26 */
	if ((ret = qca_85xx_port_vlan_group_enable(1, 0x04000002)) != 0)
		return ret;

	/* Enable port-vlan group 2 for ports 2, 3, 4, 27 */
	if ((ret = qca_85xx_port_vlan_group_enable(2, 0x0800001C)) != 0)
		return ret;

	printk(KERN_INFO "qca_85xx_sw: 8511 switch initialized\n");

	return ret;
}

struct qca_85xx_sw_chip qca8511_chip = {
	.hw_init = qca8511_hw_init,
	.max_qsgmii_if = 2,
	.max_sgmii_plus_if = 4,
	.max_ge_if = 8,
};

/* Netdev notifier callback to inform the change of state of a netdevice */
static int qca_85xx_sw_netdev_notifier_callback(struct notifier_block *this,
					     unsigned long event, void *ptr)
{
	struct net_device *dev __attribute__ ((unused)) = (struct net_device *)ptr;

	if (dev != priv->eth_dev)
		return NOTIFY_DONE;

	if (!netif_carrier_ok(dev))
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UP:
	case NETDEV_CHANGE:
		qca_85xx_sw_sgmii_plus_if_change_speed(dev);
		break;
	case NETDEV_DOWN:
	default:
		break;
	}

	return NOTIFY_DONE;
}

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

/* Init Netdev notifier event callback to notify any change in netdevice
 * attached with an external PHY */
static struct notifier_block qca_85xx_netdev_notifier __read_mostly = {
	.notifier_call		= qca_85xx_sw_netdev_notifier_callback,
};

/* Create debug-fs qca_85xx_sw dir and files */
static int qca_85xx_sw_init_debugfs_entries(void)
{
	priv->top_dentry = debugfs_create_dir("qca-85xx-sw", NULL);
	if (priv->top_dentry == NULL) {
		printk(KERN_ERR "qca_85xx_sw: Failed to create qca-85xx-sw directory in debugfs\n");
		return -1;
	}

	priv->write_dentry = debugfs_create_file("write-reg", 0400,
						priv->top_dentry,
						priv, &qca_85xx_sw_write_reg_ops);

	if (unlikely(priv->write_dentry == NULL)) {
		printk(KERN_ERR "qca_85xx_sw: Failed to create qca-85xx-sw/write-reg file in debugfs\n");
		debugfs_remove_recursive(priv->top_dentry);
		return -1;
	}

	priv->read_dentry = debugfs_create_file("read-reg", 0400,
						priv->top_dentry,
						priv, &qca_85xx_sw_read_reg_ops);

	if (unlikely(priv->read_dentry == NULL)) {
		printk(KERN_ERR "qca_85xx_sw: Failed to create qca-85xx-sw/read-reg file in debugfs\n");
		debugfs_remove_recursive(priv->top_dentry);
		return -1;
	}

	priv->reg_val = 0;
	priv->reg_addr = 0;

	return 0;
}

static struct net_device * qca_85xx_sw_get_eth_dev(
		struct qca_85xx_sw_platform_data *pdata)
{
	struct mii_bus *miibus;
	struct phy_device *phydev;
	struct device *dev;
	uint8_t busid[MII_BUS_ID_SIZE];
	uint8_t phy_id[MII_BUS_ID_SIZE + 3];

	/* Get MII BUS pointer`*/
	snprintf(busid, MII_BUS_ID_SIZE, "%s.%d",
			pdata->sgmii_plus_if_mdio_bus_name,
			pdata->sgmii_plus_if_phy_mdio_bus_id);

	dev = bus_find_device_by_name(&platform_bus_type, NULL,	busid);
	if (!dev)
		return NULL;

	miibus = dev_get_drvdata(dev);
	if (!miibus)
		return NULL;

	 /* create a phyid using MDIO bus id and MDIO bus address of phy */
	snprintf(phy_id, MII_BUS_ID_SIZE + 3,
				PHY_ID_FMT,
				miibus->id,
				pdata->sgmii_plus_if_phy_addr);

	dev = bus_find_device_by_name(&mdio_bus_type, NULL, phy_id);
	if (!dev)
		return NULL;

	phydev = to_phy_device(dev);
	if (!phydev)
		return NULL;

	return phydev->attached_dev;
}

static int __devinit qca_85xx_sw_probe(struct platform_device *pdev)
{
	struct device *miidev;
	uint8_t busid[MII_BUS_ID_SIZE];
	struct mii_bus *mdio_bus;
	struct qca_85xx_sw_platform_data *pdata = pdev->dev.platform_data;
	bool sgmii_plus_link_state_notifier_registered = false;
	int ret = -1;

	if (!pdata)
		return -ENODEV;

	priv = vzalloc(sizeof(struct qca_85xx_sw_priv));
	if (priv == NULL) {
		printk(KERN_ERR "qca_85xx_sw: Could not allocate private memory for 85xx driver\n");
		return ret;
	}

	if (pdata->chip_id == QCA_85XX_SW_ID_QCA8511) {
		priv->chip = &qca8511_chip;
		priv->read = qca_85xx_sw_mii_read;
		priv->write = qca_85xx_sw_mii_write;
	} else {
		printk(KERN_ERR "qca_85xx_sw: Unsupported 85xx switch chipset\n");
		goto err;
	}

	/* Find the MDIO bus to use */
	snprintf(busid, MII_BUS_ID_SIZE, "%s.%d", pdata->mdio_bus_name, pdata->mdio_bus_id);

	miidev = bus_find_device_by_name(&platform_bus_type, NULL, busid);
	if (!miidev) {
		printk(KERN_ERR "qca_85xx_sw: mdio bus '%s':: cannot find bus device\n", busid);
		goto err;
	}

	mdio_bus = dev_get_drvdata(miidev);
	if (!mdio_bus) {
		printk(KERN_ERR "qca_85xx_sw: mdio bus '%s':: cannot find mdio bus pointer from device\n", busid);
		goto err;
	}

	priv->mdio_bus = mdio_bus;

	/* Find the ethernet device we are bound to */
	priv->eth_dev = qca_85xx_sw_get_eth_dev(pdata);
	if (!priv->eth_dev) {
		printk(KERN_ERR "qca_85xx_sw: Could not find ethernet device\n");
		goto err;
	}

	/* Initialize the debug-fs entries */
	ret = qca_85xx_sw_init_debugfs_entries();
	if (ret < 0) {
		printk(KERN_ERR "qca_85xx_sw: Could not initialize debugfs entries\n");
		goto err;
	}

	ret = register_netdevice_notifier(&qca_85xx_netdev_notifier);
	if (ret != 0) {
		printk(KERN_ERR "qca_85xx_sw: Failed to register netdevice notifier %d\n", ret);
		goto err;
	}

	sgmii_plus_link_state_notifier_registered = true;

	ret = priv->chip->hw_init(pdata);
	if (ret != 0) {
		printk(KERN_ERR "qca_85xx_sw: Error initializing 85xx chipset\n");
		goto err;
	}

	priv->sgmii_plus_link_speed = SPEED_UNKNOWN;

	return 0;

err:
	if (sgmii_plus_link_state_notifier_registered)
		unregister_netdevice_notifier(&qca_85xx_netdev_notifier);

	if (priv->top_dentry != NULL)
		debugfs_remove_recursive(priv->top_dentry);

	if (priv->eth_dev)
		dev_put(priv->eth_dev);

	if (priv)
		vfree(priv);

	return ret;
}

static int __devexit qca_85xx_sw_remove(struct platform_device *pdev)
{
	/* Remove debugfs tree */
	if (likely(priv->top_dentry != NULL))
		debugfs_remove_recursive(priv->top_dentry);

	dev_put(priv->eth_dev);

	vfree(priv);

	unregister_netdevice_notifier(&qca_85xx_netdev_notifier);

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
