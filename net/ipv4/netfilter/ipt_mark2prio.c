/* Copyright (c) 2013, The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/netfilter/x_tables.h>

/* do our -amazing- copy */
static unsigned int
mark2prio(struct sk_buff *skb, const struct xt_action_param *par)
{
	skb->priority = skb->mark;

	return NF_ACCEPT;
}

/* always return 0, because there is no reason we can't
 * copy skb->mark to skb->priority
 */
static int check_mark2prio(const struct xt_tgchk_param *par)
{
	return 0;
}

static struct xt_target mark2prio_target __read_mostly = {
	.name		= "mark2prio",
	.family		= NFPROTO_IPV4,
	.target		= mark2prio,
	.targetsize	= 0,
	.table		= "mangle",
	.hooks		= (1 << NF_INET_POST_ROUTING),
	.checkentry	= check_mark2prio,
	.me		= THIS_MODULE,
};

static int __init mark2prio_init(void)
{
	return xt_register_target(&mark2prio_target);
}

static void __exit mark2prio_exit(void)
{
	xt_unregister_target(&mark2prio_target);
}

module_init(mark2prio_init);
module_exit(mark2prio_exit);
