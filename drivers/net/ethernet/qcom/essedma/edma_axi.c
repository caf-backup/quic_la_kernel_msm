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

#include "edma.h"
#include "ess_edma.h"
#include <linux/cpu_rmap.h>
#include <linux/of_net.h>

/* Weight round robin and virtual QID mask */
#define EDMA_WRR_VID_SCTL_MASK 0xFFFF

/* Weight round robin and virtual QID shift */
#define EDMA_WRR_VID_SCTL_SHIFT 16

char edma_axi_driver_name[] = "ess_edma";
static const u32 default_msg = NETIF_MSG_DRV | NETIF_MSG_PROBE |
	NETIF_MSG_LINK | NETIF_MSG_TIMER | NETIF_MSG_IFDOWN | NETIF_MSG_IFUP;

static unsigned long edma_hw_addr;

char edma_tx_irq[16][64];
char edma_rx_irq[8][64];
struct net_device *netdev[2];
int edma_default_ltag  __read_mostly = EDMA_LAN_DEFAULT;
int edma_default_wtag  __read_mostly = EDMA_WAN_DEFAULT;
int weight_assigned_to_queues __read_mostly;
int queue_to_virtual_queue __read_mostly;

void edma_write_reg(u16 reg_addr, u32 reg_value)
{
	writel(reg_value, ((void __iomem *)(edma_hw_addr + reg_addr)));
}

void edma_read_reg(u16 reg_addr, volatile u32 *reg_value)
{
	*reg_value = readl((void __iomem *)(edma_hw_addr + reg_addr));
}

static int edma_change_default_lan_vlan(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct edma_adapter *adapter = netdev_priv(netdev[1]);
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (write)
		adapter->default_vlan_tag = edma_default_ltag;

	return ret;
}

static int edma_change_default_wan_vlan(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct edma_adapter *adapter = netdev_priv(netdev[0]);
	int ret;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (write)
		adapter->default_vlan_tag = edma_default_wtag;

	return ret;
}

static int edma_weight_assigned_to_queues(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret, queue_id, weight;
	u32 reg_data, data;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (write) {
		queue_id = weight_assigned_to_queues & EDMA_WRR_VID_SCTL_MASK;
		if (queue_id < 0 || queue_id > 15) {
			pr_err("queue_id not within desired range\n");
			return -EINVAL;
		}

		weight = weight_assigned_to_queues >> EDMA_WRR_VID_SCTL_SHIFT;
		if (weight < 0 || weight > 0xF) {
			pr_err("queue_id not within desired range\n");
			return -EINVAL;
		}

		data = weight << EDMA_WRR_SHIFT(queue_id);
		if (queue_id <= 3) {
			edma_read_reg(REG_WRR_CTRL_Q0_Q3, &reg_data);
			reg_data &= ~(1 << EDMA_WRR_SHIFT(queue_id));
			edma_write_reg(REG_WRR_CTRL_Q0_Q3, data | reg_data);
		} else if (queue_id <= 7) {
			edma_read_reg(REG_WRR_CTRL_Q4_Q7, &reg_data);
			reg_data &= ~(1 << EDMA_WRR_SHIFT(queue_id));
			edma_write_reg(REG_WRR_CTRL_Q4_Q7, data | reg_data);
		} else if (queue_id <= 11) {
			edma_read_reg(REG_WRR_CTRL_Q8_Q11, &reg_data);
			reg_data &= ~(1 << EDMA_WRR_SHIFT(queue_id));
			edma_write_reg(REG_WRR_CTRL_Q8_Q11, data | reg_data);
		} else {
			edma_read_reg(REG_WRR_CTRL_Q12_Q15, &reg_data);
			reg_data &= ~(1 << EDMA_WRR_SHIFT(queue_id));
			edma_write_reg(REG_WRR_CTRL_Q12_Q15, data | reg_data);
		}
	}

	return ret;
}

static int edma_queue_to_virtual_queue_map(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret, queue_id, virtual_qid;
	u32 reg_data, data;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (write) {
		queue_id = queue_to_virtual_queue & EDMA_WRR_VID_SCTL_MASK;
		if (queue_id < 0 || queue_id > 15) {
			pr_err("queue_id not within desired range\n");
			return -EINVAL;
		}

		virtual_qid = queue_to_virtual_queue >> EDMA_WRR_VID_SCTL_SHIFT;
		if (virtual_qid < 0 || virtual_qid > 8) {
			pr_err("queue_id not within desired range\n");
			return -EINVAL;
		}

		data = virtual_qid << VQ_ID_SHIFT(queue_id);
		if (queue_id <= 7) {
			edma_read_reg(REG_VQ_CTRL0, &reg_data);
			reg_data &= ~(1 << VQ_ID_SHIFT(queue_id));
			edma_write_reg(REG_VQ_CTRL0, data | reg_data);
		} else {
			edma_read_reg(REG_VQ_CTRL1, &reg_data);
			reg_data &= ~(1 << VQ_ID_SHIFT(queue_id));
			edma_write_reg(REG_VQ_CTRL1, data | reg_data);
		}
	}

	return ret;
}

static struct ctl_table edma_table[] = {
	{
		.procname       = "default_lan_tag",
		.data           = &edma_default_ltag,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = edma_change_default_lan_vlan
	},
	{
		.procname       = "default_wan_tag",
		.data           = &edma_default_wtag,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = edma_change_default_wan_vlan
	},
	{
		.procname       = "weight_assigned_to_queues",
		.data           = &weight_assigned_to_queues,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = edma_weight_assigned_to_queues
	},
	{
		.procname       = "queue_to_virtual_queue_map",
		.data           = &queue_to_virtual_queue,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = edma_queue_to_virtual_queue_map
	},
	{}
};

/*
 * edma_axi_netdev_ops
 *	Describe the operations supported by registered netdevices
 *
 * static const struct net_device_ops edma_axi_netdev_ops = {
 *	.ndo_open               = edma_open,
 *	.ndo_stop               = edma_close,
 *	.ndo_start_xmit         = edma_xmit_frame,
 *	.ndo_set_mac_address    = edma_set_mac_addr,
 * }
 */
static const struct net_device_ops edma_axi_netdev_ops = {
	.ndo_open               = edma_open,
	.ndo_stop               = edma_close,
	.ndo_start_xmit         = edma_xmit,
	.ndo_set_mac_address    = edma_set_mac_addr,
#ifdef CONFIG_RFS_ACCEL
	.ndo_rx_flow_steer      = edma_rx_flow_steer,
	.ndo_register_rfs_filter = edma_register_rfs_filter,
#endif
	.ndo_get_stats          = edma_get_stats,
	.ndo_change_mtu		= edma_change_mtu,
};

/*
 * edma_axi_probe()
 *	Initialise an adapter identified by a platform_device structure.
 *
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur in the probe.
 */
static int edma_axi_probe(struct platform_device *pdev)
{
	struct edma_common_info *c_info;
	struct edma_hw *hw;
	struct edma_adapter *adapter[2];
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *pnp;
	int i, j, err = 0, ret = 0;

	/* Use to allocate net devices for multiple TX/RX queues */
	netdev[0] = alloc_etherdev_mqs(sizeof(struct edma_adapter),
			EDMA_NETDEV_TX_QUEUE, EDMA_NETDEV_RX_QUEUE);
	if (!netdev[0]) {
		dev_err(&pdev->dev, "net device alloc fails=%p\n", netdev[0]);
		goto err_alloc;
	}

	netdev[1] = alloc_etherdev_mqs(sizeof(struct edma_adapter),
			EDMA_NETDEV_TX_QUEUE, EDMA_NETDEV_RX_QUEUE);
	if (!netdev[1]) {
		dev_err(&pdev->dev, "net device alloc fails=%p\n", netdev[1]);
		goto err_alloc;
	}

	SET_NETDEV_DEV(netdev[0], &pdev->dev);
	platform_set_drvdata(pdev, netdev[0]);
	SET_NETDEV_DEV(netdev[1], &pdev->dev);
	platform_set_drvdata(pdev, netdev[1]);

	c_info = vzalloc(sizeof(struct edma_common_info));
	if (!c_info) {
		err = -ENOMEM;
		goto err_ioremap;
	}

	c_info->pdev = pdev;
	c_info->netdev[0] = netdev[0];
        c_info->netdev[1] = netdev[1];

	/* Fill ring details */
	c_info->num_tx_queues = EDMA_MAX_TRANSMIT_QUEUE;
	c_info->tx_ring_count = EDMA_TX_RING_SIZE;
	c_info->num_rx_queues = EDMA_MAX_RECEIVE_QUEUE;
	c_info->rx_ring_count = EDMA_RX_RING_SIZE;

	hw = &c_info->hw;

	/* Fill HW defaults */
	hw->tx_intr_mask = EDMA_TX_IMR_NORMAL_MASK;
	hw->rx_intr_mask = EDMA_RX_IMR_NORMAL_MASK;

	of_property_read_u32(np, "qcom,page-mode", &c_info->page_mode);
	of_property_read_u32(np, "qcom,rx_head_buf_size", &hw->rx_head_buff_size);
	of_property_read_u32(np, "qcom,port_id_wan", &c_info->edma_port_id_wan);

	if (c_info->page_mode)
		hw->rx_head_buff_size = EDMA_RX_HEAD_BUFF_SIZE_JUMBO;
	else if (!hw->rx_head_buff_size)
		hw->rx_head_buff_size = EDMA_RX_HEAD_BUFF_SIZE;

	hw->misc_intr_mask = 0;
	hw->wol_intr_mask = 0;

	hw->intr_clear_type = EDMA_INTR_CLEAR_TYPE;
	hw->intr_sw_idx_w = EDMA_INTR_SW_IDX_W_TYPE;

	/* configure RSS type to the different protocol that can be
	 * supported
	 */
	hw->rss_type = EDMA_RSS_TYPE_IPV4TCP | EDMA_RSS_TYPE_IPV6_TCP |
		EDMA_RSS_TYPE_IPV4_UDP | EDMA_RSS_TYPE_IPV6UDP |
		EDMA_RSS_TYPE_IPV4 | EDMA_RSS_TYPE_IPV6;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	c_info->hw.hw_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(c_info->hw.hw_addr)) {
		ret = PTR_ERR(c_info->hw.hw_addr);
		goto err_hwaddr;
	}

	edma_hw_addr = (unsigned long)c_info->hw.hw_addr;

	/* Parse tx queue interrupt number from device tree */
	for (i = 0; i < c_info->num_tx_queues; i++)
		c_info->tx_irq[i] = platform_get_irq(pdev, i);

	/* Parse rx queue interrupt number from device tree
	 * Here we are setting j to point to the point where we
	 * left tx interrupt parsing(i.e 16) and run run the loop
	 * from 0 to 7 to parse rx interrupt number.
	 */
	for (i = 0, j = c_info->num_tx_queues; i < c_info->num_rx_queues;
			i++, j++)
		c_info->rx_irq[i] = platform_get_irq(pdev, j);

	c_info->rx_head_buffer_len = c_info->hw.rx_head_buff_size;
	c_info->rx_page_buffer_len = PAGE_SIZE;

	err = edma_alloc_queues_tx(c_info);
	if (err) {
		dev_err(&pdev->dev, "Allocation of TX queue failed\n");
		goto err_tx_qinit;
	}

	err = edma_alloc_queues_rx(c_info);
	if (err) {
		dev_err(&pdev->dev, "Allocation of RX queue failed\n");
		goto err_rx_qinit;
	}

	err = edma_alloc_tx_rings(c_info);
	if (err) {
		dev_err(&pdev->dev, "Allocation of TX resources failed\n");
		goto err_tx_rinit;
	}

	err = edma_alloc_rx_rings(c_info);
	if (err) {
		dev_err(&pdev->dev, "Allocation of RX resources failed\n");
		goto err_rx_rinit;
	}

	for_each_available_child_of_node(np, pnp) {
		const char *mac_addr;
		mac_addr = of_get_mac_address(pnp);
		if (mac_addr) {
			if (!strcmp(pnp->name, "gmac0"))
				memcpy(netdev[EDMA_WAN]->dev_addr, mac_addr,
					ETH_ALEN);
			else
				memcpy(netdev[EDMA_LAN]->dev_addr, mac_addr,
					ETH_ALEN);
		}
	}

	/* Populate the adapter structure register the netdevice */
	for (i = 0; i < EDMA_NR_NETDEV; i++) {
		int j;
		adapter[i] = netdev_priv(netdev[i]);
		adapter[i]->netdev = netdev[i];
		adapter[i]->pdev = pdev;
		for (j = 0; j < EDMA_NR_CPU; j++)
			adapter[i]->tx_start_offset[j] = ((j << EDMA_TX_CPU_START_SHIFT) + (i << 1));
		adapter[i]->c_info = c_info;
		netdev[i]->netdev_ops = &edma_axi_netdev_ops;
		netdev[i]->features = NETIF_F_HW_CSUM | NETIF_F_RXCSUM | NETIF_F_HW_VLAN_CTAG_TX
				| NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_SG |
					NETIF_F_TSO | NETIF_F_TSO6;
		netdev[i]->hw_features = NETIF_F_HW_CSUM | NETIF_F_RXCSUM | NETIF_F_HW_VLAN_CTAG_RX
				| NETIF_F_SG | NETIF_F_TSO | NETIF_F_TSO6;
		netdev[i]->vlan_features = NETIF_F_HW_CSUM | NETIF_F_SG | NETIF_F_TSO | NETIF_F_TSO6;
		netdev[i]->wanted_features = NETIF_F_HW_CSUM | NETIF_F_SG | NETIF_F_TSO | NETIF_F_TSO6;

#ifdef CONFIG_RFS_ACCEL
		netdev[i]->features |=  NETIF_F_RXHASH | NETIF_F_NTUPLE;
		netdev[i]->hw_features |=  NETIF_F_RXHASH | NETIF_F_NTUPLE;
		netdev[i]->vlan_features |= NETIF_F_RXHASH | NETIF_F_NTUPLE;
		netdev[i]->wanted_features |= NETIF_F_RXHASH | NETIF_F_NTUPLE;
#endif
		edma_set_ethtool_ops(netdev[i]);

		/*
	 	 * This just fill in some default MAC address
	 	 */
		if (!is_valid_ether_addr(netdev[i]->dev_addr)) {
			random_ether_addr(netdev[i]->dev_addr);
			pr_info("EDMA using MAC@ - using %02x:%02x:%02x:%02x:%02x:%02x\n",
			*(netdev[i]->dev_addr), *(netdev[i]->dev_addr + 1),
			*(netdev[i]->dev_addr + 2), *(netdev[i]->dev_addr + 3),
			*(netdev[i]->dev_addr + 4), *(netdev[i]->dev_addr + 5));
		}

		err = register_netdev(netdev[i]);
		if (err)
			goto err_register;

		/* carrier off reporting is important to ethtool even BEFORE open */
		netif_carrier_off(netdev[i]);

               /* Allocate reverse irq cpu mapping structure for
		* receive queues
		*/
#ifdef CONFIG_RFS_ACCEL
		netdev[i]->rx_cpu_rmap =
			alloc_irq_cpu_rmap(EDMA_NETDEV_RX_QUEUE);
		if (!netdev[i]->rx_cpu_rmap) {
			err = -ENOMEM;
			goto err_rmap_alloc_fail;
		}
#endif
	}

        register_net_sysctl(&init_net, "net/edma", edma_table);

	/* Set default LAN tag */
	adapter[EDMA_LAN]->default_vlan_tag = EDMA_LAN_DEFAULT;
	/* Set default WAN tag */
        adapter[EDMA_WAN]->default_vlan_tag = EDMA_WAN_DEFAULT;

	/* Disable all 16 Tx and 8 rx irqs */
	edma_irq_disable(c_info);

	err = edma_reset(c_info);
	if (err) {
		err = -EIO;
		goto err_reset;
	}

	/* populate per_core_info, do a napi_Add, request 16 TX irqs,
	 * 8 RX irqs, do a napi enable
	 */
	for (i = 0; i < EDMA_NR_CPU; i++) {
		u16 tx_start;
		u8 rx_start;
		c_info->q_cinfo[i].napi.state = 0;
		netif_napi_add(netdev[0], &c_info->q_cinfo[i].napi,
					edma_poll, 64);
		napi_enable(&c_info->q_cinfo[i].napi);
		c_info->q_cinfo[i].tx_mask = EDMA_TX_PER_CPU_MASK <<
					(i << EDMA_TX_PER_CPU_MASK_SHIFT);
		c_info->q_cinfo[i].rx_mask = EDMA_RX_PER_CPU_MASK <<
					(i << EDMA_RX_PER_CPU_MASK_SHIFT);
		c_info->q_cinfo[i].tx_start = tx_start =
					i << EDMA_TX_CPU_START_SHIFT;
		c_info->q_cinfo[i].rx_start =  rx_start =
					i << EDMA_RX_CPU_START_SHIFT;
		c_info->q_cinfo[i].tx_status = 0;
		c_info->q_cinfo[i].rx_status = 0;
		c_info->q_cinfo[i].c_info = c_info;

		/* Request irq per core */
		for (j = c_info->q_cinfo[i].tx_start; j < (tx_start + 4); j++) {
			sprintf(&edma_tx_irq[j][0], "edma_eth_tx%d", j);
			err = request_irq(c_info->tx_irq[j], edma_interrupt,
				IRQF_DISABLED, &edma_tx_irq[j][0], &c_info->q_cinfo[i]);
		}

		for (j = c_info->q_cinfo[i].rx_start; j < rx_start + 2; j++) {
			sprintf(&edma_rx_irq[j][0], "edma_eth_rx%d", j);
			err = request_irq(c_info->rx_irq[j], edma_interrupt,
				IRQF_DISABLED, &edma_rx_irq[j][0], &c_info->q_cinfo[i]);
		}

#ifdef CONFIG_RFS_ACCEL
		for (j = c_info->q_cinfo[i].rx_start; j < rx_start + 2; j += 2) {
			err = irq_cpu_rmap_add(netdev[0]->rx_cpu_rmap,
				c_info->rx_irq[j]);
			if (err)
				goto err_rmap_add_fail;
		}
#endif
	}

	/* Used to clear interrupt status, allocate rx buffer,
	 * configure edma descriptors registers
	 */
	err = edma_configure(c_info);
	if (err) {
		err = -EIO;
		goto err_configure;
	}

	/* Configure RSS indirection table.
	 * 128 hash will be configured in the following
	 * pattern: hash{0,1,2,3} = {Q0,Q2,Q4,Q6} respectively
	 * and so on
	 */
	for (i = 0; i < EDMA_NUM_IDT; i++)
		edma_write_reg(EDMA_REG_RSS_IDT(i), EDMA_RSS_IDT_VALUE);

	/* Configure load balance mapping table.
	 * 4 table entry will be configured according to the
	 * following pattern: load_balance{0,1,2,3} = {Q0,Q1,Q3,Q4}
	 * respectively.
	 */
	edma_write_reg(REG_LB_RING, EDMA_LB_REG_VALUE);

	/* Enable All 16 tx and 8 rx irq mask */
	edma_irq_enable(c_info);
	edma_enable_tx_ctrl(&c_info->hw);
	edma_enable_rx_ctrl(&c_info->hw);

	return 0;

err_configure:
#ifdef CONFIG_RFS_ACCEL
	for (i = 0; i < EDMA_NR_NETDEV; i++) {
		free_irq_cpu_rmap(adapter[i]->netdev->rx_cpu_rmap);
		adapter[i]->netdev->rx_cpu_rmap = NULL;
	}
#endif
err_rmap_add_fail:
        for (i = 0; i < EDMA_NR_NETDEV; i++)
		edma_free_irqs(adapter[i]);
	for (i = 0; i < EDMA_NR_CPU; i++)
		napi_disable(&c_info->q_cinfo[i].napi);
err_rmap_alloc_fail:
err_reset:
	for (i = 0; i < EDMA_NR_NETDEV; i++)
		unregister_netdev(netdev[i]);
err_register:
	edma_free_rx_rings(c_info);
err_rx_rinit:
	edma_free_tx_rings(c_info);
err_tx_rinit:
	edma_free_queues(c_info);
err_rx_qinit:
err_tx_qinit:
	iounmap(c_info->hw.hw_addr);
err_hwaddr:
	kfree(c_info);
err_ioremap:
	for (i = 0; i < EDMA_NR_NETDEV; i++)
		free_netdev(netdev[i]);
err_alloc:
	return err;
}

/*
 * edma_axi_remove - Device Removal Routine
 *	edma_axi_remove is called by the platform subsystem to alert the driver
 *	that it should release a platform device.
 */
static int edma_axi_remove(struct platform_device *pdev)
{
	struct edma_adapter *adapter = netdev_priv(netdev[0]);
	struct edma_common_info *c_info = adapter->c_info;
	struct edma_hw *hw = &c_info->hw;
	int i;

	edma_stop_rx_tx(hw);
	for (i = 0; i < EDMA_NR_CPU; i++)
		napi_disable(&c_info->q_cinfo[i].napi);
	edma_irq_disable(c_info);
	edma_free_irqs(adapter);
	edma_reset(c_info);
	edma_free_tx_rings(c_info);
	edma_free_rx_rings(c_info);
	edma_free_queues(c_info);
	for (i = 0; i < EDMA_NR_NETDEV; i++) {
		unregister_netdev(netdev[i]);
		free_netdev(netdev[i]);
	}
	return 0;
}

static void edma_axi_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id edma_of_mtable[] = {
	{.compatible = "qcom,ess-edma" },
	{}
};
MODULE_DEVICE_TABLE(of, edma_of_mtable);

static struct platform_driver edma_axi_driver = {
	.driver = {
		.name    = edma_axi_driver_name,
		.owner   = THIS_MODULE,
		.of_match_table = edma_of_mtable,
	},
	.probe    = edma_axi_probe,
	.remove   = edma_axi_remove,
	.shutdown = edma_axi_shutdown,
};

static int __init edma_axi_init_module(void)
{
	int ret;
	pr_info("edma module_init\n");
	ret = platform_driver_register(&edma_axi_driver);
	return ret;
}
module_init(edma_axi_init_module);

static void __exit edma_axi_exit_module(void)
{
	platform_driver_unregister(&edma_axi_driver);
	pr_info("edma module_exit\n");
}
module_exit(edma_axi_exit_module);

MODULE_AUTHOR("Qualcomm Atheros Inc");
MODULE_DESCRIPTION("QCA ESS EDMA driver");
MODULE_LICENSE("GPL");
