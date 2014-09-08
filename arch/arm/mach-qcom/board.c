/* Copyright (c) 2010-2014 The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>

#include <asm/mach/arch.h>
#include <asm/system_misc.h>

static void __iomem *wdt;

#define WDT0_RST	0x38
#define WDT0_EN		0x40
#define WDT0_BARK_TIME	0x4C
#define WDT0_BITE_TIME	0x5C

static void wdt_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	writel_relaxed(1, wdt + WDT0_RST);
	writel_relaxed(5*0x31F3, wdt + WDT0_BARK_TIME);
	writel_relaxed(0x31F3, wdt + WDT0_BITE_TIME);
	writel_relaxed(1, wdt + WDT0_EN);
}

static int wdt_init(void)
{
	wdt = ioremap(0x0200A000, 4096);
	if (!wdt)
		return -EINVAL;

	arm_pm_restart = wdt_restart;
	return 0;
}
late_initcall(wdt_init);

static const char * const qcom_dt_match[] __initconst = {
	"qcom,apq8064",
	"qcom,apq8074-dragonboard",
	"qcom,apq8084",
	"qcom,ipq8062",
	"qcom,ipq8064",
	"qcom,msm8660-surf",
	"qcom,msm8960-cdp",
	NULL
};

DT_MACHINE_START(QCOM_DT, "Qualcomm (Flattened Device Tree)")
	.dt_compat = qcom_dt_match,
MACHINE_END
