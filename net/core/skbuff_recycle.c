/*
 *      Generic skb recycler
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#include <skbuff_recycle.h>

static DEFINE_PER_CPU(struct sk_buff_head, recycle_list);

inline struct sk_buff *skb_recycler_alloc(struct net_device *dev, unsigned int length) {
	unsigned long flags;
	struct sk_buff_head *h;
	struct sk_buff *skb = NULL;

	if (unlikely(length > SKB_RECYCLE_SIZE)) {
		return NULL;
	}

	h = &get_cpu_var(recycle_list);
	local_irq_save(flags);
	skb = __skb_dequeue(h);
	local_irq_restore(flags);
	put_cpu_var(recycle_list);

	if (likely(skb)) {
		struct skb_shared_info *shinfo;

		/*
		 * We're about to write a large amount to the skb to
		 * zero most of the structure so prefetch the start
		 * of the shinfo region now so it's in the D-cache
		 * before we start to write that.
		 */
		prefetchw(&skb->end);

		zero_struct(skb, offsetof(struct sk_buff, tail));
		skb->data = skb->head;
		skb_reset_tail_pointer(skb);
#ifdef NET_SKBUFF_DATA_USES_OFFSET
		skb->mac_header = ~0U;
#endif
		shinfo = skb_shinfo(skb);
		zero_struct(shinfo, offsetof(struct skb_shared_info, dataref));
		atomic_set(&shinfo->dataref, 1);

		skb_reserve(skb, NET_SKB_PAD);

		if (dev) {
			skb->dev = dev;
		}
	}

	return skb;
}

inline bool skb_recycler_consume(struct sk_buff *skb) {
	unsigned long flags;
	struct sk_buff_head *h;

	/* Can we recycle this skb?  If not, simply return that we cannot */
	if (unlikely(!consume_skb_can_recycle(skb, SKB_RECYCLE_MIN_SIZE,
					      SKB_RECYCLE_MAX_SIZE)))
		return false;

	/*
	 * If we can, then it will be much faster for us to recycle this one
	 * later than to allocate a new one from scratch.
	 */
	preempt_disable();
	h = &__get_cpu_var(recycle_list);
	local_irq_save(flags);
	/* Attempt to enqueue the CPU hot recycle list first */
	if (likely(skb_queue_len(h) < SKB_RECYCLE_MAX_SKBS)) {
		__skb_queue_head(h, skb);
		local_irq_restore(flags);
		preempt_enable();
		return true;
	}

	local_irq_restore(flags);
	preempt_enable();

	return false;
}

static int skb_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *ocpu)
{
	unsigned long oldcpu = (unsigned long)ocpu;

	if (action == CPU_DEAD || action == CPU_DEAD_FROZEN) {
		skb_queue_purge(&per_cpu(recycle_list, oldcpu));
	}

	return NOTIFY_OK;
}


void __init skb_recycler_init() {
	int cpu;

	for_each_possible_cpu(cpu) {
		skb_queue_head_init(&per_cpu(recycle_list, cpu));
	}

	hotcpu_notifier(skb_cpu_callback, 0);
}
