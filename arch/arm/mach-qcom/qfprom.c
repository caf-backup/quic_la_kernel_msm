/*
 * Copyright (c) 2014 - 2015, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/device.h>
#include <soc/qcom/scm.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>

#define QFPROM_ROW_READ_CMD			0x8
#define QFPROM_ROW_ROLLBACK_WRITE_CMD		0x9
#define QFPROM_IS_AUTHENTICATE_CMD		0x7
#define QFPROM_IS_AUTHENTICATE_CMD_RSP_SIZE	0x2

static ssize_t
qfprom_show_authenticate(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret;
	ret = scm_call(SCM_SVC_FUSE, QFPROM_IS_AUTHENTICATE_CMD,
			NULL, 0, buf, sizeof(char));

	if (ret) {
		pr_err("%s: Error in QFPROM read : %d\n",
						__func__, ret);
		return ret;
	}

	/* show needs a string response */
	if (buf[0] == 1)
		buf[0] = '1';
	else
		buf[0] = '0';

	buf[1] = '\0';

	return QFPROM_IS_AUTHENTICATE_CMD_RSP_SIZE;
}

static ssize_t
qfprom_show_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	uint64_t version __aligned(32);
	uint32_t qfprom_api_status __aligned(32);
	int32_t ret, ret1, ret2;

	struct qfprom_read_ip {
		uint32_t row_data_addr;
		uint32_t addr_type;
		uint32_t qfprom_ret_ptr;
	} rdip;

	rdip.addr_type = 0;
	rdip.row_data_addr = dma_map_single(NULL, &version,
			sizeof(version), DMA_FROM_DEVICE);

	rdip.qfprom_ret_ptr = dma_map_single(NULL, &qfprom_api_status,
			sizeof(qfprom_api_status), DMA_FROM_DEVICE);

	ret1 = dma_mapping_error(NULL, rdip.row_data_addr);
	ret2 = dma_mapping_error(NULL, rdip.qfprom_ret_ptr);

	if (ret1 == 0 && ret2 == 0) {
		ret = scm_call(SCM_SVC_FUSE, QFPROM_ROW_READ_CMD,
				&rdip, sizeof(rdip), NULL, 0);
	}
	if (ret1 == 0) {
		dma_unmap_single(NULL, rdip.row_data_addr,
				sizeof(version), DMA_FROM_DEVICE);
	}
	if (ret2 == 0) {
		dma_unmap_single(NULL, rdip.qfprom_ret_ptr,
				sizeof(qfprom_api_status), DMA_FROM_DEVICE);
	}
	if (ret1 || ret2) {
		pr_err("DMA Mapping Error",
			"(version)\n" ? ret1 : "(api_status)\n");
		return ret1 ? ret1 : ret2;
	}

	if (ret && qfprom_api_status) {
		pr_err("%s: Error in QFPROM read (%d, 0x%x)\n",
				__func__, ret, qfprom_api_status);
		return ret;
	}
	return snprintf(buf, 20, "0x%llX\n", version);
}

static ssize_t
qfprom_store_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	uint64_t version __aligned(32);
	uint32_t qfprom_api_status __aligned(32);
	int32_t ret, ret1, ret2;

	struct qfprom_write_ip {
		uint32_t row_data_addr;
		uint32_t bus_clk;
		uint32_t qfprom_ret_ptr;
	} wrip;

	/* Input validation handled here */
	ret = kstrtoull(buf, 0, &version);
	if (ret)
		return ret;

	wrip.bus_clk = (64 * 1000);
	wrip.row_data_addr = dma_map_single(NULL, &version,
			sizeof(version), DMA_TO_DEVICE);
	wrip.qfprom_ret_ptr = dma_map_single(NULL, &qfprom_api_status,
			sizeof(qfprom_api_status), DMA_TO_DEVICE);


	ret1 = dma_mapping_error(NULL, wrip.row_data_addr);
	ret2 = dma_mapping_error(NULL, wrip.qfprom_ret_ptr);

	if (ret1 == 0 && ret2 == 0) {
		ret = scm_call(SCM_SVC_FUSE, QFPROM_ROW_ROLLBACK_WRITE_CMD,
				&wrip, sizeof(wrip), NULL, 0);
	}
	if (ret1 == 0) {
		dma_unmap_single(NULL, wrip.row_data_addr,
				sizeof(version), DMA_TO_DEVICE);
	}

	if (ret2 == 0) {
		dma_unmap_single(NULL, wrip.qfprom_ret_ptr,
				sizeof(qfprom_api_status), DMA_TO_DEVICE);
	}

	if (ret1 || ret2) {
		pr_err("DMA Mapping Error",
				"(version)\n" ? ret1 : "(api_status)\n");
		return ret1 ? ret1 : ret2;
	}

	if (ret && qfprom_api_status) {
		pr_err("%s: Error in QFPROM write (%d, 0x%x)\n",
				__func__, ret, qfprom_api_status);
		return ret;
	}

	/* Return the count argument since entire buffer is used */
	return count;
}

static struct device_attribute qfprom_attrs[] = {
	__ATTR(version, 0666, qfprom_show_version,
					qfprom_store_version),
	__ATTR(authenticate, 0444, qfprom_show_authenticate,
					NULL),
};

static struct bus_type qfprom_subsys = {
	.name = "qfprom",
	.dev_name = "qfprom",
};

static struct device device_qfprom = {
	.id = 0,
	.bus = &qfprom_subsys,
};

static int __init qfprom_create_files(int size)
{
	int i;
	for (i = 0; i < size; i++) {
		int err = device_create_file(&device_qfprom, &qfprom_attrs[i]);
		if (err) {
			pr_err("%s: device_create_file(%s)=%d\n",
				__func__, qfprom_attrs[i].attr.name, err);
			return err;
		}
	}
	return 0;
}

static int __init qfprom_init(void)
{
	int err;
	/*
	 * Registering under "/sys/devices/system"
	 */
	err = subsys_system_register(&qfprom_subsys, NULL);
	if (err) {
		pr_err("%s: subsys_system_register fail (%d)\n",
			__func__, err);
		return err;
	}

	err = device_register(&device_qfprom);
	if (err) {
		pr_err("%s: device_register fail (%d)\n",
			__func__, err);
		return err;
	}
	return qfprom_create_files(ARRAY_SIZE(qfprom_attrs));
}

arch_initcall(qfprom_init);
