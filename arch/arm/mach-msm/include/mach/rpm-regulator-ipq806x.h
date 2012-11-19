/* * Copyright (c) 2012 Qualcomm Atheros, Inc. * */
/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_IPQ806X_H
#define __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_IPQ806X_H

/* Pin control input signals. */
#define RPM_VREG_PIN_CTRL_PM8921_D1	0x01
#define RPM_VREG_PIN_CTRL_PM8921_A0	0x02
#define RPM_VREG_PIN_CTRL_PM8921_A1	0x04
#define RPM_VREG_PIN_CTRL_PM8921_A2	0x08

/**
 * enum rpm_vreg_pin_fn_ipq806x - RPM regulator pin function choices
 * %RPM_VREG_PIN_FN_IPQ806X_DONT_CARE:	do not care about pin control state of
 *					the regulator; allow another master
 *					processor to specify pin control
 * %RPM_VREG_PIN_FN_IPQ806X_ENABLE:	pin control switches between disable and
 *					enable
 * %RPM_VREG_PIN_FN_IPQ806X_MODE:	pin control switches between LPM and HPM
 * %RPM_VREG_PIN_FN_IPQ806X_SLEEP_B:	regulator is forced into LPM when
 *					sleep_b signal is asserted
 * %RPM_VREG_PIN_FN_IPQ806X_NONE:	do not use pin control for the regulator
 *					and do not allow another master to
 *					request pin control
 *
 * The pin function specified in platform data corresponds to the active state
 * pin function value.  Pin function will be NONE until a consumer requests
 * pin control to be enabled.
 */
enum rpm_vreg_pin_fn_IPQ806X {
	RPM_VREG_PIN_FN_IPQ806X_DONT_CARE,
	RPM_VREG_PIN_FN_IPQ806X_ENABLE,
	RPM_VREG_PIN_FN_IPQ806X_MODE,
	RPM_VREG_PIN_FN_IPQ806X_SLEEP_B,
	RPM_VREG_PIN_FN_IPQ806X_NONE,
};

/**
 * enum rpm_vreg_force_mode_ipq806x - RPM regulator force mode choices
 * %RPM_VREG_FORCE_MODE_IPQ806X_PIN_CTRL:	allow pin control usage
 * %RPM_VREG_FORCE_MODE_IPQ806X_NONE:	do not force any mode
 * %RPM_VREG_FORCE_MODE_IPQ806X_LPM:	force into low power mode
 * %RPM_VREG_FORCE_MODE_IPQ806X_AUTO:	allow regulator to automatically select
 *					its own mode based on realtime current
 *					draw (only available for SMPS
 *					regulators)
 * %RPM_VREG_FORCE_MODE_IPQ806X_HPM:	force into high power mode
 * %RPM_VREG_FORCE_MODE_IPQ806X_BYPASS:	set regulator to use bypass mode, i.e.
 *					to act as a switch and not regulate
 *					(only available for LDO regulators)
 *
 * Force mode is used to override aggregation with other masters and to set
 * special operating modes.
 */
enum rpm_vreg_force_mode_ipq806x {
	RPM_VREG_FORCE_MODE_IPQ806X_PIN_CTRL = 0,
	RPM_VREG_FORCE_MODE_IPQ806X_NONE = 0,
	RPM_VREG_FORCE_MODE_IPQ806X_LPM,
	RPM_VREG_FORCE_MODE_IPQ806X_AUTO,		/* SMPS only */
	RPM_VREG_FORCE_MODE_IPQ806X_HPM,
	RPM_VREG_FORCE_MODE_IPQ806X_BYPASS,	/* LDO only */
};

/**
 * enum rpm_vreg_power_mode_ipq806x - power mode for SMPS regulators
 * %RPM_VREG_POWER_MODE_IPQ806X_HYSTERETIC: Use hysteretic mode for HPM and when
 *					 usage goes high in AUTO
 * %RPM_VREG_POWER_MODE_IPQ806X_PWM:	 Use PWM mode for HPM and when usage
 *					 goes high in AUTO
 */
enum rpm_vreg_power_mode_ipq806x {
	RPM_VREG_POWER_MODE_IPQ806X_HYSTERETIC,
	RPM_VREG_POWER_MODE_IPQ806X_PWM,
};

/**
 * enum rpm_vreg_id - RPM regulator ID numbers (both real and pin control)
 */
/*
 * Temporarily prefix with IPQ806X_, to avoid compile conflict with similar
 * defines in rpm-regulator-8960.h.
 * have to create the proper defines later...
 */
enum rpm_vreg_id_ipq806x {
	IPQ806X_RPM_VREG_ID_PM8921_L1,
	IPQ806X_RPM_VREG_ID_PM8921_L2,
	IPQ806X_RPM_VREG_ID_PM8921_L3,
	IPQ806X_RPM_VREG_ID_PM8921_L4,
	IPQ806X_RPM_VREG_ID_PM8921_L5,
	IPQ806X_RPM_VREG_ID_PM8921_L6,
	IPQ806X_RPM_VREG_ID_PM8921_L7,
	IPQ806X_RPM_VREG_ID_PM8921_L8,
	IPQ806X_RPM_VREG_ID_PM8921_L9,
	IPQ806X_RPM_VREG_ID_PM8921_L10,
	IPQ806X_RPM_VREG_ID_PM8921_L11,
	IPQ806X_RPM_VREG_ID_PM8921_L12,
	IPQ806X_RPM_VREG_ID_PM8921_L13,
	IPQ806X_RPM_VREG_ID_PM8921_L14,
	IPQ806X_RPM_VREG_ID_PM8921_L15,
	IPQ806X_RPM_VREG_ID_PM8921_L16,
	IPQ806X_RPM_VREG_ID_PM8921_L17,
	IPQ806X_RPM_VREG_ID_PM8921_L18,
	IPQ806X_RPM_VREG_ID_PM8921_L21,
	IPQ806X_RPM_VREG_ID_PM8921_L22,
	IPQ806X_RPM_VREG_ID_PM8921_L23,
	IPQ806X_RPM_VREG_ID_PM8921_L24,
	IPQ806X_RPM_VREG_ID_PM8921_L25,
	IPQ806X_RPM_VREG_ID_PM8921_L26,
	IPQ806X_RPM_VREG_ID_PM8921_L27,
	IPQ806X_RPM_VREG_ID_PM8921_L28,
	IPQ806X_RPM_VREG_ID_PM8921_L29,
	IPQ806X_RPM_VREG_ID_PM8921_S1,
	IPQ806X_RPM_VREG_ID_PM8921_S2,
	IPQ806X_RPM_VREG_ID_PM8921_S3,
	IPQ806X_RPM_VREG_ID_PM8921_S4,
	IPQ806X_RPM_VREG_ID_PM8921_S5,
	IPQ806X_RPM_VREG_ID_PM8921_S6,
	IPQ806X_RPM_VREG_ID_PM8921_S7,
	IPQ806X_RPM_VREG_ID_PM8921_S8,
	IPQ806X_RPM_VREG_ID_PM8921_LVS1,
	IPQ806X_RPM_VREG_ID_PM8921_LVS2,
	IPQ806X_RPM_VREG_ID_PM8921_LVS3,
	IPQ806X_RPM_VREG_ID_PM8921_LVS4,
	IPQ806X_RPM_VREG_ID_PM8921_LVS5,
	IPQ806X_RPM_VREG_ID_PM8921_LVS6,
	IPQ806X_RPM_VREG_ID_PM8921_LVS7,
	IPQ806X_RPM_VREG_ID_PM8921_USB_OTG,
	IPQ806X_RPM_VREG_ID_PM8921_HDMI_MVS,
	IPQ806X_RPM_VREG_ID_PM8921_NCP,
	IPQ806X_RPM_VREG_ID_PM8921_MAX_REAL = RPM_VREG_ID_PM8921_NCP,

	/* The following are IDs for regulator devices to enable pin control. */
	IPQ806X_RPM_VREG_ID_PM8921_L1_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L2_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L3_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L4_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L5_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L6_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L7_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L8_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L9_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L10_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L11_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L12_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L14_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L15_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L16_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L17_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L18_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L21_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L22_PC,
	IPQ806X_RPM_VREG_ID_PM8921_L23_PC,

	IPQ806X_RPM_VREG_ID_PM8921_L29_PC,
	IPQ806X_RPM_VREG_ID_PM8921_S1_PC,
	IPQ806X_RPM_VREG_ID_PM8921_S2_PC,
	IPQ806X_RPM_VREG_ID_PM8921_S3_PC,
	IPQ806X_RPM_VREG_ID_PM8921_S4_PC,

	IPQ806X_RPM_VREG_ID_PM8921_S7_PC,
	IPQ806X_RPM_VREG_ID_PM8921_S8_PC,
	IPQ806X_RPM_VREG_ID_PM8921_LVS1_PC,

	IPQ806X_RPM_VREG_ID_PM8921_LVS3_PC,
	IPQ806X_RPM_VREG_ID_PM8921_LVS4_PC,
	IPQ806X_RPM_VREG_ID_PM8921_LVS5_PC,
	IPQ806X_RPM_VREG_ID_PM8921_LVS6_PC,
	IPQ806X_RPM_VREG_ID_PM8921_LVS7_PC,

	IPQ806X_RPM_VREG_ID_PM8921_MAX = RPM_VREG_ID_PM8921_LVS7_PC,
};

/* Minimum high power mode loads in uA. */
#define RPM_VREG_IPQ806X_LDO_5_HPM_MIN_LOAD		0
#define RPM_VREG_IPQ806X_LDO_50_HPM_MIN_LOAD		5000
#define RPM_VREG_IPQ806X_LDO_150_HPM_MIN_LOAD		10000
#define RPM_VREG_IPQ806X_LDO_300_HPM_MIN_LOAD		10000
#define RPM_VREG_IPQ806X_LDO_600_HPM_MIN_LOAD		10000
#define RPM_VREG_IPQ806X_LDO_1200_HPM_MIN_LOAD		10000
#define RPM_VREG_IPQ806X_SMPS_1500_HPM_MIN_LOAD		100000
#define RPM_VREG_IPQ806X_SMPS_2000_HPM_MIN_LOAD		100000

#endif
