// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/tick.h>

#include <soc/qcom/rpmh.h>

#define ARCH_TIMER_HZ (19200000)
#define PDC_TIME_VALID_SHIFT	31
#define PDC_TIME_UPPER_MASK	0xFFFFFF
static struct cpumask cpu_pm_state_mask;
static raw_spinlock_t cpu_pm_state_lock;

static struct device *sys_pm_dev;

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

static int setup_pdc_wakeup_timer(bool suspend)
{
	int cpu;
	struct tcs_cmd cmd[2] = { { 0 } };
	ktime_t next_wakeup, cpu_wakeup;
	uint64_t wakeup_cycles = ~0U;

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

	return rpmh_write_pdc_data(sys_pm_dev, cmd, ARRAY_SIZE(cmd));
}

static int sys_pm_notifier(struct notifier_block *b,
			       unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		raw_spin_lock(&cpu_pm_state_lock);
		cpumask_set_cpu(smp_processor_id(), &cpu_pm_state_mask);
		if (cpumask_equal(&cpu_pm_state_mask, cpu_possible_mask)) {
			if (rpmh_ctrlr_idle(sys_pm_dev)) {
				/* Flush the sleep/wake sets */
				rpmh_flush(sys_pm_dev);
				/*
				 * The next wakeup value is converted to ticks
				 * and copied to the Power Domain Controller
				 * that has its own timer, which is in an
				 * always-on power domain. The programming is
				 * done through a separate register on the RSC
				 */
				setup_pdc_wakeup_timer(false);
			} else {
				pr_err("%s:rpmh controller is busy\n",
						__func__);
				raw_spin_unlock(&cpu_pm_state_lock);
				return NOTIFY_BAD;
			}
		}
		raw_spin_unlock(&cpu_pm_state_lock);
		break;
	case CPU_PM_EXIT:
	case CPU_PM_ENTER_FAILED:
		raw_spin_lock(&cpu_pm_state_lock);
		cpumask_clear_cpu(smp_processor_id(), &cpu_pm_state_mask);
		raw_spin_unlock(&cpu_pm_state_lock);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block sys_pm_notifier_block = {
	.notifier_call = sys_pm_notifier,
	.priority = -1, /* Should be last in the order of notifications */
};

static int sys_pm_probe(struct platform_device *pdev)
{
	sys_pm_dev = &pdev->dev;

	if (IS_ERR_OR_NULL(sys_pm_dev)) {
		pr_err("%s fail\n", __func__);
		return PTR_ERR(sys_pm_dev);
	}

	raw_spin_lock_init(&cpu_pm_state_lock);
	cpu_pm_register_notifier(&sys_pm_notifier_block);

	return 0;
}

static int sys_pm_suspend(struct device *dev)
{
	if (rpmh_ctrlr_idle(dev)) {
		/* Flush the sleep/wake sets in RSC controller */
		rpmh_flush(dev);
		setup_pdc_wakeup_timer(true);
		rpmh_notify_suspend(sys_pm_dev, true);
	} else {
		pr_err("%s:rpmh controller is busy\n", __func__);
		return -EBUSY;
	}

	return 0;
}

static int sys_pm_resume(struct device *dev)
{
	rpmh_notify_suspend(sys_pm_dev, false);
	return 0;
}

static const struct dev_pm_ops sys_pm_dev_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sys_pm_suspend, sys_pm_resume)
};

static const struct of_device_id sys_pm_drv_match[] = {
	{ .compatible = "qcom,system-pm", },
	{ }
};

static struct platform_driver sys_pm_driver = {
	.probe = sys_pm_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = sys_pm_drv_match,
		.pm = &sys_pm_dev_pm_ops,
	},
};
builtin_platform_driver(sys_pm_driver);
