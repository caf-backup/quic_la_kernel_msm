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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>

#include "msm_drv.h"
#include "dp_extcon.h"
#include "dp_parser.h"
#include "dp_power.h"
#include "dp_catalog.h"
#include "dp_aux.h"
#include "dp_link.h"
#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_display.h"
#include "dp_drm.h"
#include "dp_debug.h"

static struct msm_dp *g_dp_display;
#define HPD_STRING_SIZE 30

struct dp_display_private {
	char *name;
	int irq;

	/* state variables */
	bool core_initialized;
	bool power_on;
	bool hpd_irq_on;
	bool audio_supported;

	struct platform_device *pdev;
	struct dentry *root;
	struct completion notification_comp;

	struct dp_usbpd   *usbpd;
	struct dp_parser  *parser;
	struct dp_power   *power;
	struct dp_catalog *catalog;
	struct dp_aux     *aux;
	struct dp_link    *link;
	struct dp_panel   *panel;
	struct dp_ctrl    *ctrl;
	struct dp_debug   *debug;

	struct dp_usbpd_cb usbpd_cb;
	struct dp_display_mode mode;
	struct msm_dp dp_display;

};

static const struct of_device_id dp_dt_match[] = {
	{.compatible = "qcom,dp-display"},
	{}
};

static int dp_get_pll(struct dp_display_private *dp_priv)
{
	struct platform_device *pdev = NULL;
	struct platform_device *pll_pdev;
	struct device_node *pll_node;
	struct dp_parser *dp_parser = NULL;

	if (!dp_priv) {
		pr_err("Invalid Arguments\n");
		return -EINVAL;
	}

	pdev = dp_priv->pdev;
	dp_parser = dp_priv->parser;

	if (!dp_parser) {
		pr_err("Parser not initialized.\n");
		return -EINVAL;
	}

	pll_node = of_parse_phandle(pdev->dev.of_node, "pll-node", 0);
	if (!pll_node) {
		dev_err(&pdev->dev, "cannot find pll device\n");
		return -ENXIO;
	}

	pll_pdev = of_find_device_by_node(pll_node);
	if (pll_pdev)
		dp_parser->pll = platform_get_drvdata(pll_pdev);

	of_node_put(pll_node);

	if (!pll_pdev || !dp_parser->pll) {
		dev_err(&pdev->dev, "%s: pll driver is not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	dp_parser->pll_dev = get_device(&pll_pdev->dev);

	return 0;
}

static irqreturn_t dp_display_irq(int irq, void *dev_id)
{
	struct dp_display_private *dp = dev_id;

	if (!dp) {
		pr_err("invalid data\n");
		return IRQ_NONE;
	}

	/* DP controller isr */
	dp->ctrl->isr(dp->ctrl);

	/* DP aux isr */
	dp->aux->isr(dp->aux);

	return IRQ_HANDLED;
}

static int dp_display_bind(struct device *dev, struct device *master,
			   void *data)
{
	int rc = 0;
	struct dp_display_private *dp;
	struct drm_device *drm;
	struct msm_drm_private *priv;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		rc = -EINVAL;
		goto end;
	}

	drm = dev_get_drvdata(master);
	dp = platform_get_drvdata(pdev);
	if (!drm || !dp) {
		pr_err("invalid param(s), drm %pK, dp %pK\n",
				drm, dp);
		rc = -EINVAL;
		goto end;
	}

	dp->dp_display.drm_dev = drm;
	priv = drm->dev_private;
	priv->dp = &(dp->dp_display);

	rc = dp->parser->parse(dp->parser);
	if (rc) {
		pr_err("device tree parsing failed\n");
		goto end;
	}

	rc = dp_get_pll(dp);
	if (rc) {
		pr_err(" DP get PLL instance failed\n");
		goto end;
	}

	rc = dp->aux->drm_aux_register(dp->aux);
	if (rc) {
		pr_err("DRM DP AUX register failed\n");
		goto end;
	}

	rc = dp->power->power_client_init(dp->power);
	if (rc) {
		pr_err("Power client create failed\n");
		goto end;
	}

	//rc = dp_extcon_register(dp->usbpd);
	if (rc) {
		pr_err("extcon registration failed\n");
		goto end;
	}

end:
	return rc;
}

static void dp_display_unbind(struct device *dev, struct device *master,
			      void *data)
{
	struct dp_display_private *dp;
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = dev_get_drvdata(master);
	struct msm_drm_private *priv = drm->dev_private;

	if (!dev || !pdev) {
		pr_err("invalid param(s)\n");
		return;
	}

	dp = platform_get_drvdata(pdev);
	if (!dp) {
		pr_err("Invalid params\n");
		return;
	}

	//dp_extcon_unregister(dp->usbpd);

	(void)dp->power->power_client_deinit(dp->power);
	(void)dp->aux->drm_aux_deregister(dp->aux);
	priv->dp = NULL;
}

static const struct component_ops dp_display_comp_ops = {
	.bind = dp_display_bind,
	.unbind = dp_display_unbind,
};

static bool dp_display_is_ds_bridge(struct dp_panel *panel)
{
	return (panel->dpcd[DP_DOWNSTREAMPORT_PRESENT] &
		DP_DWN_STRM_PORT_PRESENT);
}

static bool dp_display_is_sink_count_zero(struct dp_display_private *dp)
{
	return dp_display_is_ds_bridge(dp->panel) &&
		(dp->link->sink_count.count == 0);
}

static void dp_display_send_hpd_event(struct msm_dp *dp_display)
{
	struct dp_display_private *dp;
	struct drm_connector *connector;

	if (!dp_display) {
		pr_err("invalid input\n");
		return;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);
	if (!dp) {
		pr_err("invalid params\n");
		return;
	}

	connector = dp->dp_display.connector;
	drm_helper_hpd_irq_event(connector->dev);
}

static int dp_display_send_hpd_notification(struct dp_display_private *dp,
					    bool hpd)
{
	if ((hpd && dp->dp_display.is_connected) ||
			(!hpd && !dp->dp_display.is_connected)) {
		pr_info("HPD already %s\n", (hpd ? "on" : "off"));
		return 0;
	}

	/* reset video pattern flag on disconnect */
	if (!hpd)
		dp->panel->video_test = false;

	dp->dp_display.is_connected = hpd;
	reinit_completion(&dp->notification_comp);
	dp_display_send_hpd_event(&dp->dp_display);

	if (!wait_for_completion_timeout(&dp->notification_comp, HZ * 2)) {
		pr_warn("%s timeout\n", hpd ? "connect" : "disconnect");
		return -EINVAL;
	}

	return 0;
}

static int dp_display_process_hpd_high(struct dp_display_private *dp)
{
	int rc = 0;
	struct edid *edid;

	dp->aux->init(dp->aux, dp->parser->aux_cfg);

	if (dp->link->psm_enabled)
		goto notify;

	rc = dp->panel->read_sink_caps(dp->panel, dp->dp_display.connector);
	if (rc)
		goto notify;

	dp->link->process_request(dp->link);

	if (dp_display_is_sink_count_zero(dp)) {
		pr_debug("no downstream devices connected\n");
		rc = -EINVAL;
		goto end;
	}

	edid = dp->panel->edid;

	dp->audio_supported = drm_detect_monitor_audio(edid);

	dp->panel->handle_sink_request(dp->panel);

	dp->dp_display.max_pclk_khz = dp->parser->max_pclk_khz;
notify:
	dp_display_send_hpd_notification(dp, true);

end:
	return rc;
}

static void dp_display_host_init(struct dp_display_private *dp)
{
	bool flip = false;

	if (dp->core_initialized) {
		pr_debug("DP core already initialized\n");
		return;
	}

	if (dp->usbpd->orientation == ORIENTATION_CC2)
		flip = true;

	dp->power->init(dp->power, flip);
	dp->ctrl->init(dp->ctrl, flip);
	enable_irq(dp->irq);
	dp->core_initialized = true;
}

static void dp_display_host_deinit(struct dp_display_private *dp)
{
	if (!dp->core_initialized) {
		pr_debug("DP core already off\n");
		return;
	}

	dp->ctrl->deinit(dp->ctrl);
	dp->power->deinit(dp->power);
	disable_irq(dp->irq);
	dp->core_initialized = false;
}

static void dp_display_process_hpd_low(struct dp_display_private *dp)
{
	/* cancel any pending request */
	dp->ctrl->abort(dp->ctrl);

	dp_display_send_hpd_notification(dp, false);

	dp->aux->deinit(dp->aux);
}

static int dp_display_usbpd_configure_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	dp_display_host_init(dp);

	if (dp->usbpd->hpd_high)
		dp_display_process_hpd_high(dp);
end:
	return rc;
}

static void dp_display_clean(struct dp_display_private *dp)
{
	dp->ctrl->push_idle(dp->ctrl);
	dp->ctrl->off(dp->ctrl);
}

static int dp_display_usbpd_disconnect_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		rc = -EINVAL;
		goto end;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		rc = -ENODEV;
		goto end;
	}

	/* cancel any pending request */
	dp->ctrl->abort(dp->ctrl);

	rc = dp_display_send_hpd_notification(dp, false);

	/* if cable is disconnected, reset psm_enabled flag */
	if (!dp->usbpd->alt_mode_cfg_done)
		dp->link->psm_enabled = false;

	if ((rc < 0) && dp->power_on)
		dp_display_clean(dp);

	dp_display_host_deinit(dp);
end:
	return rc;
}

static void dp_display_handle_video_request(struct dp_display_private *dp)
{
	if (dp->link->sink_request & DP_TEST_LINK_VIDEO_PATTERN) {
		/* force disconnect followed by connect */
		dp->usbpd->connect(dp->usbpd, false);
		dp->panel->video_test = true;
		dp->usbpd->connect(dp->usbpd, true);
		dp->link->send_test_response(dp->link);
	}
}

static int dp_display_handle_hpd_irq(struct dp_display_private *dp)
{
	if (dp->link->sink_request & DS_PORT_STATUS_CHANGED) {
		dp_display_send_hpd_notification(dp, false);

		if (dp_display_is_sink_count_zero(dp)) {
			pr_debug("sink count is zero, nothing to do\n");
			return 0;
		}

		return dp_display_process_hpd_high(dp);
	}

	dp->ctrl->handle_sink_request(dp->ctrl);

	dp_display_handle_video_request(dp);

	return 0;
}

static int dp_display_usbpd_attention_cb(struct device *dev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dev) {
		pr_err("invalid dev\n");
		return -EINVAL;
	}

	dp = dev_get_drvdata(dev);
	if (!dp) {
		pr_err("no driver data found\n");
		return -ENODEV;
	}

	if (dp->usbpd->hpd_irq) {
		dp->hpd_irq_on = true;

		rc = dp->link->process_request(dp->link);
		/* check for any test request issued by sink */
		if (!rc)
			dp_display_handle_hpd_irq(dp);

		dp->hpd_irq_on = false;
		goto end;
	}

	if (!dp->usbpd->hpd_high) {
		dp_display_process_hpd_low(dp);
		goto end;
	}

	if (dp->usbpd->alt_mode_cfg_done)
		dp_display_process_hpd_high(dp);
end:
	return rc;
}

static void dp_display_deinit_sub_modules(struct dp_display_private *dp)
{
	dp_ctrl_put(dp->ctrl);
	dp_link_put(dp->link);
	dp_panel_put(dp->panel);
	dp_aux_put(dp->aux);
	dp_power_put(dp->power);
	dp_catalog_put(dp->catalog);
	dp_parser_put(dp->parser);
	dp_extcon_put(dp->usbpd);
	dp_debug_put(dp->debug);
}

static int dp_init_sub_modules(struct dp_display_private *dp)
{
	int rc = 0;
	struct device *dev = &dp->pdev->dev;
	struct dp_usbpd_cb *cb = &dp->usbpd_cb;
	struct dp_ctrl_in ctrl_in = {
		.dev = dev,
	};
	struct dp_panel_in panel_in = {
		.dev = dev,
	};

	/* Callback APIs used for cable status change event */
	cb->configure  = dp_display_usbpd_configure_cb;
	cb->disconnect = dp_display_usbpd_disconnect_cb;
	cb->attention  = dp_display_usbpd_attention_cb;

	dp->usbpd = dp_extcon_get(dev, cb);
	if (IS_ERR(dp->usbpd)) {
		rc = PTR_ERR(dp->usbpd);
		pr_err("failed to initialize extcon, rc = %d\n", rc);
		dp->usbpd = NULL;
		goto error;
	}

	dp->parser = dp_parser_get(dp->pdev);
	if (IS_ERR(dp->parser)) {
		rc = PTR_ERR(dp->parser);
		pr_err("failed to initialize parser, rc = %d\n", rc);
		dp->parser = NULL;
		goto error_parser;
	}

	dp->catalog = dp_catalog_get(dev, &dp->parser->io);
	if (IS_ERR(dp->catalog)) {
		rc = PTR_ERR(dp->catalog);
		pr_err("failed to initialize catalog, rc = %d\n", rc);
		dp->catalog = NULL;
		goto error_catalog;
	}

	dp->power = dp_power_get(dp->parser);
	if (IS_ERR(dp->power)) {
		rc = PTR_ERR(dp->power);
		pr_err("failed to initialize power, rc = %d\n", rc);
		dp->power = NULL;
		goto error_power;
	}

	dp->aux = dp_aux_get(dev, &dp->catalog->aux, dp->parser->aux_cfg);
	if (IS_ERR(dp->aux)) {
		rc = PTR_ERR(dp->aux);
		pr_err("failed to initialize aux, rc = %d\n", rc);
		dp->aux = NULL;
		goto error_aux;
	}

	dp->link = dp_link_get(dev, dp->aux);
	if (IS_ERR(dp->link)) {
		rc = PTR_ERR(dp->link);
		pr_err("failed to initialize link, rc = %d\n", rc);
		dp->link = NULL;
		goto error_link;
	}

	panel_in.aux = dp->aux;
	panel_in.catalog = &dp->catalog->panel;
	panel_in.link = dp->link;

	dp->panel = dp_panel_get(&panel_in);
	if (IS_ERR(dp->panel)) {
		rc = PTR_ERR(dp->panel);
		pr_err("failed to initialize panel, rc = %d\n", rc);
		dp->panel = NULL;
		goto error_panel;
	}

	ctrl_in.link = dp->link;
	ctrl_in.panel = dp->panel;
	ctrl_in.aux = dp->aux;
	ctrl_in.power = dp->power;
	ctrl_in.catalog = &dp->catalog->ctrl;
	ctrl_in.parser = dp->parser;

	dp->ctrl = dp_ctrl_get(&ctrl_in);
	if (IS_ERR(dp->ctrl)) {
		rc = PTR_ERR(dp->ctrl);
		pr_err("failed to initialize ctrl, rc = %d\n", rc);
		dp->ctrl = NULL;
		goto error_ctrl;
	}

	dp->debug = dp_debug_get(dev, dp->panel, dp->usbpd,
				dp->link, &dp->dp_display.connector);
	if (IS_ERR(dp->debug)) {
		rc = PTR_ERR(dp->debug);
		pr_err("failed to initialize debug, rc = %d\n", rc);
		dp->debug = NULL;
		goto error_debug;
	}

	return rc;
error_debug:
	dp_ctrl_put(dp->ctrl);
error_ctrl:
	dp_panel_put(dp->panel);
error_panel:
	dp_link_put(dp->link);
error_link:
	dp_aux_put(dp->aux);
error_aux:
	dp_power_put(dp->power);
error_power:
	dp_catalog_put(dp->catalog);
error_catalog:
	dp_parser_put(dp->parser);
error_parser:
	dp_extcon_put(dp->usbpd);
error:
	return rc;
}

static int dp_display_set_mode(struct msm_dp *dp_display,
			       struct dp_display_mode *mode)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}
	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->panel->pinfo = mode->timing;
	dp->panel->init_info(dp->panel);
error:
	return rc;
}

static int dp_display_prepare(struct msm_dp *dp)
{
	return 0;
}

static int dp_display_enable(struct msm_dp *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (dp->power_on) {
		pr_debug("Link already setup, return\n");
		return 0;
	}

	dp->aux->init(dp->aux, dp->parser->aux_cfg);

	rc = dp->ctrl->on(dp->ctrl);
	if (!rc)
		dp->power_on = true;
error:
	return rc;
}

static int dp_display_post_enable(struct msm_dp *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	complete_all(&dp->notification_comp);

end:
	return rc;
}

static int dp_display_pre_disable(struct msm_dp *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (dp->usbpd->alt_mode_cfg_done && (dp->usbpd->hpd_high ||
		dp->usbpd->forced_disconnect))
		dp->link->psm_config(dp->link, &dp->panel->link_info, true);

	dp->ctrl->push_idle(dp->ctrl);
error:
	return rc;
}

static int dp_display_disable(struct msm_dp *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	if (!dp->power_on || !dp->core_initialized)
		goto error;

	dp->ctrl->off(dp->ctrl);

	dp->power_on = false;

	complete_all(&dp->notification_comp);
error:
	return rc;
}

static int dp_request_irq(struct msm_dp *dp_display)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	dp->irq = irq_of_parse_and_map(dp->pdev->dev.of_node, 0);
	if (dp->irq < 0) {
		rc = dp->irq;
		pr_err("failed to get irq: %d\n", rc);
		return rc;
	}

	rc = devm_request_irq(&dp->pdev->dev, dp->irq, dp_display_irq,
		IRQF_TRIGGER_HIGH, "dp_display_isr", dp);
	if (rc < 0) {
		pr_err("failed to request IRQ%u: %d\n",
				dp->irq, rc);
		return rc;
	}
	disable_irq(dp->irq);

	return 0;
}

static struct dp_debug *dp_get_debug(struct msm_dp *dp_display)
{
	struct dp_display_private *dp;

	if (!dp_display) {
		pr_err("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	dp = container_of(dp_display, struct dp_display_private, dp_display);

	return dp->debug;
}

static int dp_display_unprepare(struct msm_dp *dp)
{
	return 0;
}

static int dp_display_validate_mode(struct msm_dp *dp, u32 mode_pclk_khz)
{
	const u32 num_components = 3, default_bpp = 24;
	struct dp_display_private *dp_display;
	struct drm_dp_link *link_info;
	u32 mode_rate_khz = 0, supported_rate_khz = 0, mode_bpp = 0;

	if (!dp || !mode_pclk_khz || !dp->connector) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);
	link_info = &dp_display->panel->link_info;

	mode_bpp = dp->connector->display_info.bpc * num_components;
	if (!mode_bpp)
		mode_bpp = default_bpp;

	mode_bpp = dp_display->panel->get_mode_bpp(dp_display->panel,
			mode_bpp, mode_pclk_khz);

	mode_rate_khz = mode_pclk_khz * mode_bpp;
	supported_rate_khz = link_info->num_lanes * link_info->rate * 8;

	if (mode_rate_khz > supported_rate_khz)
		return MODE_BAD;

	return MODE_OK;
}

static int dp_display_get_modes(struct msm_dp *dp,
				struct dp_display_mode *dp_mode)
{
	struct dp_display_private *dp_display;
	int ret = 0;

	if (!dp) {
		pr_err("invalid params\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	ret = dp_display->panel->get_modes(dp_display->panel,
		dp->connector, dp_mode);
	if (dp_mode->timing.pixel_clk_khz)
		dp->max_pclk_khz = dp_mode->timing.pixel_clk_khz;
	return ret;
}

static bool dp_display_check_video_test(struct msm_dp *dp)
{
	struct dp_display_private *dp_display;

	if (!dp) {
		pr_err("invalid params\n");
		return false;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	if (dp_display->panel->video_test)
		return true;

	return false;
}

static int dp_display_get_test_bpp(struct msm_dp *dp)
{
	struct dp_display_private *dp_display;

	if (!dp) {
		pr_err("invalid params\n");
		return 0;
	}

	dp_display = container_of(dp, struct dp_display_private, dp_display);

	return dp_link_bit_depth_to_bpp(
		dp_display->link->test_video.test_bit_depth);
}

static int dp_display_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct dp_display_private *dp;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	init_completion(&dp->notification_comp);

	dp->pdev = pdev;
	dp->name = "drm_dp";

	rc = dp_init_sub_modules(dp);
	if (rc) {
		pr_err("init sub module failed\n");
		devm_kfree(&pdev->dev, dp);
		return -EPROBE_DEFER;
	}

	platform_set_drvdata(pdev, dp);

	g_dp_display = &dp->dp_display;

	g_dp_display->enable        = dp_display_enable;
	g_dp_display->post_enable   = dp_display_post_enable;
	g_dp_display->pre_disable   = dp_display_pre_disable;
	g_dp_display->disable       = dp_display_disable;
	g_dp_display->set_mode      = dp_display_set_mode;
	g_dp_display->validate_mode = dp_display_validate_mode;
	g_dp_display->get_modes     = dp_display_get_modes;
	g_dp_display->prepare       = dp_display_prepare;
	g_dp_display->unprepare     = dp_display_unprepare;
	g_dp_display->request_irq   = dp_request_irq;
	g_dp_display->get_debug     = dp_get_debug;
	g_dp_display->send_hpd_event    = dp_display_send_hpd_event;
	g_dp_display->is_video_test = dp_display_check_video_test;
	g_dp_display->get_test_bpp = dp_display_get_test_bpp;

	rc = component_add(&pdev->dev, &dp_display_comp_ops);
	if (rc) {
		pr_err("component add failed, rc=%d\n", rc);
		dp_display_deinit_sub_modules(dp);
		devm_kfree(&pdev->dev, dp);
	}

	return rc;
}

int dp_display_get_displays(void **displays, int count)
{
	if (!displays) {
		pr_err("invalid data\n");
		return -EINVAL;
	}

	if (count != 1) {
		pr_err("invalid number of displays\n");
		return -EINVAL;
	}

	displays[0] = g_dp_display;
	return count;
}

int dp_display_get_num_of_displays(void)
{
	return 1;
}

static int dp_display_remove(struct platform_device *pdev)
{
	struct dp_display_private *dp;

	if (!pdev)
		return -EINVAL;

	dp = platform_get_drvdata(pdev);

	dp_display_deinit_sub_modules(dp);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, dp);

	return 0;
}

static struct platform_driver dp_display_driver = {
	.probe  = dp_display_probe,
	.remove = dp_display_remove,
	.driver = {
		.name = "msm-dp-display",
		.of_match_table = dp_dt_match,
	},
};

int __init msm_dp_register(void)
{
	int ret;

	msm_dp_pll_driver_register();
	ret = platform_driver_register(&dp_display_driver);
	if (ret) {
		pr_err("driver register failed");
		return ret;
	}

	return ret;
}

void __exit msm_dp_unregister(void)
{
	msm_dp_pll_driver_unregister();
	platform_driver_unregister(&dp_display_driver);
}

int msm_dp_modeset_init(struct msm_dp *dp_display, struct drm_device *dev,
			struct drm_encoder *encoder)
{
	struct msm_drm_private *priv;
	int ret;

	if (WARN_ON(!encoder) || WARN_ON(!dp_display) || WARN_ON(!dev))
		return -EINVAL;

	priv = dev->dev_private;
	dp_display->drm_dev = dev;

	ret = dp_drm_bridge_init(dp_display, encoder);
	if (ret) {
		dev_err(dev->dev, "failed to create dp bridge: %d\n", ret);
		dp_display->bridge = NULL;
		return ret;
	}
	dp_display->encoder = encoder;

	dp_display->connector = dp_drm_connector_init(dp_display);
	if (IS_ERR(dp_display->connector)) {
		ret = PTR_ERR(dp_display->connector);
		dev_err(dev->dev,
			"failed to create dp connector: %d\n", ret);
		dp_display->connector = NULL;
		goto conn_fail;
	}

	priv->bridges[priv->num_bridges++] = dp_display->bridge;
	priv->connectors[priv->num_connectors++] = dp_display->connector;
	return 0;

conn_fail:
	if (dp_display->bridge) {
		dp_drm_bridge_deinit(dp_display);
		dp_display->bridge = NULL;
	}
	return ret;
}
