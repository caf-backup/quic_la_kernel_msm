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

/*
 * Regulators that are present when using SMB PMIC
 */
VREG_CONSUMERS(S1a) = {
	REGULATOR_SUPPLY("smb208_s1a", NULL),
	/*
	 * TODO Map the PCIe voltages to appropriate regulators after adding
	 * them to regulator list
	 */
	REGULATOR_SUPPLY("vp_pcie", "msm_pcie.0"),
	REGULATOR_SUPPLY("vptx_pcie", "msm_pcie.0"),
	REGULATOR_SUPPLY("vp_pcie", "msm_pcie.1"),
	REGULATOR_SUPPLY("vptx_pcie", "msm_pcie.1"),
	REGULATOR_SUPPLY("vp_pcie", "msm_pcie.2"),
	REGULATOR_SUPPLY("vptx_pcie", "msm_pcie.2"),
};

VREG_CONSUMERS(S1b) = {
	REGULATOR_SUPPLY("smb208_s1b", NULL),
	REGULATOR_SUPPLY("VDD_UBI0", "qca-nss.0"),
	REGULATOR_SUPPLY("VDD_UBI1", "qca-nss.1"),

};

VREG_CONSUMERS(S2a) = {
	REGULATOR_SUPPLY("smb208_s2a", NULL),
	REGULATOR_SUPPLY("krait0", "acpuclk-ipq806x"),
	REGULATOR_SUPPLY("krait0_dummy", "acpuclk-ipq806x"),
};

VREG_CONSUMERS(S2b) = {
	REGULATOR_SUPPLY("smb208_s2b", NULL),
	REGULATOR_SUPPLY("krait1", "acpuclk-ipq806x"),
	REGULATOR_SUPPLY("krait1_dummy", "acpuclk-ipq806x"),
};

VREG_CONSUMERS(S3a) = {
	REGULATOR_SUPPLY("smb208_s3a", NULL),
	REGULATOR_SUPPLY("sdc_vdd_io", "msm_sdcc.3"),
};

VREG_CONSUMERS(S3b) = {
	REGULATOR_SUPPLY("smb208_s3b",NULL),
	REGULATOR_SUPPLY("sata_ext_3p3v", "msm_sata.0"),
};

VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("smb208_s4",  NULL),
	REGULATOR_SUPPLY("sata_pmp_pwr", "msm_sata.0"),
};

VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("smb208_s5",  NULL),
	REGULATOR_SUPPLY("sdc_vdd", "msm_sdcc.3"),
};

VREG_CONSUMERS(S6a) = {
	REGULATOR_SUPPLY("smb208_s6a",  NULL),
	REGULATOR_SUPPLY("pcie_ext_3p3v", "msm_pcie.0"),
	REGULATOR_SUPPLY("pcie_ext_3p3v", "msm_pcie.1"),
	REGULATOR_SUPPLY("pcie_ext_3p3v", "msm_pcie.2"),
};

VREG_CONSUMERS(S6b) = {
	REGULATOR_SUPPLY("smb208_s6b",  NULL),
	REGULATOR_SUPPLY("vdd_pcie_vph", "msm_pcie.0"),
	REGULATOR_SUPPLY("vdd_pcie_vph", "msm_pcie.1"),
	REGULATOR_SUPPLY("vdd_pcie_vph", "msm_pcie.2"),
};

VREG_CONSUMERS(EXT_MPP8) = {
	REGULATOR_SUPPLY("ext_mpp8", NULL),
	REGULATOR_SUPPLY("vbus", "msm_ehci_host.1"),
};
VREG_CONSUMERS(EXT_3P3V) = {
	REGULATOR_SUPPLY("ext_3p3v", NULL),
	REGULATOR_SUPPLY("vdd-phy", "spi0.2"),
	REGULATOR_SUPPLY("mhl_usb_hs_switch", "msm_otg"),
	REGULATOR_SUPPLY("lvds_vccs_3p3v", "lvds.0"),
	REGULATOR_SUPPLY("dsi1_vccs_3p3v", "mipi_dsi.1"),
	REGULATOR_SUPPLY("hdmi_mux_vdd",  "hdmi_msm.0"),
};
VREG_CONSUMERS(EXT_TS_SW) = {
	REGULATOR_SUPPLY("ext_ts_sw", NULL),
	REGULATOR_SUPPLY("vdd_ana", "3-005b"),
};
VREG_CONSUMERS(EXT_SATA_PWR) = {
	REGULATOR_SUPPLY("ext_sata_pwr", NULL),
};

VREG_CONSUMERS(EXT_5V) = {
	REGULATOR_SUPPLY("ext_5v", NULL),
	REGULATOR_SUPPLY("ext_ddr3", NULL),
	REGULATOR_SUPPLY("vbus", "msm_ehci_host.0"),
};


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

/*
 * TODO - Not all members are required for SMB based regulators.
 * Can be optimized
 */
#define RPM_SMB_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, _default_uV, \
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
		.id			=  RPM_VREG_ID_SMB208_##_id, \
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

#define RPM_SMB_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _supply_regulator, _system_uA, _freq, _force_mode, \
		 _sleep_set_force_mode) \
	RPM_SMB_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _system_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_IPQ806X_NONE, \
		 RPM_VREG_FORCE_MODE_IPQ806X_##_force_mode, \
		 RPM_VREG_FORCE_MODE_IPQ806X_##_sleep_set_force_mode, \
		 RPM_VREG_POWER_MODE_IPQ806X_PWM, RPM_VREG_STATE_OFF, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA)

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

static struct rpm_regulator_init_data
ipq806x_rpm_regulator_smb_init_data[] __devinitdata = {
	/*       ID a_on pd ss min_uV   max_uV  supply sys_uA  freq  fm  ss_fm */
#ifdef CONFIG_MSM_SMB_FIXED_VOLTAGE
	RPM_SMB_SMPS(S1a, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 1200000, 0p50, NONE, NONE),
	RPM_SMB_SMPS(S1b, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 1679000, 0p50, NONE, NONE),
	RPM_SMB_SMPS(S2a, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 1740000, 0p50, NONE, NONE),
	RPM_SMB_SMPS(S2b, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 1740000, 0p50, NONE, NONE),
#else
	RPM_SMB_SMPS(S1a, 0, 1, 1, 500000, SMB208_DEFAULT_UV, NULL, 1200000, 0p50, NONE, NONE),
	RPM_SMB_SMPS(S1b, 0, 1, 1, 500000, SMB208_DEFAULT_UV, NULL, 1679000, 0p50, NONE, NONE),
	RPM_SMB_SMPS(S2a, 0, 1, 1, 500000, SMB208_DEFAULT_UV, NULL, 1740000, 0p50, NONE, NONE),
	RPM_SMB_SMPS(S2b, 0, 1, 1, 500000, SMB208_DEFAULT_UV, NULL, 1740000, 0p50, NONE, NONE),
#endif
	/*
	 * These regulators are always fixed.
	 */
	RPM_SMB_SMPS(S3a, 0, 1, 0, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 1250000, 1p00, NONE, NONE),
	RPM_SMB_SMPS(S3b, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 472000, 1p00, NONE, NONE),
	RPM_SMB_SMPS(S4, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 472000, 1p00, NONE, NONE),
	RPM_SMB_SMPS(S5, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 472000, 1p00, NONE, NONE),
	RPM_SMB_SMPS(S6a, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 472000, 1p00, NONE, NONE),
	RPM_SMB_SMPS(S6b, 0, 1, 1, SMB208_FIXED_MINUV, SMB208_FIXED_MAXUV, NULL, 472000, 1p00, NONE, NONE),
};

#define RPM_SMB_REG_MAP(_id, _voter, _supply, _dev_name) \
{ \
	.vreg_id = RPM_VREG_ID_SMB208_##_id, \
	.sleep_also = 0, \
	.voter = _voter, \
	.supply = _supply, \
	.dev_name = _dev_name, \
}

static struct rpm_regulator_consumer_mapping
msm_rpm_regulator_smb_tb732_consumer_mapping[] __devinitdata = {
	/* SMB208_S1a   - VDD_UBI */
	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER1, "VDD_UBI0",       "nss-drv"),
	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER2, "VDD_UBI1",       "nss-drv"),

	/* SMB208_S1b HFPLLs , VDD_CX */
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER1, "krait0_hfpll", "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER2, "krait1_hfpll", "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER4, "l2_hfpll",     "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER5, "krait0_mem",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER6, "krait1_mem",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER7, "krait0_dig",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER8, "krait1_dig",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER11, "VDD_CX",        NULL),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER12, "VCC_CDC_SDCx",  NULL),

	/* SMB207_S3a  -  VDD12_S17, VDD12_QSGMII_PHY */
	RPM_SMB_REG_MAP(S3a, RPM_VREG_VOTER1, "VDD_S17",  "nss_gmac"),
	RPM_SMB_REG_MAP(S3a, RPM_VREG_VOTER2, "VDD_QGMII_PHY",  "nss_gmac"),

	/* SMB207_S3b  - VDDPX_1,VDD15_DDR3 */
	RPM_SMB_REG_MAP(S3b, RPM_VREG_VOTER1, "VDD_IO_1", NULL),
	RPM_SMB_REG_MAP(S3b, RPM_VREG_VOTER2, "VDD_DDR",  NULL),
};

static struct rpm_regulator_consumer_mapping
msm_rpm_regulator_smb_db14x_consumer_mapping[] __devinitdata = {

	/* SMB208_S1b  - HFPLLs , VDD_CX,VDD_CDC_SDCx */
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER1, "krait0_hfpll", "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER2, "krait1_hfpll", "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER4, "l2_hfpll",     "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER5, "krait0_mem",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER6, "krait1_mem",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER7, "krait0_dig",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1a,  RPM_VREG_VOTER8, "krait1_dig",   "acpuclk-ipq806x"),

	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER11, "VDD_CX",        NULL),
	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER12, "VCC_CDC_SDCx",  NULL),

	/* SMB208_S1a   - VDD_UBI */
	RPM_SMB_REG_MAP(S1b, RPM_VREG_VOTER1, "VDD_UBI0",       "qca-nss.0"),
	RPM_SMB_REG_MAP(S1b, RPM_VREG_VOTER2, "VDD_UBI1",       "qca-nss.1"),

	/* SMB207_S3a  -  VDD12_S17, VDD12_QSGMII_PHY */
	RPM_SMB_REG_MAP(S3a, RPM_VREG_VOTER1, "VDD_S17",  "nss_gmac"),
	RPM_SMB_REG_MAP(S3a, RPM_VREG_VOTER2, "VDD_QGMII_PHY",  "nss_gmac"),

	/* SMB207_S3b  - VDDPX_1,VDD15_DDR3 */
	RPM_SMB_REG_MAP(S3b, RPM_VREG_VOTER1, "VDD_IO_1", NULL),
	RPM_SMB_REG_MAP(S3b, RPM_VREG_VOTER2, "VDD_DDR",  NULL),
};

static struct rpm_regulator_consumer_mapping
msm_rpm_regulator_smb_rumi3_consumer_mapping[] __devinitdata = {
	/*
	 * These will be tested using sysfs using command line interface
	 * and dont need to be mapped to any consumers
	 */
	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER1, "VDD_SMB_S1a",       NULL),
	RPM_SMB_REG_MAP(S1b, RPM_VREG_VOTER9, "VDD_SMB_S1b",       NULL),

	/* SMB208_S1a   - VDD_UBI */
	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER2, "VDD_UBI0",       "nss-drv"),
	RPM_SMB_REG_MAP(S1a, RPM_VREG_VOTER3, "VDD_UBI1",       "nss-drv"),

	/* SMB208_S1b  - VDD_CX,VDD_CDC_SDCx */
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER1, "krait0_hfpll", "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER2, "krait1_hfpll", "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER4, "l2_hfpll",     "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER5, "krait0_mem",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER6, "krait1_mem",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER7, "krait0_dig",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER8, "krait1_dig",   "acpuclk-ipq806x"),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER11, "VDD_CX",        NULL),
	RPM_SMB_REG_MAP(S1b,  RPM_VREG_VOTER12, "VCC_CDC_SDCx",  NULL),

	/* SMB207_S3a  -  VDD12_S17, VDD12_QSGMII_PHY */
	RPM_SMB_REG_MAP(S3a, RPM_VREG_VOTER1, "VDD_S17",  "nss_gmac"),
	RPM_SMB_REG_MAP(S3a, RPM_VREG_VOTER2, "VDD_QGMII_PHY",  "nss_gmac"),

	/* SMB207_S3b  - VDDPX_1,VDD15_DDR3 */
	RPM_SMB_REG_MAP(S3b, RPM_VREG_VOTER1, "VDD_IO_1", NULL),
	RPM_SMB_REG_MAP(S3b, RPM_VREG_VOTER2, "VDD_DDR",  NULL),

};

/* Regulators that are only present when using SMB208 */
struct rpm_regulator_platform_data
ipq806x_rpm_regulator_smb_pdata __devinitdata = {
	.init_data = ipq806x_rpm_regulator_smb_init_data,
	.num_regulators = ARRAY_SIZE(ipq806x_rpm_regulator_smb_init_data),
	.version = RPM_VREG_VERSION_IPQ806X,
	.vreg_id_vdd_mem = RPM_VREG_ID_SMB208_S1b,
	.vreg_id_vdd_dig = RPM_VREG_ID_SMB208_S1b,
	.requires_tcxo_workaround = false,
	/*
	 * Consumer map is board-dependent , and is initialized in board-specific
	 * fixup function
	 */
	.consumer_map = NULL,
	.consumer_map_len = 0,
};

/*
 *  Board specific configuration for SMB based designs
 *  1) Regulator consumer mapping
 *  2) Fixed regulator configuration
 */
void __init fixup_ipq806x_smb_power_grid(void)
{

#ifdef CONFIG_MSM_SMB_FIXED_VOLTAGE
	static struct rpm_regulator_init_data *rpm_data;
	int i;
#endif
	/*
	 * For RUMI, though the voltage rails are always fixed, we could test
	 * the voltage scaling using SMB test board, and observing the voltages
	 * on the pins.
	 */
	if (machine_is_ipq806x_rumi3()) {
		ipq806x_rpm_regulator_smb_pdata.consumer_map =
			msm_rpm_regulator_smb_rumi3_consumer_mapping;
		ipq806x_rpm_regulator_smb_pdata.consumer_map_len =
			ARRAY_SIZE(msm_rpm_regulator_smb_rumi3_consumer_mapping);
	}
	else if (machine_is_ipq806x_tb726()) {
		ipq806x_rpm_regulator_smb_pdata.consumer_map =
			msm_rpm_regulator_smb_tb732_consumer_mapping;
		ipq806x_rpm_regulator_smb_pdata.consumer_map_len =
			ARRAY_SIZE(msm_rpm_regulator_smb_tb732_consumer_mapping);
	} else if (machine_is_ipq806x_db149() ||
			machine_is_ipq806x_db147() ||
			machine_is_ipq806x_ap148()) {
		ipq806x_rpm_regulator_smb_pdata.consumer_map =
			msm_rpm_regulator_smb_db14x_consumer_mapping;
		ipq806x_rpm_regulator_smb_pdata.consumer_map_len =
			ARRAY_SIZE(msm_rpm_regulator_smb_db14x_consumer_mapping);
	}

#ifdef CONFIG_MSM_SMB_FIXED_VOLTAGE
	rpm_vreg_regulator_set_enable();

	for (i = 0; i < ARRAY_SIZE(ipq806x_rpm_regulator_smb_init_data); i++) {

		rpm_data = &ipq806x_rpm_regulator_smb_init_data[i];

		rpm_data->init_data.constraints.min_uV = SMB208_FIXED_MINUV ;
		rpm_data->init_data.constraints.max_uV = SMB208_FIXED_MAXUV;

		/*
		 * Placeholder for any change in supported operations
		 */

		/* rpm_data->init_data.constraints.valid_ops_mask = 0; */
	}
#endif
}
