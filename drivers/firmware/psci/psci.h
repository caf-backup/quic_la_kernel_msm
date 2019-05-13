/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __PSCI_H
#define __PSCI_H

struct cpuidle_driver;
struct device;
struct device_node;

int psci_set_osi_mode(void);
bool psci_has_osi_support(void);

#ifdef CONFIG_CPU_IDLE
void psci_set_domain_state(u32 state);
int psci_dt_parse_state_node(struct device_node *np, u32 *state);
#ifdef CONFIG_PM_GENERIC_DOMAINS_OF
int psci_dt_init_pm_domains(struct device_node *np);
int psci_dt_pm_domains_parse_states(struct cpuidle_driver *drv,
		struct device_node *cpu_node, u32 *psci_states);
struct device *psci_dt_attach_cpu(int cpu);
#else
static inline int psci_dt_init_pm_domains(struct device_node *np) { return 0; }
static inline int psci_dt_pm_domains_parse_states(struct cpuidle_driver *drv,
		struct device_node *cpu_node, u32 *psci_states) { return 0; }
static inline struct device *psci_dt_attach_cpu(int cpu) { return NULL; }
#endif
#endif

#endif /* __PSCI_H */
