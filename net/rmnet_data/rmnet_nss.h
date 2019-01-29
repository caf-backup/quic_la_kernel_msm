/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _RMNET_NSS_H_
#define _RMENT_NSS_H_

#include <linux/netdevice.h>
#include <linux/skbuff.h>

struct rmnet_nss_cb {
	int (*nss_create)(struct net_device *dev);
	int (*nss_free)(struct net_device *dev);
	int (*nss_tx)(struct sk_buff *skb);
};

extern struct rmnet_nss_cb *rmnet_nss_callbacks;

#endif
