/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <linux/reset-controller.h>
#include <dt-bindings/clock/qcom,gcc-ipq6018.h>

#include "common.h"
#include "clk-regmap.h"
#include "reset.h"

static int clk_dummy_is_enabled(struct clk_hw *hw)
{
	return 1;
};

static int clk_dummy_enable(struct clk_hw *hw)
{
	return 0;
};

static void clk_dummy_disable(struct clk_hw *hw)
{
	return;
};

static u8 clk_dummy_get_parent(struct clk_hw *hw)
{
	return 0;
};

static int clk_dummy_set_parent(struct clk_hw *hw, u8 index)
{
	return 0;
};

static int clk_dummy_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	return 0;
};

static int clk_dummy_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	return 0;
};

static unsigned long clk_dummy_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	return parent_rate;
};

static const struct clk_ops clk_dummy_ops = {
	.is_enabled = clk_dummy_is_enabled,
	.enable = clk_dummy_enable,
	.disable = clk_dummy_disable,
	.get_parent = clk_dummy_get_parent,
	.set_parent = clk_dummy_set_parent,
	.set_rate = clk_dummy_set_rate,
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_determine_rate,
};

#define DEFINE_DUMMY_CLK(clk_name)				\
(&(struct clk_regmap) {						\
	.hw.init = &(struct clk_init_data){			\
		.name = #clk_name,				\
		.parent_names = (const char *[]){ "xo"},	\
		.num_parents = 1,				\
		.ops = &clk_dummy_ops,				\
	},							\
})

static struct clk_regmap *gcc_ipq6018_clks[] = {
	[GPLL0] = DEFINE_DUMMY_CLK(gpll0),
	[UBI32_PLL] = DEFINE_DUMMY_CLK(ubi32_pll),
	[GPLL6] = DEFINE_DUMMY_CLK(gpll6),
	[GPLL4] = DEFINE_DUMMY_CLK(gpll4),
	[PCNOC_BFDCD_CLK_SRC] = DEFINE_DUMMY_CLK(pcnoc_bfdcd_clk_src),
	[GPLL2] = DEFINE_DUMMY_CLK(gpll2),
	[NSS_CRYPTO_PLL] = DEFINE_DUMMY_CLK(nss_crypto_pll),
	[NSS_PPE_CLK_SRC] = DEFINE_DUMMY_CLK(nss_ppe_clk_src),
	[GCC_XO_CLK_SRC] = DEFINE_DUMMY_CLK(gcc_xo_clk_src),
	[NSS_CE_CLK_SRC] = DEFINE_DUMMY_CLK(nss_ce_clk_src),
	[GCC_SLEEP_CLK_SRC] = DEFINE_DUMMY_CLK(gcc_sleep_clk_src),
	[APSS_AHB_CLK_SRC] = DEFINE_DUMMY_CLK(apss_ahb_clk_src),
	[NSS_PORT5_RX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port5_rx_clk_src),
	[NSS_PORT5_TX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port5_tx_clk_src),
	[USB0_MASTER_CLK_SRC] = DEFINE_DUMMY_CLK(usb0_master_clk_src),
	[USB1_MASTER_CLK_SRC] = DEFINE_DUMMY_CLK(usb1_master_clk_src),
	[APSS_AHB_POSTDIV_CLK_SRC] = DEFINE_DUMMY_CLK(apss_ahb_postdiv_clk_src),
	[NSS_PORT1_RX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port1_rx_clk_src),
	[NSS_PORT1_TX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port1_tx_clk_src),
	[NSS_PORT2_RX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port2_rx_clk_src),
	[NSS_PORT2_TX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port2_tx_clk_src),
	[NSS_PORT3_RX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port3_rx_clk_src),
	[NSS_PORT3_TX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port3_tx_clk_src),
	[NSS_PORT4_RX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port4_rx_clk_src),
	[NSS_PORT4_TX_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port4_tx_clk_src),
	[NSS_PORT5_RX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port5_rx_div_clk_src),
	[NSS_PORT5_TX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port5_tx_div_clk_src),
	[APSS_AXI_CLK_SRC] = DEFINE_DUMMY_CLK(apss_axi_clk_src),
	[NSS_CRYPTO_CLK_SRC] = DEFINE_DUMMY_CLK(nss_crypto_clk_src),
	[NSS_PORT1_RX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port1_rx_div_clk_src),
	[NSS_PORT1_TX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port1_tx_div_clk_src),
	[NSS_PORT2_RX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port2_rx_div_clk_src),
	[NSS_PORT2_TX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port2_tx_div_clk_src),
	[NSS_PORT3_RX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port3_rx_div_clk_src),
	[NSS_PORT3_TX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port3_tx_div_clk_src),
	[NSS_PORT4_RX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port4_rx_div_clk_src),
	[NSS_PORT4_TX_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_port4_tx_div_clk_src),
	[NSS_UBI0_CLK_SRC] = DEFINE_DUMMY_CLK(nss_ubi0_clk_src),
	[UBI_MPT_CLK_SRC] = DEFINE_DUMMY_CLK(ubi_mpt_clk_src),
	[BLSP1_QUP1_I2C_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup1_i2c_apps_clk_src),
	[BLSP1_QUP1_SPI_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup1_spi_apps_clk_src),
	[BLSP1_QUP2_I2C_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup2_i2c_apps_clk_src),
	[BLSP1_QUP2_SPI_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup2_spi_apps_clk_src),
	[BLSP1_QUP3_I2C_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup3_i2c_apps_clk_src),
	[BLSP1_QUP3_SPI_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup3_spi_apps_clk_src),
	[BLSP1_QUP4_I2C_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup4_i2c_apps_clk_src),
	[BLSP1_QUP4_SPI_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup4_spi_apps_clk_src),
	[BLSP1_QUP5_I2C_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup5_i2c_apps_clk_src),
	[BLSP1_QUP5_SPI_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup5_spi_apps_clk_src),
	[BLSP1_QUP6_I2C_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup6_i2c_apps_clk_src),
	[BLSP1_QUP6_SPI_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_qup6_spi_apps_clk_src),
	[BLSP1_UART1_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_uart1_apps_clk_src),
	[BLSP1_UART2_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_uart2_apps_clk_src),
	[BLSP1_UART3_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_uart3_apps_clk_src),
	[BLSP1_UART4_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_uart4_apps_clk_src),
	[BLSP1_UART5_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_uart5_apps_clk_src),
	[BLSP1_UART6_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(blsp1_uart6_apps_clk_src),
	[CRYPTO_CLK_SRC] = DEFINE_DUMMY_CLK(crypto_clk_src),
	[NSS_UBI0_DIV_CLK_SRC] = DEFINE_DUMMY_CLK(nss_ubi0_div_clk_src),
	[SDCC1_APPS_CLK_SRC] = DEFINE_DUMMY_CLK(sdcc1_apps_clk_src),
	[USB0_AUX_CLK_SRC] = DEFINE_DUMMY_CLK(usb0_aux_clk_src),
	[USB0_MOCK_UTMI_CLK_SRC] = DEFINE_DUMMY_CLK(usb0_mock_utmi_clk_src),
	[USB0_PIPE_CLK_SRC] = DEFINE_DUMMY_CLK(usb0_pipe_clk_src),
	[USB1_AUX_CLK_SRC] = DEFINE_DUMMY_CLK(usb1_aux_clk_src),
	[USB1_MOCK_UTMI_CLK_SRC] = DEFINE_DUMMY_CLK(usb1_mock_utmi_clk_src),
	[USB1_PIPE_CLK_SRC] = DEFINE_DUMMY_CLK(usb1_pipe_clk_src),
	[GP1_CLK_SRC] = DEFINE_DUMMY_CLK(gp1_clk_src),
	[GP2_CLK_SRC] = DEFINE_DUMMY_CLK(gp2_clk_src),
	[GP3_CLK_SRC] = DEFINE_DUMMY_CLK(gp3_clk_src),
	[SYSTEM_NOC_BFDCD_CLK_SRC] = DEFINE_DUMMY_CLK(system_noc_bfdcd_clk_src),
	[QDSS_TSCTR_CLK_SRC] = DEFINE_DUMMY_CLK(qdss_tsctr_clk_src),
	[QDSS_AT_CLK_SRC] = DEFINE_DUMMY_CLK(qdss_at_clk_src),
	[SDCC1_ICE_CORE_CLK_SRC] = DEFINE_DUMMY_CLK(sdcc1_ice_core_clk_src),
	[USB1_MOCK_UTMI_CLK_SRC] = DEFINE_DUMMY_CLK(usb1_mock_utmi_clk_src),
	[PCIE0_AXI_CLK_SRC] = DEFINE_DUMMY_CLK(pcie0_axi_clk_src),
	[PCIE0_AUX_CLK_SRC] = DEFINE_DUMMY_CLK(pcie0_aux_clk_src),
	[PCIE0_PIPE_CLK_SRC] = DEFINE_DUMMY_CLK(pcie0_pipe_clk_src),
	[PCIE0_RCHNG_CLK_SRC] = DEFINE_DUMMY_CLK(pcie0_rchng_clk_src),
	[UBI32_MEM_NOC_BFDCD_CLK_SRC] = DEFINE_DUMMY_CLK(ubi32_mem_noc_bfdcd_clk_src),
	[GCC_APSS_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_apss_ahb_clk),
	[GCC_APSS_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_apss_axi_clk),
	[GCC_BLSP1_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_ahb_clk),
	[GCC_BLSP1_QUP1_I2C_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup1_i2c_apps_clk),
	[GCC_BLSP1_QUP1_SPI_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup1_spi_apps_clk),
	[GCC_BLSP1_QUP2_I2C_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup2_i2c_apps_clk),
	[GCC_BLSP1_QUP2_SPI_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup2_spi_apps_clk),
	[GCC_BLSP1_QUP3_I2C_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup3_i2c_apps_clk),
	[GCC_BLSP1_QUP3_SPI_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup3_spi_apps_clk),
	[GCC_BLSP1_QUP4_I2C_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup4_i2c_apps_clk),
	[GCC_BLSP1_QUP4_SPI_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup4_spi_apps_clk),
	[GCC_BLSP1_QUP5_I2C_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup5_i2c_apps_clk),
	[GCC_BLSP1_QUP5_SPI_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup5_spi_apps_clk),
	[GCC_BLSP1_QUP6_I2C_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup6_i2c_apps_clk),
	[GCC_BLSP1_QUP6_SPI_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_qup6_spi_apps_clk),
	[GCC_BLSP1_UART1_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_uart1_apps_clk),
	[GCC_BLSP1_UART2_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_uart2_apps_clk),
	[GCC_BLSP1_UART3_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_uart3_apps_clk),
	[GCC_BLSP1_UART4_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_uart4_apps_clk),
	[GCC_BLSP1_UART5_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_uart5_apps_clk),
	[GCC_BLSP1_UART6_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_blsp1_uart6_apps_clk),
	[GCC_CRYPTO_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_crypto_ahb_clk),
	[GCC_CRYPTO_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_crypto_axi_clk),
	[GCC_CRYPTO_CLK] = DEFINE_DUMMY_CLK(gcc_crypto_clk),
	[GCC_MEM_NOC_NSS_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_mem_noc_nss_axi_clk),
	[GCC_XO_CLK] = DEFINE_DUMMY_CLK(gcc_xo_clk),
	[GCC_XO_DIV4_CLK] = DEFINE_DUMMY_CLK(gcc_xo_div4_clk),
	[GCC_MDIO_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_mdio_ahb_clk),
	[GCC_CRYPTO_PPE_CLK] = DEFINE_DUMMY_CLK(gcc_crypto_ppe_clk),
	[GCC_NSS_CE_APB_CLK] = DEFINE_DUMMY_CLK(gcc_nss_ce_apb_clk),
	[GCC_NSS_CE_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_nss_ce_axi_clk),
	[GCC_NSS_CFG_CLK] = DEFINE_DUMMY_CLK(gcc_nss_cfg_clk),
	[GCC_NSS_CRYPTO_CLK] = DEFINE_DUMMY_CLK(gcc_nss_crypto_clk),
	[GCC_NSS_CSR_CLK] = DEFINE_DUMMY_CLK(gcc_nss_csr_clk),
	[GCC_NSS_EDMA_CFG_CLK] = DEFINE_DUMMY_CLK(gcc_nss_edma_cfg_clk),
	[GCC_NSS_EDMA_CLK] = DEFINE_DUMMY_CLK(gcc_nss_edma_clk),
	[GCC_NSS_NOC_CLK] = DEFINE_DUMMY_CLK(gcc_nss_noc_clk),
	[GCC_NSS_PORT1_RX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port1_rx_clk),
	[GCC_NSS_PORT1_TX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port1_tx_clk),
	[GCC_NSS_PORT2_RX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port2_rx_clk),
	[GCC_NSS_PORT2_TX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port2_tx_clk),
	[GCC_NSS_PORT3_RX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port3_rx_clk),
	[GCC_NSS_PORT3_TX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port3_tx_clk),
	[GCC_NSS_PORT4_RX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port4_rx_clk),
	[GCC_NSS_PORT4_TX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port4_tx_clk),
	[GCC_NSS_PORT5_RX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port5_rx_clk),
	[GCC_NSS_PORT5_TX_CLK] = DEFINE_DUMMY_CLK(gcc_nss_port5_tx_clk),
	[GCC_NSS_PPE_CFG_CLK] = DEFINE_DUMMY_CLK(gcc_nss_ppe_cfg_clk),
	[GCC_NSS_PPE_CLK] = DEFINE_DUMMY_CLK(gcc_nss_ppe_clk),
	[GCC_NSS_PPE_IPE_CLK] = DEFINE_DUMMY_CLK(gcc_nss_ppe_ipe_clk),
	[GCC_NSS_PTP_REF_CLK] = DEFINE_DUMMY_CLK(gcc_nss_ptp_ref_clk),
	[GCC_NSSNOC_CE_APB_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_ce_apb_clk),
	[GCC_NSSNOC_CE_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_ce_axi_clk),
	[GCC_NSSNOC_CRYPTO_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_crypto_clk),
	[GCC_NSSNOC_PPE_CFG_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_ppe_cfg_clk),
	[GCC_NSSNOC_PPE_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_ppe_clk),
	[GCC_NSSNOC_QOSGEN_REF_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_qosgen_ref_clk),
	[GCC_NSSNOC_TIMEOUT_REF_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_timeout_ref_clk),
	[GCC_NSSNOC_UBI0_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_ubi0_ahb_clk),
	[GCC_PORT1_MAC_CLK] = DEFINE_DUMMY_CLK(gcc_port1_mac_clk),
	[GCC_PORT2_MAC_CLK] = DEFINE_DUMMY_CLK(gcc_port2_mac_clk),
	[GCC_PORT3_MAC_CLK] = DEFINE_DUMMY_CLK(gcc_port3_mac_clk),
	[GCC_PORT4_MAC_CLK] = DEFINE_DUMMY_CLK(gcc_port4_mac_clk),
	[GCC_PORT5_MAC_CLK] = DEFINE_DUMMY_CLK(gcc_port5_mac_clk),
	[GCC_UBI0_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_ubi0_ahb_clk),
	[GCC_UBI0_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_ubi0_axi_clk),
	[GCC_UBI0_CORE_CLK] = DEFINE_DUMMY_CLK(gcc_ubi0_core_clk),
	[GCC_PRNG_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_prng_ahb_clk),
	[GCC_QPIC_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_qpic_ahb_clk),
	[GCC_QPIC_CLK] = DEFINE_DUMMY_CLK(gcc_qpic_clk),
	[GCC_SDCC1_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_sdcc1_ahb_clk),
	[GCC_SDCC1_APPS_CLK] = DEFINE_DUMMY_CLK(gcc_sdcc1_apps_clk),
	[GCC_UNIPHY0_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_ahb_clk),
	[GCC_UNIPHY0_PORT1_RX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port1_rx_clk),
	[GCC_UNIPHY0_PORT1_TX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port1_tx_clk),
	[GCC_UNIPHY0_PORT2_RX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port2_rx_clk),
	[GCC_UNIPHY0_PORT2_TX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port2_tx_clk),
	[GCC_UNIPHY0_PORT3_RX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port3_rx_clk),
	[GCC_UNIPHY0_PORT3_TX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port3_tx_clk),
	[GCC_UNIPHY0_PORT4_RX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port4_rx_clk),
	[GCC_UNIPHY0_PORT4_TX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port4_tx_clk),
	[GCC_UNIPHY0_PORT5_RX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port5_rx_clk),
	[GCC_UNIPHY0_PORT5_TX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_port5_tx_clk),
	[GCC_UNIPHY0_SYS_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy0_sys_clk),
	[GCC_UNIPHY1_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy1_ahb_clk),
	[GCC_UNIPHY1_PORT5_RX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy1_port5_rx_clk),
	[GCC_UNIPHY1_PORT5_TX_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy1_port5_tx_clk),
	[GCC_UNIPHY1_SYS_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy1_sys_clk),
	[GCC_UNIPHY2_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy2_ahb_clk),
	[GCC_UNIPHY2_SYS_CLK] = DEFINE_DUMMY_CLK(gcc_uniphy2_sys_clk),
	[GCC_USB0_AUX_CLK] = DEFINE_DUMMY_CLK(gcc_usb0_aux_clk),
	[GCC_USB0_MASTER_CLK] = DEFINE_DUMMY_CLK(gcc_usb0_master_clk),
	[GCC_USB0_MOCK_UTMI_CLK] = DEFINE_DUMMY_CLK(gcc_usb0_mock_utmi_clk),
	[GCC_USB0_PHY_CFG_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_usb0_phy_cfg_ahb_clk),
	[GCC_USB0_PIPE_CLK] = DEFINE_DUMMY_CLK(gcc_usb0_pipe_clk),
	[GCC_USB0_SLEEP_CLK] = DEFINE_DUMMY_CLK(gcc_usb0_sleep_clk),
	[GCC_GP1_CLK] = DEFINE_DUMMY_CLK(gcc_gp1_clk),
	[GCC_GP2_CLK] = DEFINE_DUMMY_CLK(gcc_gp2_clk),
	[GCC_GP3_CLK] = DEFINE_DUMMY_CLK(gcc_gp3_clk),
	[GCC_NSSNOC_SNOC_CLK] = DEFINE_DUMMY_CLK(gcc_nssnoc_snoc_clk),
	[GCC_UBI0_NC_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_ubi0_nc_axi_clk),
	[GPLL0_MAIN] = DEFINE_DUMMY_CLK(gpll0_main),
	[UBI32_PLL_MAIN] = DEFINE_DUMMY_CLK(ubi32_pll_main),
	[GPLL6_MAIN] = DEFINE_DUMMY_CLK(gpll6_main),
	[GPLL4_MAIN] = DEFINE_DUMMY_CLK(gpll4_main),
	[GPLL2_MAIN] = DEFINE_DUMMY_CLK(gpll2_main),
	[NSS_CRYPTO_PLL_MAIN] = DEFINE_DUMMY_CLK(nss_crypto_pll_main),
	[GCC_CMN_12GPLL_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_cmn_12gpll_ahb_clk),
	[GCC_CMN_12GPLL_SYS_CLK] = DEFINE_DUMMY_CLK(gcc_cmn_12gpll_sys_clk),
	[GCC_SNOC_BUS_TIMEOUT2_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_snoc_bus_timeout2_ahb_clk),
	[GCC_SYS_NOC_USB0_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_sys_noc_usb0_axi_clk),
	[GCC_SNOC_BUS_TIMEOUT3_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_snoc_bus_timeout3_ahb_clk),
	[GCC_QDSS_AT_CLK] = DEFINE_DUMMY_CLK(gcc_qdss_at_clk),
	[GCC_QDSS_DAP_CLK] = DEFINE_DUMMY_CLK(gcc_qdss_dap_clk),
	[GCC_SDCC1_ICE_CORE_CLK] = DEFINE_DUMMY_CLK(gcc_sdcc1_ice_core_clk),
	[GCC_USB1_MASTER_CLK] = DEFINE_DUMMY_CLK(gcc_usb1_master_clk),
	[GCC_USB1_MOCK_UTMI_CLK] = DEFINE_DUMMY_CLK(gcc_usb1_mock_utmi_clk),
	[GCC_USB1_PHY_CFG_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_usb1_phy_cfg_ahb_clk),
	[GCC_USB1_SLEEP_CLK] = DEFINE_DUMMY_CLK(gcc_usb1_sleep_clk),
	[GCC_PCIE0_AHB_CLK] = DEFINE_DUMMY_CLK(gcc_pcie0_ahb_clk),
	[GCC_PCIE0_AUX_CLK] = DEFINE_DUMMY_CLK(gcc_pcie0_aux_clk),
	[GCC_PCIE0_AXI_M_CLK] = DEFINE_DUMMY_CLK(gcc_pcie0_axi_m_clk),
	[GCC_PCIE0_AXI_S_BRIDGE_CLK] = DEFINE_DUMMY_CLK(gcc_pcie0_axi_s_bridge_clk),
	[GCC_PCIE0_AXI_S_CLK] = DEFINE_DUMMY_CLK(gcc_pcie0_axi_s_clk),
	[GCC_PCIE0_PIPE_CLK] = DEFINE_DUMMY_CLK(gcc_pcie0_pipe_clk),
	[GCC_SYS_NOC_PCIE0_AXI_CLK] = DEFINE_DUMMY_CLK(gcc_sys_noc_pcie0_axi_clk),
	[GCC_MEM_NOC_UBI32_CLK] = DEFINE_DUMMY_CLK(gcc_mem_noc_ubi32_clk),
	[GCC_SNOC_NSSNOC_CLK] = DEFINE_DUMMY_CLK(gcc_snoc_nssnoc_clk),
};

static const struct qcom_reset_map gcc_ipq6018_resets[] = {
	[GCC_BLSP1_BCR] = { 0x01000, 0 },
	[GCC_BLSP1_QUP1_BCR] = { 0x02000, 0 },
	[GCC_BLSP1_UART1_BCR] = { 0x02038, 0 },
	[GCC_BLSP1_QUP2_BCR] = { 0x03008, 0 },
	[GCC_BLSP1_UART2_BCR] = { 0x03028, 0 },
	[GCC_BLSP1_QUP3_BCR] = { 0x04008, 0 },
	[GCC_BLSP1_UART3_BCR] = { 0x04028, 0 },
	[GCC_BLSP1_QUP4_BCR] = { 0x05008, 0 },
	[GCC_BLSP1_UART4_BCR] = { 0x05028, 0 },
	[GCC_BLSP1_QUP5_BCR] = { 0x06008, 0 },
	[GCC_BLSP1_UART5_BCR] = { 0x06028, 0 },
	[GCC_BLSP1_QUP6_BCR] = { 0x07008, 0 },
	[GCC_BLSP1_UART6_BCR] = { 0x07028, 0 },
	[GCC_IMEM_BCR] = { 0x0e000, 0 },
	[GCC_SMMU_BCR] = { 0x12000, 0 },
	[GCC_APSS_TCU_BCR] = { 0x12050, 0 },
	[GCC_SMMU_XPU_BCR] = { 0x12054, 0 },
	[GCC_PCNOC_TBU_BCR] = { 0x12058, 0 },
	[GCC_SMMU_CFG_BCR] = { 0x1208c, 0 },
	[GCC_PRNG_BCR] = { 0x13000, 0 },
	[GCC_BOOT_ROM_BCR] = { 0x13008, 0 },
	[GCC_CRYPTO_BCR] = { 0x16000, 0 },
	[GCC_WCSS_BCR] = { 0x18000, 0 },
	[GCC_WCSS_Q6_BCR] = { 0x18100, 0 },
	[GCC_NSS_BCR] = { 0x19000, 0 },
	[GCC_SEC_CTRL_BCR] = { 0x1a000, 0 },
	[GCC_DDRSS_BCR] = { 0x1e000, 0 },
	[GCC_SYSTEM_NOC_BCR] = { 0x26000, 0 },
	[GCC_PCNOC_BCR] = { 0x27018, 0 },
	[GCC_TCSR_BCR] = { 0x28000, 0 },
	[GCC_QDSS_BCR] = { 0x29000, 0 },
	[GCC_DCD_BCR] = { 0x2a000, 0 },
	[GCC_MSG_RAM_BCR] = { 0x2b000, 0 },
	[GCC_MPM_BCR] = { 0x2c000, 0 },
	[GCC_SPMI_BCR] = { 0x2e000, 0 },
	[GCC_SPDM_BCR] = { 0x2f000, 0 },
	[GCC_RBCPR_BCR] = { 0x33000, 0 },
	[GCC_RBCPR_MX_BCR] = { 0x33014, 0 },
	[GCC_TLMM_BCR] = { 0x34000, 0 },
	[GCC_RBCPR_WCSS_BCR] = { 0x3a000, 0 },
	[GCC_USB0_PHY_BCR] = { 0x3e034, 0 },
	[GCC_USB3PHY_0_PHY_BCR] = { 0x3e03c, 0 },
	[GCC_USB0_BCR] = { 0x3e070, 0 },
	[GCC_USB1_PHY_BCR] = { 0x3f034, 0 },
	[GCC_USB3PHY_1_PHY_BCR] = { 0x3f03c, 0 },
	[GCC_USB1_BCR] = { 0x3f070, 0 },
	[GCC_QUSB2_0_PHY_BCR] = { 0x4103c, 0 },
	[GCC_QUSB2_1_PHY_BCR] = { 0x41040, 0 },
	[GCC_SDCC1_BCR] = { 0x42000, 0 },
	[GCC_SNOC_BUS_TIMEOUT0_BCR] = { 0x47000, 0 },
	[GCC_SNOC_BUS_TIMEOUT1_BCR] = { 0x47008, 0 },
	[GCC_SNOC_BUS_TIMEOUT2_BCR] = { 0x47010, 0 },
	[GCC_PCNOC_BUS_TIMEOUT0_BCR] = { 0x48000, 0 },
	[GCC_PCNOC_BUS_TIMEOUT1_BCR] = { 0x48008, 0 },
	[GCC_PCNOC_BUS_TIMEOUT2_BCR] = { 0x48010, 0 },
	[GCC_PCNOC_BUS_TIMEOUT3_BCR] = { 0x48018, 0 },
	[GCC_PCNOC_BUS_TIMEOUT4_BCR] = { 0x48020, 0 },
	[GCC_PCNOC_BUS_TIMEOUT5_BCR] = { 0x48028, 0 },
	[GCC_PCNOC_BUS_TIMEOUT6_BCR] = { 0x48030, 0 },
	[GCC_PCNOC_BUS_TIMEOUT7_BCR] = { 0x48038, 0 },
	[GCC_PCNOC_BUS_TIMEOUT8_BCR] = { 0x48040, 0 },
	[GCC_PCNOC_BUS_TIMEOUT9_BCR] = { 0x48048, 0 },
	[GCC_UNIPHY0_BCR] = { 0x56000, 0 },
	[GCC_UNIPHY1_BCR] = { 0x56100, 0 },
	[GCC_CMN_12GPLL_BCR] = { 0x56300, 0 },
	[GCC_QPIC_BCR] = { 0x57018, 0 },
	[GCC_MDIO_BCR] = { 0x58000, 0 },
	[GCC_WCSS_CORE_TBU_BCR] = { 0x66000, 0 },
	[GCC_WCSS_Q6_TBU_BCR] = { 0x67000, 0 },
	[GCC_USB0_TBU_BCR] = { 0x6a000, 0 },
	[GCC_PCIE0_TBU_BCR] = { 0x6b000, 0 },
	[GCC_PCIE0_BCR] = { 0x75004, 0 },
	[GCC_PCIE0_PHY_BCR] = { 0x75038, 0 },
	[GCC_PCIE0PHY_PHY_BCR] = { 0x7503c, 0 },
	[GCC_PCIE0_LINK_DOWN_BCR] = { 0x75044, 0 },
	[GCC_DCC_BCR] = { 0x77000, 0 },
	[GCC_APC0_VOLTAGE_DROOP_DETECTOR_BCR] = { 0x78000, 0 },
	[GCC_APC1_VOLTAGE_DROOP_DETECTOR_BCR] = { 0x79000, 0 },
	[GCC_SMMU_CATS_BCR] = { 0x7c000, 0 },
	[GCC_UBI0_AXI_ARES] = { 0x68010, 0 },
	[GCC_UBI0_AHB_ARES] = { 0x68010, 1 },
	[GCC_UBI0_NC_AXI_ARES] = { 0x68010, 2 },
	[GCC_UBI0_DBG_ARES] = { 0x68010, 3 },
	[GCC_UBI0_CORE_CLAMP_ENABLE] = { 0x68010, 4 },
	[GCC_UBI0_CLKRST_CLAMP_ENABLE] = { 0x68010, 5 },
	[GCC_UBI0_UTCM_ARES] = { 0x68010, 6 },
	[GCC_NSS_CFG_ARES] = { 0x68010, 16 },
	[GCC_NSS_NOC_ARES] = { 0x68010, 18 },
	[GCC_NSS_CRYPTO_ARES] = { 0x68010, 19 },
	[GCC_NSS_CSR_ARES] = { 0x68010, 20 },
	[GCC_NSS_CE_APB_ARES] = { 0x68010, 21 },
	[GCC_NSS_CE_AXI_ARES] = { 0x68010, 22 },
	[GCC_NSSNOC_CE_APB_ARES] = { 0x68010, 23 },
	[GCC_NSSNOC_CE_AXI_ARES] = { 0x68010, 24 },
	[GCC_NSSNOC_UBI0_AHB_ARES] = { 0x68010, 25 },
	[GCC_NSSNOC_SNOC_ARES] = { 0x68010, 27 },
	[GCC_NSSNOC_CRYPTO_ARES] = { 0x68010, 28 },
	[GCC_NSSNOC_ATB_ARES] = { 0x68010, 29 },
	[GCC_NSSNOC_QOSGEN_REF_ARES] = { 0x68010, 30 },
	[GCC_NSSNOC_TIMEOUT_REF_ARES] = { 0x68010, 31 },
	[GCC_PCIE0_PIPE_ARES] = { 0x75040, 0 },
	[GCC_PCIE0_SLEEP_ARES] = { 0x75040, 1 },
	[GCC_PCIE0_CORE_STICKY_ARES] = { 0x75040, 2 },
	[GCC_PCIE0_AXI_MASTER_ARES] = { 0x75040, 3 },
	[GCC_PCIE0_AXI_SLAVE_ARES] = { 0x75040, 4 },
	[GCC_PCIE0_AHB_ARES] = { 0x75040, 5 },
	[GCC_PCIE0_AXI_MASTER_STICKY_ARES] = { 0x75040, 6 },
	[GCC_PCIE0_AXI_SLAVE_STICKY_ARES] = { 0x75040, 7 },
	[GCC_PPE_FULL_RESET] = { 0x68014, 0, 0xf0000},
	[GCC_UNIPHY0_SOFT_RESET] = { 0x56004, 0, 0x3ff2},
	[GCC_UNIPHY0_XPCS_RESET] = { 0x56004, 2 },
	[GCC_UNIPHY1_SOFT_RESET] = { 0x56104, 0, 0x32},
	[GCC_UNIPHY1_XPCS_RESET] = { 0x56104, 2 },
	[GCC_EDMA_HW_RESET] = { 0x68014, 0, 0x300000},
};

static const struct of_device_id gcc_ipq6018_match_table[] = {
	{ .compatible = "qcom,gcc-ipq6018" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_ipq6018_match_table);

static const struct regmap_config gcc_ipq6018_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
	.max_register   = 0x7fffc,
	.fast_io	= true,
};

static const struct qcom_cc_desc gcc_ipq6018_desc = {
	.config = &gcc_ipq6018_regmap_config,
	.clks = gcc_ipq6018_clks,
	.num_clks = ARRAY_SIZE(gcc_ipq6018_clks),
	.resets = gcc_ipq6018_resets,
	.num_resets = ARRAY_SIZE(gcc_ipq6018_resets),
};

static int gcc_ipq6018_probe(struct platform_device *pdev)
{
		return qcom_cc_probe(pdev, &gcc_ipq6018_desc);
}

static int gcc_ipq6018_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver gcc_ipq6018_driver = {
	.probe = gcc_ipq6018_probe,
	.remove = gcc_ipq6018_remove,
	.driver = {
		.name   = "qcom,gcc-ipq6018",
		.owner  = THIS_MODULE,
		.of_match_table = gcc_ipq6018_match_table,
	},
};

static int __init gcc_ipq6018_init(void)
{
	return platform_driver_register(&gcc_ipq6018_driver);
}
core_initcall(gcc_ipq6018_init);

static void __exit gcc_ipq6018_exit(void)
{
	platform_driver_unregister(&gcc_ipq6018_driver);
}
module_exit(gcc_ipq6018_exit);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. GCC IPQ6018 Driver");
MODULE_LICENSE("Dual BSD/GPLv2");
MODULE_ALIAS("platform:gcc-ipq6018");
