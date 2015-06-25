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

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <asm/div64.h>

#include "clk-qcapll.h"

#define PLL_CONFIG1_SRESET_L		BIT(0)
#define PLL_CONFIG1_REG_BYPASS		BIT(2)
#define PLL_MODULATION_START		BIT(0)

#define PLL_POSTDIV_MASK	0x380
#define PLL_POSTDIV_SHFT	7
#define PLL_REFDIV_MASK		0x7
#define PLL_REFDIV_SHFT		0
#define PLL_TGT_INT_SHFT	1
#define PLL_TGT_INT_MASK	0x3FE
#define PLL_TGT_FRAC_MASK	0x1FFFF800
#define PLL_TGT_FRAC_SHFT	11

#define PLL_CONFIG_PLLPWD BIT(1)

static int clk_qcapll_enable(struct clk_hw *hw)
{
	struct clk_qcapll *pll = to_clk_qcapll(hw);
	int ret;

	/* Disable PLL bypass mode. */
	ret = regmap_update_bits(pll->clkr.regmap, pll->config_reg,
				 PLL_CONFIG_PLLPWD, PLL_CONFIG_PLLPWD);
	if (ret)
		return ret;

	return 0;
}

static void clk_qcapll_disable(struct clk_hw *hw)
{
	struct clk_qcapll *pll = to_clk_qcapll(hw);

	/* Enable PLL bypass mode. */
	regmap_update_bits(pll->clkr.regmap, pll->config_reg, PLL_CONFIG_PLLPWD,
			   0);
}

static unsigned long
clk_qcapll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_qcapll *pll = to_clk_qcapll(hw);
	u32 ref_div, post_plldiv, tgt_div_frac, tgt_div_int;
	u32 rate, config, mod_reg;

	regmap_read(pll->clkr.regmap, pll->config_reg, &config);
	regmap_read(pll->clkr.regmap, pll->current_mod_pll_reg, &mod_reg);

	ref_div = (config >> PLL_REFDIV_SHFT) & PLL_REFDIV_MASK;
	post_plldiv = (config >> PLL_POSTDIV_SHFT) & PLL_POSTDIV_MASK;
	tgt_div_frac = (mod_reg >> PLL_TGT_FRAC_SHFT) & PLL_TGT_FRAC_MASK;
	tgt_div_int = (mod_reg >> PLL_TGT_INT_SHFT) & PLL_TGT_INT_MASK;

	/*FICO = (Fref / (refdiv+1)) * (Ninv + Nfrac[17:5]/2^13
	   + Nfrac[4:0]/(25*2^13)). */
	rate = parent_rate / (ref_div + 1);

	if ((tgt_div_frac & 0x1f) && (tgt_div_frac >> 5)) {
		rate *= tgt_div_int + ((tgt_div_frac >> 5) / 8192) +
		    ((tgt_div_frac & 0x1f) / (8192 * 25));
	} else {
		rate *= tgt_div_int;
	}
	return rate;
}

static const
struct pll_freq_tbl *find_freq(const struct pll_freq_tbl *f, unsigned long rate)
{
	if (!f)
		return NULL;

	for (; f->freq; f++)
		if (rate <= f->freq)
			return f;

	return NULL;
}

static long
clk_qcapll_determine_rate(struct clk_hw *hw, unsigned long rate,
			  unsigned long *p_rate, struct clk **p)
{
	struct clk_qcapll *pll = to_clk_qcapll(hw);
	const struct pll_freq_tbl *f;

	f = find_freq(pll->freq_tbl, rate);
	if (!f)
		return clk_qcapll_recalc_rate(hw, *p_rate);

	return f->freq;
}

static int
clk_qcapll_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long p_rate)
{
	struct clk_qcapll *pll = to_clk_qcapll(hw);
	const struct pll_freq_tbl *f;
	u32 val, mask;
	int ret;

	f = find_freq(pll->freq_tbl, rate);
	if (!f)
		return -EINVAL;

	val = f->postplldiv << PLL_POSTDIV_SHFT;
	val |= f->refdiv << PLL_REFDIV_SHFT;

	mask = PLL_POSTDIV_MASK | PLL_REFDIV_MASK;
	ret = regmap_update_bits(pll->clkr.regmap, pll->config_reg, mask, val);
	if (ret)
		return ret;

	val = f->tgt_div_int << PLL_TGT_INT_SHFT;
	val |= f->tgt_div_frac << PLL_TGT_FRAC_SHFT;

	mask = PLL_TGT_FRAC_MASK | PLL_TGT_INT_MASK;
	regmap_update_bits(pll->clkr.regmap, pll->mod_reg, mask, val);
	if (ret)
		return ret;

	/* Start the PLL start the Modulation. */
	ret = regmap_update_bits(pll->clkr.regmap, pll->mod_reg,
				 PLL_MODULATION_START, 1);
	if (ret)
		return ret;

	/* Wait until PLL is locked. */
	udelay(50);

	return 0;
}

const struct clk_ops clk_qcapll_ops = {
	.enable = clk_qcapll_enable,
	.disable = clk_qcapll_disable,
	.recalc_rate = clk_qcapll_recalc_rate,
	.determine_rate = clk_qcapll_determine_rate,
	.set_rate = clk_qcapll_set_rate,
};

EXPORT_SYMBOL_GPL(clk_qcapll_ops);

static void clk_qcapll_configure(struct clk_qcapll *pll, struct regmap *regmap,
				 const struct qcapll_config *config)
{
	u32 val;
	u32 mask;

	/*Reset the QCAPLL */
	regmap_write(regmap, pll->config1_reg,
		     !(config->audio_pll_config1_sreset_l_mask));
	udelay(2);
	regmap_write(regmap, pll->config1_reg,
		     config->audio_pll_config1_sreset_l_mask);

	val = config->pll_config_postplldiv;
	val |= config->pll_config_refdiv;

	mask = config->pll_config_postplldiv_mask;
	mask |= config->pll_config_refdiv_mask;
	regmap_update_bits(regmap, pll->config_reg, mask, val);

	val = config->pll_modulation_tgt_div_frac;
	val |= config->pll_modulation_tgt_div_int;

	mask = config->pll_modulation_tgt_div_frac_mask;
	mask |= config->pll_modulation_tgt_div_int_mask;
	regmap_update_bits(regmap, pll->mod_reg, mask, val);
}

void clk_qcapll_configure_adss(struct clk_qcapll *pll, struct regmap *regmap,
			       const struct qcapll_config *config)
{
	clk_qcapll_configure(pll, regmap, config);
}

EXPORT_SYMBOL_GPL(clk_qcapll_configure_adss);
