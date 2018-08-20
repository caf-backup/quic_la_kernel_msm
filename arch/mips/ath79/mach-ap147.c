/*
 *  Atheros AP147 reference board support
 *
 *  Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *  Copyright (C) 2014 Matthias Schiffer <mschiffer@universe-factory.net>
 *  Copyright (C) 2015 Sven Eckelmann <sven@open-mesh.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/ath9k_platform.h>
#include <linux/ar8216_platform.h>

#include <asm/mach-ath79/ar71xx_regs.h>
#include <asm/mach-ath79/ath79.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

#include "common.h"
#include "dev-ap9x-pci.h"
#include "dev-eth.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-spi.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "dev-i2c.h"
#include "machtypes.h"
#include "pci.h"

#define AP147_GPIO_LED_WAN	4
#define AP147_GPIO_LED_LAN1	16
#define AP147_GPIO_LED_LAN2	15
#define AP147_GPIO_LED_LAN3	14
#define AP147_GPIO_LED_LAN4	11
#define AP147_GPIO_LED_STATUS	13
#define AP147_GPIO_LED_WLAN_2G	12

#define AP147_GPIO_BTN_WPS	17

#define AP147_KEYS_POLL_INTERVAL	20	/* msecs */
#define AP147_KEYS_DEBOUNCE_INTERVAL	(3 * AP147_KEYS_POLL_INTERVAL)

#define AP147_MAC0_OFFSET		0
#define AP147_MAC1_OFFSET		6
#define AP147_WMAC_CALDATA_OFFSET	0x1000

static struct gpio_led ap147_leds_gpio[] __initdata = {
	{
		.name		= "ap147:green:status",
		.gpio		= AP147_GPIO_LED_STATUS,
		.active_low	= 1,
	}, {
		.name		= "ap147:green:wlan-2g",
		.gpio		= AP147_GPIO_LED_WLAN_2G,
		.active_low	= 1,
	}, {
		.name		= "ap147:green:lan1",
		.gpio		= AP147_GPIO_LED_LAN1,
		.active_low	= 1,
	}, {
		.name		= "ap147:green:lan2",
		.gpio		= AP147_GPIO_LED_LAN2,
		.active_low	= 1,
	}, {
		.name		= "ap147:green:lan3",
		.gpio		= AP147_GPIO_LED_LAN3,
		.active_low	= 1,
	}, {
		.name		= "ap147:green:lan4",
		.gpio		= AP147_GPIO_LED_LAN4,
		.active_low	= 1,
	}, {
		.name		= "ap147:green:wan",
		.gpio		= AP147_GPIO_LED_WAN,
		.active_low	= 1,
	},
};

static struct gpio_keys_button ap147_gpio_keys[] __initdata = {
	{
		.desc		= "wps button",
		.type		= EV_KEY,
		.code		= KEY_WPS_BUTTON,
		.debounce_interval = AP147_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= AP147_GPIO_BTN_WPS,
		.active_low	= 1,
	}
};

static struct gpio_led ap147ioe_leds_gpio[] __initdata = {
	/* add LED definition here */
};

static struct gpio_keys_button ap147ioe_gpio_keys[] __initdata = {
	/* add KEY definition here */
};

static struct ath79_spi_controller_data ap147ioe_spi0_cdata =
{
	.cs_type        = ATH79_SPI_CS_TYPE_INTERNAL,
	.is_flash       = true,
	.cs_line        = 0,
};

static struct ath79_spi_controller_data ap147ioe_spi1_cdata =
{
	.cs_type        = ATH79_SPI_CS_TYPE_INTERNAL,
	.is_flash       = false,
	.cs_line        = 1,
};

static struct spi_board_info ap147ioe_spi_info[] = {
	{
		.bus_num        = 0,
		.chip_select    = 0,
		.max_speed_hz   = 25000000,
		.modalias       = "m25p80",
		.controller_data = &ap147ioe_spi0_cdata,
		.platform_data  = NULL,
	},
	{
		.bus_num        = 0,
		.chip_select    = 1,
		.max_speed_hz   = 50000000,
		.modalias       = "ath79-spinand",
		.controller_data = &ap147ioe_spi1_cdata,
		.platform_data  = NULL,
	}
};

static struct ath79_spi_platform_data ap147ioe_spi_data = {
	.bus_num                = 0,
	.num_chipselect         = 2,
	.word_banger            = true,
};

static void __init ap147_common_setup(void)
{
	u8 *art = (u8 *)KSEG1ADDR(0x1fff0000);

	ath79_register_usb();

	ath79_register_pci();

	ath79_register_wmac(art + AP147_WMAC_CALDATA_OFFSET, NULL);

	ath79_setup_ar933x_phy4_switch(false, false);

	ath79_register_mdio(0, 0x0);
	ath79_register_mdio(1, 0x0);

	/* WAN */
	ath79_switch_data.phy4_mii_en = 1;
	ath79_eth0_data.phy_if_mode = PHY_INTERFACE_MODE_MII;
	ath79_eth0_data.duplex = DUPLEX_FULL;
	ath79_eth0_data.speed = SPEED_100;
	ath79_eth0_data.phy_mask = BIT(4);
	ath79_init_mac(ath79_eth0_data.mac_addr, art + AP147_MAC0_OFFSET, 0);
	ath79_register_eth(0);

	/* LAN */
	ath79_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_GMII;
	ath79_eth1_data.speed = SPEED_1000;
	ath79_eth1_data.duplex = DUPLEX_FULL;
	ath79_switch_data.phy_poll_mask |= BIT(4);
	ath79_init_mac(ath79_eth1_data.mac_addr, art + AP147_MAC1_OFFSET, 0);
	ath79_register_eth(1);
}

static void __init ap147_setup(void)
{
	ath79_register_m25p80(NULL);
	ath79_register_leds_gpio(-1, ARRAY_SIZE(ap147_leds_gpio),
				 ap147_leds_gpio);
	ath79_register_gpio_keys_polled(-1, AP147_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(ap147_gpio_keys),
					ap147_gpio_keys);
	ap147_common_setup();
}

static void __init ap147ioe_setup(void)
{
	ath79_register_m25p80(NULL);
	ap147_common_setup();
}

static void __init ap147ioe_dual_setup(void)
{
	ath79_register_m25p80_multi(NULL);
	ap147_common_setup();
}

static void __init ap147ioe_nand_setup(void)
{
	ath79_register_spi(&ap147ioe_spi_data, ap147ioe_spi_info, 2);
	ap147_common_setup();
}

MIPS_MACHINE(ATH79_MACH_AP147_010, "AP147-010", "Qualcomm Atheros AP147-010 reference board", ap147_setup);
MIPS_MACHINE(ATH79_MACH_AP147IOE, "AP147IOE", "Qualcomm Atheros AP147 IoE reference board", ap147ioe_setup);
MIPS_MACHINE(ATH79_MACH_AP147IOE_NAND, "AP147IOE-NAND", "Qualcomm Atheros AP147 IoE nand reference board", ap147ioe_nand_setup);
MIPS_MACHINE(ATH79_MACH_AP147IOE_DUAL, "AP147IOE-DUAL", "Qualcomm Atheros AP147 IoE dual reference board", ap147ioe_dual_setup);
