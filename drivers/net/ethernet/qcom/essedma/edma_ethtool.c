/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include "edma.h"

struct edma_ethtool_stats {
	uint8_t stat_string[ETH_GSTRING_LEN];
	uint32_t stat_offset;
};

#define DRVINFO_LEN	32
#define EDMA_STAT(m)	offsetof(struct net_device_stats, m)

/**
 * @brief Array of strings describing statistics
 */
static const struct edma_ethtool_stats edma_gstrings_stats[] = {
	{"rx_packets", EDMA_STAT(rx_packets)},
	{"tx_packets", EDMA_STAT(tx_packets)},
	{"rx_bytes", EDMA_STAT(rx_bytes)},
	{"tx_bytes", EDMA_STAT(tx_bytes)},
};

#define EDMA_STATS_LEN ARRAY_SIZE(edma_gstrings_stats)

/*
 * edma_get_strset_count()
 *	Get strset count
 */
static int32_t edma_get_strset_count(struct net_device *netdev,
			int32_t sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return EDMA_STATS_LEN;
	default:
		netdev_dbg(netdev, "%s: Invalid string set", __func__);
		return -EOPNOTSUPP;
	}
}


/*
 * edma_get_strings()
 *	get stats string
 */
static void edma_get_strings(struct net_device *netdev, uint32_t stringset,
			  uint8_t *data)
{
	uint8_t *p = data;
	uint32_t i;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < EDMA_STATS_LEN; i++) {
			memcpy(p, edma_gstrings_stats[i].stat_string,
				min(ETH_GSTRING_LEN, strlen(edma_gstrings_stats[i].stat_string) + 1));
			p += ETH_GSTRING_LEN;
		}
		break;
	}
}


/*
 * edma_get_ethtool_stats()
 *	Get ethtool statistics
 */
static void edma_get_ethtool_stats(struct net_device *netdev,
		struct ethtool_stats *stats, uint64_t *data)
{
	struct edma_adapter *adapter = netdev_priv(netdev);
	int32_t i;
	uint8_t *p = NULL;

	for (i = 0; i < EDMA_STATS_LEN; i++) {
		p = (uint8_t *)&(adapter->stats) +
			edma_gstrings_stats[i].stat_offset;
		data[i] = *(uint32_t *)p;
	}
}


/**
 * edma_get_drvinfo()
 *	get edma driver info
 */
static void edma_get_drvinfo(struct net_device *dev,
		struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, "ess_edma", DRVINFO_LEN);
	strlcpy(info->bus_info, "axi", ETHTOOL_BUSINFO_LEN);
}

/*
 * edma_nway_reset()
 *	Reset the phy, if available.
 */
static int edma_nway_reset(struct net_device *netdev)
{
	return -EINVAL;
}

/*
 * edma_get_wol()
 *	get wake on lan info
 */
static void edma_get_wol(struct net_device *netdev,
			     struct ethtool_wolinfo *wol)
{
	wol->supported = 0;
	wol->wolopts = 0;
}

/*
 * edma_get_msglevel()
 *	get message level.
 */
static uint32_t edma_get_msglevel(struct net_device *netdev)
{
	return 0;
}

/*
 * edma_get_settings()
 *	Get edma settings
 */
static int32_t edma_get_settings(struct net_device *netdev,
			     struct ethtool_cmd *ecmd)
{

	/* set speed and duplex */
	ecmd->speed = SPEED_1000;
        ecmd->duplex = DUPLEX_FULL;

        /* Populate capabilities advertised by self */
        ecmd->advertising = 0;
	ecmd->autoneg = 0;
	ecmd->port = PORT_TP;
	ecmd->transceiver = XCVR_EXTERNAL;

	return 0;
}

/*
 * edma_set_settings()
 *	Set EDMA settings
 */
static int32_t edma_set_settings(struct net_device *netdev,
		struct ethtool_cmd *ecmd)
{
	return 0;
}

/*
 * edma_set_priv_flags()
 *	Set EDMA private flags
 */
static int32_t edma_set_priv_flags(struct net_device *netdev, u32 flags)
{
	return 0;
}

/*
 * edma_get_priv_flags()
 *	get edma driver flags
 */
static uint32_t edma_get_priv_flags(struct net_device *netdev)
{
	return 0;
}

/**
 * Ethtool operations
 */
struct ethtool_ops edma_ethtool_ops = {
	.get_drvinfo = &edma_get_drvinfo,
	.get_link = &ethtool_op_get_link,
	.get_msglevel = &edma_get_msglevel,
	.nway_reset = &edma_nway_reset,
	.get_wol = &edma_get_wol,
	.get_settings = &edma_get_settings,
	.set_settings = &edma_set_settings,
	.get_strings = &edma_get_strings,
	.get_sset_count = &edma_get_strset_count,
	.get_ethtool_stats = &edma_get_ethtool_stats,
	.get_priv_flags = edma_get_priv_flags,
	.set_priv_flags = edma_set_priv_flags,
};

/*
 * edma_set_ethtool_ops
 *	Set ethtool operations
 */
void edma_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &edma_ethtool_ops);
}
