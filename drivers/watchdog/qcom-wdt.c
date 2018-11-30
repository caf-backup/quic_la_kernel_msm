// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/watchdog.h>
#include <linux/of_device.h>
#include <soc/qcom/memory_dump.h>

static struct qcom_wdt *wdata;

#define MAX_CPU_CTX_SIZE	2048
#define MAX_CPU_SCANDUMP_SIZE	0x10100

enum wdt_reg {
	WDT_RST,
	WDT_EN,
	WDT_STS,
	WDT_BARK_TIME,
	WDT_BITE_TIME,
};

static const u32 reg_offset_data_apcs_tmr[] = {
	[WDT_RST] = 0x38,
	[WDT_EN] = 0x40,
	[WDT_STS] = 0x44,
	[WDT_BARK_TIME] = 0x4C,
	[WDT_BITE_TIME] = 0x5C,
};

static const u32 reg_offset_data_kpss[] = {
	[WDT_RST] = 0x4,
	[WDT_EN] = 0x8,
	[WDT_STS] = 0xC,
	[WDT_BARK_TIME] = 0x10,
	[WDT_BITE_TIME] = 0x14,
};

struct qcom_wdt {
	struct watchdog_device	wdd;
	struct device		*dev;
	struct clk		*clk;
	unsigned long		rate;
	void __iomem		*base;
	const u32		*layout;
	cpumask_t		alive_mask;
};

static void __iomem *wdt_addr(struct qcom_wdt *wdt, enum wdt_reg reg)
{
	return wdt->base + wdt->layout[reg];
}

static inline
struct qcom_wdt *to_qcom_wdt(struct watchdog_device *wdd)
{
	return container_of(wdd, struct qcom_wdt, wdd);
}

static void keep_alive_response(void *info)
{
	int cpu = smp_processor_id();
	struct qcom_wdt *wdt = (struct qcom_wdt *)info;

	cpumask_set_cpu(cpu, &wdt->alive_mask);
	/* Make sure alive mask is cleared and set in order */
	smp_mb();
}

/*
 * If this function does not return, it implies one of the
 * other cpu's is not responsive.
 */
static void ping_other_cpus(struct qcom_wdt *wdt)
{
	int cpu;

	cpumask_clear(&wdt->alive_mask);
	/* Make sure alive mask is cleared and set in order */
	smp_mb();
	for_each_online_cpu(cpu) {
		/*if (!cpu_isolated(cpu))*/
			smp_call_function_single(cpu, keep_alive_response,
						 wdt, 1);
	}
}

static int qcom_wdt_start(struct watchdog_device *wdd)
{
	struct qcom_wdt *wdt = to_qcom_wdt(wdd);

	writel(0, wdt_addr(wdt, WDT_EN));
	writel(1, wdt_addr(wdt, WDT_RST));
	writel(wdd->timeout * wdt->rate, wdt_addr(wdt, WDT_BARK_TIME));
	writel(wdd->timeout * wdt->rate, wdt_addr(wdt, WDT_BITE_TIME));
	writel(1, wdt_addr(wdt, WDT_EN));
	return 0;
}

static int qcom_wdt_stop(struct watchdog_device *wdd)
{
	struct qcom_wdt *wdt = to_qcom_wdt(wdd);

	writel(0, wdt_addr(wdt, WDT_EN));
	return 0;
}

static int qcom_wdt_ping(struct watchdog_device *wdd)
{
	struct qcom_wdt *wdt = to_qcom_wdt(wdd);

	/* ping other CPUs here */
	ping_other_cpus(wdt);

	writel(1, wdt_addr(wdt, WDT_RST));
	return 0;
}

static int qcom_wdt_set_timeout(struct watchdog_device *wdd,
				unsigned int timeout)
{
	wdd->timeout = timeout;
	return qcom_wdt_start(wdd);
}

void qcom_wdt_bite(void)
{
	u32 timeout;

	if (!wdata)
		return;

	pr_info("Causing a watchdog bite!");

	/*
	 * Trigger watchdog bite:
	 *    Setup BITE_TIME to be 128ms, and enable WDT.
	 */
	timeout = 128 * wdata->rate / 1000;

	writel(0, wdt_addr(wdata, WDT_EN));
	writel(1, wdt_addr(wdata, WDT_RST));
	writel(timeout, wdt_addr(wdata, WDT_BARK_TIME));
	writel(timeout, wdt_addr(wdata, WDT_BITE_TIME));
	writel(1, wdt_addr(wdata, WDT_EN));

	/*
	 * Actually make sure the above sequence hits hardware before sleeping.
	 */
	wmb();

	msleep(150);
	pr_err("Wdog - STS: 0x%x, CTL: 0x%x, BARK TIME: 0x%x, BITE TIME: 0x%x",
		__raw_readl(wdt_addr(wdata, WDT_STS)),
		__raw_readl(wdt_addr(wdata, WDT_EN)),
		__raw_readl(wdt_addr(wdata, WDT_BARK_TIME)),
		__raw_readl(wdt_addr(wdata, WDT_BITE_TIME)));
}

static int qcom_wdt_restart(struct watchdog_device *wdd, unsigned long action,
			    void *data)
{
	struct qcom_wdt *wdt = to_qcom_wdt(wdd);
	u32 timeout;

	/*
	 * Trigger watchdog bite:
	 *    Setup BITE_TIME to be 128ms, and enable WDT.
	 */
	timeout = 128 * wdt->rate / 1000;

	writel(0, wdt_addr(wdt, WDT_EN));
	writel(1, wdt_addr(wdt, WDT_RST));
	writel(timeout, wdt_addr(wdt, WDT_BARK_TIME));
	writel(timeout, wdt_addr(wdt, WDT_BITE_TIME));
	writel(1, wdt_addr(wdt, WDT_EN));

	/*
	 * Actually make sure the above sequence hits hardware before sleeping.
	 */
	wmb();

	msleep(150);
	return 0;
}

static void qcom_wdt_configure_bark_dump(struct qcom_wdt *wdt)
{
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *cpu_data;
	int cpu;
	void *cpu_buf;

	cpu_data = kzalloc(sizeof(struct msm_dump_data) *
			   num_present_cpus(), GFP_KERNEL);
	if (!cpu_data)
		goto out0;

	cpu_buf = kzalloc(MAX_CPU_CTX_SIZE * num_present_cpus(),
			  GFP_KERNEL);
	if (!cpu_buf)
		goto out1;

	for_each_cpu(cpu, cpu_present_mask) {
		cpu_data[cpu].addr = virt_to_phys(cpu_buf +
						cpu * MAX_CPU_CTX_SIZE);
		cpu_data[cpu].len = MAX_CPU_CTX_SIZE;
		snprintf(cpu_data[cpu].name, sizeof(cpu_data[cpu].name),
			"KCPU_CTX%d", cpu);
		dump_entry.id = MSM_DUMP_DATA_CPU_CTX + cpu;
		dump_entry.addr = virt_to_phys(&cpu_data[cpu]);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
					     &dump_entry);
		/*
		 * Don't free the buffers in case of error since
		 * registration may have succeeded for some cpus.
		 */
		if (ret)
			pr_err("cpu %d reg dump setup failed\n", cpu);
	}

	return;
out1:
	kfree(cpu_data);
out0:
	return;
}

static void qcom_wdt_configure_scandump(struct qcom_wdt *wdt)
{
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *cpu_data;
	int cpu;
	static dma_addr_t dump_addr;
	static void *dump_vaddr;

	for_each_cpu(cpu, cpu_present_mask) {
		cpu_data = devm_kzalloc(wdt->dev,
					sizeof(struct msm_dump_data),
					GFP_KERNEL);
		if (!cpu_data)
			continue;

		dump_vaddr = (void *) dma_alloc_coherent(wdt->dev,
							 MAX_CPU_SCANDUMP_SIZE,
							 &dump_addr,
							 GFP_KERNEL);
		if (!dump_vaddr) {
			dev_err(wdt->dev, "Couldn't get memory for dump\n");
			continue;
		}
		memset(dump_vaddr, 0x0, MAX_CPU_SCANDUMP_SIZE);

		cpu_data->addr = dump_addr;
		cpu_data->len = MAX_CPU_SCANDUMP_SIZE;
		snprintf(cpu_data->name, sizeof(cpu_data->name),
			"KSCANDUMP%d", cpu);
		dump_entry.id = MSM_DUMP_DATA_SCANDUMP_PER_CPU + cpu;
		dump_entry.addr = virt_to_phys(cpu_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS,
					     &dump_entry);
		if (ret) {
			dev_err(wdt->dev, "Dump setup failed, id = %d\n",
				MSM_DUMP_DATA_SCANDUMP_PER_CPU + cpu);
			dma_free_coherent(wdt->dev, MAX_CPU_SCANDUMP_SIZE,
					  dump_vaddr,
					  dump_addr);
			devm_kfree(wdt->dev, cpu_data);
		}
	}
}

static const struct watchdog_ops qcom_wdt_ops = {
	.start		= qcom_wdt_start,
	.stop		= qcom_wdt_stop,
	.ping		= qcom_wdt_ping,
	.set_timeout	= qcom_wdt_set_timeout,
	.restart        = qcom_wdt_restart,
	.owner		= THIS_MODULE,
};

static const struct watchdog_info qcom_wdt_info = {
	.options	= WDIOF_KEEPALIVEPING
			| WDIOF_MAGICCLOSE
			| WDIOF_SETTIMEOUT
			| WDIOF_CARDRESET,
	.identity	= KBUILD_MODNAME,
};

static int qcom_wdt_probe(struct platform_device *pdev)
{
	struct qcom_wdt *wdt;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	const u32 *regs;
	u32 percpu_offset;
	int ret;

	regs = of_device_get_match_data(&pdev->dev);
	if (!regs) {
		dev_err(&pdev->dev, "Unsupported QCOM WDT module\n");
		return -ENODEV;
	}

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	/* We use CPU0's DGT for the watchdog */
	if (of_property_read_u32(np, "cpu-offset", &percpu_offset))
		percpu_offset = 0;

	res->start += percpu_offset;
	res->end += percpu_offset;

	wdt->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	wdt->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(wdt->clk)) {
		dev_err(&pdev->dev, "failed to get input clock\n");
		return PTR_ERR(wdt->clk);
	}

	ret = clk_prepare_enable(wdt->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup clock\n");
		return ret;
	}

	/*
	 * We use the clock rate to calculate the max timeout, so ensure it's
	 * not zero to avoid a divide-by-zero exception.
	 *
	 * WATCHDOG_CORE assumes units of seconds, if the WDT is clocked such
	 * that it would bite before a second elapses it's usefulness is
	 * limited.  Bail if this is the case.
	 */
	wdt->rate = clk_get_rate(wdt->clk);
	if (wdt->rate == 0 ||
	    wdt->rate > 0x10000000U) {
		dev_err(&pdev->dev, "invalid clock rate\n");
		ret = -EINVAL;
		goto err_clk_unprepare;
	}

	wdt->dev = &pdev->dev;

	cpumask_clear(&wdt->alive_mask);
	qcom_wdt_configure_scandump(wdt);
	qcom_wdt_configure_bark_dump(wdt);

	wdt->wdd.info = &qcom_wdt_info;
	wdt->wdd.ops = &qcom_wdt_ops;
	wdt->wdd.min_timeout = 1;
	wdt->wdd.max_timeout = 0x10000000U / wdt->rate;
	wdt->wdd.parent = &pdev->dev;
	wdt->layout = regs;

	if (readl(wdt_addr(wdt, WDT_STS)) & 1)
		wdt->wdd.bootstatus = WDIOF_CARDRESET;

	/*
	 * If 'timeout-sec' unspecified in devicetree, assume a 30 second
	 * default, unless the max timeout is less than 30 seconds, then use
	 * the max instead.
	 */
	wdt->wdd.timeout = min(wdt->wdd.max_timeout, 30U);
	watchdog_init_timeout(&wdt->wdd, 0, &pdev->dev);

	wdata = wdt;

	ret = watchdog_register_device(&wdt->wdd);
	if (ret) {
		dev_err(&pdev->dev, "failed to register watchdog\n");
		goto err_clk_unprepare;
	}

	platform_set_drvdata(pdev, wdt);
	return 0;

err_clk_unprepare:
	clk_disable_unprepare(wdt->clk);
	return ret;
}

static int qcom_wdt_remove(struct platform_device *pdev)
{
	struct qcom_wdt *wdt = platform_get_drvdata(pdev);

	watchdog_unregister_device(&wdt->wdd);
	clk_disable_unprepare(wdt->clk);
	return 0;
}

static const struct of_device_id qcom_wdt_of_table[] = {
	{ .compatible = "qcom,kpss-timer", .data = reg_offset_data_apcs_tmr },
	{ .compatible = "qcom,scss-timer", .data = reg_offset_data_apcs_tmr },
	{ .compatible = "qcom,kpss-wdt", .data = reg_offset_data_kpss },
	{ },
};
MODULE_DEVICE_TABLE(of, qcom_wdt_of_table);

static struct platform_driver qcom_watchdog_driver = {
	.probe	= qcom_wdt_probe,
	.remove	= qcom_wdt_remove,
	.driver	= {
		.name		= KBUILD_MODNAME,
		.of_match_table	= qcom_wdt_of_table,
	},
};
module_platform_driver(qcom_watchdog_driver);

MODULE_DESCRIPTION("QCOM KPSS Watchdog Driver");
MODULE_LICENSE("GPL v2");
