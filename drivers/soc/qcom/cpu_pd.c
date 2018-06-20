// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/cpuhotplug.h>
#include <linux/ktime.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/slab.h>
#include <linux/tick.h>

#include <soc/qcom/rpmh.h>

#define ARCH_TIMER_HZ (19200000)
#define PDC_TIME_VALID_SHIFT	31
#define PDC_TIME_UPPER_MASK	0xFFFFFF
static struct device *cpu_pd_dev;
static bool suspend;

static uint64_t us_to_ticks(uint64_t time_us)
{
	uint64_t sec, nsec, time_cycles;

	sec = time_us;
	do_div(sec, USEC_PER_SEC);
	nsec = time_us - sec * USEC_PER_SEC;

	if (nsec > 0) {
		nsec = nsec * NSEC_PER_USEC;
		do_div(nsec, NSEC_PER_SEC);
	}

	sec += nsec;

	time_cycles = (u64)sec * ARCH_TIMER_HZ;

	return time_cycles;
}

static int setup_pdc_wakeup_timer(struct device *dev)
{
	int cpu;
	struct tcs_cmd cmd[2] = { { 0 } };
	ktime_t next_wakeup, cpu_wakeup;
	uint64_t wakeup_cycles = ~0ULL;

	if (!suspend) {
		/*
		 * Find the next wakeup for any of the online CPUs
		 */
		next_wakeup = ktime_set(KTIME_SEC_MAX, 0);
		for_each_online_cpu(cpu) {
			cpu_wakeup = tick_nohz_get_next_wakeup(cpu);
			if (ktime_before(cpu_wakeup, next_wakeup))
				next_wakeup = cpu_wakeup;
		}
		wakeup_cycles = us_to_ticks(ktime_to_us(next_wakeup));
	}

	cmd[0].data =  (wakeup_cycles >> 32) & PDC_TIME_UPPER_MASK;
	cmd[0].data |= 1 << PDC_TIME_VALID_SHIFT;
	cmd[1].data = (wakeup_cycles & 0xFFFFFFFF);

	return rpmh_write_pdc_data(dev, cmd, ARRAY_SIZE(cmd));
}

static int cpu_pd_power_off(struct generic_pm_domain *domain)
{
	if (rpmh_ctrlr_idle(cpu_pd_dev)) {
		/* Flush the sleep/wake sets */
		rpmh_flush(cpu_pd_dev);
		/*
		 * The next wakeup value is converted to ticks
		 * and copied to the Power Domain Controller
		 * that has its own timer, which is in an
		 * always-on power domain. The programming is
		 * done through a separate register on the RSC
		 */
		setup_pdc_wakeup_timer(cpu_pd_dev);
	} else {
		pr_debug("rpmh controller is busy\n");
		return -EBUSY;
	}

	return 0;
}

static int cpu_pd_starting(unsigned int cpu)
{
	if (!suspend && of_genpd_attach_cpu(cpu))
		pr_err("%s: genpd_attach_cpu fail\n", __func__);
	return 0;
}

static int cpu_pd_dying(unsigned int cpu)
{
	if (!suspend)
		of_genpd_detach_cpu(cpu);
	return 0;
}

static int cpu_pm_domain_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct generic_pm_domain *cpu_pd;
	int ret = -EINVAL;

	if (!np) {
		dev_err(dev, "device tree node not found\n");
		return -ENODEV;
	}

	if (!of_find_property(np, "#power-domain-cells", NULL)) {
		pr_err("power-domain-cells not found\n");
		return -ENODEV;
	}

	cpu_pd_dev = &pdev->dev;
	if (IS_ERR_OR_NULL(cpu_pd_dev))
		return PTR_ERR(cpu_pd_dev);

	cpu_pd = devm_kzalloc(dev, sizeof(*cpu_pd), GFP_KERNEL);
	if (!cpu_pd)
		return -ENOMEM;

	cpu_pd->name = kasprintf(GFP_KERNEL, "%s", np->name);
	if (!cpu_pd->name)
		goto free_cpu_pd;
	cpu_pd->name = kbasename(cpu_pd->name);
	cpu_pd->power_off = cpu_pd_power_off;
	cpu_pd->flags |= GENPD_FLAG_IRQ_SAFE;

	ret = pm_genpd_init(cpu_pd, NULL, false);
	if (ret)
		goto free_name;

	ret = of_genpd_add_provider_simple(np, cpu_pd);
	if (ret)
		goto remove_pd;

	pr_info("init PM domain %s\n", cpu_pd->name);

	/* Install hotplug callbacks */
	ret = cpuhp_setup_state(CPUHP_AP_QCOM_SYS_PM_DOMAIN_STARTING,
				"AP_QCOM_SYS_PM_DOMAIN_STARTING",
				cpu_pd_starting, cpu_pd_dying);
	if (ret)
		goto remove_hotplug;

	return 0;

remove_hotplug:
	cpuhp_remove_state(CPUHP_AP_QCOM_SYS_PM_DOMAIN_STARTING);

remove_pd:
	pm_genpd_remove(cpu_pd);

free_name:
	kfree(cpu_pd->name);

free_cpu_pd:
	kfree(cpu_pd);
	cpu_pd_dev = NULL;
	pr_err("failed to init PM domain ret=%d %pOF\n", ret, np);
	return ret;
}

static int cpu_pd_suspend(struct device *dev)
{
	suspend = true;
	return 0;
}

static int cpu_pd_resume(struct device *dev)
{
	suspend = false;
	return 0;
}

static const struct dev_pm_ops cpu_pd_dev_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(cpu_pd_suspend, cpu_pd_resume)
};


static const struct of_device_id cpu_pd_drv_match[] = {
	{ .compatible = "qcom,cpu-pm-domain", },
	{ }
};

static struct platform_driver cpu_pm_domain_driver = {
	.probe = cpu_pm_domain_probe,
	.driver	= {
		.name = "cpu_pm_domain",
		.of_match_table = cpu_pd_drv_match,
		.pm = &cpu_pd_dev_pm_ops,
	},
};
builtin_platform_driver(cpu_pm_domain_driver);
