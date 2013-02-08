/* * Copyright (c) 2012 Qualcomm Atheros, Inc. * */
/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/regulator/pm8xxx-regulator.h>

#include "board-ipq806x.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/* Regulators that are present when using either PM8921 */
/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8921_l1",		NULL),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8921_l2",		NULL),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.0"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.1"),
	REGULATOR_SUPPLY("mipi_csi_vdd",	"msm_csid.2"),
	REGULATOR_SUPPLY("lvds_pll_vdda",	"lvds.0"),
	REGULATOR_SUPPLY("dsi1_pll_vdda",	"mipi_dsi.1"),
	REGULATOR_SUPPLY("HRD_VDDD_CDC_D",		"tabla2x-slim"),
	REGULATOR_SUPPLY("HRD_CDC_VDDA_A_1P2V",	"tabla2x-slim"),
	REGULATOR_SUPPLY("dsi_pll_vdda",	"mdp.0"),
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8921_l3",		NULL),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8921_l4",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_otg"),
	REGULATOR_SUPPLY("iris_vddxo",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("bha_vddxo",		"bt_power"),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8921_l5",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.1"),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8921_l6",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8921_l7",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.3"),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8921_l8",		NULL),
	REGULATOR_SUPPLY("cam_vana",		"4-001a"),
	REGULATOR_SUPPLY("cam_vana",		"4-0010"),
	REGULATOR_SUPPLY("cam_vana",		"4-0048"),
	REGULATOR_SUPPLY("cam_vana",		"4-006c"),
	REGULATOR_SUPPLY("cam_vana",		"4-0034"),
	REGULATOR_SUPPLY("cam_vana",		"4-0020"),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8921_l9",		NULL),
	REGULATOR_SUPPLY("vdd",			"3-0024"),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8921_l10",		NULL),
	REGULATOR_SUPPLY("iris_vddpa",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("bha_vddpa",		"bt_power"),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8921_l11",		NULL),
	REGULATOR_SUPPLY("dsi1_avdd",		"mipi_dsi.1"),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("cam_vdig",		"4-001a"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0010"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0048"),
	REGULATOR_SUPPLY("cam_vdig",		"4-006c"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0034"),
	REGULATOR_SUPPLY("cam_vdig",		"4-0020"),
	REGULATOR_SUPPLY("8921_l12",		NULL),
};
VREG_CONSUMERS(L13) = {
	REGULATOR_SUPPLY("8921_l13",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8921_l14",		NULL),
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8921_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8921_l16",		NULL),
	REGULATOR_SUPPLY("cam_vaf",		"4-001a"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0010"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0048"),
	REGULATOR_SUPPLY("cam_vaf",		"4-006c"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0034"),
	REGULATOR_SUPPLY("cam_vaf",		"4-0020"),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8921_l17",		NULL),
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8921_l18",		NULL),
};
VREG_CONSUMERS(L21) = {
	REGULATOR_SUPPLY("8921_l21",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8921_l22",		NULL),
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8921_l23",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8921_l24",		NULL),
	REGULATOR_SUPPLY("riva_vddmx",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(L25) = {
	REGULATOR_SUPPLY("8921_l25",		NULL),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"0-000d"),
};
VREG_CONSUMERS(L26) = {
	REGULATOR_SUPPLY("8921_l26",		NULL),
	REGULATOR_SUPPLY("core_vdd",		"pil-q6v4-lpass"),
};
VREG_CONSUMERS(L27) = {
	REGULATOR_SUPPLY("8921_l27",		NULL),
};
VREG_CONSUMERS(L28) = {
	REGULATOR_SUPPLY("8921_l28",		NULL),
};
VREG_CONSUMERS(L29) = {
	REGULATOR_SUPPLY("8921_l29",		NULL),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8921_s2",		NULL),
	REGULATOR_SUPPLY("iris_vddrfa",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8921_s3",		NULL),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_otg"),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_ehci_host.0"),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_ehci_host.1"),
	REGULATOR_SUPPLY("HSIC_VDDCX",		"msm_hsic_host"),
	REGULATOR_SUPPLY("riva_vddcx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("vp_pcie",             "msm_pcie"),
	REGULATOR_SUPPLY("vptx_pcie",           "msm_pcie"),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8921_s4",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.1"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"0-000d"),
	REGULATOR_SUPPLY("riva_vddpx",		"wcnss_wlan.0"),
	REGULATOR_SUPPLY("bha_vddpx",		"bt_power"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-005b"),
	REGULATOR_SUPPLY("vcc_i2c",		"3-0024"),
	REGULATOR_SUPPLY("vddp",		"0-0048"),
	REGULATOR_SUPPLY("hdmi_lvl_tsl",	"hdmi_msm.0"),
	REGULATOR_SUPPLY("vdd-io",		"spi0.2"),
	REGULATOR_SUPPLY("sata_pmp_pwr",        "msm_sata.0"),
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8921_s5",		NULL),
	REGULATOR_SUPPLY("krait0",		"acpuclk-ipq806x"),
};
VREG_CONSUMERS(S6) = {
	REGULATOR_SUPPLY("8921_s6",		NULL),
	REGULATOR_SUPPLY("krait1",		"acpuclk-ipq806x"),
};
VREG_CONSUMERS(S7) = {
	REGULATOR_SUPPLY("8921_s7",		NULL),
};
VREG_CONSUMERS(S8) = {
	REGULATOR_SUPPLY("8921_s8",		NULL),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8921_lvs1",		NULL),
	REGULATOR_SUPPLY("iris_vddio",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(LVS3) = {
	REGULATOR_SUPPLY("8921_lvs3",		NULL),
};
VREG_CONSUMERS(LVS4) = {
	REGULATOR_SUPPLY("8921_lvs4",		NULL),
};
VREG_CONSUMERS(LVS5) = {
	REGULATOR_SUPPLY("8921_lvs5",		NULL),
	REGULATOR_SUPPLY("cam_vio",		"4-001a"),
	REGULATOR_SUPPLY("cam_vio",		"4-0010"),
	REGULATOR_SUPPLY("cam_vio",		"4-0048"),
	REGULATOR_SUPPLY("cam_vio",		"4-006c"),
	REGULATOR_SUPPLY("cam_vio",		"4-0034"),
	REGULATOR_SUPPLY("cam_vio",		"4-0020"),
};
VREG_CONSUMERS(LVS6) = {
	REGULATOR_SUPPLY("8921_lvs6",		NULL),
	REGULATOR_SUPPLY("vdd_pcie_vph",        "msm_pcie"),
};
VREG_CONSUMERS(LVS7) = {
	REGULATOR_SUPPLY("8921_lvs7",		NULL),
	REGULATOR_SUPPLY("pll_vdd",		"pil_riva"),
	REGULATOR_SUPPLY("lvds_vdda",		"lvds.0"),
	REGULATOR_SUPPLY("dsi1_vddio",		"mipi_dsi.1"),
	REGULATOR_SUPPLY("dsi_pll_vddio",	"mdp.0"),
	REGULATOR_SUPPLY("hdmi_vdda",		"hdmi_msm.0"),
};
VREG_CONSUMERS(USB_OTG) = {
	REGULATOR_SUPPLY("8921_usb_otg",	NULL),
	REGULATOR_SUPPLY("vbus_otg",		"msm_otg"),
};

VREG_CONSUMERS(EXT_MPP8) = {
	REGULATOR_SUPPLY("ext_mpp8",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.1"),
};
VREG_CONSUMERS(EXT_3P3V) = {
	REGULATOR_SUPPLY("ext_3p3v",		NULL),
	REGULATOR_SUPPLY("vdd-phy",		"spi0.2"),
	REGULATOR_SUPPLY("mhl_usb_hs_switch",	"msm_otg"),
	REGULATOR_SUPPLY("lvds_vccs_3p3v",      "lvds.0"),
	REGULATOR_SUPPLY("dsi1_vccs_3p3v",      "mipi_dsi.1"),
	REGULATOR_SUPPLY("hdmi_mux_vdd",        "hdmi_msm.0"),
	REGULATOR_SUPPLY("pcie_ext_3p3v",       "msm_pcie"),
};
VREG_CONSUMERS(EXT_TS_SW) = {
	REGULATOR_SUPPLY("ext_ts_sw",		NULL),
	REGULATOR_SUPPLY("vdd_ana",		"3-005b"),
};
VREG_CONSUMERS(EXT_SATA_PWR) = {
	REGULATOR_SUPPLY("ext_sata_pwr",        NULL),
	REGULATOR_SUPPLY("sata_ext_3p3v",       "msm_sata.0"),
};

/* Regulators that are only present when using PM8921 */
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8921_s1",		NULL),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8921_lvs2",		NULL),
	REGULATOR_SUPPLY("iris_vdddig",		"wcnss_wlan.0"),
};
VREG_CONSUMERS(NCP) = {
	REGULATOR_SUPPLY("8921_ncp",		NULL),
};
VREG_CONSUMERS(EXT_5V) = {
	REGULATOR_SUPPLY("ext_5v",		NULL),
	REGULATOR_SUPPLY("ext_ddr3",		NULL),
	REGULATOR_SUPPLY("vbus",		"msm_ehci_host.0"),
};

#define PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, _modes, _ops, \
			 _apply_uV, _pull_down, _always_on, _supply_regulator, \
			 _system_uA, _enable_time, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _max_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
				.name			= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= _reg_id, \
		.pull_down_enable	= _pull_down, \
		.system_uA		= _system_uA, \
		.enable_time		= _enable_time, \
	}

#define PM8XXX_LDO(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_NLDO1200(_id, _name, _always_on, _pull_down, _min_uV, \
		_max_uV, _enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_SMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_FTSMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS \
		| REGULATOR_CHANGE_MODE, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_VS(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_VS300(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_NCP(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, 0, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, 0, \
		_always_on, _supply_regulator, 0, _enable_time, _reg_id)

#define PM8XXX_BOOST(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, 0, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, 0, \
		_always_on, _supply_regulator, 0, _enable_time, _reg_id)

/* Pin control initialization */
#define PM8XXX_PC(_id, _name, _always_on, _pin_fn, _pin_ctrl, \
		  _supply_regulator, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
				.name		= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator  = _supply_regulator, \
		}, \
		.id		= _reg_id, \
		.pin_fn		= PM8XXX_VREG_PIN_FN_##_pin_fn, \
		.pin_ctrl	= _pin_ctrl, \
	}

#define GPIO_VREG(_id, _reg_name, _gpio_label, _gpio, _supply_regulator, \
		_active_low) \
	[GPIO_VREG_ID_##_id] = { \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.regulator_name = _reg_name, \
		.gpio_label	= _gpio_label, \
		.gpio		= _gpio, \
		.active_low     = _active_low, \
	}

#define SAW_VREG_INIT(_id, _name, _min_uV, _max_uV) \
	{ \
		.constraints = { \
			.name		= _name, \
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE, \
			.min_uV		= _min_uV, \
			.max_uV		= _max_uV, \
		}, \
		.num_consumer_supplies	= ARRAY_SIZE(vreg_consumers_##_id), \
		.consumer_supplies	= vreg_consumers_##_id, \
	}

#define RPM_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, _default_uV, \
		 _peak_uA, _avg_uA, _pull_down, _pin_ctrl, _freq, _pin_fn, \
		 _force_mode, _sleep_set_force_mode, _power_mode, _state, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _min_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= RPM_VREG_ID_PM8921_##_id, \
		.default_uV		= _default_uV, \
		.peak_uA		= _peak_uA, \
		.avg_uA			= _avg_uA, \
		.pull_down_enable	= _pull_down, \
		.pin_ctrl		= _pin_ctrl, \
		.freq			= RPM_VREG_FREQ_##_freq, \
		.pin_fn			= _pin_fn, \
		.force_mode		= _force_mode, \
		.sleep_set_force_mode	= _sleep_set_force_mode, \
		.power_mode		= _power_mode, \
		.state			= _state, \
		.sleep_selectable	= _sleep_selectable, \
		.system_uA		= _system_uA, \
	}

#define RPM_LDO(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _system_uA, _init_peak_uA) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _init_peak_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_NONE, RPM_VREG_POWER_MODE_IPQ806X_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, _system_uA)

#define RPM_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _supply_regulator, _system_uA, _freq, _force_mode, \
		 _sleep_set_force_mode) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _system_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_##_force_mode, \
		 RPM_VREG_FORCE_MODE_IPQ806X_##_sleep_set_force_mode, \
		 RPM_VREG_POWER_MODE_IPQ806X_PWM, RPM_VREG_STATE_OFF, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA)

#define RPM_VS(_id, _always_on, _pd, _sleep_selectable, _supply_regulator) \
	RPM_INIT(_id, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, 0, 1000, 1000, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_NONE, RPM_VREG_POWER_MODE_IPQ806X_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

#define RPM_NCP(_id, _always_on, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _freq) \
	RPM_INIT(_id, _min_uV, _max_uV, 0, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS, 0, _max_uV, 1000, 1000, 0, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_NONE, RPM_VREG_POWER_MODE_IPQ806X_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

/* Pin control initialization */
#define RPM_PC_INIT(_id, _always_on, _pin_fn, _pin_ctrl, _supply_regulator) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id	  = RPM_VREG_ID_PM8921_##_id##_PC, \
		.pin_fn	  = RPM_VREG_PIN_FN_IPQ806X_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

/* GPIO regulator constraints */
struct gpio_regulator_platform_data
ipq806x_gpio_regulator_pdata[] __devinitdata = {
	/*        ID      vreg_name gpio_label   gpio   supply  active_low */
	GPIO_VREG(EXT_5V, "ext_5v", "ext_5v_en",
				PM8921_MPP_PM_TO_SYS(7), NULL, 0),
	GPIO_VREG(EXT_3P3V, "ext_3p3v", "ext_3p3v_en",
		  IPQ806X_EXT_3P3V_REG_EN_GPIO, NULL, 0),
	GPIO_VREG(EXT_TS_SW, "ext_ts_sw", "ext_ts_sw_en",
		  PM8921_GPIO_PM_TO_SYS(23), "ext_3p3v", 0),
	GPIO_VREG(EXT_MPP8, "ext_mpp8", "ext_mpp8_en",
			PM8921_MPP_PM_TO_SYS(8), NULL, 1),
	GPIO_VREG(EXT_SATA_PWR, "ext_sata_pwr", "ext_sata_pwr_en",
			PM8921_MPP_PM_TO_SYS(4), "ext_3p3v", 1),
};

/* SAW regulator constraints */
struct regulator_init_data ipq806x_saw_regulator_pdata_8921_s5 =
	/*	      ID  vreg_name	       min_uV   max_uV */
	SAW_VREG_INIT(S5, "8921_s5",	       850000, 1300000);
struct regulator_init_data ipq806x_saw_regulator_pdata_8921_s6 =
	SAW_VREG_INIT(S6, "8921_s6",	       850000, 1300000);

/* PM8921 regulator constraints */
struct pm8xxx_regulator_platform_data
ipq806x_pm8921_regulator_pdata[] __devinitdata = {
	/*
	 *		ID   name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_NLDO1200(L26, "8921_l26", 0, 1, 375000, 1050000, 200, "8921_s7",
		0, 1),

	/*           ID        name     always_on pd       en_t supply reg_ID */
	PM8XXX_VS300(USB_OTG,  "8921_usb_otg",  0, 0,         0, "ext_5v", 2),
};

static struct rpm_regulator_init_data
ipq806x_rpm_regulator_init_data[] __devinitdata = {
	/*	ID a_on pd ss min_uV   max_uV  supply sys_uA  freq  fm  ss_fm */
	RPM_SMPS(S1, 1, 1, 0, 1225000, 1225000, NULL, 100000, 3p20, NONE, NONE),
	RPM_SMPS(S2, 0, 1, 0, 1300000, 1300000, NULL,      0, 1p60, NONE, NONE),
	RPM_SMPS(S3, 0, 1, 1,  500000, 1150000, NULL, 100000, 4p80, NONE, NONE),
	RPM_SMPS(S4, 1, 1, 0, 1800000, 1800000, NULL, 100000, 1p60, AUTO, AUTO),
	RPM_SMPS(S7, 0, 0, 0, 1300000, 1300000, NULL, 100000, 3p20, NONE, NONE),
	RPM_SMPS(S8, 0, 1, 0, 2200000, 2200000, NULL,      0, 1p60, NONE, NONE),

	/*	ID a_on pd ss min_uV   max_uV   supply    sys_uA init_ip */
	RPM_LDO(L1,  1, 1, 0, 1100000, 1100000, "8921_s4",     0,  1000),
	RPM_LDO(L2,  0, 1, 0, 1200000, 1200000, "8921_s4",     0,     0),
	RPM_LDO(L3,  0, 1, 0, 3075000, 3075000, NULL,          0,     0),
	RPM_LDO(L4,  1, 1, 0, 1800000, 1800000, NULL,          0, 10000),
	RPM_LDO(L5,  0, 1, 0, 2950000, 2950000, NULL,          0,     0),
	RPM_LDO(L6,  0, 1, 0, 2950000, 2950000, NULL,          0,     0),
	RPM_LDO(L7,  0, 1, 0, 1850000, 2950000, NULL,          0,     0),
	RPM_LDO(L8,  0, 1, 0, 2800000, 2800000, NULL,          0,     0),
	RPM_LDO(L9,  0, 1, 0, 3000000, 3000000, NULL,          0,     0),
	RPM_LDO(L10, 0, 1, 0, 2900000, 2900000, NULL,          0,     0),
	RPM_LDO(L11, 0, 1, 0, 3000000, 3000000, NULL,          0,     0),
	RPM_LDO(L12, 0, 1, 0, 1200000, 1200000, "8921_s4",     0,     0),
	RPM_LDO(L13, 0, 0, 0, 2220000, 2220000, NULL,          0,     0),
	RPM_LDO(L14, 0, 1, 0, 1800000, 1800000, NULL,          0,     0),
	RPM_LDO(L15, 0, 1, 0, 1800000, 2950000, NULL,          0,     0),
	RPM_LDO(L16, 0, 1, 0, 2800000, 2800000, NULL,          0,     0),
	RPM_LDO(L17, 0, 1, 0, 2000000, 2000000, NULL,          0,     0),
	RPM_LDO(L18, 0, 1, 0, 1300000, 1800000, "8921_s4",     0,     0),
	RPM_LDO(L21, 0, 1, 0, 1050000, 1050000, NULL,          0,     0),
	RPM_LDO(L22, 0, 1, 0, 2600000, 2600000, NULL,          0,     0),
	RPM_LDO(L23, 0, 1, 0, 1800000, 1800000, NULL,          0,     0),
	RPM_LDO(L24, 0, 1, 1,  750000, 1150000, "8921_s1", 10000, 10000),
	RPM_LDO(L25, 1, 1, 0, 1250000, 1250000, "8921_s1", 10000, 10000),
	RPM_LDO(L27, 0, 0, 0, 1100000, 1100000, "8921_s7",     0,     0),
	RPM_LDO(L28, 0, 1, 0, 1050000, 1050000, "8921_s7",     0,     0),
	RPM_LDO(L29, 0, 1, 0, 2000000, 2000000, NULL,          0,     0),

	/*     ID  a_on pd ss                   supply */
	RPM_VS(LVS1, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS3, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS4, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS5, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS6, 0, 1, 0,                   "8921_s4"),
	RPM_VS(LVS7, 0, 1, 1,                   "8921_s4"),
};

static struct rpm_regulator_init_data
ipq806x_rpm_regulator_pm8921_init_data[] __devinitdata = {
	/*     ID  a_on pd ss                   supply */
	RPM_VS(LVS2, 0, 1, 0,                   "8921_s1"),

	/*	ID a_on    ss min_uV   max_uV   supply     freq */
	RPM_NCP(NCP, 0,    0, 1800000, 1800000, "8921_l6", 1p60),
};

int ipq806x_pm8921_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(ipq806x_pm8921_regulator_pdata);

#define RPM_REG_MAP(_id, _sleep_also, _voter, _supply, _dev_name) \
	{ \
		.vreg_id = RPM_VREG_ID_PM8921_##_id, \
		.sleep_also = _sleep_also, \
		.voter = _voter, \
		.supply = _supply, \
		.dev_name = _dev_name, \
	}
static struct rpm_regulator_consumer_mapping
	      msm_rpm_regulator_consumer_mapping[] __devinitdata = {
	RPM_REG_MAP(LVS7, 0, 1, "krait0_hfpll", "acpuclk-ipq806x"),
	RPM_REG_MAP(LVS7, 0, 2, "krait1_hfpll", "acpuclk-ipq806x"),
	RPM_REG_MAP(LVS7, 0, 6, "l2_hfpll",     "acpuclk-ipq806x"),
	RPM_REG_MAP(L24,  0, 1, "krait0_mem",   "acpuclk-ipq806x"),
	RPM_REG_MAP(L24,  0, 2, "krait1_mem",   "acpuclk-ipq806x"),
	RPM_REG_MAP(S3,   0, 1, "krait0_dig",   "acpuclk-ipq806x"),
	RPM_REG_MAP(S3,   0, 2, "krait1_dig",   "acpuclk-ipq806x"),
};

struct rpm_regulator_platform_data ipq806x_rpm_regulator_pdata __devinitdata = {
	.init_data		  = ipq806x_rpm_regulator_init_data,
	.num_regulators		  = ARRAY_SIZE(ipq806x_rpm_regulator_init_data),
	.version		  = RPM_VREG_VERSION_IPQ806X,
	.vreg_id_vdd_mem	  = RPM_VREG_ID_PM8921_L24,
	.vreg_id_vdd_dig	  = RPM_VREG_ID_PM8921_S3,
	.requires_tcxo_workaround = true,
	.consumer_map		  = msm_rpm_regulator_consumer_mapping,
	.consumer_map_len = ARRAY_SIZE(msm_rpm_regulator_consumer_mapping),
};

/* Regulators that are only present when using PM8921 */
struct rpm_regulator_platform_data
ipq806x_rpm_regulator_pm8921_pdata __devinitdata = {
	.init_data		  = ipq806x_rpm_regulator_pm8921_init_data,
	.num_regulators	= ARRAY_SIZE(ipq806x_rpm_regulator_pm8921_init_data),
	.version		  = RPM_VREG_VERSION_IPQ806X,
	.vreg_id_vdd_mem	  = RPM_VREG_ID_PM8921_L24,
	.vreg_id_vdd_dig	  = RPM_VREG_ID_PM8921_S3,
	.requires_tcxo_workaround = true,
};

#define VREG_DUMMY_CONSUMERS(_id) \
	struct regulator_consumer_supply dummy_supplies_##_id[]

VREG_DUMMY_CONSUMERS(1) = {
        REGULATOR_SUPPLY("ssusb_vdd_dig",    "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(2) = {
	REGULATOR_SUPPLY("SSUSB_VDDCX",      "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(3) = {
	REGULATOR_SUPPLY("SSUSB_1p8",        "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(4) = {
	REGULATOR_SUPPLY("hsusb_vdd_dig",    "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(5) = {
	REGULATOR_SUPPLY("HSUSB_VDDCX",      "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(6) = {
	REGULATOR_SUPPLY("HSUSB_3p3",        "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(7) = {
	REGULATOR_SUPPLY("HSUSB_1p8",        "msm-dwc3.0"),
};
VREG_DUMMY_CONSUMERS(8) = {
	REGULATOR_SUPPLY("ssusb_vdd_dig",    "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(9) = {
	REGULATOR_SUPPLY("SSUSB_VDDCX",      "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(10) = {
	REGULATOR_SUPPLY("SSUSB_1p8",        "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(11) = {
	REGULATOR_SUPPLY("hsusb_vdd_dig",    "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(12) = {
	REGULATOR_SUPPLY("HSUSB_VDDCX",      "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(13) = {
	REGULATOR_SUPPLY("HSUSB_3p3",        "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(14) = {
	REGULATOR_SUPPLY("HSUSB_1p8",        "msm-dwc3.1"),
};
VREG_DUMMY_CONSUMERS(15) = {
	REGULATOR_SUPPLY("hsic_vdd_dig",  "msm_hsic_host"),
};
VREG_DUMMY_CONSUMERS(16) = {
        REGULATOR_SUPPLY("HSIC_VDDCX",  "msm_hsic_host"),
};

#define REG_INIT_DATA(_id)\
	struct regulator_init_data ipq806x_dummy_regulator##_id

#define DUMMY_VREG_INIT(_id, _name, _min_uV, _max_uV) \
	{ \
		.constraints = { \
			.name        = _name, \
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE, \
			.min_uV         = _min_uV, \
			.max_uV         = _max_uV, \
		}, \
		.num_consumer_supplies = \
				ARRAY_SIZE(dummy_supplies_##_id),\
		.consumer_supplies = dummy_supplies_##_id,\
	}

REG_INIT_DATA(_ssusb_vdd_dig1) =
	DUMMY_VREG_INIT(1, "dummy1",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_SSUSB_VDDCX1) =
	DUMMY_VREG_INIT(2, "dummy2",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_SSUSB_1p81) =
	DUMMY_VREG_INIT(3, "dummy3",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_hsusb_vdd_dig1) =
	DUMMY_VREG_INIT(4, "dummy4",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_HSUSB_VDDCX1) =
	DUMMY_VREG_INIT(5, "dummy5",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_HSUSB_3p31) =
	DUMMY_VREG_INIT(6, "dummy6",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_HSUSB_1p81) =
	DUMMY_VREG_INIT(7, "dummy7",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_ssusb_vdd_dig2) =
	DUMMY_VREG_INIT(8, "dummy8",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_SSUSB_VDDCX2) =
	DUMMY_VREG_INIT(9, "dummy9",    0x0, 0xFFFFFFF);
REG_INIT_DATA(_SSUSB_1p82) =
	DUMMY_VREG_INIT(10, "dummy10",  0x0, 0xFFFFFFF);
REG_INIT_DATA(_hsusb_vdd_dig2) =
	DUMMY_VREG_INIT(11, "dummy11",  0x0, 0xFFFFFFF);
REG_INIT_DATA(_HSUSB_VDDCX2) =
	DUMMY_VREG_INIT(12, "dummy12",  0x0, 0xFFFFFFF);
REG_INIT_DATA(_HSUSB_3p32) =
	DUMMY_VREG_INIT(13, "dummy13",  0x0, 0xFFFFFFF);
REG_INIT_DATA(_HSUSB_1p82) =
	DUMMY_VREG_INIT(14, "dummy14",  0x0, 0xFFFFFFF);
REG_INIT_DATA(_hsic_vdd_dig1) =
        DUMMY_VREG_INIT(15, "dummy15",  0x0, 0xFFFFFFF);

#define FIXED_VOLTAGE_CONFIG(_supp_name, _id, _regul_id)\
	struct fixed_voltage_config ipq806x_fixed_regul_##_regul_id##_id = {\
        .supply_name = _supp_name, \
        .microvolts = 0,\
        .gpio = -EINVAL,\
        .enabled_at_boot = 1, \
        .init_data = &ipq806x_dummy_regulator_##_regul_id##_id,\
        }

FIXED_VOLTAGE_CONFIG("fixed_dummy1",   1, ssusb_vdd_dig);
FIXED_VOLTAGE_CONFIG("fixed_dummy2",   1, SSUSB_VDDCX);
FIXED_VOLTAGE_CONFIG("fixed_dummy3",   1, SSUSB_1p8);
FIXED_VOLTAGE_CONFIG("fixed_dummy4",   1, hsusb_vdd_dig);
FIXED_VOLTAGE_CONFIG("fixed_dummy5",   1, HSUSB_VDDCX);
FIXED_VOLTAGE_CONFIG("fixed_dummy6",   1, HSUSB_3p3);
FIXED_VOLTAGE_CONFIG("fixed_dummy7",   1, HSUSB_1p8);
FIXED_VOLTAGE_CONFIG("fixed_dummy8",   2, ssusb_vdd_dig);
FIXED_VOLTAGE_CONFIG("fixed_dummy9",   2, SSUSB_VDDCX);
FIXED_VOLTAGE_CONFIG("fixed_dummy10",  2, SSUSB_1p8);
FIXED_VOLTAGE_CONFIG("fixed_dummy11",  2, hsusb_vdd_dig);
FIXED_VOLTAGE_CONFIG("fixed_dummy12",  2, HSUSB_VDDCX);
FIXED_VOLTAGE_CONFIG("fixed_dummy13",  2, HSUSB_3p3);
FIXED_VOLTAGE_CONFIG("fixed_dummy14",  2, HSUSB_1p8);
FIXED_VOLTAGE_CONFIG("fixed_dummy15",  1, hsic_vdd_dig);
