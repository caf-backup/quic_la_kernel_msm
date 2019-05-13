// SPDX-License-Identifier: GPL-2.0
/*
 * PM domains for CPUs via genpd - managed by PSCI.
 *
 * Copyright (C) 2019 Linaro Ltd.
 * Author: Ulf Hansson <ulf.hansson@linaro.org>
 *
 */

#define pr_fmt(fmt) "psci: " fmt

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/pm_domain.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>

#include <asm/cpuidle.h>

#include "psci.h"

#if defined(CONFIG_CPU_IDLE) && defined(CONFIG_PM_GENERIC_DOMAINS_OF)

struct psci_pd_provider {
	struct list_head link;
	struct device_node *node;
};

static LIST_HEAD(psci_pd_providers);
static bool osi_mode_enabled;

static int psci_pd_power_off(struct generic_pm_domain *pd)
{
	struct genpd_power_state *state = &pd->states[pd->state_idx];
	u32 *pd_state;

	/* If we have failed to enable OSI mode, then abort power off. */
	if (psci_has_osi_support() && !osi_mode_enabled)
		return -EBUSY;

	if (!state->data)
		return 0;

	/* When OSI mode is enabled, set the corresponding domain state. */
	pd_state = state->data;
	psci_set_domain_state(*pd_state);

	return 0;
}

static int psci_pd_parse_state_nodes(struct genpd_power_state *states,
				int state_count)
{
	int i, ret;
	u32 psci_state, *psci_state_buf;

	for (i = 0; i < state_count; i++) {
		ret = psci_dt_parse_state_node(to_of_node(states[i].fwnode),
					&psci_state);
		if (ret)
			goto free_state;

		psci_state_buf = kmalloc(sizeof(u32), GFP_KERNEL);
		if (!psci_state_buf) {
			ret = -ENOMEM;
			goto free_state;
		}
		*psci_state_buf = psci_state;
		states[i].data = psci_state_buf;
	}

	return 0;

free_state:
	i--;
	for (; i >= 0; i--)
		kfree(states[i].data);
	return ret;
}

static int psci_pd_parse_states(struct device_node *np,
			struct genpd_power_state **states, int *state_count)
{
	int ret;

	/* Parse the domain idle states. */
	ret = of_genpd_parse_idle_states(np, states, state_count);
	if (ret)
		return ret;

	/* Fill out the PSCI specifics for each found state. */
	ret = psci_pd_parse_state_nodes(*states, *state_count);
	if (ret)
		kfree(*states);

	return ret;
}

static void psci_pd_free_states(struct genpd_power_state *states,
				unsigned int state_count)
{
	int i;

	for (i = 0; i < state_count; i++)
		kfree(states[i].data);
	kfree(states);
}

static int psci_pd_enter_pc(struct cpuidle_device *dev,
			struct cpuidle_driver *drv, int idx)
{
	return CPU_PM_CPU_IDLE_ENTER(arm_cpuidle_suspend, idx);
}

static int psci_pd_enter_s2idle_pc(struct cpuidle_device *dev,
			struct cpuidle_driver *drv, int idx)
{
	return psci_pd_enter_pc(dev, drv, idx);
}

static void psci_pd_convert_states(struct cpuidle_state *idle_state,
			u32 *psci_state, struct genpd_power_state *state)
{
	u32 *state_data = state->data;
	u64 target_residency_us = state->residency_ns;
	u64 exit_latency_us = state->power_on_latency_ns +
			state->power_off_latency_ns;

	*psci_state = *state_data;
	do_div(target_residency_us, 1000);
	idle_state->target_residency = target_residency_us;
	do_div(exit_latency_us, 1000);
	idle_state->exit_latency = exit_latency_us;
	idle_state->enter = &psci_pd_enter_pc;
	idle_state->enter_s2idle = &psci_pd_enter_s2idle_pc;
	idle_state->flags |= CPUIDLE_FLAG_TIMER_STOP;

	strlcpy(idle_state->name, to_of_node(state->fwnode)->name,
		CPUIDLE_NAME_LEN - 1);
	strlcpy(idle_state->desc, to_of_node(state->fwnode)->name,
		CPUIDLE_NAME_LEN - 1);
}

static bool psci_pd_is_provider(struct device_node *np)
{
	struct psci_pd_provider *pd_prov, *it;

	list_for_each_entry_safe(pd_prov, it, &psci_pd_providers, link) {
		if (pd_prov->node == np)
			return true;
	}

	return false;
}

static int psci_pd_init(struct device_node *np)
{
	struct generic_pm_domain *pd;
	struct psci_pd_provider *pd_provider;
	struct dev_power_governor *pd_gov;
	struct genpd_power_state *states = NULL;
	int ret = -ENOMEM, state_count = 0;

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		goto out;

	pd_provider = kzalloc(sizeof(*pd_provider), GFP_KERNEL);
	if (!pd_provider)
		goto free_pd;

	pd->name = kasprintf(GFP_KERNEL, "%pOF", np);
	if (!pd->name)
		goto free_pd_prov;

	/*
	 * For OSI mode, parse the domain idle states and let genpd manage the
	 * state selection for those being compatible with "domain-idle-state".
	 */
	if (psci_has_osi_support()) {
		ret = psci_pd_parse_states(np, &states, &state_count);
		if (ret)
			goto free_name;
		pd->free_states = psci_pd_free_states;
	}

	pd->name = kbasename(pd->name);
	pd->power_off = psci_pd_power_off;
	pd->states = states;
	pd->state_count = state_count;
	pd->flags |= GENPD_FLAG_IRQ_SAFE | GENPD_FLAG_CPU_DOMAIN;

	/* Use governor for CPU PM domains if it has some states to manage. */
	pd_gov = state_count > 0 ? &pm_domain_cpu_gov : NULL;

	ret = pm_genpd_init(pd, pd_gov, false);
	if (ret) {
		psci_pd_free_states(states, state_count);
		goto free_name;
	}

	ret = of_genpd_add_provider_simple(np, pd);
	if (ret)
		goto remove_pd;

	pd_provider->node = of_node_get(np);
	list_add(&pd_provider->link, &psci_pd_providers);

	pr_debug("init PM domain %s\n", pd->name);
	return 0;

remove_pd:
	pm_genpd_remove(pd);
free_name:
	kfree(pd->name);
free_pd_prov:
	kfree(pd_provider);
free_pd:
	kfree(pd);
out:
	pr_err("failed to init PM domain ret=%d %pOF\n", ret, np);
	return ret;
}

static void psci_pd_remove(void)
{
	struct psci_pd_provider *pd_provider, *it;
	struct generic_pm_domain *genpd;

	list_for_each_entry_safe(pd_provider, it, &psci_pd_providers, link) {
		of_genpd_del_provider(pd_provider->node);

		genpd = of_genpd_remove_last(pd_provider->node);
		if (!IS_ERR(genpd))
			kfree(genpd);

		of_node_put(pd_provider->node);
		list_del(&pd_provider->link);
		kfree(pd_provider);
	}
}

static int psci_pd_init_topology(struct device_node *np)
{
	struct device_node *node;
	struct of_phandle_args child, parent;
	int ret;

	for_each_child_of_node(np, node) {
		if (of_parse_phandle_with_args(node, "power-domains",
					"#power-domain-cells", 0, &parent))
			continue;

		child.np = node;
		child.args_count = 0;

		ret = of_genpd_add_subdomain(&parent, &child);
		of_node_put(parent.np);
		if (ret) {
			of_node_put(node);
			return ret;
		}
	}

	return 0;
}

int psci_dt_init_pm_domains(struct device_node *np)
{
	struct device_node *node;
	int ret, pd_count = 0;

	/*
	 * Parse child nodes for the "#power-domain-cells" property and
	 * initialize a genpd/genpd-of-provider pair when it's found.
	 */
	for_each_child_of_node(np, node) {
		if (!of_find_property(node, "#power-domain-cells", NULL))
			continue;

		ret = psci_pd_init(node);
		if (ret)
			goto put_node;

		pd_count++;
	}

	/* Bail out if not using the hierarchical CPU topology. */
	if (!pd_count)
		return 0;

	/* Link genpd masters/subdomains to model the CPU topology. */
	ret = psci_pd_init_topology(np);
	if (ret)
		goto remove_pd;

	/* Try to enable OSI mode if supported. */
	if (psci_has_osi_support()) {
		ret = psci_set_osi_mode();
		if (ret)
			pr_warn("failed to enable OSI mode: %d\n", ret);
		else
			osi_mode_enabled = true;
	}

	pr_info("Initialized CPU PM domain topology\n");
	return pd_count;

put_node:
	of_node_put(node);
remove_pd:
	if (pd_count)
		psci_pd_remove();
	pr_err("failed to create CPU PM domains ret=%d\n", ret);
	return ret;
}

int psci_dt_pm_domains_parse_states(struct cpuidle_driver *drv,
			struct device_node *cpu_node, u32 *psci_states)
{
	struct genpd_power_state *pd_states;
	struct of_phandle_args args;
	int ret, pd_state_count, i, state_idx, psci_idx;
	u32 cpu_psci_state = psci_states[drv->state_count - 2];
	struct device_node *np = of_node_get(cpu_node);


	/* Walk the CPU topology to find compatible domain idle states. */
	while (np) {
		ret = of_parse_phandle_with_args(np, "power-domains",
					"#power-domain-cells", 0, &args);
		of_node_put(np);
		if (ret)
			return 0;

		np = args.np;

		/* Verify that the node represents a psci pd provider. */
		if (!psci_pd_is_provider(np)) {
			of_node_put(np);
			return 0;
		}

		/* Parse for compatible domain idle states. */
		ret = psci_pd_parse_states(np, &pd_states, &pd_state_count);
		if (ret) {
			of_node_put(np);
			return ret;
		}

		i = 0;
		while (i < pd_state_count) {

			state_idx = drv->state_count;
			if (state_idx >= CPUIDLE_STATE_MAX) {
				pr_warn("exceeding max cpuidle states\n");
				of_node_put(np);
				return 0;
			}

			/* WFI state is not part of psci_states. */
			psci_idx = state_idx - 1 + i;
			psci_pd_convert_states(&drv->states[state_idx + i],
					&psci_states[psci_idx], &pd_states[i]);

			/*
			 * In the hierarchical CPU topology the master PM domain
			 * idle state's DT property, "arm,psci-suspend-param",
			 * don't contain the bits for the idle state of the CPU,
			 * let's add those here.
			 */
			psci_states[psci_idx] |= cpu_psci_state;
			pr_debug("psci-power-state %#x index %d\n",
				psci_states[psci_idx], psci_idx);

			drv->state_count++;
			i++;
		}
		psci_pd_free_states(pd_states, pd_state_count);
	}

	return 0;
}
#endif
