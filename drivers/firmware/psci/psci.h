/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __PSCI_H
#define __PSCI_H

struct device_node;

int psci_set_osi_mode(void);
bool psci_has_osi_support(void);

#ifdef CONFIG_CPU_IDLE
void psci_set_domain_state(u32 state);
int psci_dt_parse_state_node(struct device_node *np, u32 *state);
#endif

#endif /* __PSCI_H */
