/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <asm/stacktrace.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/module.h>

#include "skbuff_debug.h"

static int skbuff_debugobj_enabled __read_mostly = 1;

inline u32 skbuff_debugobj_sum(struct sk_buff *skb)
{
	int pos = offsetof(struct sk_buff, free_addr);
	u32 sum = 0;

	while (pos--)
		sum += ((u8 *)skb)[pos];

	return sum;
}

struct skbuff_debugobj_walking {
	int pos;
	void **d;
};

static int skbuff_debugobj_walkstack(struct stackframe *frame, void *p)
{
	struct skbuff_debugobj_walking *w = (struct skbuff_debugobj_walking *)p;
	u32 pc = frame->pc;

	if (w->pos < DEBUG_OBJECTS_SKBUFF_STACKSIZE - 1) {
		w->d[w->pos++] = (void *)pc;
		return 0;
	}

	return -ENOENT;
}

#ifdef CONFIG_ARM
static void skbuff_debugobj_get_stack(void **ret)
{
	struct stackframe frame;
	register unsigned long current_sp asm ("sp");
	struct skbuff_debugobj_walking w = {0, ret};
	void *p = &w;

	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_sp;
	frame.lr = (unsigned long)__builtin_return_address(0);
	frame.pc = (unsigned long)skbuff_debugobj_get_stack;

	walk_stackframe(&frame, skbuff_debugobj_walkstack, p);

	ret[w.pos] = NULL;
}
#else
#error
static void skbuff_debugobj_get_stack(void **ret)
{
	/* not supported */
	ret[0] = 0xdeadbeef;
}
#endif

void skbuff_debugobj_print_stack(void **stack)
{
	int i;

	for (i = 0; stack[i]; i++)
		pr_emerg("\t %pS (0x%p)\n", stack[i], stack[i]);
}

void skbuff_debugobj_print_skb(struct sk_buff *skb)
{
	pr_emerg("skb_debug: current process = %s (pid %i)\n",
			current->comm, current->pid);
	pr_emerg("skbuff_debug: free stack:\n");
	skbuff_debugobj_print_stack(skb->free_addr);
	pr_emerg("skbuff_debug: alloc stack:\n");
	skbuff_debugobj_print_stack(skb->alloc_addr);
}

/*
 * skbuff_debugobj_fixup():
 *      Called when an error is detected in the state machine for
 *	the objects
 */
static int skbuff_debugobj_fixup(void *addr, enum debug_obj_state state)
{
	struct sk_buff *skb = (struct sk_buff *)addr;
	ftrace_dump(DUMP_ALL);
	pr_emerg("skbuff_debug: state = %d, skb = 0x%p sum = %d (%d)\n",
			state, skb, skb->sum, skbuff_debugobj_sum(skb));
	skbuff_debugobj_print_skb(skb);
	BUG();

	return 0;
}

static struct debug_obj_descr skbuff_debug_descr = {
	.name		= "sk_buff_struct",
	.fixup_init	= skbuff_debugobj_fixup,
	.fixup_activate	= skbuff_debugobj_fixup,
	.fixup_destroy	= skbuff_debugobj_fixup,
	.fixup_free	= skbuff_debugobj_fixup,
};

inline void skbuff_debugobj_activate(struct sk_buff *skb)
{
	int ret = 0;

	if (!skbuff_debugobj_enabled)
		return;

	skbuff_debugobj_get_stack(skb->alloc_addr);
	ret = debug_object_activate(skb, &skbuff_debug_descr);
	if (ret)
		goto err_act;

	skbuff_debugobj_sum_validate(skb);

	return;

err_act:
	ftrace_dump(DUMP_ALL);
	pr_emerg("skb_debug: failed to activate err = %d skb = 0x%p sum = %d (%d)\n",
			ret, skb, skb->sum, skbuff_debugobj_sum(skb));
	skbuff_debugobj_print_skb(skb);
	BUG();
}

inline void skbuff_debugobj_init_and_activate(struct sk_buff *skb)
{
	if (!skbuff_debugobj_enabled)
		return;

	/* if we're coming from the slab, the skb->sum might
	 * be invalid anyways */
	skb->sum = skbuff_debugobj_sum(skb);

	debug_object_init(skb, &skbuff_debug_descr);
	skbuff_debugobj_activate(skb);
}

inline void skbuff_debugobj_deactivate(struct sk_buff *skb)
{
	int obj_state;

	if (!skbuff_debugobj_enabled)
		return;

	skb->sum = skbuff_debugobj_sum(skb);

	obj_state = debug_object_get_state(skb);

	if (obj_state == ODEBUG_STATE_ACTIVE) {
		debug_object_deactivate(skb, &skbuff_debug_descr);
		skbuff_debugobj_get_stack(skb->free_addr);
		return;
	}

	ftrace_dump(DUMP_ALL);
	pr_emerg("skbuff_debug: deactivating inactive object skb=0x%p state=%d sum = %d (%d)\n",
			skb, obj_state, skb->sum, skbuff_debugobj_sum(skb));
	skbuff_debugobj_print_skb(skb);
	BUG();
}

inline void skbuff_debugobj_sum_validate(struct sk_buff *skb)
{

	if (!skbuff_debugobj_enabled || skb == NULL)
		return;

	if (skb->sum == skbuff_debugobj_sum(skb))
		return;

	ftrace_dump(DUMP_ALL);
	pr_emerg("skb_debug: skb changed while deactive skb = 0x%p sum = %d (%d)\n",
			skb, skb->sum, skbuff_debugobj_sum(skb));
	skbuff_debugobj_print_skb(skb);
	BUG();
}

inline void skbuff_debugobj_sum_update(struct sk_buff *skb)
{
	if (!skbuff_debugobj_enabled || skb == NULL)
		return;

	skb->sum = skbuff_debugobj_sum(skb);
}

inline void skbuff_debugobj_destroy(struct sk_buff *skb)
{
	if (!skbuff_debugobj_enabled)
		return;

	debug_object_destroy(skb, &skbuff_debug_descr);
}

static int __init disable_object_debug(char *str)
{
	skbuff_debugobj_enabled = 0;

	pr_info("skbuff_debug: debug objects is disabled\n");
	return 0;
}

early_param("no_skbuff_debug_objects", disable_object_debug);
