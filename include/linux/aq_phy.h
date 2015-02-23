/*
 * Aquantia PHY driver header file
 *
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef AQ_PHY_H
#define AQ_PHY_H

/* Board specific data structure */
struct aq_phy_platform_data {
	uint16_t phy_addr;		/* Aqunatia PHY Address */
	uint32_t mdio_bus_id;		/* MDIO Bus number where AQ
					   PHY is connected */
	char *mdio_bus_name;		/* MDIO bus name */
};

/* Define the Aqunatia device id */
#define AQ_DEVICE_ID	0x3a1b4a2

/* PMD Line side Rx signal detect register */
#define AQ_PHY_PMD_SIGNAL_DETECT_REG	0xa

/* Mask and Bit definition for AQ_PHY_PMD_SIGNAL_DETECT_REG */
#define AQ_PHY_PMD_SIGNAL_DETECT_MASK	0x1

/* Autonegotiation Vendor Status register */
#define AQ_PHY_LINK_REG		0xC800

/* Mask and Bit definition for AQ_PHY_LINK_REG */
#define AQ_PHY_LINK_DUPLEX_MASK	0x1
#define AQ_PHY_LINK_SPEED_10000	0x3
#define AQ_PHY_LINK_SPEED_5000	0x5
#define AQ_PHY_LINK_SPEED_2500	0x4
#define AQ_PHY_LINK_SPEED_1000	0x2
#define AQ_PHY_LINK_SPEED_100	0x1
#define AQ_PHY_LINK_SPEED_10	0x0

/* Line side link status register */
#define AQ_PHY_PMA_RX_LINK_CURRENT_STATUS_REG	0xE800

/* Mask and Bit definition for AQ_PHY_PMA_RX_LINK_CURRENT_STATUS_REG */
#define AQ_PHY_PMA_RX_LINK_CURRENT_STATUS_MASK	0x1

#endif /*AQ_PHY_H */
