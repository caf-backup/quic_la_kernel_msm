/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _DP_DISPLAY_H_
#define _DP_DISPLAY_H_

#include <drm/drmP.h>

#include "dp_panel.h"

struct msm_dp {
	struct drm_device *drm_dev;
	struct dp_bridge *dp_bridge;
	struct drm_connector *connector;
	struct drm_bridge *bridge;

	/* the encoder we are hooked to (outside of dsi block) */
	struct drm_encoder *encoder;
	bool is_connected;
	u32 max_pclk_khz;

	int (*enable)(struct msm_dp *dp_display);
	int (*post_enable)(struct msm_dp *dp_display);

	int (*pre_disable)(struct msm_dp *dp_display);
	int (*disable)(struct msm_dp *dp_display);

	int (*set_mode)(struct msm_dp *dp_display,
			struct dp_display_mode *mode);
	int (*validate_mode)(struct msm_dp *dp_display, u32 mode_pclk_khz);
	int (*get_modes)(struct msm_dp *dp_display,
		struct dp_display_mode *dp_mode);
	int (*prepare)(struct msm_dp *dp_display);
	int (*unprepare)(struct msm_dp *dp_display);
	int (*request_irq)(struct msm_dp *dp_display);
	struct dp_debug *(*get_debug)(struct msm_dp *dp_display);
	void (*send_hpd_event)(struct msm_dp *dp_display);
	bool (*is_video_test)(struct msm_dp *dp_display);
	int (*get_test_bpp)(struct msm_dp *dp_display);
};

int dp_display_get_num_of_displays(void);
int dp_display_get_displays(void **displays, int count);
void __init msm_dp_pll_driver_register(void);
void __exit msm_dp_pll_driver_unregister(void);

#endif /* _DP_DISPLAY_H_ */
