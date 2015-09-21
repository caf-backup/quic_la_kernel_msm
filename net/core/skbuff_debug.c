/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include "skbuff_debug.h"

/* skbuff_debugobj_fixup():
 *	Called when an error is detected in the state machine for
 *	the objects
 */
static int skbuff_debugobj_fixup(void *addr, enum debug_obj_state state)
{
	WARN(1, "skbuff_debug: state = %d, skb = 0x%p\n", state, addr);

	return 0;
}

static struct debug_obj_descr skbuff_debug_descr = {
	.name		= "sk_buff_struct",
	.fixup_init	= skbuff_debugobj_fixup,
	.fixup_activate	= skbuff_debugobj_fixup,
	.fixup_destroy	= skbuff_debugobj_fixup,
	.fixup_free	= skbuff_debugobj_fixup,
};

inline void skbuff_debugobj_init_and_activate(struct sk_buff *skb)
{
	debug_object_init(skb, &skbuff_debug_descr);
	debug_object_activate(skb, &skbuff_debug_descr);
}

inline void skbuff_debugobj_activate(struct sk_buff *skb)
{
	debug_object_activate(skb, &skbuff_debug_descr);
}

inline void skbuff_debugobj_deactivate(struct sk_buff *skb)
{
	debug_object_deactivate(skb, &skbuff_debug_descr);
}

inline void skbuff_debugobj_destroy(struct sk_buff *skb)
{
	debug_object_destroy(skb, &skbuff_debug_descr);
}
