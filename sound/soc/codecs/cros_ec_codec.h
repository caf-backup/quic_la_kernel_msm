/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ChromeOS EC Codec
 *
 * Copyright 2018 Google, Inc
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

#ifndef __CROS_EC_CODEC_H
#define __CROS_EC_CODEC_H

#include <linux/irqreturn.h>

/*
 * struct cros_ec_codec_data - ChromeOS EC codec driver data.
 *
 * @dev: Device structure used in sysfs.
 * @ec_device: cros_ec_device structure to talk to the physical device.
 * @component: Pointer to the component.
 * @suspended: Flag to maintain suspend state.
 */
struct cros_ec_codec_data {
	struct device *dev;
	struct cros_ec_device *ec_device;
	struct snd_soc_component *component;
	bool suspended;
};

#endif  /* __CROS_EC_CODEC_H */
