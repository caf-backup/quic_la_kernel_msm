/* * Copyright (c) 2012 Qualcomm Atheros, Inc. * */
/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include "devices.h"
#include "board-ipq806x.h"

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_NONE,
	.drv = GPIOMUX_DRV_8MA,
	.func = GPIOMUX_FUNC_GPIO,
};
#endif

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
struct msm_gpiomux_config ipq806x_ethernet_configs[] = {
	{
		.gpio = 43,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
			[GPIOMUX_ACTIVE] = &gpio_eth_config,
		}
	},
};
#endif

#ifdef CONFIG_MSM_VCAP
static struct gpiomux_setting gpio_vcap_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_2,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_3,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_4,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_5,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_6,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_7,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_8,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_9,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
	{
		.func = GPIOMUX_FUNC_A,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},
};

struct msm_gpiomux_config vcap_configs[] = {
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[7],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[7],
		}
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[1],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[1],
		}
	},
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[8],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[8],
		}
	},
	{
		.gpio = 22,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[7],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[7],
		}
	},
	{
		.gpio = 12,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[6],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[6],
		}
	},
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[9],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[9],
		}
	},
	{
		.gpio = 11,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[10],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[10],
		}
	},
	{
		.gpio = 10,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[9],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[9],
		}
	},
	{
		.gpio = 9,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[1],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[1],
		}
	},
	{
		.gpio = 8,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[3],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[3],
		}
	},
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[7],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[7],
		}
	},
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[7],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[7],
		}
	},
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[1],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[1],
		}
	},
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[4],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[4],
		}
	},
	{
		.gpio = 84,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[3],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[3],
		}
	},
	{
		.gpio = 5,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[3],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[3],
		}
	},
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[6],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[6],
		}
	},
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[5],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[5],
		}
	},
	{
		.gpio = 82,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[4],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[4],
		}
	},
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[4],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[4],
		}
	},
	{
		.gpio = 87,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[2],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[2],
		}
	},
	{
		.gpio = 13,
		.settings = {
			[GPIOMUX_SUSPENDED] =	&gpio_vcap_config[6],
			[GPIOMUX_ACTIVE] =		&gpio_vcap_config[6],
		}
	},
};
#endif

static struct gpiomux_setting ext_regulator_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting gsbi1_suspended_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi1_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi2_suspended_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi2_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi4_suspended_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi4_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5_suspended_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* External 3.3 V regulator enable */
static struct msm_gpiomux_config ipq806x_ext_regulator_configs[] __initdata = {
	{
		.gpio = IPQ806X_EXT_3P3V_REG_EN_GPIO,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ext_regulator_config,
		},
	},
};

static struct gpiomux_setting i2s_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting i2s_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config ipq806x_mi2s_configs[] __initdata = {
	{
		.gpio	= 27,		/* mi2s ws */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 28,		/* mi2s sclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 29,		/* mi2s dout3 - TX*/
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
	{
		.gpio	= 32,		/* mi2s dout0 - RX */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},

	{
		.gpio	= 33,		/* mi2s mclk */
		.settings = {
			[GPIOMUX_ACTIVE]    = &i2s_act_cfg,
			[GPIOMUX_SUSPENDED] = &i2s_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config ipq806x_gsbi1_i2c_configs[] __initdata = {
	{
		.gpio      = 53,			/* GSBI1 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi1_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi1_active_cfg,
		},
	},
	{
		.gpio      = 54,			/* GSBI1 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi1_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi1_active_cfg,
		},
	},
};

static struct msm_gpiomux_config ipq806x_gsbi2_i2c_configs[] __initdata = {
	{
		.gpio      = 24,			/* GSBI1 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi2_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi2_active_cfg,
		},
	},
	{
		.gpio      = 25,			/* GSBI1 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi2_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi2_active_cfg,
		},
	},
};

static struct msm_gpiomux_config ipq806x_gsbi4_i2c_configs[] __initdata = {
	{
		.gpio      = 12,			/* GSBI4 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi4_active_cfg,
		},
	},
	{
		.gpio      = 13,			/* GSBI4 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi4_active_cfg,
		},
	},
};

#ifdef CONFIG_SPI_QUP
/* GSBI5 pin configuration */
static struct gpiomux_setting gsbi5_spi_clk_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gsbi5_spi_cs_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gsbi5_spi_data_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config ipq806x_gsbi5_spi_configs[] __initdata = {
	{
		.gpio      = 21,                        /* GSBI5 SPI CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gsbi5_spi_clk_cfg,
			[GPIOMUX_SUSPENDED] = &gsbi5_spi_clk_cfg,
		},
	},
	{
		.gpio      = 20,                        /* GSBI5 SPI CS */
		.settings = {
			[GPIOMUX_ACTIVE] = &gsbi5_spi_cs_cfg,
			[GPIOMUX_SUSPENDED] = &gsbi5_spi_cs_cfg,
		},
	},
	{
		.gpio      = 19,                        /* GSBI5 SPI MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gsbi5_spi_data_cfg,
			[GPIOMUX_SUSPENDED] = &gsbi5_spi_data_cfg,
		},
	},
	{
		.gpio      = 18,                        /* GSBI5 SPI MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gsbi5_spi_data_cfg,
			[GPIOMUX_SUSPENDED] = &gsbi5_spi_data_cfg,
		},
	},
};
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct gpiomux_setting sdc1_clk_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc1_cmd_data_0_3_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc1_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc1_data_1_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config ipq806x_sdc1_configs[] __initdata = {
	{
		.gpio      = 42,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_clk_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
	{
		.gpio      = 45,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},

	},
	{
		.gpio      = 44,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
	{
		.gpio      = 43,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_data_1_suspended_cfg,
		},
	},
	{
		.gpio      = 41,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
	{
		.gpio      = 40,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	{
		.gpio      = 47,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
	{
		.gpio      = 46,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
	{
		.gpio      = 39,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
	{
		.gpio      = 38,
		.settings = {
			[GPIOMUX_ACTIVE] = &sdc1_cmd_data_0_3_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc1_suspended_cfg,
		},
	},
#endif

};
#endif

static struct gpiomux_setting ipq806x_sdc3_card_det_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config ipq806x_sdc3_configs[] __initdata = {
	{
		.gpio      = 26,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ipq806x_sdc3_card_det_cfg,
			[GPIOMUX_ACTIVE] = &ipq806x_sdc3_card_det_cfg,
		},
	},
};


#ifdef CONFIG_MSM_PCIE
static struct gpiomux_setting pcie_rst_n = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting pcie_pwr_en = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config ipq806x_pcie_configs[] __initdata = {
	{
		.gpio      = PCIE_RST_GPIO,
		.settings = {
		[GPIOMUX_SUSPENDED] = &pcie_rst_n,
		[GPIOMUX_ACTIVE] = &pcie_rst_n,
		},
	},
	{
		.gpio      = PCIE_PWR_EN_GPIO,
		.settings = {
		[GPIOMUX_SUSPENDED] = &pcie_pwr_en,
		[GPIOMUX_ACTIVE] = &pcie_pwr_en,
		},
	},
};

#endif

void __init ipq806x_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return;
	}

	if (machine_is_ipq806x_rumi3()) {
		msm_gpiomux_install(ipq806x_gsbi2_i2c_configs,
				ARRAY_SIZE(ipq806x_gsbi2_i2c_configs));
	}

	if (machine_is_ipq806x_db149()) {
		msm_gpiomux_install(ipq806x_gsbi1_i2c_configs,
				ARRAY_SIZE(ipq806x_gsbi1_i2c_configs));
		msm_gpiomux_install(ipq806x_gsbi2_i2c_configs,
				ARRAY_SIZE(ipq806x_gsbi2_i2c_configs));
		msm_gpiomux_install(ipq806x_gsbi4_i2c_configs,
				ARRAY_SIZE(ipq806x_gsbi4_i2c_configs));
#ifdef CONFIG_MSM_VCAP
		msm_gpiomux_install(vcap_configs,
				ARRAY_SIZE(vcap_configs));
#endif
		msm_gpiomux_install(ipq806x_mi2s_configs,
			ARRAY_SIZE(ipq806x_mi2s_configs));
	}

#ifdef CONFIG_SPI_QUP
	msm_gpiomux_install(ipq806x_gsbi5_spi_configs,
			ARRAY_SIZE(ipq806x_gsbi5_spi_configs));
#endif

	msm_gpiomux_install(ipq806x_ext_regulator_configs,
			ARRAY_SIZE(ipq806x_ext_regulator_configs));
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	msm_gpiomux_install(ipq806x_sdc1_configs,
			ARRAY_SIZE(ipq806x_sdc1_configs));
#endif
	msm_gpiomux_install(ipq806x_sdc3_configs,
			ARRAY_SIZE(ipq806x_sdc3_configs));
#ifdef CONFIG_MSM_PCIE
	msm_gpiomux_install(ipq806x_pcie_configs,
			ARRAY_SIZE(ipq806x_pcie_configs));

#endif
}
