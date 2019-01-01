/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#if 0
#include <soc/qcom/memory_dump.h>
#endif
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/cpumask.h>
#include <uapi/linux/sched/types.h>

#define MODULE_NAME "msm_watchdog"
#define WDT0_RST	0x04
#define WDT0_EN		0x08
#define WDT0_STS	0x0C
#define WDT0_BARK_TIME	0x10
#define WDT0_BITE_TIME	0x14

#define MAX_CPU_CTX_SIZE	2048

static struct msm_watchdog_data *wdog_data;
/*
 * user_pet_enable:
 *	Require userspace to write to a sysfs file every pet_time milliseconds.
 *	Disabled by default on boot.
 */
struct msm_watchdog_data {
	unsigned int __iomem phys_base;
	size_t size;
	void __iomem *base;
	void __iomem *wdog_absent_base;
	struct device *dev;
	unsigned int pet_time;
	unsigned int bark_time;
	unsigned int bark_irq;
	unsigned int bite_irq;
	bool do_ipi_ping;
	bool wakeup_irq_enable;
	unsigned long long last_pet;
	unsigned int min_slack_ticks;
	unsigned long long min_slack_ns;
	void *scm_regsave;
	cpumask_t alive_mask;
	struct mutex disable_lock;
	bool irq_ppi;
	struct msm_watchdog_data __percpu **wdog_cpu_dd;
	struct notifier_block panic_blk;

	bool enabled;
	bool user_pet_enabled;

	struct task_struct *watchdog_task;
	struct timer_list pet_timer;
	wait_queue_head_t pet_complete;

	bool timer_expired;
	bool user_pet_complete;
	unsigned long long timer_fired;
	unsigned long long thread_start;
	unsigned long long ping_start[NR_CPUS];
	unsigned long long ping_end[NR_CPUS];
	unsigned int cpu_scandump_sizes[NR_CPUS];
};

/*
 * On the kernel command line specify
 * watchdog_v2.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0000);

/*
 * Userspace Watchdog Support:
 * Write 1 to the "user_pet_enabled" file to enable hw support for a
 * userspace watchdog.
 * Userspace is required to pet the watchdog by continuing to write 1
 * to this file in the expected interval.
 * Userspace may disable this requirement by writing 0 to this same
 * file.
 */
void msm_trigger_wdog_bite(void)
{
	if (!wdog_data)
		return;
	pr_info("Causing a watchdog bite!");
	__raw_writel(11, wdog_data->base + WDT0_EN);
	__raw_writel(1, wdog_data->base + WDT0_BITE_TIME);
	/* Mke sure bite time is written before we reset */
	mb();
		__raw_writel(1, wdog_data->base + WDT0_RST);
	/* Make sure we wait only after reset */
	mb();
	/* Delay to make sure bite occurs */
	mdelay(10000);
	pr_err("Wdog - STS: 0x%x, CTL: 0x%x, BARK TIME: 0x%x, BITE TIME: 0x%x",
		__raw_readl(wdog_data->base + WDT0_STS),
		__raw_readl(wdog_data->base + WDT0_EN),
		__raw_readl(wdog_data->base + WDT0_BARK_TIME),
		__raw_readl(wdog_data->base + WDT0_BITE_TIME));
}

#if 0
static void configure_bark_dump(struct msm_watchdog_data *wdog_dd)
{
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *cpu_data;
	int cpu;
	void *cpu_buf;

	cpu_data = kcalloc(num_present_cpus(), sizeof(struct msm_dump_data),
								GFP_KERNEL);
	if (!cpu_data)
		goto out0;

	cpu_buf = kcalloc(num_present_cpus(), MAX_CPU_CTX_SIZE, GFP_KERNEL);
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

static void init_watchdog_data(struct msm_watchdog_data *wdog_dd)
{
		configure_bark_dump(wdog_dd);
}
#endif
static const struct of_device_id msm_wdog_match_table[] = {
	{ .compatible = "qcom,msm-watchdog" },
	{}
};

static void dump_pdata(struct msm_watchdog_data *pdata)
{
	dev_dbg(pdata->dev, "wdog bark_time %d", pdata->bark_time);
	dev_dbg(pdata->dev, "wdog pet_time %d", pdata->pet_time);
	dev_dbg(pdata->dev, "wdog perform ipi ping %d", pdata->do_ipi_ping);
	dev_dbg(pdata->dev, "wdog base address is 0x%lx\n", (unsigned long)
								pdata->base);
}

static int msm_wdog_dt_to_pdata(struct platform_device *pdev,
					struct msm_watchdog_data *pdata)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wdt-base");
	if (!res)
		return -ENODEV;
	pdata->size = resource_size(res);
	pdata->phys_base = res->start;
	if (unlikely(!(devm_request_mem_region(&pdev->dev, pdata->phys_base,
					       pdata->size, "msm-watchdog")))) {

		dev_err(&pdev->dev, "%s cannot reserve watchdog region\n",
								__func__);
		return -ENXIO;
	}
	pdata->base  = devm_ioremap(&pdev->dev, pdata->phys_base,
							pdata->size);
	if (!pdata->base) {
		dev_err(&pdev->dev, "%s cannot map wdog register space\n",
				__func__);
		return -ENXIO;
	}

	dump_pdata(pdata);
	return 0;
}

static int msm_watchdog_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_watchdog_data *wdog_dd;

	if (!pdev->dev.of_node || !enable)
		return -ENODEV;
	wdog_dd = kzalloc(sizeof(struct msm_watchdog_data), GFP_KERNEL);
	if (!wdog_dd)
		return -EIO;
	ret = msm_wdog_dt_to_pdata(pdev, wdog_dd);
	if (ret)
		goto err;

	wdog_data = wdog_dd;
	wdog_dd->dev = &pdev->dev;
	platform_set_drvdata(pdev, wdog_dd);
	cpumask_clear(&wdog_dd->alive_mask);
	//init_watchdog_data(wdog_dd);

	return 0;
err:
	kzfree(wdog_dd);
	return ret;
}

static struct platform_driver msm_watchdog_driver = {
	.probe = msm_watchdog_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_wdog_match_table,
	},
};

static int init_watchdog(void)
{
	return platform_driver_register(&msm_watchdog_driver);
}

pure_initcall(init_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_LICENSE("GPL v2");
