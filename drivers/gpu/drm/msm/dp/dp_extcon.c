/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/extcon.h>

#include "dp_extcon.h"

/* DP specific VDM commands */
#define DP_USBPD_VDM_STATUS	0x10
#define DP_USBPD_VDM_CONFIGURE	0x11

/* USBPD-TypeC specific Macros */
#define VDM_VERSION		0x0
#define USB_C_DP_SID		0xFF01

enum dp_usbpd_pin_assignment {
	DP_USBPD_PIN_A,
	DP_USBPD_PIN_B,
	DP_USBPD_PIN_C,
	DP_USBPD_PIN_D,
	DP_USBPD_PIN_E,
	DP_USBPD_PIN_F,
	DP_USBPD_PIN_MAX,
};

struct dp_usbpd_capabilities {
	enum dp_usbpd_port port;
	bool receptacle_state;
	u8 ulink_pin_config;
	u8 dlink_pin_config;
};

struct dp_extcon_private {
	u32 vdo;
	struct device *dev;
	struct notifier_block extcon_nb;
	struct extcon_dev *extcon;
	struct workqueue_struct *extcon_wq;
	struct work_struct event_work;
	struct usbpd *pd;
	struct dp_usbpd_cb *dp_cb;
	struct dp_usbpd_capabilities cap;
	struct dp_usbpd dp_usbpd;
};

static const char *dp_usbpd_pin_name(u8 pin)
{
	switch (pin) {
	case DP_USBPD_PIN_A: return "DP_USBPD_PIN_ASSIGNMENT_A";
	case DP_USBPD_PIN_B: return "DP_USBPD_PIN_ASSIGNMENT_B";
	case DP_USBPD_PIN_C: return "DP_USBPD_PIN_ASSIGNMENT_C";
	case DP_USBPD_PIN_D: return "DP_USBPD_PIN_ASSIGNMENT_D";
	case DP_USBPD_PIN_E: return "DP_USBPD_PIN_ASSIGNMENT_E";
	case DP_USBPD_PIN_F: return "DP_USBPD_PIN_ASSIGNMENT_F";
	default: return "UNKNOWN";
	}
}

static const char *dp_usbpd_port_name(enum dp_usbpd_port port)
{
	switch (port) {
	case DP_USBPD_PORT_NONE: return "DP_USBPD_PORT_NONE";
	case DP_USBPD_PORT_UFP_D: return "DP_USBPD_PORT_UFP_D";
	case DP_USBPD_PORT_DFP_D: return "DP_USBPD_PORT_DFP_D";
	case DP_USBPD_PORT_D_UFP_D: return "DP_USBPD_PORT_D_UFP_D";
	default: return "DP_USBPD_PORT_NONE";
	}
}

static void dp_usbpd_init_port(enum dp_usbpd_port *port, u32 in_port)
{
	switch (in_port) {
	case 0:
		*port = DP_USBPD_PORT_NONE;
		break;
	case 1:
		*port = DP_USBPD_PORT_UFP_D;
		break;
	case 2:
		*port = DP_USBPD_PORT_DFP_D;
		break;
	case 3:
		*port = DP_USBPD_PORT_D_UFP_D;
		break;
	default:
		*port = DP_USBPD_PORT_NONE;
	}
	pr_debug("port:%s\n", dp_usbpd_port_name(*port));
}

void dp_usbpd_get_capabilities(struct dp_extcon_private *pd)
{
	struct dp_usbpd_capabilities *cap = &pd->cap;
	u32 buf = pd->vdo;
	int port = buf & 0x3;

	cap->receptacle_state = (buf & BIT(6)) ? true : false;
	cap->dlink_pin_config = (buf >> 8) & 0xff;
	cap->ulink_pin_config = (buf >> 16) & 0xff;

	dp_usbpd_init_port(&cap->port, port);
}

void dp_usbpd_get_status(struct dp_extcon_private *pd)
{
	struct dp_usbpd *status = &pd->dp_usbpd;
	u32 buf = pd->vdo;
	int port = buf & 0x3;

	status->low_pow_st     = (buf & BIT(2)) ? true : false;
	status->adaptor_dp_en  = (buf & BIT(3)) ? true : false;
	status->multi_func     = (buf & BIT(4)) ? true : false;
	status->usb_config_req = (buf & BIT(5)) ? true : false;
	status->exit_dp_mode   = (buf & BIT(6)) ? true : false;
	status->hpd_high       = (buf & BIT(7)) ? true : false;
	status->hpd_irq        = (buf & BIT(8)) ? true : false;

	pr_debug("low_pow_st = %d, adaptor_dp_en = %d, multi_func = %d\n",
			status->low_pow_st, status->adaptor_dp_en,
			status->multi_func);
	pr_debug("usb_config_req = %d, exit_dp_mode = %d, hpd_high =%d\n",
			status->usb_config_req,
			status->exit_dp_mode, status->hpd_high);
	pr_debug("hpd_irq = %d\n", status->hpd_irq);

	dp_usbpd_init_port(&status->port, port);
}

u32 dp_usbpd_gen_config_pkt(struct dp_extcon_private *pd)
{
	u8 pin_cfg, pin;
	u32 config = 0;
	const u32 ufp_d_config = 0x2, dp_ver = 0x1;

	if (pd->cap.receptacle_state)
		pin_cfg = pd->cap.ulink_pin_config;
	else
		pin_cfg = pd->cap.dlink_pin_config;

	for (pin = DP_USBPD_PIN_A; pin < DP_USBPD_PIN_MAX; pin++) {
		if (pin_cfg & BIT(pin)) {
			if (pd->dp_usbpd.multi_func) {
				if (pin == DP_USBPD_PIN_D)
					break;
			} else {
				break;
			}
		}
	}

	if (pin == DP_USBPD_PIN_MAX)
		pin = DP_USBPD_PIN_C;

	pr_debug("pin assignment: %s\n", dp_usbpd_pin_name(pin));

	config |= BIT(pin) << 8;

	config |= (dp_ver << 2);
	config |= ufp_d_config;

	pr_debug("config = 0x%x\n", config);
	return config;
}

static int dp_extcon_connect(struct dp_usbpd *dp_usbpd, bool hpd)
{
	int rc = 0;
	struct dp_extcon_private *extcon;

	if (!dp_usbpd) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	extcon = container_of(dp_usbpd, struct dp_extcon_private, dp_usbpd);

	dp_usbpd->hpd_high = hpd;
	dp_usbpd->forced_disconnect = !hpd;

	if (hpd)
		extcon->dp_cb->configure(extcon->dev);
	else
		extcon->dp_cb->disconnect(extcon->dev);

error:
	return rc;
}

static int dp_extcon_get_lanes(struct dp_extcon_private *extcon_priv)
{
	union extcon_property_value property;
	u8 lanes;

	if (!extcon_priv || !extcon_priv->extcon) {
		pr_err("Invalid input arguments\n");
		return 0;
	}

	extcon_get_property(extcon_priv->extcon,
					EXTCON_DISP_DP,
					EXTCON_PROP_USB_SS,
					&property);
	lanes = ((property.intval) ? 2 : 4);

	return lanes;
}


static void dp_extcon_event_work(struct work_struct *work)
{
	struct dp_extcon_private *extcon_priv;
	int dp_state, ret;
	int lanes;
	union extcon_property_value property;

	extcon_priv = container_of(work,
			struct dp_extcon_private, event_work);

	if (!extcon_priv || !extcon_priv->extcon) {
		pr_err("Invalid extcon device handler\n");
		return;
	}

	dp_state = extcon_get_state(extcon_priv->extcon,
						EXTCON_DISP_DP);

	if (dp_state > 0) {
		ret = extcon_get_property(extcon_priv->extcon,
					EXTCON_DISP_DP,
					EXTCON_PROP_USB_TYPEC_POLARITY,
					&property);
		if (ret) {
			pr_err("Get Polarity property failed\n");
			return;
		}
		extcon_priv->dp_usbpd.orientation =
			((property.intval) ?
				ORIENTATION_CC2 : ORIENTATION_CC1);

		lanes = dp_extcon_get_lanes(extcon_priv);
		if (!lanes)
			return;
		extcon_priv->dp_usbpd.multi_func =
					((lanes == 2) ? false : true);

		if (extcon_priv->dp_cb && extcon_priv->dp_cb->configure)
			dp_extcon_connect(&extcon_priv->dp_usbpd, true);
	} else {
		if (extcon_priv->dp_cb && extcon_priv->dp_cb->disconnect)
			dp_extcon_connect(&extcon_priv->dp_usbpd, false);
	}
}

static int dp_extcon_create_workqueue(struct dp_extcon_private *extcon_priv)
{
	extcon_priv->extcon_wq = create_singlethread_workqueue("drm_dp_extcon");
	if (IS_ERR_OR_NULL(extcon_priv->extcon_wq)) {
		pr_err("Error creating extcon wq\n");
		return -EPERM;
	}

	INIT_WORK(&extcon_priv->event_work, dp_extcon_event_work);

	return 0;
}

static int dp_extcon_event_notify(struct notifier_block *nb,
				  unsigned long event, void *priv)
{
	struct dp_extcon_private *extcon_priv;

	extcon_priv = container_of(nb, struct dp_extcon_private,
						extcon_nb);

	queue_work(extcon_priv->extcon_wq, &extcon_priv->event_work);
	return NOTIFY_DONE;
}

int dp_extcon_register(struct dp_usbpd *dp_usbpd)
{
	struct dp_extcon_private *extcon_priv;
	int ret = 0;

	if (!dp_usbpd)
		return -EINVAL;

	pr_err("Start++++");
	extcon_priv = container_of(dp_usbpd, struct dp_extcon_private, dp_usbpd);

	extcon_priv->extcon_nb.notifier_call = dp_extcon_event_notify;
	ret = devm_extcon_register_notifier(extcon_priv->dev, extcon_priv->extcon,
					    EXTCON_DISP_DP,
					    &extcon_priv->extcon_nb);
	if (ret) {
		dev_err(extcon_priv->dev,
			"register EXTCON_DISP_DP notifier err\n");
		ret = -EINVAL;
	}

	ret = dp_extcon_create_workqueue(extcon_priv);
	if (ret) {
		pr_err("Failed to create workqueue\n");
		dp_extcon_unregister(dp_usbpd);
	}

	pr_err("End----");
	return ret;
}

void dp_extcon_unregister(struct dp_usbpd *dp_usbpd)
{
	struct dp_extcon_private *extcon_priv;

	if (!dp_usbpd) {
		pr_err("Invalid input\n");
		return;
	}

	pr_err("Start++++");
	extcon_priv = container_of(dp_usbpd, struct dp_extcon_private, dp_usbpd);

	devm_extcon_unregister_notifier(extcon_priv->dev, extcon_priv->extcon,
					    EXTCON_DISP_DP,
					    &extcon_priv->extcon_nb);

	if (extcon_priv->extcon_wq)
		destroy_workqueue(extcon_priv->extcon_wq);

	pr_err("End----");
	return;
}

struct dp_usbpd *dp_extcon_get(struct device *dev, struct dp_usbpd_cb *cb)
{
	int rc = 0;
	struct dp_extcon_private *dp_extcon;
	struct dp_usbpd *dp_usbpd;

	if (!cb) {
		pr_err("invalid cb data\n");
		rc = -EINVAL;
		goto error;
	}

	pr_err("%s: Start", __func__);
	dp_extcon = devm_kzalloc(dev, sizeof(*dp_extcon), GFP_KERNEL);
	if (!dp_extcon) {
		rc = -ENOMEM;
		goto error;
	}

	dp_extcon->extcon = extcon_get_edev_by_phandle(dev, 0);
	if (!dp_extcon->extcon) {
		pr_err("invalid extcon data\n");
		rc = -EINVAL;
		devm_kfree(dev, dp_extcon);
		goto error;
        }

	dp_extcon->dev = dev;
	dp_extcon->dp_cb = cb;

	dp_extcon->dp_usbpd.connect = dp_extcon_connect;
	dp_usbpd = &dp_extcon->dp_usbpd;

	pr_err("%s: end", __func__);
	return dp_usbpd;
error:
	return ERR_PTR(rc);
}

void dp_extcon_put(struct dp_usbpd *dp_usbpd)
{
	struct dp_extcon_private *extcon;

	if (!dp_usbpd)
		return;

	extcon = container_of(dp_usbpd, struct dp_extcon_private, dp_usbpd);

	devm_kfree(extcon->dev, extcon);

	pr_err("%s: end", __func__);
}
