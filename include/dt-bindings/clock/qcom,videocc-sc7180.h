/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SC7180_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SC7180_H

/* VIDEO_CC clocks */
#define VIDEO_PLL0				0
#define VIDEO_CC_APB_CLK			1
#define VIDEO_CC_VCODEC0_AXI_CLK		2
#define VIDEO_CC_VCODEC0_CORE_CLK		3
#define VIDEO_CC_VENUS_AHB_CLK			4
#define VIDEO_CC_VENUS_CLK_SRC			5
#define VIDEO_CC_VENUS_CTL_AXI_CLK		6
#define VIDEO_CC_VENUS_CTL_CORE_CLK		7
#define VIDEO_CC_XO_CLK				8

/* VIDEO_CC GDSCRs */
#define VENUS_GDSC				0
#define VCODEC0_GDSC				1

#endif
