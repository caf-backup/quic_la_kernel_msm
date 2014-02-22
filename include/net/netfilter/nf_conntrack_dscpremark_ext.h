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

/* DSCP remark conntrack extension APIs. */

#ifndef _NF_CONNTRACK_DSCPREMARK_H
#define _NF_CONNTRACK_DSCPREMARK_H
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>

/*
 * DSCP remark conntrack extension structure.
 */
struct nf_ct_dscpremark_ext {
	bool magic;
	__u8 imask;
	__u8 itag;
	__u8 omask;
	__u8 oval;
};

/*
 * nf_ct_dscpremark_ext_find()
 *	Finds the extension data of the conntrack entry if it exists.
 */
static inline struct nf_ct_dscpremark_ext *nf_ct_dscpremark_ext_find(const struct nf_conn *ct)
{
#ifdef CONFIG_NF_CONNTRACK_DSCPREMARK_EXT
	return nf_ct_ext_find(ct, NF_CT_EXT_DSCPREMARK);
#else
	return NULL;
#endif
}

/*
 * nf_ct_dscpremark_ext_add()
 *	Adds the extension data to the conntrack entry.
 */
static inline struct nf_ct_dscpremark_ext *nf_ct_dscpremark_ext_add(struct nf_conn *ct, gfp_t gfp)
{
#ifdef CONFIG_NF_CONNTRACK_DSCPREMARK_EXT
	struct nf_ct_dscpremark_ext *ncde;

	ncde = nf_ct_ext_add(ct, NF_CT_EXT_DSCPREMARK, gfp);
	if (!ncde)
		return NULL;

	return ncde;
#else
	return NULL;
#endif
};

#ifdef CONFIG_NF_CONNTRACK_DSCPREMARK_EXT
extern int nf_conntrack_dscpremark_ext_init(void);
extern void nf_conntrack_dscpremark_ext_fini(void);
#else
/*
 * nf_conntrack_dscpremark_ext_init()
 */
static inline int nf_conntrack_dscpremark_ext_init(void)
{
	return 0;
}

/*
 * nf_conntrack_dscpremark_ext_fini()
 */
static inline void nf_conntrack_dscpremark_ext_fini(void)
{
	return;
}
#endif /* CONFIG_NF_CONNTRACK_DSCPREMARK_EXT */
#endif /* _NF_CONNTRACK_DSCPREMARK_H */
