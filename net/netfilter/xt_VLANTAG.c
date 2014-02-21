/*
 **************************************************************************
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

/* VLAN tagging iptables target kernel module. */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/dsfield.h>

#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_vlantag_ext.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_vlan.h>
#include <linux/if_vlan.h>
#include <linux/netfilter/xt_VLANTAG.h>
#include <linux/../../net/8021q/vlan.h>
#include <linux/../../net/offload/offload.h>

//#define VLANTAG_DEBUG

#if !defined(VLANTAG_DEBUG)
#define DEBUGP(type, args...)
#else
#define DEBUGP(args...) printk(KERN_INFO args);
#endif

/*
 * vlantag_retag_based_on_packet()
 *	Tags the vlan tag field based on the sk_buff.
 */
static bool vlantag_retag_based_on_packet(const struct xt_vlantag_target_info *info, struct sk_buff *skb, bool *drop_flag)
{
	unsigned short tci;

	if (vlan_tx_tag_present(skb)) {
		tci = ntohs(vlan_tx_tag_get(skb));

		if ((tci & info->imask) != info->itag) {
			DEBUGP("xt_VLANTAG: input info doesn't match with the packet\n'");
			return false;
		}

		tci  = (tci & info->omask) | info->oval;
		skb->vlan_tci = htons(tci);
		return true;
	}

	if (skb->protocol == htons(ETH_P_8021Q)){
		struct vlan_ethhdr *veth = (struct vlan_ethhdr *)skb->data;

		if (veth->h_vlan_proto != htons(ETH_P_8021Q)) {
			*drop_flag = true;
			DEBUGP("xt_VLANTAG: Drop the packet, because vlan tag is broken\n");
			return false;
		}

		tci = ntohs(veth->h_vlan_TCI);
		if ((tci & info->imask) != info->itag) {
			DEBUGP("xt_VLANTAG: input info doesn't match with the packet\n'");
			return false;
		}

		tci  = (tci & info->omask) | info->oval;
		veth->h_vlan_TCI = htons(tci);
		return true;
        }

	DEBUGP("No VLAN tag is present\n");
	return false;
}

/*
 * vlantag_retag_based_on_device()
 *	Tags the vlan tag field based on the net_device.
 */
static bool vlantag_retag_based_on_device(const struct xt_vlantag_target_info *info, struct sk_buff *skb)
{
	unsigned short tci = vlan_dev_priv(skb->dev)->vlan_id;
	if ((tci & info->imask) != info->itag) {
		return false;
	}

	tci  = (tci & info->omask) | info->oval;
	vlan_dev_priv(skb->dev)->vlan_id = tci;
	return true;
}

/*
 * vlantag_target()
 *	One of the iptables hooks has a packet for us to analyze, do so.
 */
static bool vlantag_target(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct xt_vlantag_target_info *info = par->targinfo;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct nf_ct_vlantag_ext *ncve;
	bool drop_flag = false;

	if (is_vlan_dev(skb->dev)) {
		if (!vlantag_retag_based_on_device(info, skb)) {
			DEBUGP("xt_VLANTAG: input info doesn't match with the vlan device\n");
			return true;
		}

	} else if (!vlantag_retag_based_on_packet(info, skb, &drop_flag)) {

		if (drop_flag) {
			return false;
		}
		return true;
	}

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct) {
		DEBUGP("xt_VLANTAG: No conntrack connection\n");
		return true;
	}

	ncve = nf_ct_vlantag_ext_find(ct);
	if (!ncve) {
		DEBUGP("xt_VLANTAG: No conntrack extension\n");
		return true;
	}

	ncve->magic = true;
	ncve->imask = info->imask;
	ncve->itag = info->itag;
	ncve->omask = info->omask;
	ncve->oval = info->oval;

	DEBUGP("xt_VLANTAG: target continue\n");
        return true;
}

/*
 * vlantag_target_xt()
 *	iptables netfilter hook function.
 */
static unsigned int vlantag_target_xt(struct sk_buff *skb, const struct xt_action_param *par)
{
	if (vlantag_target(skb, par)) {
		return  XT_CONTINUE;
	}

	return NF_DROP;
}


/*
 * vlantag_target_ebt()
 *	One of the iptables bridge hooks has a packet for us to analyze, do so.
 */
static unsigned int vlantag_target_ebt(struct sk_buff *skb, const struct xt_action_param *par)
{
	DEBUGP("xt_VLANTAG: target ebt packet\n");

	if (vlantag_target(skb, par)) {
		return EBT_CONTINUE;
	}

	return EBT_DROP;
}

/*
 * vlantag_check()
 *	check info set by iptables.
 */
static int vlantag_check(const struct xt_tgchk_param *par)
{
	const struct xt_vlantag_target_info *info = par->targinfo;

	/*
	 * TODO: Do we need to check anything else here?
	 */
	if ((info->imask == 0) || (info->omask == 0)) {
		DEBUGP("Do not allow to set the masks to 0\n");
		return -EDOM;
	}

	/*
	 * By the standard 0x000 and 0xfff VLAN ids are reserved and shouldn't be set.
	 */
	if ((info->oval == 0) || (info->oval & 0x0fff) == 0x0fff) {
		DEBUGP("Reserved VLAN ids (0x000 and 0xfff) cannot be set\n");
		return -EDOM;
	}

	return 0;
}

/*
 * vlantag_get_target_info()
 *	Public API to get the vlan tag masks and values.
 */
bool vlantag_get_target_info(struct nf_conn *ct, u_int16_t *imask, u_int16_t *itag, u_int16_t *omask, u_int16_t *oval)
{
	struct nf_ct_vlantag_ext *ncve;

	ncve = nf_ct_vlantag_ext_find(ct);
	if (!ncve) {
		DEBUGP("No vlan tag extension\n");
		return false;
	}

	if (ncve->magic == false) {
		DEBUGP("vlan tag info is not set\n");
		return false;
	}

	*imask = ncve->imask;
	*itag = ncve->itag;
	*omask = ncve->omask;
	*oval = ncve->oval;

	return true;
}
EXPORT_SYMBOL(vlantag_get_target_info);

static struct xt_target vlantag[] __read_mostly = {
	{
		.name		= "VLANTAG",
		.family		= NFPROTO_UNSPEC,
		.target		= vlantag_target_xt,
		.checkentry	= vlantag_check,
		.targetsize	= sizeof(struct xt_vlantag_target_info),
		.hooks		= ((1 << NF_INET_POST_ROUTING) | (1 << NF_INET_FORWARD)),
		.me		= THIS_MODULE,
	},
	{
		.name		= "VLANTAG",
		.family		= NFPROTO_BRIDGE,
		.target		= vlantag_target_ebt,
		.checkentry	= vlantag_check,
		.targetsize	= sizeof(struct xt_vlantag_target_info),
		.hooks		= ((1 << NF_INET_POST_ROUTING) | (1 << NF_INET_FORWARD)),
		.me		= THIS_MODULE,
	},
};

/*
 * vlantag_tg_init()
 *	Module init function.
 */
static int __init vlantag_tg_init(void)
{
	offload_vlantag_register(vlantag_get_target_info);

	return xt_register_targets(vlantag, ARRAY_SIZE(vlantag));
}

/*
 * vlantag_tg_fini()
 * 	Module exit function.
 */
static void __exit vlantag_tg_fini(void)
{
	offload_vlantag_unregister();

	xt_unregister_targets(vlantag, ARRAY_SIZE(vlantag));
}

module_init(vlantag_tg_init);
module_exit(vlantag_tg_fini);

MODULE_LICENSE("GPL");
