/*
 * QCA 85xx switch driver platform data
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

#ifndef QCA_85XX_SW_H
#define QCA_85XX_SW_H

/*
 * Device IDS for the 85xx switch family
 */
enum {
	QCA_85XX_SW_ID_QCA8511 = 0x01,
	QCA_85XX_SW_ID_QCA8512 = 0x02,
	QCA_85XX_SW_ID_QCA8513 = 0x03,
	QCA_85XX_SW_ID_QCA8519 = 0x04,
};

/*
 * GMAC port modes
 */
enum qca_85xx_sw_port_mode {
	QCA_85XX_SW_PORT_MODE_NOT_CONFIGURED = 0,
	QCA_85XX_SW_PORT_MODE_QSGMII = 1,
	QCA_85XX_SW_PORT_MODE_SGMII = 2,
	QCA_85XX_SW_PORT_MODE_SGMII_PLUS = 3,
	QCA_85XX_SW_PORT_MODE_XAUI = 4,
};

/*
 * QSGMII port platform configuration data
 */
struct qca_85xx_sw_qsgmii_cfg {
	enum qca_85xx_sw_port_mode port_mode;
	uint32_t port_base;
	bool is_speed_forced;
	uint32_t forced_speed;
	uint32_t forced_duplex;
};

/*
 * SGMII port platform configuration data
 */
struct qca_85xx_sw_sgmii_cfg {
	enum qca_85xx_sw_port_mode port_mode;
	bool is_speed_forced;
	uint32_t forced_speed;
	uint32_t forced_duplex;
};

/*
 * 85xx Switch platform configuration
 */
struct qca_85xx_sw_platform_data {
	const char	*eth_dev_name;
	const char	*mdio_bus_name;
	int		mdio_bus_id;
	uint32_t	chip_id;
	struct qca_85xx_sw_qsgmii_cfg qsgmii_cfg;
	struct qca_85xx_sw_sgmii_cfg port_24_cfg;
	struct qca_85xx_sw_sgmii_cfg port_25_cfg;
	struct qca_85xx_sw_sgmii_cfg port_26_cfg;
	struct qca_85xx_sw_sgmii_cfg port_27_cfg;
	struct qca_85xx_sw_sgmii_cfg port_28_cfg;
	struct qca_85xx_sw_sgmii_cfg port_29_cfg;
};

/*
 * 85xx Switch GMAC port IDs
 */
enum qca_85xx_sw_gmac_port {
	QCA_85XX_SW_PORT_1 = 1,
	QCA_85XX_SW_PORT_5 = 5,
	QCA_85XX_SW_PORT_9 = 9,
	QCA_85XX_SW_PORT_13 = 13,
	QCA_85XX_SW_PORT_17 = 17,
	QCA_85XX_SW_PORT_21 = 21,
	QCA_85XX_SW_PORT_23 = 23,
	QCA_85XX_SW_PORT_24 = 24,
	QCA_85XX_SW_PORT_25 = 25,
	QCA_85XX_SW_PORT_26 = 26,
	QCA_85XX_SW_PORT_27 = 27,
	QCA_85XX_SW_PORT_28 = 28,
	QCA_85XX_SW_PORT_29 = 29,
};

/*
 * Register definitions
 */

#define GLOBAL_CTRL_1 				0x8
#define SD_CLK_SEL				0x20

/*
 * Bit definitions for SD_CLK_SEL
 */
#define SD_CLK_SEL_QSGMII_1_RX			0x1
#define SD_CLK_SEL_SGMII_PORT26_RX		0x1000
#define SD_CLK_SEL_SGMII_PORT27_RX		0x2000
#define SD_CLK_SEL_SGMII_PORT28_RX		0x4000
#define SD_CLK_SEL_SGMII_PORT29_RX		0x8000

/*
 * Bit definitions for GLOBAL_CTRL_1
 */
#define GLOBAL_CTRL_1_MAC1_SGMII_EN		0x4
#define GLOBAL_CTRL_1_PORT_26_SGMII_PLUS_EN	0x4000
#define GLOBAL_CTRL_1_PORT_26_SGMII_EN		0x8000
#define GLOBAL_CTRL_1_PORT_27_SGMII_PLUS_EN	0x10000
#define GLOBAL_CTRL_1_PORT_27_SGMII_EN		0x20000
#define GLOBAL_CTRL_1_PORT_28_SGMII_PLUS_EN	0x40000
#define GLOBAL_CTRL_1_PORT_28_SGMII_EN		0x80000
#define GLOBAL_CTRL_1_PORT_29_SGMII_PLUS_EN	0x100000
#define GLOBAL_CTRL_1_PORT_29_SGMII_EN		0x200000


#define PORT_STATUS_CFG_PORT0 	0x100
#define port_status_cfg(n)	(PORT_STATUS_CFG_PORT0 + (n * 0x100))

/*
 * Bit definitions for PORT_STATUS_CFG
 */
#define PORT_STATUS_FORCE_SPEED_10	0x0
#define PORT_STATUS_FORCE_SPEED_100	0x1
#define PORT_STATUS_FORCE_SPEED_1000	0x2
#define PORT_STATUS_TXMAC_EN		0x4
#define PORT_STATUS_RXMAC_EN		0x8
#define PORT_STATUS_TX_FLOW_EN		0x10
#define PORT_STATUS_RX_FLOW_EN		0x20
#define PORT_STATUS_FORCE_DUPLEX_FULL	0x40
#define PORT_STATUS_TX_HALF_FLOW_EN	0x80
#define PORT_STATUS_AUTONEG_EN		0x200
#define PORT_STATUS_AUTO_FLOW_CTRL_EN	0x1000

#define QSGMII_CTRL_GRP0_BASE	0x13000

/* CH0 Control for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_1_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x0)

/* QSGMII settings for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_1_CTRL1(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x4)

/* QSGMII settings for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_1_CTRL2(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x8)

/* CH1 Control for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_2_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0xc)

/* CH2 Control for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_3_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x10)

/* CH3 Control for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_4_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x14)

/* CH0 Control for QSGMII1/QSGMII3/QSGMII5 */
#define QSGMII_5_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x18)

/* QSGMII settings for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_5_CTRL1(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x1c)

/* QSGMII settings for QSGMII0/QSGMII2/QSGMII4 */
#define QSGMII_5_CTRL2(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x20)

/* Register not defined */
#define QSGMII_RESERVED	0x24

/* CH1 Control for QSGMII1/QSGMII3/QSGMII5 */
#define QSGMII_6_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x28)

/* CH2 Control for QSGMII1/QSGMII3/QSGMII5 */
#define QSGMII_7_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x2c)

/* CH3 Control for QSGMII1/QSGMII3/QSGMII5 */
#define QSGMII_8_CTRL0(group)	(QSGMII_CTRL_GRP0_BASE + (0x50*group) + 0x30)

/*
 * Bit definitions for QSGMII_1_CTRL0
 */
#define QSGMII_CTRL_FORCE_DUPLEX_FULL 	0x1
#define QSGMII_CTRL_FORCE_LINK_UP 	0x2
#define QSGMII_CTRL_FORCE_SPEED_10 	0x0
#define QSGMII_CTRL_FORCE_SPEED_100 	0x4
#define QSGMII_CTRL_FORCE_SPEED_1000 	0x8
#define QSGMII_CTRL_FORCE_SPEED 	0x00200000
#define QSGMII_CTRL_QSGMII_MODE_PHY	0x00400000
#define QSGMII_CTRL_QSGMII_MODE_MAC	0x00800000

#define SGMII_CTRL0_PORT26		0x13200		/* Control for SGMII ports  - Port 26 */
#define SGMII_CTRL0_PORT27		0x13220		/* Control for SGMII ports  - Port 27 */
#define SGMII_CTRL0_PORT28		0x13240		/* Control for SGMII ports  - Port 28 */
#define SGMII_CTRL0_PORT29		0x13260		/* Control for SGMII ports  - Port 29 */

/*
 * Bit definitions for SGMII_CTRL0
 */
#define SGMII_CTRL0_FORCE_DUPLEX_FULL 	0x1
#define SGMII_CTRL0_FORCE_SPEED_10	0x0
#define SGMII_CTRL0_FORCE_SPEED_100	0x4
#define SGMII_CTRL0_FORCE_SPEED_1000	0x8
#define SGMII_CTRL0_FORCE_MODE_EN	0x200000

#define SGMII_CTRL0_SGMII_MODE_1000BASE_X	0x0
#define SGMII_CTRL0_SGMII_MODE_PHY		0x400000
#define SGMII_CTRL0_SGMII_MODE_MAC		0x800000

#define XAUI_SGMII_SERDES13_CTRL0 0x1341c	/* Serdes controls for SGMII/SGMII+ Port 26 to 29 */
#define XAUI_SGMII_SERDES13_CTRL1 0x13420	/* Serdes controls for SGMII/SGMII+ Port 26 to 29 */

#endif /* QCA_85XX_SW_H */
