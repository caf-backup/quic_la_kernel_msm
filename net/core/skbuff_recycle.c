/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
 */
/* Generic skb recycler */
#include "skbuff_recycle.h"

static DEFINE_PER_CPU(struct sk_buff_head, recycle_list);
#ifdef CONFIG_SKB_RECYCLER_MULTI_CPU
static DEFINE_PER_CPU(struct sk_buff_head, recycle_spare_list);
static struct global_recycler glob_recycler;
#endif

inline struct sk_buff *skb_recycler_alloc(struct net_device *dev,
					  unsigned int length)
{
	unsigned long flags;
	struct sk_buff_head *h;
	struct sk_buff *skb = NULL;

	if (unlikely(length > SKB_RECYCLE_SIZE))
		return NULL;

	h = &get_cpu_var(recycle_list);
	local_irq_save(flags);
	skb = __skb_dequeue(h);
#ifdef CONFIG_SKB_RECYCLER_MULTI_CPU
	if (unlikely(!skb)) {
		u8 head;

		spin_lock(&glob_recycler.lock);
		/* If global recycle list is not empty, use global buffers */
		head = glob_recycler.head;
		if (unlikely(head == glob_recycler.tail)) {
			spin_unlock(&glob_recycler.lock);
		} else {
			/* Move SKBs from global list to CPU pool */
			skb_queue_splice_init(&glob_recycler.pool[head], h);
			head = (head + 1) & SKB_RECYCLE_MAX_SHARED_POOLS_MASK;
			glob_recycler.head = head;
			spin_unlock(&glob_recycler.lock);
			/* We have refilled the CPU pool - dequeue */
			skb = __skb_dequeue(h);
		}
	}
#endif
	local_irq_restore(flags);
	put_cpu_var(recycle_list);

	if (likely(skb)) {
		struct skb_shared_info *shinfo;

		/* We're about to write a large amount to the skb to
		 * zero most of the structure so prefetch the start
		 * of the shinfo region now so it's in the D-cache
		 * before we start to write that.
		 */
		shinfo = skb_shinfo(skb);
		prefetchw(shinfo);

		zero_struct(skb, offsetof(struct sk_buff, tail));
#ifdef NET_SKBUFF_DATA_USES_OFFSET
		skb->mac_header = ~0U;
#endif
		zero_struct(shinfo, offsetof(struct skb_shared_info, dataref));
		atomic_set(&shinfo->dataref, 1);

		skb->data = skb->head + NET_SKB_PAD;
		skb_reset_tail_pointer(skb);

		if (dev)
			skb->dev = dev;
	}

	return skb;
}

inline bool skb_recycler_consume(struct sk_buff *skb)
{
	unsigned long flags;
	struct sk_buff_head *h;

	/* Can we recycle this skb?  If not, simply return that we cannot */
	if (unlikely(!consume_skb_can_recycle(skb, SKB_RECYCLE_MIN_SIZE,
					      SKB_RECYCLE_MAX_SIZE)))
		return false;

	/* If we can, then it will be much faster for us to recycle this one
	 * later than to allocate a new one from scratch.
	 */
	h = &get_cpu_var(recycle_list);
	local_irq_save(flags);
	/* Attempt to enqueue the CPU hot recycle list first */
	if (likely(skb_queue_len(h) < SKB_RECYCLE_MAX_SKBS)) {
		__skb_queue_head(h, skb);
		local_irq_restore(flags);
		preempt_enable();
		return true;
	}
#ifdef CONFIG_SKB_RECYCLER_MULTI_CPU
	h = this_cpu_ptr(&recycle_spare_list);

	/* The CPU hot recycle list was full; if the spare list is also full,
	 * attempt to move the spare list to the global list for other CPUs to
	 * use.
	 */
	if (unlikely(skb_queue_len(h) >= SKB_RECYCLE_SPARE_MAX_SKBS)) {
		u8 cur_tail, next_tail;

		spin_lock(&glob_recycler.lock);
		cur_tail = glob_recycler.tail;
		next_tail = (cur_tail + 1) & SKB_RECYCLE_MAX_SHARED_POOLS_MASK;
		if (next_tail != glob_recycler.head) {
			struct sk_buff_head *p = &glob_recycler.pool[cur_tail];

			/* Move SKBs from CPU pool to Global pool*/
			skb_queue_splice_init(h, p);

			/* Done with global list init */
			glob_recycler.tail = next_tail;
			spin_unlock(&glob_recycler.lock);

			/* We have now cleared room in the spare;
			 * Initialize and enqueue skb into spare
			 */
			__skb_queue_head(h, skb);

			local_irq_restore(flags);
			preempt_enable();
			return true;
		}
		/* We still have a full spare because the global is also full */
		spin_unlock(&glob_recycler.lock);
	} else {
		/* We have room in the spare list; enqueue to spare list */
		__skb_queue_head(h, skb);
		local_irq_restore(flags);
		preempt_enable();
		return true;
	}
#endif

	local_irq_restore(flags);
	preempt_enable();

	return false;
}

static void skb_recycler_free_skb(struct sk_buff_head *list)
{
	struct sk_buff *skb = NULL;

	while ((skb = skb_dequeue(list)) != NULL) {
		skb_release_data(skb);
		kfree_skbmem(skb);
	}
}

static int skb_cpu_callback(unsigned int ocpu)
{
	unsigned long oldcpu = (unsigned long)ocpu;

	skb_recycler_free_skb(&per_cpu(recycle_list, oldcpu));
#ifdef CONFIG_SKB_RECYCLER_MULTI_CPU
	spin_lock(&glob_recycler.lock);
	skb_recycler_free_skb(&per_cpu(recycle_spare_list, oldcpu));
	spin_unlock(&glob_recycler.lock);
#endif

	return NOTIFY_OK;
}

void __init skb_recycler_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		skb_queue_head_init(&per_cpu(recycle_list, cpu));
	}

#ifdef CONFIG_SKB_RECYCLER_MULTI_CPU
	for_each_possible_cpu(cpu) {
		skb_queue_head_init(&per_cpu(recycle_spare_list, cpu));
	}

	spin_lock_init(&glob_recycler.lock);
	unsigned int i;

	for (i = 0; i < SKB_RECYCLE_MAX_SHARED_POOLS; i++)
		skb_queue_head_init(&glob_recycler.pool[i]);
	glob_recycler.head = 0;
	glob_recycler.tail = 0;
#endif
	cpuhp_setup_state_nocalls(CPUHP_NET_DEV_DEAD, "net/skbuff_recycler:dead:",NULL, skb_cpu_callback);
}
