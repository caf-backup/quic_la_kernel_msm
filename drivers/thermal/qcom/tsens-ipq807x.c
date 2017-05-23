/*
 * Copyright (c) 2015, 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include "tsens.h"

/* TSENS register data */
#define TSENS_CNTL_ADDR			0x4
#define TSENS_MEASURE_PERIOD_ADDR	0x8
#define TSENS_MEASURE_PERIOD		0x1
#define TSENS_TRDY_TIMEOUT_US		200
#define TSENS_THRESHOLD_MAX_CODE	0x3ff
#define TSENS_THRESHOLD_MIN_CODE	0x0

#define TSENS_SN_EN_ALL	(BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7) \
			| BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12) \
			| BIT(13) | BIT(14) | BIT(15))
#define TSENS_SN_CTRL_EN		BIT(0)
#define TSENS_SN_SW_RST			BIT(1)
#define TSENS_SN_ADC_CLK_SEL		BIT(2)
#define TSENS_SN_TEMP_DEGC		BIT(21)

#define TSENS_TM_TRDY			0x10e4
#define TSENS_TRDY_MASK			BIT(0)
#define TSENS_TM_CODE_BIT_MASK		0xfff
#define TSENS_TM_CODE_SIGN_BIT		0x800

#define TSENS_TM_INT_EN			0x1004
#define TSENS_TM_CRITICAL_INT_EN	BIT(2)
#define TSENS_TM_UPPER_INT_EN		BIT(1)
#define TSENS_TM_LOWER_INT_EN		BIT(0)

#define TSENS_TM_SN_CRITICAL_THRESHOLD_MASK	0xfff
#define TSENS_TM_SN_CRITICAL_THRESHOLD		0x1060
#define TSENS_TM_SN_STATUS			0x10a0
#define TSENS_TM_SN_STATUS_VALID_BIT		BIT(21)
#define TSENS_TM_SN_STATUS_CRITICAL_STATUS	BIT(19)
#define TSENS_TM_SN_STATUS_UPPER_STATUS		BIT(18)
#define TSENS_TM_SN_STATUS_LOWER_STATUS		BIT(17)
#define TSENS_TM_SN_LAST_TEMP_MASK		0xfff

static int suspend_ipq807x(struct tsens_device *tmdev)
{
	return -EINVAL;
}

static int resume_ipq807x(struct tsens_device *tmdev)
{
	return -EINVAL;
}

static int enable_ipq807x(struct tsens_device *tmdev, int id)
{
	int ret;
	u32 reg_cntl;

	ret = regmap_read(tmdev->map, TSENS_CNTL_ADDR, &reg_cntl);
	if (ret)
		return -EINVAL;

	/* Enable TSENS monitoring */
	reg_cntl &= TSENS_SN_CTRL_EN;
	regmap_write(tmdev->map, TSENS_CNTL_ADDR, reg_cntl);

	/* Enable interrupt registers */
	regmap_write(tmdev->map, TSENS_TM_INT_EN, TSENS_TM_CRITICAL_INT_EN
			| TSENS_TM_UPPER_INT_EN | TSENS_TM_LOWER_INT_EN);

	return 0;
}

static void disable_ipq807x(struct tsens_device *tmdev)
{
	int ret;
	u32 reg_cntl;

	ret = regmap_read(tmdev->map, TSENS_CNTL_ADDR, &reg_cntl);
	if (ret)
		return;

	/* Disable TSENS monitoring */
	reg_cntl &= ~TSENS_SN_CTRL_EN;
	regmap_write(tmdev->map, TSENS_CNTL_ADDR, reg_cntl);

	/* Disable interrupt registers */
	regmap_write(tmdev->map, TSENS_TM_INT_EN, 0);
}

static int init_ipq807x(struct tsens_device *tmdev)
{
	int ret, i;
	u32 reg_cntl;

	init_common(tmdev);
	if (!tmdev->map)
		return -ENODEV;

	/* Store all sensor address for future use */
	for (i = 0; i < tmdev->num_sensors; i++)
		tmdev->sensor[i].status = TSENS_TM_SN_STATUS + (i * 4);

	/* Assert sw reset */
	ret = regmap_update_bits(tmdev->map, TSENS_CNTL_ADDR,
					TSENS_SN_SW_RST, TSENS_SN_SW_RST);
	if (ret)
		return ret;

	/* Update measure period to 2ms */
	regmap_write(tmdev->map, TSENS_MEASURE_PERIOD_ADDR,
						TSENS_MEASURE_PERIOD);

	ret = regmap_read(tmdev->map, TSENS_CNTL_ADDR, &reg_cntl);
	if (ret)
		return -EINVAL;

	/* Enable TSENS and all 15 sensors */
	reg_cntl |= TSENS_SN_CTRL_EN | TSENS_SN_EN_ALL | TSENS_SN_TEMP_DEGC;
	reg_cntl &= ~TSENS_SN_SW_RST;
	regmap_write(tmdev->map, TSENS_CNTL_ADDR, reg_cntl);

	/* Enable interrupt registers */
	regmap_write(tmdev->map, TSENS_TM_INT_EN, TSENS_TM_CRITICAL_INT_EN
			| TSENS_TM_UPPER_INT_EN	| TSENS_TM_LOWER_INT_EN);

	return 0;
}

static int get_temp_ipq807x(struct tsens_device *tmdev, int id, int *temp)
{
	int ret, last_temp;
	u32 code, trdy;
	const struct tsens_sensor *s = &tmdev->sensor[id];
	unsigned long timeout;

	timeout = jiffies + usecs_to_jiffies(TSENS_TRDY_TIMEOUT_US);
	do {
		ret = regmap_read(tmdev->map, TSENS_TM_TRDY, &trdy);
		if (ret)
			return ret;
		if (!(trdy & TSENS_TRDY_MASK))
			continue;

		ret = regmap_read(tmdev->map, s->status, &code);
		if (ret)
			return ret;

		/* Check whether the temp is valid */
		if (code & TSENS_TM_SN_STATUS_VALID_BIT)
			continue;

		last_temp = code & TSENS_TM_SN_LAST_TEMP_MASK;

		if (last_temp & TSENS_TM_CODE_SIGN_BIT)
			/* Sign extension for negative value */
			last_temp |= (TSENS_TM_CODE_BIT_MASK);

		*temp = last_temp;

		return 0;
	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

const struct tsens_ops ops_ipq807x = {
	.init		= init_ipq807x,
	.get_temp	= get_temp_ipq807x,
	.enable		= enable_ipq807x,
	.disable	= disable_ipq807x,
	.suspend	= suspend_ipq807x,
	.resume		= resume_ipq807x,
};

const struct tsens_data data_ipq807x = {
	.num_sensors	= 16,
	.ops		= &ops_ipq807x,
};
