// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2014, 2018, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "icbw: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interconnect.h>

struct dev_data {
	struct icc_path *path;
	u32 cur_ab;
	u32 cur_pb;
	unsigned long gov_ab;
	struct devfreq *df;
	struct devfreq_dev_profile dp;
};

static int icbw_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	int ret;
	u32 new_pb, new_ab = d->gov_ab;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp))
		return PTR_ERR(opp);
	dev_pm_opp_put(opp);

	new_pb = *freq;
	if (d->cur_pb == new_pb && d->cur_ab == new_ab)
		return 0;

	dev_dbg(dev, "BW KBps: AB: %u PB: %u\n", new_ab, new_pb);

	ret = icc_set(d->path, new_ab, new_pb);
	if (ret) {
		dev_err(dev, "bandwidth request failed (%d)\n", ret);
	} else {
		d->cur_pb = new_pb;
		d->cur_ab = new_ab;
	}

	return ret;
}

static int icbw_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct dev_data *d = dev_get_drvdata(dev);

	stat->private_data = &d->gov_ab;
	return 0;
}

static int devfreq_icbw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	int ret;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	dev_set_drvdata(dev, d);

	p = &d->dp;
	p->polling_ms = 50;
	p->target = icbw_target;
	p->get_dev_status = icbw_get_dev_status;

	d->path = of_icc_get(dev, NULL);
	if (IS_ERR(d->path)) {
		dev_err(dev, "Unable to register interconnect path\n");
		return PTR_ERR(d->path);
	}

	ret = dev_pm_opp_of_add_table(dev);
	if (ret) {
		dev_err(dev, "Couldn't find OPP table\n");
		return ret;
	}

	d->df = devfreq_add_device(dev, p, "performance", NULL);
	if (IS_ERR(d->df)) {
		icc_put(d->path);
		return PTR_ERR(d->df);
	}

	return 0;
}

static int devfreq_icbw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dev_data *d = dev_get_drvdata(dev);

	devfreq_remove_device(d->df);
	icc_put(d->path);
	return 0;
}

static const struct of_device_id icbw_match_table[] = {
	{ .compatible = "devfreq-icbw" },
	{}
};

static struct platform_driver icbw_driver = {
	.probe = devfreq_icbw_probe,
	.remove = devfreq_icbw_remove,
	.driver = {
		.name = "devfreq-icbw",
		.of_match_table = icbw_match_table,
	},
};

module_platform_driver(icbw_driver);
MODULE_DESCRIPTION("Interconnect bandwidth voting driver");
MODULE_LICENSE("GPL v2");
