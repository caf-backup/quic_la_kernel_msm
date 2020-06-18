// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, Linaro Limited.
 * Copyright (c) 2015, 2017 The Linux Foundation. All rights reserved.
 *
 */

#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "tsens.h"

/* TSENS register data */
#define TSENS_CNTL_ADDR			0x4
#define TSENS_CONFIG_ADDR		0x50
#define TSENS_TRDY_TIMEOUT_US		20000
#define TSENS_THRESHOLD_MAX_CODE	0x3ff
#define TSENS_THRESHOLD_MIN_CODE	0x0
#define TSENS_SN_CTRL_EN		BIT(0)

#define TSENS_TM_TRDY			0x10e4
#define TSENS_TRDY_MASK			BIT(0)

#define TSENS_TM_SN_STATUS			0x1044
#define TSENS_TM_SN_STATUS_VALID_BIT		BIT(14)
#define TSENS_TM_SN_STATUS_CRITICAL_STATUS	BIT(19)
#define TSENS_TM_SN_STATUS_UPPER_STATUS		BIT(12)
#define TSENS_TM_SN_STATUS_LOWER_STATUS		BIT(11)
#define TSENS_TM_SN_LAST_TEMP_MASK		0x3ff

#define MAX_SENSOR				5

/* eeprom layout data for ipq5018 */
#define S0_P1_MASK_4_0	0xf8000000
#define BASE0_MASK	0x0007f800
#define BASE1_MASK	0x07f80000
#define CAL_SEL_MASK	0x00000700

#define S3_P1_MASK_0	0x80000000
#define S2_P2_MASK	0x7e000000
#define S2_P1_MASK	0x01f80000
#define S1_P2_MASK	0x0007e000
#define S1_P1_MASK	0x00001f80
#define S0_P2_MASK	0x0000007e
#define S0_P1_MASK_5	0x00000001

#define S4_P1_MASK	0x0001f800
#define S3_P2_MASK	0x000007e0
#define S3_P1_MASK_5_1	0x0000001f

#define S4_P2_MASK	0x0000003f

#define S0_P1_SHIFT_4_0	27
#define BASE0_SHIFT	11
#define BASE1_SHIFT	19
#define CAL_SEL_SHIFT	8

#define S3_P1_SHIFT_0	31
#define S2_P2_SHIFT	25
#define S2_P1_SHIFT	19
#define S1_P2_SHIFT	13
#define S1_P1_SHIFT	7
#define S0_P2_SHIFT	1
#define S0_P1_SHIFT_5	0

#define S4_P1_SHIFT	11
#define S3_P2_SHIFT	5
#define S3_P1_SHIFT_5_1	0

#define S4_P2_SHIFT	0

#define CONFIG			0x9c
#define CONFIG_MASK		0xf
#define CONFIG_SHIFT		0

#define EN                      BIT(0)
#define SW_RST                  BIT(1)
#define SENSOR0_EN              BIT(3)
#define SLP_CLK_ENA             BIT(26)
#define MEASURE_PERIOD          1
#define SENSOR0_SHIFT           3

static int init_ipq5018(struct tsens_device *tmdev)
{
	int ret;
	u32 reg_cntl;

	init_common(tmdev);
	if (!tmdev->map)
		return -ENODEV;

	reg_cntl = SW_RST;
	ret = regmap_update_bits(tmdev->map, TSENS_CNTL_ADDR, SW_RST, reg_cntl);
	if (ret)
		return ret;

	reg_cntl |= SLP_CLK_ENA | (MEASURE_PERIOD << 18);
	reg_cntl &= ~SW_RST;
	ret = regmap_update_bits(tmdev->map, TSENS_CONFIG_ADDR,
				CONFIG_MASK, CONFIG);

	reg_cntl |= GENMASK(MAX_SENSOR - 1, 0) << SENSOR0_SHIFT;
	ret = regmap_write(tmdev->map, TSENS_CNTL_ADDR, reg_cntl);
	if (ret)
		return ret;

	reg_cntl |= EN;
	ret = regmap_write(tmdev->map, TSENS_CNTL_ADDR, reg_cntl);
	if (ret)
		return ret;

	return 0;
}

static int calibrate_ipq5018(struct tsens_device *tmdev)
{
	u32 base0 = 0, base1 = 0;
	u32 p1[MAX_SENSOR], p2[MAX_SENSOR];
	u32 mode = 0, lsb = 0, msb = 0;
	u32 *qfprom_cdata;
	int i;

	qfprom_cdata = (u32 *)qfprom_read(tmdev->dev, "calib");
	if (IS_ERR(qfprom_cdata))
		return PTR_ERR(qfprom_cdata);

	mode = (qfprom_cdata[0] & CAL_SEL_MASK) >> CAL_SEL_SHIFT;

	dev_dbg(tmdev->dev, "calibration mode is %d\n", mode);

	switch (mode) {
	case TWO_PT_CALIB:
		base1 = (qfprom_cdata[0] & BASE1_MASK) >> BASE1_SHIFT;
		p2[0] = (qfprom_cdata[1] & S0_P2_MASK) >> S0_P2_SHIFT;
		p2[1] = (qfprom_cdata[1] & S1_P2_MASK) >> S1_P2_SHIFT;
		p2[2] = (qfprom_cdata[1] & S2_P2_MASK) >> S2_P2_SHIFT;
		p2[3] = (qfprom_cdata[2] & S3_P2_MASK) >> S3_P2_SHIFT;
		p2[4] = (qfprom_cdata[3] & S4_P2_MASK) >> S4_P2_SHIFT;
		for (i = 0; i < MAX_SENSOR; i++)
			p2[i] = ((base1 + p2[i]) << 2);
		/* Fall through */
	case ONE_PT_CALIB2:
		base0 = (qfprom_cdata[0] & BASE0_MASK) >> BASE0_SHIFT;
		/* This value is split over two registers, 5 bits and 1 bit */
		lsb = (qfprom_cdata[0] & S0_P1_MASK_4_0) >> S0_P1_SHIFT_4_0;
		msb = (qfprom_cdata[1] & S0_P1_MASK_5) >> S0_P1_SHIFT_5;
		p1[0] = msb << 5 | lsb;
		p1[1] = (qfprom_cdata[1] & S1_P1_MASK) >> S1_P1_SHIFT;
		p1[2] = (qfprom_cdata[1] & S2_P1_MASK) >> S2_P1_SHIFT;
		/* This value is split over two registers, 1 bit and 5 bits */
		lsb = (qfprom_cdata[1] & S3_P1_MASK_0) >> S3_P1_SHIFT_0;
		msb = (qfprom_cdata[2] & S3_P1_MASK_5_1) >> S3_P1_SHIFT_5_1;
		p1[3] = msb << 1 | lsb;
		p1[4] = (qfprom_cdata[2] & S4_P1_MASK) >> S4_P1_SHIFT;
		for (i = 0; i < MAX_SENSOR; i++)
			p1[i] = (((base0) + p1[i]) << 2);
		break;
	default:
		pr_info("Using default calibration\n");
		p1[0] = 403;
		p2[0] = 688;
		p1[1] = 390;
		p2[1] = 674;
		p1[2] = 341;
		p2[2] = 635;
		p1[3] = 387;
		p2[3] = 673;
		p1[4] = 347;
		p2[4] = 639;
		break;
	}

	compute_intercept_slope(tmdev, p1, p2, mode);
	kfree(qfprom_cdata);

	return 0;
}

static int get_temp_ipq5018(struct tsens_device *tmdev, int id, int *temp)
{
	const struct tsens_sensor *s = &tmdev->sensor[id];
	u32 code;
	unsigned int sensor_addr;
	int ret, last_temp = 0, last_temp2 = 0, last_temp3 = 0;

	sensor_addr = TSENS_TM_SN_STATUS + s->hw_id * 4;
	ret = regmap_read(tmdev->map, sensor_addr, &code);
	if (ret)
		return ret;
	last_temp = code & TSENS_TM_SN_LAST_TEMP_MASK;
	if (code & TSENS_TM_SN_STATUS_VALID_BIT)
		goto done;

	/* Try a second time */
	ret = regmap_read(tmdev->map, sensor_addr, &code);
	if (ret)
		return ret;
	if (code & TSENS_TM_SN_STATUS_VALID_BIT) {
		last_temp = code & TSENS_TM_SN_LAST_TEMP_MASK;
		goto done;
	} else {
		last_temp2 = code & TSENS_TM_SN_LAST_TEMP_MASK;
	}

	/* Try a third/last time */
	ret = regmap_read(tmdev->map, sensor_addr, &code);
	if (ret)
		return ret;
	if (code & TSENS_TM_SN_STATUS_VALID_BIT) {
		last_temp = code & TSENS_TM_SN_LAST_TEMP_MASK;
		goto done;
	} else {
		last_temp3 = code & TSENS_TM_SN_LAST_TEMP_MASK;
	}

	if (last_temp == last_temp2)
		last_temp = last_temp2;
	else if (last_temp2 == last_temp3)
		last_temp = last_temp3;
done:
	/* Convert temperature from ADC code to Celsius */
	*temp = code_to_degc(last_temp, s);

	return 0;
}

static const struct tsens_ops ops_ipq5018 = {
	.init		= init_ipq5018,
	.calibrate	= calibrate_ipq5018,
	.get_temp	= get_temp_ipq5018,
};

const struct tsens_data data_ipq5018 = {
	.num_sensors	= MAX_SENSOR,
	.ops		= &ops_ipq5018,
};
