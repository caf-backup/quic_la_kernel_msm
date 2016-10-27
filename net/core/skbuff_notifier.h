#ifndef SKBUFF_NOTIFIER_H
#define SKBUFF_NOTIFIER_H

#include <linux/notifier.h>
#include <linux/skbuff.h>

/* notifier events */
#define SKB_RECYCLER_NOTIFIER_SUMERR   0x0001
#define SKB_RECYCLER_NOTIFIER_DBLFREE  0x0002
#define SKB_RECYCLER_NOTIFIER_DBLALLOC 0x0004
#define SKB_RECYCLER_NOTIFIER_FSM      0x0008

#if defined(CONFIG_DEBUG_OBJECTS_SKBUFF)
int skb_recycler_notifier_register(struct notifier_block *nb);
int skb_recycler_notifier_unregister(struct notifier_block *nb);
int skb_recycler_notifier_send_event(unsigned long action,
				     struct sk_buff *skb);
#else
static inline int skb_recycler_notifier_register(struct notifier_block *nb)
{
	return 0;
}

static inline int skb_recycler_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}

static inline int skb_recycler_notifier_send_event(unsigned long action,
						   struct sk_buff *skb)
{
	return 1;
}
#endif /* CONFIG_DEBUG_OBJECTS_SKBUFF */

#endif /* SKBUFF_NOTIFIER_H */
