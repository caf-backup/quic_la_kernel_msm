/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

enum efreq_lvl {
	L0, L1, L2, L3, L_INVALID
};

static struct cpufreq_frequency_table qca_ipq40xx_freq_table[] = {
	{L0, 626000},
	{L1, 500000},
	{L2, 200000},
	{L3, 48000},
	{L_INVALID, CPUFREQ_TABLE_END},
};

static unsigned int transition_latency;
static DEFINE_PER_CPU(struct clk *, cpu_clks);
static struct cpufreq_frequency_table *freq_table;

static int ipq40xx_set_target(struct cpufreq_policy *policy, unsigned int index)
{
	int ret;
	unsigned int old_freq, new_freq;
	long freq_Hz, freq_exact;
	struct clk *cpu_clk;

	cpu_clk = per_cpu(cpu_clks, policy->cpu);
	freq_Hz = clk_round_rate(cpu_clk, freq_table[index].frequency * 1000);
	if (freq_Hz <= 0)
		freq_Hz = freq_table[index].frequency * 1000;

	freq_exact = freq_Hz;
	new_freq = freq_Hz;
	old_freq = clk_get_rate(cpu_clk);

	ret = clk_set_rate(cpu_clk, freq_exact);
	if (ret)
		pr_err("failed to set clock rate: %d\n", ret);

	return ret;
}

static unsigned int ipq40xx_get_target(unsigned int cpu)
{
	struct clk *cpu_clk;

	cpu_clk = per_cpu(cpu_clks, cpu);
	if (cpu_clk)
		return clk_get_rate(cpu_clk) / 1000;

	return 0;
}

static int ipq40xx_cpufreq_init(struct cpufreq_policy *policy)
{
	policy->clk = per_cpu(cpu_clks, policy->cpu);

	if (!freq_table) {
		pr_err("Freq table not initialized.\n");
		return -ENODEV;
	}
	cpufreq_frequency_table_get_attr(freq_table, 0);
	return cpufreq_generic_init(policy, freq_table, transition_latency);
}

static struct cpufreq_driver qca_ipq40xx_cpufreq_driver = {
	.flags = CPUFREQ_STICKY,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = ipq40xx_set_target,
	.get = ipq40xx_get_target,
	.init = ipq40xx_cpufreq_init,
	.name = "ipq40xx_freq",
	.attr = cpufreq_generic_attr,
};

static int __init ipq40xx_cpufreq_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct clk *clk;
	unsigned int cpu;
	struct device *dev;
	int ret;

	for_each_possible_cpu(cpu) {
		dev = get_cpu_device(cpu);
		if (!dev) {
			pr_err("failed to get A7 device\n");
			ret = -ENOENT;
			goto out_put_node;
		}
		per_cpu(cpu_clks, cpu) = clk = devm_clk_get(dev, NULL);
		if (IS_ERR(clk)) {
			pr_err("failed to get clk device\n");
			ret = PTR_ERR(clk);
			goto out_put_node;
		}
	}

	freq_table = qca_ipq40xx_freq_table;

	if (!of_property_read_u32(np, "clock-latency", &transition_latency)) {
		pr_info("%s: Clock latency not found. Defaults...\n"
			, __func__);
		transition_latency = CPUFREQ_ETERNAL;
	}

	ret = cpufreq_register_driver(&qca_ipq40xx_cpufreq_driver);
	if (ret) {
		pr_err("failed register driver: %d\n", ret);
		goto out_put_node;
	}
	return 0;

out_put_node:
	of_node_put(np);
	return ret;
}

static int __exit ipq40xx_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&qca_ipq40xx_cpufreq_driver);
	return 0;
}

static struct of_device_id ipq40xx_match_table[] = {
	{.compatible = "qca,ipq40xx_freq"},
	{},
};

static struct platform_driver qca_ipq40xx_cpufreq_platdrv = {
	.probe		= ipq40xx_cpufreq_probe,
	.remove		= ipq40xx_cpufreq_remove,
	.driver = {
		.name	= "cpufreq-ipq40xx",
		.owner	= THIS_MODULE,
		.of_match_table = ipq40xx_match_table,
	},
};

module_platform_driver(qca_ipq40xx_cpufreq_platdrv);

MODULE_DESCRIPTION("QCA IPQ40XX CPUfreq driver");
