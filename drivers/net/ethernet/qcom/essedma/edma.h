/*
 * Copyright (c) 2014 - 2015, The Linux Foundation. All rights reserved.
 *
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
 */

#ifndef _EDMA_H_
#define _EDMA_H_

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/smp.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysctl.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <asm-generic/bug.h>
#include "ess_edma.h"

#define EDMA_NR_CPU 4
#define EDMA_NR_NETDEV 2
#define EDMA_MAX_RECEIVE_QUEUE 8
#define EDMA_MAX_TRANSMIT_QUEUE 16
#define EDMA_PORT_ID_SHIFT 12
#define EDMA_PORT_ID_MASK 0x7
#define EDMA_WAN 0
#define EDMA_LAN 1
#define EDMA_LAN_DEFAULT 1
#define EDMA_WAN_DEFAULT 2
#define EDMA_PORT_SHIFT 1

#define EDMA_NETDEV_TX_QUEUE 4 /* TX queue exposed to Linux kernel */
#define EDMA_NETDEV_RX_QUEUE 4 /* RX queue exposed to Linux kernel */

#define EDMA_NUM_TXQ_PER_CORE 4
#define EDMA_NUM_RXQ_PER_CORE 2
#define EDMA_TPD_EOP_SHIFT 31

/* tpd word 3 bit 18-28 */
#define EDMA_TPD_PORT_BITMAP_SHIFT 18

#define EDMA_TPD_FROM_CPU_SHIFT 25

/* Enable Tx for all ports */
#define EDMA_PORT_ENABLE_ALL 0x3E

#define EDMA_FROM_CPU_MASK 0x80
#define EDMA_SKB_PRIORITY_MASK 0x38

#define EDMA_RX_RING_SIZE 512
#define EDMA_TX_RING_SIZE 512

#define EDMA_RX_HEAD_BUFF_SIZE_JUMBO 256
#define EDMA_RX_HEAD_BUFF_SIZE 1540

#define EDMA_MAX_JUMBO_FRAME_SIZE 9000

#define EDMA_INTR_CLEAR_TYPE 0
#define EDMA_INTR_SW_IDX_W_TYPE 0
#define EDMA_FIFO_THRESH_TYPE 0
#define EDMA_RSS_TYPE 0
#define EDMA_RX_IMT 0x0160
#define EDMA_TX_IMT 0x0400
#define EDMA_TPD_BURST 5
#define EDMA_TXF_BURST 0x100
#define EDMA_RFD_BURST 8
#define EDMA_RFD_THR 16
#define EDMA_RFD_LTHR 0

#define EDMA_TX_PER_CPU_MASK 0xF
#define EDMA_RX_PER_CPU_MASK 0x3
#define EDMA_TX_PER_CPU_MASK_SHIFT 0x2
#define EDMA_RX_PER_CPU_MASK_SHIFT 0x1
#define EDMA_TX_CPU_START_SHIFT 0x2
#define EDMA_RX_CPU_START_SHIFT 0x1

#define EDMA_HW_CHECKSUM 0x00000001
#define EDMA_VLAN_TX_TAG_INSERT_FLAG 0x00000002
#define EDMA_VLAN_TX_TAG_INSERT_DEFAULT_FLAG 0x00000004

#define EDMA_SW_DESC_FLAG_LAST 0x1
#define EDMA_SW_DESC_FLAG_SKB_HEAD 0x2
#define EDMA_SW_DESC_FLAG_SKB_FRAG 0x4
#define EDMA_SW_DESC_FLAG_SKB_NONE 0x8

#define EDMA_MAX_SKB_FRAGS (MAX_SKB_FRAGS + 1)

/* Ethtool specific list of EDMA supported features */
#define EDMA_SUPPORTED_FEATURES (SUPPORTED_10baseT_Half \
					| SUPPORTED_10baseT_Full \
					| SUPPORTED_100baseT_Half \
					| SUPPORTED_100baseT_Full \
					| SUPPORTED_1000baseT_Full)

#define EDMA_RX_ATH_HDR_VERSION 0x2
#define EDMA_RX_ATH_HDR_VERSION_SHIFT 14
#define EDMA_RX_ATH_HDR_PRIORITY_SHIFT 11
#define EDMA_RX_ATH_PORT_TYPE_SHIFT 6
#define EDMA_RX_ATH_HDR_RSTP_PORT_TYPE 0x4

#define EDMA_ETH_HDR_LEN 12
#define EDMA_ETH_TYPE_MASK 0xFFFF

#define EDMA_TX_ATH_HDR_PORT_BITMAP_MASK 0x7F
#define EDMA_TX_ATH_HDR_FROM_CPU_MASK 0x80
#define EDMA_TX_ATH_HDR_FROM_CPU_SHIFT 7

/* link status variable */
enum edma_link_status {
	LINKDOWN = 0,
	LINKUP = 1,
};

/* edma transmit descriptor */
struct edma_tx_desc {
	__le16  len; /* full packet including CRC */
	__le16  svlan_tag; /* vlan tag */
	__le32  word1; /* byte 4-7 */
	__le32  addr; /* address of buffer */
	__le32  word3; /* byte 12 */
};

/* edma receive return descriptor */
struct edma_rx_return_desc {
	__le32  word0;
	__le32  word1;
	__le32  word2;
	__le32  word3;
};

/* RFD descriptor */
struct edma_rx_free_desc {
	__le32  buffer_addr; /* buffer address */
};

/* edma hw specific data */
struct edma_hw {
	unsigned long  __iomem *hw_addr; /* inner register address */
	struct edma_adapter *adapter; /* netdevice adapter */
	u32 rx_intr_mask; /*rx interrupt mask */
	u32 tx_intr_mask; /* tx interrupt nask */
	u32 misc_intr_mask; /* misc interrupt mask */
	u32 wol_intr_mask; /* wake on lan interrupt mask */
	bool intr_clear_type; /* interrupt clear */
	bool intr_sw_idx_w; /* interrupt software index */
	u32 rx_head_buff_size; /* Rx buffer size */
	u8 rss_type; /* rss protocol type */
};

/* edma_sw_desc stores software descriptor
 * SW descriptor has 1:1 map with HW descriptor
 */
struct edma_sw_desc {
	struct sk_buff *skb;
	struct netdev_queue *nq;
	dma_addr_t dma; /* dma address */
	u16 length; /* Tx/Rx buffer length */
	u32 flags;
};

/* per core queue related information */
struct queue_per_cpu_info {
	struct napi_struct napi; /* napi associated with the core */
	u32 tx_mask; /* tx interrupt mask */
	u32 rx_mask; /* rx interrupt mask */
	u32 tx_status; /* tx interrupt status */
	u32 rx_status; /* rx interrupt status */
	u32 tx_start; /* tx queue start */
	u32 rx_start; /* rx queue start */
	struct edma_common_info *c_info; /* edma common info */
};

/* edma specific common info */
struct edma_common_info {
	struct edma_tx_desc_ring *tpd_ring[16]; /* 16 Tx queues */
	struct edma_rfd_desc_ring *rfd_ring[8]; /* 8 Rx queues */
	struct platform_device *pdev; /* device structure */
	struct net_device *netdev[2]; /* net device */
	int num_rx_queues; /* number of rx queue */
	int num_tx_queues; /* number of tx queue */
	int tx_irq[16]; /* number of tx irq */
	int rx_irq[8]; /* number of rx irq */
	int edma_port_id_wan; /* wan port id */
	int from_cpu; /* from CPU TPD field */
	int dp_bitmap; /* port bitmap */
	u16 tx_ring_count; /* Tx ring count */
	u16 rx_ring_count; /* Rx ring*/
	u16 rx_head_buffer_len; /* rx buffer length */
	u16 rx_page_buffer_len; /* rx buffer length */
	u32 page_mode; /* Jumbo frame supported flag */
	struct edma_hw hw; /* edma hw specific structure */
	struct queue_per_cpu_info q_cinfo[EDMA_NR_CPU]; /* per cpu information */
	spinlock_t int_lock; /* protect interrupt registers access */
};

/* transimit packet descriptor (tpd) ring */
struct edma_tx_desc_ring {
	u8 queue_index; /* queue index */
	u16 size; /* descriptor ring length in bytes */
	u16 count; /* number of descriptors in the ring */
	void *hw_desc; /* descriptor ring virtual address */
	dma_addr_t dma; /* descriptor ring physical address */
	u16 sw_next_to_fill; /* next Tx descriptor to fill */
	u16 sw_next_to_clean; /* next Tx descriptor to clean */
	struct edma_sw_desc *sw_desc; /* buffer associated with ring */
};

/* receive free descriptor (rfd) ring */
struct edma_rfd_desc_ring {
	u8 queue_index; /* queue index */
	u16 size; /* descriptor ring length in bytes */
	u16 count; /* number of descriptors in the ring */
	void *hw_desc; /* descriptor ring virtual address */
	dma_addr_t dma; /* descriptor ring physical address */
	u16 sw_next_to_fill; /* next descriptor to fill */
	u16 sw_next_to_clean; /* next descriptor to clean */
	struct edma_sw_desc *sw_desc; /* buffer associated with ring */
};

/* edma_rfs_flter_node - rfs filter node in hash table */
struct edma_rfs_filter_node {
	struct flow_keys keys;
	u32 flow_id; /* flow_id of filter provided by kernel */
	u16 filter_id; /* filter id of filter returned by adaptor */
	u16 rq_id; /* desired rq index */
	struct hlist_node node; /* edma rfs list node */
};

/* edma_rfs_flow_tbl - rfs flow table */
struct edma_rfs_flow_table {
	u16 max_num_filter; /* Maximum number of filters edma supports */
	int filter_available; /* Number of free filters available */
	u16 hashtoclean; /* hash table index to clean next */
	struct hlist_head hlist_head[EDMA_RFS_FLOW_ENTRIES];
	spinlock_t lock;
	struct timer_list expire_rfs; /* timer function for edma_rps_may_expire_flow */
};

/* EDMA net device structure */
struct edma_adapter {
	struct net_device *netdev; /* netdevice */
	struct platform_device *pdev; /* platform device */
	struct edma_common_info *c_info; /* edma common info */
	struct edma_rfs_flow_table rfs; /* edma rfs flow table */
	struct net_device_stats stats; /* netdev statistics */
	set_rfs_filter_callback_t set_rfs_rule;
	int32_t forced_speed; /* link force speed */
	int32_t forced_duplex; /* link force duplex */
	int32_t link_state; /* phy link state */
	u32 tx_start_offset[EDMA_NR_CPU]; /* tx queue start */
	int default_vlan_tag; /* vlan tag */
};

int edma_alloc_queues_tx(struct edma_common_info *c_info);
int edma_alloc_queues_rx(struct edma_common_info *c_info);
int edma_open(struct net_device *netdev);
int edma_close(struct net_device *netdev);
int edma_alloc_tx_rings(struct edma_common_info *c_info);
int edma_alloc_rx_rings(struct edma_common_info *c_info);
void edma_free_tx_rings(struct edma_common_info *c_info);
void edma_free_rx_rings(struct edma_common_info *c_info);
void edma_free_queues(struct edma_common_info *c_info);
void edma_irq_disable(struct edma_common_info *c_info);
int edma_reset(struct edma_common_info *c_info);
int edma_poll(struct napi_struct *napi, int budget);
netdev_tx_t edma_xmit(struct sk_buff *skb,
		struct net_device *netdev);
int edma_configure(struct edma_common_info *c_info);
void edma_irq_enable(struct edma_common_info *c_info);
void edma_enable_tx_ctrl(struct edma_hw *hw);
void edma_enable_rx_ctrl(struct edma_hw *hw);
void edma_stop_rx_tx(struct edma_hw *hw);
void edma_free_irqs(struct edma_adapter *adapter);
irqreturn_t edma_interrupt(int irq, void *dev);
void edma_write_reg(u16 reg_addr, u32 reg_value);
void edma_read_reg(u16 reg_addr, volatile u32 *reg_value);
struct net_device_stats *edma_get_stats(struct net_device *netdev);
int edma_set_mac_addr(struct net_device *netdev, void *p);
int edma_rx_flow_steer(struct net_device *dev, const struct sk_buff *skb,
		u16 rxq, u32 flow_id);
int edma_register_rfs_filter(struct net_device *netdev,
		set_rfs_filter_callback_t set_filter);
void edma_flow_may_expire(unsigned long data);
void edma_set_ethtool_ops(struct net_device *netdev);
int edma_change_mtu(struct net_device *netdev, int new_mtu);
void edma_set_stp_rstp(bool tag);
void edma_assign_ath_hdr_type(int tag);
#endif /* _EDMA_H_ */
