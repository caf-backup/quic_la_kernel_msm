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

#define pr_fmt(fmt)	"[drm-dp]: %s: " fmt, __func__

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "msm_kms.h"
//#include "dpu_connector.h"
#include "dp_drm.h"
#include "dp_debug.h"

struct dp_connector {
	struct drm_connector base;
	struct msm_dp *dp_display;
};
#define to_dp_connector(x) container_of(x, struct dp_connector, base)
#define to_dp_bridge(x)     container_of((x), struct dp_bridge, base)

static void convert_to_dp_mode(const struct drm_display_mode *drm_mode,
			struct dp_display_mode *dp_mode, struct msm_dp *dp)
{
	const u32 num_components = 3;

	memset(dp_mode, 0, sizeof(*dp_mode));

	dp_mode->timing.h_active = drm_mode->hdisplay;
	dp_mode->timing.h_back_porch = drm_mode->htotal - drm_mode->hsync_end;
	dp_mode->timing.h_sync_width = drm_mode->htotal -
			(drm_mode->hsync_start + dp_mode->timing.h_back_porch);
	dp_mode->timing.h_front_porch = drm_mode->hsync_start -
					 drm_mode->hdisplay;
	dp_mode->timing.h_skew = drm_mode->hskew;

	dp_mode->timing.v_active = drm_mode->vdisplay;
	dp_mode->timing.v_back_porch = drm_mode->vtotal - drm_mode->vsync_end;
	dp_mode->timing.v_sync_width = drm_mode->vtotal -
		(drm_mode->vsync_start + dp_mode->timing.v_back_porch);

	dp_mode->timing.v_front_porch = drm_mode->vsync_start -
					 drm_mode->vdisplay;

	if (dp->is_video_test(dp))
		dp_mode->timing.bpp = dp->get_test_bpp(dp);
	else
		dp_mode->timing.bpp = dp->connector->display_info.bpc *
		num_components;

	if (!dp_mode->timing.bpp)
		dp_mode->timing.bpp = 24;

	dp_mode->timing.refresh_rate = drm_mode->vrefresh;

	dp_mode->timing.pixel_clk_khz = drm_mode->clock;

	dp_mode->timing.v_active_low =
		!!(drm_mode->flags & DRM_MODE_FLAG_NVSYNC);

	dp_mode->timing.h_active_low =
		!!(drm_mode->flags & DRM_MODE_FLAG_NHSYNC);
}

static void convert_to_drm_mode(const struct dp_display_mode *dp_mode,
				struct drm_display_mode *drm_mode)
{
	u32 flags = 0;

	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = dp_mode->timing.h_active;
	drm_mode->hsync_start = drm_mode->hdisplay +
				dp_mode->timing.h_front_porch;
	drm_mode->hsync_end = drm_mode->hsync_start +
			      dp_mode->timing.h_sync_width;
	drm_mode->htotal = drm_mode->hsync_end + dp_mode->timing.h_back_porch;
	drm_mode->hskew = dp_mode->timing.h_skew;

	drm_mode->vdisplay = dp_mode->timing.v_active;
	drm_mode->vsync_start = drm_mode->vdisplay +
				dp_mode->timing.v_front_porch;
	drm_mode->vsync_end = drm_mode->vsync_start +
			      dp_mode->timing.v_sync_width;
	drm_mode->vtotal = drm_mode->vsync_end + dp_mode->timing.v_back_porch;

	drm_mode->vrefresh = dp_mode->timing.refresh_rate;
	drm_mode->clock = dp_mode->timing.pixel_clk_khz;

	if (dp_mode->timing.h_active_low)
		flags |= DRM_MODE_FLAG_NHSYNC;
	else
		flags |= DRM_MODE_FLAG_PHSYNC;

	if (dp_mode->timing.v_active_low)
		flags |= DRM_MODE_FLAG_NVSYNC;
	else
		flags |= DRM_MODE_FLAG_PVSYNC;

	drm_mode->flags = flags;

	drm_mode->type = 0x48;
	drm_mode_set_name(drm_mode);
}

static void dp_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct msm_dp *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	/* By this point mode should have been validated through mode_fixup */
	rc = dp->set_mode(dp, &bridge->dp_mode);
	if (rc) {
		pr_err("[%d] failed to perform a mode set, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->prepare(dp);
	if (rc) {
		pr_err("[%d] DP display prepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->enable(dp);
	if (rc) {
		pr_err("[%d] DP display enable failed, rc=%d\n",
		       bridge->id, rc);
		dp->unprepare(dp);
	}
}

static void dp_bridge_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct msm_dp *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	rc = dp->post_enable(dp);
	if (rc)
		pr_err("[%d] DP display post enable failed, rc=%d\n",
		       bridge->id, rc);
}

static void dp_bridge_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct msm_dp *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	rc = dp->pre_disable(dp);
	if (rc) {
		pr_err("[%d] DP display pre disable failed, rc=%d\n",
		       bridge->id, rc);
	}
}

static void dp_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct msm_dp *dp;

	if (!drm_bridge) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	rc = dp->disable(dp);
	if (rc) {
		pr_err("[%d] DP display disable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->unprepare(dp);
	if (rc) {
		pr_err("[%d] DP display unprepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}
}

static void dp_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dp_bridge *bridge;
	struct msm_dp *dp;

	if (!drm_bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	memset(&bridge->dp_mode, 0x0, sizeof(struct dp_display_mode));
	convert_to_dp_mode(adjusted_mode, &bridge->dp_mode, dp);
}

static bool dp_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	bool ret = true;
	struct dp_display_mode dp_mode;
	struct dp_bridge *bridge;
	struct msm_dp *dp;

	if (!drm_bridge || !mode || !adjusted_mode) {
		pr_err("Invalid params\n");
		ret = false;
		goto end;
	}

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	convert_to_dp_mode(mode, &dp_mode, dp);
	convert_to_drm_mode(&dp_mode, adjusted_mode);
end:
	return ret;
}

static const struct drm_bridge_funcs dp_bridge_ops = {
	.mode_fixup   = dp_bridge_mode_fixup,
	.pre_enable   = dp_bridge_pre_enable,
	.enable       = dp_bridge_enable,
	.disable      = dp_bridge_disable,
	.post_disable = dp_bridge_post_disable,
	.mode_set     = dp_bridge_mode_set,
};

int dp_connector_post_init(struct drm_connector *connector, void *display)
{
	struct msm_dp *dp_display = display;

	if (!dp_display)
		return -EINVAL;

	dp_display->connector = connector;
	return 0;
}

/**
 * dp_connector_detect - callback to determine if connector is connected
 * @connector: Pointer to drm connector structure
 * @force: Force detect setting from drm framework
 * Returns: Connector 'is connected' status
 */
static enum drm_connector_status dp_connector_detect(struct drm_connector *conn,
		bool force)
{
	enum drm_connector_status status = connector_status_unknown;
	static enum drm_connector_status current_status = connector_status_unknown;
	struct msm_dp *dp;
	struct dp_connector *dp_conn;
	struct msm_drm_private *priv = conn->dev->dev_private;
	struct msm_kms *kms = priv->kms;

	dp_conn = to_dp_connector(conn);
	dp = dp_conn->dp_display;

	pr_err("is_connected = %s\n",
		(dp->is_connected) ? "true" : "false");

	status = (dp->is_connected) ? connector_status_connected :
					connector_status_disconnected;

	if (dp->is_connected && dp->bridge->encoder
				&& (current_status != status)
				&& kms->funcs->set_encoder_mode) {
		kms->funcs->set_encoder_mode(kms,
				dp->bridge->encoder, false);
		pr_err("call set_encoder_mode\n");
	}
	current_status = status;
	return status;
}

static void dp_connector_destroy(struct drm_connector *connector)
{
	struct dp_connector *dp_conn = to_dp_connector(connector);

	DBG("");

	drm_connector_cleanup(connector);

	kfree(dp_conn);
}

void dp_connector_send_hpd_event(void *display)
{
	struct msm_dp *dp;

	if (!display) {
		pr_err("invalid input\n");
		return;
	}

	dp = display;

	if (dp->send_hpd_event)
		dp->send_hpd_event(dp);
}

/**
 * dp_connector_get_modes - callback to add drm modes via drm_mode_probed_add()
 * @connector: Pointer to drm connector structure
 * Returns: Number of modes added
 */
static int dp_connector_get_modes(struct drm_connector *connector)
{
	int rc = 0;
	struct msm_dp *dp;
	struct dp_display_mode *dp_mode = NULL;
	struct drm_display_mode *m, drm_mode;
	struct dp_connector *dp_conn;

	if (!connector)
		return 0;

	dp_conn = to_dp_connector(connector);
	dp = dp_conn->dp_display;

	dp_mode = kzalloc(sizeof(*dp_mode),  GFP_KERNEL);
	if (!dp_mode)
		return 0;

	/* pluggable case assumes EDID is read when HPD */
	if (dp->is_connected) {
		rc = dp->get_modes(dp, dp_mode);
		if (!rc)
			pr_err("failed to get DP sink modes, rc=%d\n", rc);

		if (dp_mode->timing.pixel_clk_khz) { /* valid DP mode */
			memset(&drm_mode, 0x0, sizeof(drm_mode));
			convert_to_drm_mode(dp_mode, &drm_mode);
			m = drm_mode_duplicate(connector->dev, &drm_mode);
			if (!m) {
				pr_err("failed to add mode %ux%u\n",
				       drm_mode.hdisplay,
				       drm_mode.vdisplay);
				kfree(dp_mode);
				return 0;
			}
			m->width_mm = connector->display_info.width_mm;
			m->height_mm = connector->display_info.height_mm;
			drm_mode_probed_add(connector, m);
		}
	} else {
		pr_err("No sink connected\n");
	}
	kfree(dp_mode);

	return rc;
}

int dp_drm_bridge_init(void *data, struct drm_encoder *encoder)
{
	int rc = 0;
	struct dp_bridge *dp_bridge;
	struct drm_device *dev;
	struct msm_dp *dp_display = data;
	struct msm_drm_private *priv = NULL;

	dp_bridge = kzalloc(sizeof(*dp_bridge), GFP_KERNEL);
	if (!dp_bridge) {
		rc = -ENOMEM;
		goto error;
	}

	dev = dp_display->drm_dev;
	dp_bridge->display = dp_display;
	dp_bridge->base.funcs = &dp_bridge_ops;
	dp_bridge->base.encoder = encoder;

	priv = dev->dev_private;

	rc = drm_bridge_attach(encoder, &dp_bridge->base, NULL);
	if (rc) {
		pr_err("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	rc = dp_display->request_irq(dp_display);
	if (rc) {
		pr_err("request_irq failed, rc=%d\n", rc);
		goto error_free_bridge;
	}

	encoder->bridge = &dp_bridge->base;
	dp_display->dp_bridge = dp_bridge;
	dp_display->bridge = &dp_bridge->base;

	return 0;
error_free_bridge:
	kfree(dp_bridge);
error:
	return rc;
}

void dp_drm_bridge_deinit(void *data)
{
	struct msm_dp *display = data;
	struct dp_bridge *bridge = display->dp_bridge;

	if (bridge && bridge->base.encoder)
		bridge->base.encoder->bridge = NULL;

	kfree(bridge);
}

/**
 * dp_connector_mode_valid - callback to determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * Returns: Validity status for specified mode
 */
static enum drm_mode_status dp_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	struct msm_dp *dp_disp;
	struct dp_debug *debug;
	struct dp_connector *dp_conn;

	if (!mode || !connector) {
		pr_err("invalid params\n");
		return MODE_ERROR;
	}

	dp_conn = to_dp_connector(connector);
	dp_disp = dp_conn->dp_display;
	debug = dp_disp->get_debug(dp_disp);

	mode->vrefresh = drm_mode_vrefresh(mode);

	if (mode->clock > dp_disp->max_pclk_khz)
		return MODE_BAD;

	if (debug->debug_en && (mode->hdisplay != debug->hdisplay ||
			mode->vdisplay != debug->vdisplay ||
			mode->vrefresh != debug->vrefresh ||
			mode->picture_aspect_ratio != debug->aspect_ratio))
		return MODE_BAD;

	return dp_disp->validate_mode(dp_disp, mode->clock);
}

static const struct drm_connector_funcs dp_connector_funcs = {
	.detect = dp_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs dp_connector_helper_funcs = {
	.get_modes = dp_connector_get_modes,
	.mode_valid = dp_connector_mode_valid,
};

/* connector initialization */
struct drm_connector *dp_drm_connector_init(struct msm_dp *dp_display)
{
	struct drm_connector *connector = NULL;
	struct dp_connector *dp_connector;
	int ret;

	dp_connector = kzalloc(sizeof(*dp_connector), GFP_KERNEL);
	if (!dp_connector)
		return ERR_PTR(-ENOMEM);

	dp_connector->dp_display = dp_display;

	connector = &dp_connector->base;

	ret = drm_connector_init(dp_display->drm_dev, connector, &dp_connector_funcs,
			DRM_MODE_CONNECTOR_DisplayPort);
	if (ret)
		return ERR_PTR(ret);

	drm_connector_helper_add(connector, &dp_connector_helper_funcs);

	/*
	 * Enable HPD to let hpd event is handled
	 * when cable is attached to the host.
	 */
	connector->polled = DRM_CONNECTOR_POLL_HPD;

	/* Display driver doesn't support interlace now. */
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	drm_connector_attach_encoder(connector, dp_display->encoder);

	return connector;
}
