/*
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_IF_BRIDGE_H
#define _LINUX_IF_BRIDGE_H


#include <linux/netdevice.h>
#include <uapi/linux/if_bridge.h>
#include <linux/bitops.h>

#define BR_HAIRPIN_MODE		BIT(0)
#define BR_BPDU_GUARD		BIT(1)
#define BR_ROOT_BLOCK		BIT(2)
#define BR_MULTICAST_FAST_LEAVE	BIT(3)
#define BR_ADMIN_COST		BIT(4)
#define BR_LEARNING		BIT(5)
#define BR_FLOOD		BIT(6)
#define BR_AUTO_MASK		(BR_FLOOD | BR_LEARNING)
#define BR_PROMISC		BIT(7)
#define BR_PROXYARP		BIT(8)
#define BR_PROXYARP_WIFI	BIT(10)
#define BR_ISOLATE_MODE		BIT(11)

extern void brioctl_set(int (*ioctl_hook)(struct net *, unsigned int, void __user *));
extern struct net_device *br_port_dev_get(struct net_device *dev, unsigned char *addr);
extern void br_refresh_fdb_entry(struct net_device *dev, const char *addr);
extern void br_dev_update_stats(struct net_device *dev, struct rtnl_link_stats64 *nlstats);
extern bool br_fdb_has_entry(struct net_device *dev, const char *addr, __u16 vid);
extern void br_fdb_update_register_notify(struct notifier_block *nb);
extern void br_fdb_update_unregister_notify(struct notifier_block *nb);

typedef int br_should_route_hook_t(struct sk_buff *skb);
extern br_should_route_hook_t __rcu *br_should_route_hook;

#endif
