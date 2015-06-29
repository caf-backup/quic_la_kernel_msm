/*
 * Copyright (c) 2014, 2015 The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

#include <dt-bindings/clock/qca,adcc-ipq40xx.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-rcg.h"
#include "clk-qcapll.h"
#include "clk-branch.h"
#include "reset.h"

#define AUDIO_PLL_CONFIG_REG				0x00038
#define AUDIO_PLL_MODULATION_REG			0x0003C
#define AUDIO_PLL_MOD_STEP_REG				0x00040
#define CURRENT_AUDIO_PLL_MODULATION_REG		0x00044
#define AUDIO_PLL_CONFIG1_REG				0x00048
#define AUDIO_ATB_SETTING_REG				0x0004C
#define AUDIO_RXB_CFG_MUXR_REG				0x00104
#define AUDIO_RXB_MISC_REG				0x00108
#define AUDIO_RXB_CBCR_REG				0x0010C
#define AUDIO_RXM_CMD_RCGR_REG				0x00120
#define AUDIO_RXM_CFG_RCGR_REG				0x00124
#define AUDIO_RXM_MISC_REG				0x00128
#define AUDIO_RXM_CBCR_REG				0x0012C
#define AUDIO_TXB_CFG_MUXR_REG				0x00144
#define AUDIO_TXB_MISC_REG				0x00148
#define AUDIO_TXB_CBCR_REG				0x0014C
#define AUDIO_SPDIF_MISC_REG				0x00150
#define AUDIO_SPDIF_CBCR_REG				0x00154
#define AUDIO_SPDIFDIV2_MISC_REG			0x00158
#define AUDIO_SPDIFDIV2_CBCR_REG			0x0015C
#define AUDIO_TXM_CMD_RCGR_REG				0x00160
#define AUDIO_TXM_CFG_RCGR_REG				0x00164
#define AUDIO_TXM_MISC_REG				0x00168
#define AUDIO_TXM_CBCR_REG				0x0016C
#define AUDIO_SAMPLE_CBCR_REG				0x0018C
#define AUDIO_PCM_CMD_RCGR_REG				0x001A0
#define AUDIO_PCM_CFG_RCGR_REG				0x001A4
#define AUDIO_PCM_MISC_REG				0x001A8
#define AUDIO_PCM_CBCR_REG				0x001AC
#define AUDIO_XO_CBCR_REG				0x001CC
#define AUDIO_SPDIFINFAST_CMD_RCGR_REG			0x001E0
#define AUDIO_SPDIFINFAST_CFG_RCGR_REG			0x001E4
#define AUDIO_SPDIFINFAST_CBCR_REG			0x001EC
#define AUDIO_AHB_CBCR_REG				0x00200
#define AUDIO_AHB_I2S0_CBCR_REG				0x00204
#define AUDIO_AHB_I2S3_CBCR_REG				0x00208
#define AUDIO_AHB_MBOX0_CBCR_REG			0x0020C
#define AUDIO_AHB_MBOX3_CBCR_REG			0x00210

static const u8 adcc_xo_adpll_rxmpadclk_map[] = {
	[0]		= 0,
	[1]		= 1,
	[2]		= 2,
};

static const char *adcc_xo_adpll_rxmpadclk[] = {
	"xo",
	"apll",
	"rxmpadclk",
};

static const u8 adcc_xo_adpll_rxbpadclk_rxmpadclk_map[] = {
	[0]		= 0,
	[1]		= 1,
	[2]		= 2,
	[3]		= 3,
};

static const char *adcc_xo_adpll_rxbpadclk_rxmpadclk[] = {
	"xo",
	"apll",
	"rxbpadclk",
	"rxmpadclk",
};

static const u8 adcc_xo_adpll_txmpadclk_map[] = {
	[0]		= 0,
	[1]		= 1,
	[2]		= 2,
};

static const char *adcc_xo_adpll_txmpadclk[] = {
	"xo",
	"apll",
	"txmpadclk",
};

static const u8 adcc_xo_adpll_txbpadclk_txmpadclk_map[] = {
	[0]		= 0,
	[1]		= 1,
	[2]		= 2,
	[3]		= 3,
};

static const char *adcc_xo_adpll_txbpadclk_txmpadclk[] = {
	"xo",
	"apll",
	"txbpadclk",
	"txmpadclk",
};

static const u8 adcc_xo_adpll_map[] = {
	[0]		= 0,
	[1]		= 1,
};

static const char *adcc_xo_adpll[] = {
	"xo",
	"apll",
};

#define F(f, s, h, m, n) { (unsigned long)(f), (s), \
				(h & 0xff00)|(2 * (h&0xff) - 1), (m), (n) }
#define P_XO 0
#define ADSS_PLL 1
#define MCLK_MCLK_IN 2
#define BCLK_BCLK_IN 2
#define BCLK_MCLK_IN 3


static const struct pll_freq_tbl adss_freq_tbl[] = {
	{327680000, 1, 5, 40, 0x3D708},
	{361267200, 1, 5, 45, 0xA234},
	{368641000, 1, 5, 46, 0x51E9},
	{393216000, 1, 5, 49, 0x9bA6},
	{395136000, 1, 5, 49, 0x19168},
	{}
};

static struct clk_qcapll adss_pll_src = {
	.config_reg		= 0x0004,
	.mod_reg		= 0x0008,
	.modstep_reg	= 0x000c,
	.current_mod_pll_reg = 0x0014,
	.config1_reg = 0x0000,
	.freq_tbl = adss_freq_tbl,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "adss_pll",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_qcapll_ops,
		.flags = CLK_SET_PARENT_GATE,
	},
};

static const struct freq_tbl ftbl_m_clk[] = {
	F(4096000, ADSS_PLL, 0xB04, 0, 0),
	F(5645000, ADSS_PLL, 0x704, 0, 0),
	F(6144000, ADSS_PLL, 0x704, 0, 0),
	F(8192000, ADSS_PLL, 0x504, 0, 0),
	F(11290000, ADSS_PLL, 0xF01, 0, 0),
	F(12288000, ADSS_PLL, 0xF01, 0, 0),
	F(14112000, ADSS_PLL, 0xD01, 0, 0),
	F(15360000, ADSS_PLL, 0xB01, 0, 0),
	F(16384000, ADSS_PLL, 0xB01, 0, 0),
	F(20480000, ADSS_PLL, 0x701, 0, 0),
	F(22579200, ADSS_PLL, 0x701, 0, 0),
	F(24576000, ADSS_PLL, 0x701, 0, 0),
	F(30720000, ADSS_PLL, 0x701, 0, 0),
	{ }
};

static struct clk_cdiv_rcg2 rxm_clk_src = {
	.cdiv.offset = AUDIO_RXM_MISC_REG,
	.cdiv.shift = 4,
	.cdiv.mask = 0xf,
	.cmd_rcgr = AUDIO_RXM_CMD_RCGR_REG,
	.hid_width = 5,
	.parent_map = adcc_xo_adpll_rxmpadclk_map,
	.freq_tbl = ftbl_m_clk,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "rxm_clk_src",
		.parent_names = adcc_xo_adpll_rxmpadclk,
		.num_parents = 3,
		.ops = &clk_cdiv_rcg2_ops,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_branch adcc_rxm_clk_src = {
	.halt_reg = AUDIO_RXM_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_RXM_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_rxm_clk_src",
			.parent_names = (const char *[]){"rxm_clk_src"},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_cdiv_rcg2 txm_clk_src = {
	.cdiv.offset = AUDIO_TXM_MISC_REG,
	.cdiv.shift = 4,
	.cdiv.mask = 0xf,
	.cmd_rcgr = AUDIO_TXM_CMD_RCGR_REG,
	.hid_width = 5,
	.parent_map = adcc_xo_adpll_txmpadclk_map,
	.freq_tbl = ftbl_m_clk,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "txm_clk_src",
		.parent_names = adcc_xo_adpll_txmpadclk,
		.num_parents = 3,
		.ops = &clk_cdiv_rcg2_ops,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_branch adcc_txm_clk_src = {
	.halt_reg = AUDIO_TXM_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_TXM_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_txm_clk_src",
			.parent_names = (const char *[]){
				"txm_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static const struct freq_tbl ftbl_bclk_clk[] = {
	F(1024000, ADSS_PLL, 191, 0, 0),
	F(1411200, ADSS_PLL, 127, 0, 0),
	F(1536000, ADSS_PLL, 127, 0, 0),
	F(2048000, ADSS_PLL, 95, 0, 0),
	F(2822400, ADSS_PLL, 63, 0, 0),
	F(3072000, ADSS_PLL, 63, 0, 0),
	F(4096000, ADSS_PLL, 47, 0, 0),
	F(5120000, ADSS_PLL, 31, 0, 0),
	F(5644800, ADSS_PLL, 31, 0, 0),
	F(6144000, ADSS_PLL, 31, 0, 0),
	F(7056000, ADSS_PLL, 23, 0, 0),
	F(7680000, ADSS_PLL, 23, 0, 0),
	F(8192000, ADSS_PLL, 23, 0, 0),
	F(10240000, ADSS_PLL, 15, 0, 0),
	F(11290000, ADSS_PLL, 15, 0, 0),
	F(12288000, ADSS_PLL, 15, 0, 0),
	F(14112000, ADSS_PLL, 15, 0, 0),
	F(15360000, ADSS_PLL, 11, 0, 0),
	F(24576000, ADSS_PLL,  7, 0, 0),
	F(30720000, ADSS_PLL,  5, 0, 0),
	{ }
};

static struct clk_muxr_misc txb_clk_src = {
	.misc.offset = AUDIO_TXB_MISC_REG,
	.misc.shift = 1,
	.misc.mask = 0x3FF,
	.muxr.offset = AUDIO_TXB_CFG_MUXR_REG,
	.muxr.shift = 8,
	.muxr.mask = 0x7,
	.parent_map = adcc_xo_adpll_txbpadclk_txmpadclk_map,
	.freq_tbl = ftbl_bclk_clk,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "txb_clk_src",
		.parent_names = adcc_xo_adpll_txbpadclk_txmpadclk,
		.num_parents = 4,
		.ops = &clk_muxr_misc_ops,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_branch adcc_txb_clk_src = {
	.halt_reg = AUDIO_TXB_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_TXB_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_txb_clk_src",
			.parent_names = (const char *[]){
				"txb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_muxr_misc rxb_clk_src = {
	.misc.offset = AUDIO_RXB_MISC_REG,
	.misc.shift = 1,
	.misc.mask = 0x3FF,
	.muxr.offset = AUDIO_RXB_CFG_MUXR_REG,
	.muxr.shift = 8,
	.muxr.mask = 0x7,
	.parent_map = adcc_xo_adpll_rxbpadclk_rxmpadclk_map,
	.freq_tbl = ftbl_bclk_clk,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "rxb_clk_src",
		.parent_names = adcc_xo_adpll_rxbpadclk_rxmpadclk,
		.num_parents = 4,
		.ops = &clk_muxr_misc_ops,
		.flags = CLK_SET_PARENT_GATE,
	},
};


static struct clk_branch adcc_rxb_clk_src = {
	.halt_reg = AUDIO_RXB_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_RXB_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_rxb_clk_src",
			.parent_names = (const char *[]){
				"rxb_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};



static const struct freq_tbl ftbl_adcc_pcm_clk[] = {
	F(8192000, ADSS_PLL, 0x504, 0, 0),
	{ }
};

static struct clk_cdiv_rcg2 pcm_clk_src = {
	.cdiv.offset = AUDIO_PCM_MISC_REG,
	.cdiv.shift = 4,
	.cdiv.mask = 0xf,
	.cmd_rcgr = AUDIO_PCM_CMD_RCGR_REG,
	.hid_width = 5,
	.parent_map = adcc_xo_adpll_map,
	.freq_tbl = ftbl_adcc_pcm_clk,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "pcm_clk_src",
		.parent_names = adcc_xo_adpll,
		.num_parents = 2,
		.ops = &clk_cdiv_rcg2_ops,
		.flags = CLK_SET_PARENT_GATE,
	},
};



static struct clk_branch adcc_pcm_clk_src = {
	.halt_reg = AUDIO_PCM_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_PCM_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_pcm_clk_src",
			.parent_names = (const char *[]){
				"pcm_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};



static const struct freq_tbl ftbl_adcc_spdifinfast_clk[] = {
	F(49152000, ADSS_PLL, 0x04, 0, 0),
	{ }
};

static struct clk_cdiv_rcg2 spdifinfast_src = {
	.cmd_rcgr = AUDIO_SPDIFINFAST_CMD_RCGR_REG,
	.hid_width = 5,
	.parent_map = adcc_xo_adpll_map,
	.freq_tbl = ftbl_adcc_spdifinfast_clk,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "spdifinfast_src",
		.parent_names = adcc_xo_adpll,
		.num_parents = 2,
		.ops = &clk_cdiv_rcg2_ops,
		.flags = CLK_SET_PARENT_GATE,
	},
};

static struct clk_branch adcc_spdifinfast_src = {
	.halt_reg = AUDIO_SPDIFINFAST_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_SPDIFINFAST_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_spdifinfast_src",
			.parent_names = (const char *[]){
				"XO",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};

static const struct freq_tbl spdif_src_tbl[] = {
	F(0xFFFFFFFF, ADSS_PLL, 0x0, 0, 0),
	{ }
};

static struct clk_muxr_misc spdif_src = {
	.misc.offset = AUDIO_SPDIF_MISC_REG,
	.misc.shift = 1,
	.misc.mask = 0x3FF,
	.parent_map = adcc_xo_adpll_rxbpadclk_rxmpadclk_map,
	.freq_tbl = spdif_src_tbl,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "spdif_src",
		.parent_names = adcc_xo_adpll_rxbpadclk_rxmpadclk,
		.num_parents = 3,
		.ops = &clk_muxr_misc_ops,
		.flags = CLK_SET_PARENT_GATE,
	},
};

static struct clk_branch adcc_spdif_src = {
	.halt_reg = AUDIO_SPDIF_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_SPDIF_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_spdif_src",
			.parent_names = (const char *[]){
				"spdif_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};

static const struct freq_tbl spdif2_src_tbl[] = {
	F(0xFFFFFFFF, ADSS_PLL, 0x0, 0, 0),
	{ }
};

static struct clk_muxr_misc spdifdiv2_src = {
	.misc.offset = AUDIO_SPDIFDIV2_MISC_REG,
	.misc.shift = 1,
	.misc.mask = 0x3FF,
	.parent_map = adcc_xo_adpll_rxbpadclk_rxmpadclk_map,
	.freq_tbl = spdif2_src_tbl,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "spdifdiv2_src",
		.parent_names = adcc_xo_adpll_rxbpadclk_rxmpadclk,
		.num_parents = 3,
		.ops = &clk_muxr_misc_ops,
		.flags = CLK_SET_PARENT_GATE,
	},
};

static struct clk_branch adcc_spdifdiv2_src = {
	.halt_reg = AUDIO_SPDIFDIV2_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_SPDIFDIV2_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_spdifdiv2_src",
			.parent_names = (const char *[]){
				"spdifdiv2_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};

static struct clk_branch adcc_sample_src = {
	.halt_reg = AUDIO_SAMPLE_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_SAMPLE_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_sample_src",
			.parent_names = (const char *[]){
				"pcnoc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};

static struct clk_branch adcc_xo_src = {
	.halt_reg = AUDIO_XO_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_XO_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_xo_src",
			.parent_names = (const char *[]){
				"XO",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
			.flags = CLK_SET_PARENT_GATE,
		},
	},
};


static struct clk_branch adcc_ahb_src = {
	.halt_reg = AUDIO_AHB_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_AHB_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_audio_ahb_src",
			.parent_names = (const char *[]){
				"pcnoc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch adcc_ahb_i2s0_src = {
	.halt_reg = AUDIO_AHB_I2S0_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_AHB_I2S0_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_audio_ahb_i2s0",
			.parent_names = (const char *[]){
				"pcnoc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch adcc_ahb_i2s3_src = {
	.halt_reg = AUDIO_AHB_I2S3_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_AHB_I2S3_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_audio_ahb_i2s3",
			.parent_names = (const char *[]){
				"pcnoc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch adcc_ahb_mbox0_src = {
	.halt_reg = AUDIO_AHB_MBOX0_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_AHB_MBOX0_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_audio_mbox0_src",
			.parent_names = (const char *[]){
				"pcnoc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch adcc_ahb_mbox3_src = {
	.halt_reg = AUDIO_AHB_MBOX3_CBCR_REG,
	.clkr = {
		.enable_reg = AUDIO_AHB_MBOX3_CBCR_REG,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "adcc_ahb_mbox3_src",
			.parent_names = (const char *[]){
				"pcnoc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *adcc_ipq40xx_clocks[] = {
	[ADSS_PLL_SRC]				= &adss_pll_src.clkr,
	[RXM_CLK_SRC]				= &rxm_clk_src.clkr,
	[ADCC_RXM_CLK_SRC]			= &adcc_rxm_clk_src.clkr,
	[TXM_CLK_SRC]				= &txm_clk_src.clkr,
	[ADCC_TXM_CLK_SRC]			= &adcc_txm_clk_src.clkr,
	[TXB_CLK_SRC]				= &txb_clk_src.clkr,
	[ADCC_TXB_CLK_SRC]			= &adcc_txb_clk_src.clkr,
	[RXB_CLK_SRC]				= &rxb_clk_src.clkr,
	[ADCC_RXB_CLK_SRC]			= &adcc_rxb_clk_src.clkr,
	[PCM_CLK_SRC]				= &pcm_clk_src.clkr,
	[ADCC_PCM_CLK_SRC]			= &adcc_pcm_clk_src.clkr,
	[AUDIO_SPDIFINFAST_SRC]			= &spdifinfast_src.clkr,
	[ADCC_AUDIO_SPDIFINFAST_SRC]		= &adcc_spdifinfast_src.clkr,
	[ADCC_AUDIO_AHB_SRC]			= &adcc_ahb_src.clkr,
	[ADCC_AHB_I2S0]				= &adcc_ahb_i2s0_src.clkr,
	[ADCC_AHB_I2S3]				= &adcc_ahb_i2s3_src.clkr,
	[ADCC_AHB_MBOX0_SRC]			= &adcc_ahb_mbox0_src.clkr,
	[ADCC_AHB_MBOX3_SRC]			= &adcc_ahb_mbox3_src.clkr,
	[SPDIF_SRC]				= &spdif_src.clkr,
	[ADCC_SPDIF_SRC]			= &adcc_spdif_src.clkr,
	[SPDIFDIV2_SRC]				= &spdifdiv2_src.clkr,
	[ADCC_SPDIFDIV2_SRC]			= &adcc_spdifdiv2_src.clkr,
	[ADCC_SAMPLE_SRC]			= &adcc_sample_src.clkr,
	[ADCC_XO_SRC]				= &adcc_xo_src.clkr,
};

static const struct qcom_reset_map adcc_ipq40xx_resets[] = {
};

struct cdiv_fixed {
	char	*name;
	char	*pname;
	u32	flags;
	u32	mult;
	u32	div;
};

static const struct regmap_config adcc_ipq40xx_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x2ff,
	.fast_io	= true,
};

static const struct qcom_cc_desc adcc_ipq40xx_desc = {
	.config = &adcc_ipq40xx_regmap_config,
	.clks = adcc_ipq40xx_clocks,
	.num_clks = ARRAY_SIZE(adcc_ipq40xx_clocks),
	.resets = adcc_ipq40xx_resets,
	.num_resets = ARRAY_SIZE(adcc_ipq40xx_resets),
};

static const struct of_device_id adcc_ipq40xx_match_table[] = {
	{ .compatible = "qcom,adcc-ipq40xx" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_ipq40xx_match_table);

static int adcc_ipq40xx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *id;

	id = of_match_device(adcc_ipq40xx_match_table, dev);
	if (!id)
		return -ENODEV;

	/* High speed external clock */
	clk_register_fixed_rate(dev, "xo", NULL, CLK_IS_ROOT, 48000000);
	/* External sleep clock */
	clk_register_fixed_rate(dev, "adcc_sleep_clk_src", NULL,
							CLK_IS_ROOT, 32768);

	return qcom_cc_probe(pdev, &adcc_ipq40xx_desc);
}

static int adcc_ipq40xx_remove(struct platform_device *pdev)
{
	qcom_cc_remove(pdev);
	return 0;
}

static struct platform_driver adcc_ipq40xx_driver = {
	.probe		= adcc_ipq40xx_probe,
	.remove		= adcc_ipq40xx_remove,
	.driver		= {
		.name	= "adcc-ipq40xx",
		.owner	= THIS_MODULE,
		.of_match_table = adcc_ipq40xx_match_table,
	},
};

static int __init adcc_ipq40xx_init(void)
{
	return platform_driver_register(&adcc_ipq40xx_driver);
}
core_initcall(adcc_ipq40xx_init);

static void __exit adcc_ipq40xx_exit(void)
{
	platform_driver_unregister(&adcc_ipq40xx_driver);
}
module_exit(adcc_ipq40xx_exit);

MODULE_DESCRIPTION("ADSS CLOCK ipq40xx Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:adcc-ipq40xx");
