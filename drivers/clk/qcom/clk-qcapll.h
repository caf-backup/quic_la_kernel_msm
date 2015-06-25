/*
 * Copyright (c) 2013, 2015 The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_CLK_QCA_PLL_H__
#define __QCOM_CLK_QCA_PLL_H__

#include <linux/clk-provider.h>
#include "clk-regmap.h"

/**
 * struct pll_freq_tbl - PLL frequency table
 * @postplldiv: postplldiv value
 * @refdiv: refdiv value
 * @tgt_div_int: tgt_div_int value
 * @tgt_div_frac: tgt_div_frac values
 */
struct pll_freq_tbl {
	unsigned long freq;
	u8 postplldiv;
	u8 refdiv;
	u8 tgt_div_int;
	u32 tgt_div_frac;
};

/**
 * struct clk_pll - phase locked loop (PLL)
 * @l_reg: L register
 * @m_reg: M register
 * @n_reg: N register
 * @config_reg: config register
 * @mode_reg: mode register
 * @status_reg: status register
 * @status_bit: ANDed with @status_reg to determine if PLL is enabled
 * @freq_tbl: PLL frequency table
 * @hw: handle between common and hardware-specific interfaces
 */
struct clk_qcapll {
	u32 config_reg;
	u32 mod_reg;
	u32 modstep_reg;
	u32 current_mod_pll_reg;
	u32 config1_reg;

	const struct pll_freq_tbl *freq_tbl;
	struct clk_regmap clkr;
};

extern const struct clk_ops clk_qcapll_ops;

#define to_clk_qcapll(_hw) container_of(to_clk_regmap(_hw), \
						struct clk_qcapll, clkr)

struct qcapll_config {
	u32 pll_config_updating;
	u32 pll_config_updating_mask;
	u32 pll_config_postplldiv;
	u32 pll_config_postplldiv_mask;
	u32 pll_config_refdiv;
	u32 pll_config_refdiv_mask;
	u32 pll_modulation_tgt_div_frac;
	u32 pll_modulation_tgt_div_frac_mask;
	u32 pll_modulation_tgt_div_int;
	u32 pll_modulation_tgt_div_int_mask;
	u32 pll_modulation_start;
	u32 pll_modulation_start_mask;
	u32 audio_pll_config1_sreset_l;
	u32 audio_pll_config1_sreset_l_mask;
	u32 current_pll_modulation_frac;
	u32 current_pll_modulation_frac_mask;
	u32 current_audio_pll_modulation_int;
	u32 current_audio_pll_modulation_int_mask;
};

void clk_pll_configure_adss(struct clk_qcapll *pll, struct regmap *regmap,
			    const struct qcapll_config *config);

#endif
