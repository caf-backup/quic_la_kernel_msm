/* Qualcomm Secure Execution Environment Communicator (QSEECOM) driver
 *
 * Copyright (c) 2012, 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Usage:
 *
 *(1) Step 1: To provide the sampleapp files to the kernel driver
 *
 * Concatenate all the seg files into 1 segment file and feed it as
 * input to sys/firmware/seg_file and feed the mdt file as input to
 * sys/firmware/mdt_file as below
 *
 * In platform ipq8064 or ipq40xx, the sample app is divided into 4
 * segments whereas in ipq807x, the sample app is divided into 7 segments
 *
 * cat sampleapp.b00 > sampleapp.b0x
 * cat sampleapp.b01 >> sampleapp.b0x
 * cat sampleapp.b02 >> sampleapp.b0x
 * cat sampleapp.b03 >> sampleapp.b0x
 *
 * Below 3 steps are required if its platform ipq807x
 * cat sampleapp.b04 >> sampleapp.b0x
 * cat sampleapp.b05 >> sampleapp.b0x
 * cat sampleapp.b06 >> sampleapp.b0x
 *
 * cat /lib/firmware/sampleapp.mdt > /sys/firmware/mdt_file
 * cat /lib/firmware/sampleapp.b0x > /sys/firmware/seg_file
 *
 *(2) Step 2: To start loading the sampleapp
 *
 * echo 1 > /sys/firmware/tzapp/load_start
 *
 *(3) Step 3: Perform operations required in the application
 *
 * To test Crypto functionality:
 * echo 1 > /sys/firmware/tzapp/crypto
 *
 * To give input to Encryption:
 * echo '6bc1bee22e409f9' > /sys/firmware/tzapp/encrypt
 *
 * To view encryption output:
 * cat /sys/firmware/tzapp/encrypt
 *
 * To give input to Decryption:
 * cat /sys/firmware/tzapp/encrypt > /sys/firmware/tzapp/decrypt
 *
 * To view decryption output:
 * cat /sys/firmware/tzapp/decrypt
 *
 * To give input to Multiplication op:
 * echo 100 > /sys/firmware/tzapp/basic_data
 *
 * To view Secure Multiplication output:
 * cat /sys/firmware/tzapp/basic_data
 *
 *(4) Step 4: To start unloading the sampleapp
 *
 * echo 0 > /sys/firmware/tzapp/load_start
 *
 * If the user doesn't unload the app, then the app is unloaded when the
 * device driver is removed
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/highuid.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/qcom_scm.h>
#include <linux/sysfs.h>
#include <linux/dma-mapping.h>
#include <linux/string.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>

#define CLIENT_CMD1_BASIC_DATA	1
#define CLIENT_CMD8_RUN_CRYPTO_TEST	3
#define CLIENT_CMD8_RUN_CRYPTO_ENCRYPT	8
#define CLIENT_CMD9_RUN_CRYPTO_DECRYPT	9
#define CLIENT_CMD_AUTH			26
#define MAX_INPUT_SIZE	4096
#define QSEE_APP_NOTIFY_COMMAND		13
#define QSEE_64				64
#define QSEE_32				32

#define MAX_ENCRYPTED_DATA_SIZE (2072 * sizeof(uint8_t))
#define MAX_PLAIN_DATA_SIZE (2048 * sizeof(uint8_t))
#define ENCRYPTED_DATA_HEADER \
	(MAX_ENCRYPTED_DATA_SIZE - MAX_PLAIN_DATA_SIZE)
#define KEY_BLOB_SIZE (56 * sizeof(uint8_t))
#define KEY_SIZE (32 * sizeof(uint8_t))

static int app_state;
struct qseecom_props *props;

enum tz_storage_service_cmd_t {
	TZ_STOR_SVC_GENERATE_KEY = 0x00000001,
	TZ_STOR_SVC_SEAL_DATA = 0x00000002,
	TZ_STOR_SVC_UNSEAL_DATA = 0x00000003,
	TZ_STOR_SVC_IMPORT_KEY = 0x00000004,
};

struct tz_storage_service_key_blob_t {
	uint8_t *key_material;
	size_t key_material_len;
};

struct tz_storage_service_import_key_cmd_t {
	enum tz_storage_service_cmd_t cmd_id;
	struct tz_storage_service_key_blob_t key_blob;
	uint8_t *input_key;
	size_t input_key_len;
};

struct tz_storage_service_gen_key_cmd_t {
	enum tz_storage_service_cmd_t cmd_id;
	struct tz_storage_service_key_blob_t key_blob;
};

struct tz_storage_service_gen_key_resp_t {
	enum tz_storage_service_cmd_t cmd_id;
	size_t status;
	size_t key_blob_size;
};

struct tz_storage_service_seal_data_cmd_t {
	enum tz_storage_service_cmd_t cmd_id;
	struct tz_storage_service_key_blob_t key_blob;
	uint8_t *plain_data;
	size_t plain_data_len;
	uint8_t *output_buffer;
	size_t output_len;
};

struct tz_storage_service_seal_data_resp_t {
	enum tz_storage_service_cmd_t cmd_id;
	size_t status;
	size_t sealed_data_len;
};

struct tz_storage_service_unseal_data_cmd_t {
	enum tz_storage_service_cmd_t cmd_id;
	struct tz_storage_service_key_blob_t key_blob;
	uint8_t *sealed_data;
	size_t sealed_dlen;
	uint8_t *output_buffer;
	size_t output_len;
};

struct tz_storage_service_unseal_data_resp_t {
	enum tz_storage_service_cmd_t cmd_id;
	size_t	status;
	size_t unsealed_data_len;
};

struct qsee_32_send_cmd {
	uint32_t cmd_id;
	uint32_t data;
	uint32_t data2;
	uint32_t len;
	uint32_t start_pkt;
	uint32_t end_pkt;
	uint32_t test_buf_size;
};

struct qsee_64_send_cmd {
	uint32_t cmd_id;
	uint64_t data;
	uint64_t data2;
	uint32_t len;
	uint32_t start_pkt;
	uint32_t end_pkt;
	uint32_t test_buf_size;
};

struct qsee_send_cmd_rsp {
	uint32_t data;
	int32_t status;
};

enum qseecom_qceos_cmd_status {
	QSEOS_RESULT_SUCCESS = 0,
	QSEOS_RESULT_INCOMPLETE,
	QSEOS_RESULT_FAILURE  = 0xFFFFFFFF
};

enum qseecom_qceos_cmd_id {
	QSEOS_APP_START_COMMAND      = 0x01,
	QSEOS_APP_SHUTDOWN_COMMAND,
	QSEOS_APP_LOOKUP_COMMAND,
	QSEOS_REGISTER_LISTENER,
	QSEOS_DEREGISTER_LISTENER,
	QSEOS_CLIENT_SEND_DATA_COMMAND,
	QSEOS_LISTENER_DATA_RSP_COMMAND,
	QSEOS_LOAD_EXTERNAL_ELF_COMMAND,
	QSEOS_UNLOAD_EXTERNAL_ELF_COMMAND,
	QSEOS_CMD_MAX	  = 0xEFFFFFFF
};

static uint32_t qsee_app_id;
static void *qsee_sbuffer;
static int32_t basic_output;
static size_t enc_len;
static size_t dec_len;
static int basic_data_len;
static int mdt_size;
static int seg_size;
static int auth_size;
static uint8_t *mdt_file;
static uint8_t *seg_file;
static uint8_t *auth_file;

static struct kobject *sec_kobj;
static uint8_t *key;
static size_t key_len;
static uint8_t *key_blob;
static size_t key_blob_len;
static uint8_t *sealed_buf;
static size_t seal_len;
static uint8_t *unsealed_buf;
static size_t unseal_len;
static struct device *qdev;

#define MUL		0x1
#define ENC		0x2
#define DEC		0x4
#define CRYPTO		0x8
#define AUTH_OTP	0x10
#define AES_SEC_KEY	0x20

static ssize_t
generate_key_blob(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct scm_cmd_buf_t scm_cmd_buf;
	struct tz_storage_service_gen_key_cmd_t *req_ptr = NULL;
	struct tz_storage_service_gen_key_resp_t *resp_ptr = NULL;
	size_t req_size = 0;
	size_t resp_size = 0;
	size_t req_order = 0;
	size_t resp_order = 0;
	dma_addr_t dma_req_addr = 0;
	dma_addr_t dma_resp_addr = 0;
	dma_addr_t dma_key_blob = 0;
	struct page *req_page = NULL;
	struct page *resp_page = NULL;

	dev = qdev;
	key_blob_len = 0;

	req_order = get_order(sizeof(struct tz_storage_service_gen_key_cmd_t));
	req_page = alloc_pages(GFP_KERNEL, req_order);

	resp_order = get_order(sizeof(struct
					tz_storage_service_gen_key_resp_t));
	resp_page = alloc_pages(GFP_KERNEL, resp_order);

	if (!req_page || !resp_page) {
		pr_err("\nCannot allocate memory for key material\n");
		if (req_page)
			free_pages((unsigned long)page_address(req_page),
								req_order);
		if (resp_page)
			free_pages((unsigned long)page_address(resp_page),
								resp_order);
		return -ENOMEM;
	}

	req_ptr = page_address(req_page);
	resp_ptr = page_address(resp_page);

	key_blob = memset(key_blob, 0, KEY_BLOB_SIZE);

	req_ptr->cmd_id = TZ_STOR_SVC_GENERATE_KEY;

	req_ptr->key_blob.key_material_len = KEY_BLOB_SIZE;
	dma_key_blob = dma_map_single(dev, key_blob, KEY_BLOB_SIZE,
							DMA_FROM_DEVICE);
	req_ptr->key_blob.key_material = (uint8_t *)dma_key_blob;

	rc = dma_mapping_error(dev, dma_key_blob);
	if (rc) {
		pr_err("DMA Mapping Error(key blob)\n");
		goto err_end;
	}

	req_size = sizeof(struct tz_storage_service_gen_key_cmd_t);
	dma_req_addr = dma_map_single(dev, req_ptr, req_size, DMA_TO_DEVICE);

	rc = dma_mapping_error(dev, dma_req_addr);
	if (rc) {
		pr_err("DMA Mapping Error(request str)\n");
		goto err_map_req;
	}

	resp_size = sizeof(struct tz_storage_service_gen_key_resp_t);
	dma_resp_addr = dma_map_single(dev, resp_ptr, resp_size,
							DMA_FROM_DEVICE);

	rc = dma_mapping_error(dev, dma_resp_addr);
	if (rc) {
		pr_err("DMA Mapping Error(response str)\n");
		goto err_map_resp;
	}

	scm_cmd_buf.req_size = req_size;
	scm_cmd_buf.req_addr = dma_req_addr;
	scm_cmd_buf.resp_size = resp_size;
	scm_cmd_buf.resp_addr = dma_resp_addr;

	rc = qcom_scm_tls_hardening(&scm_cmd_buf, sizeof(scm_cmd_buf));

	dma_unmap_single(dev, dma_resp_addr, resp_size, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_FROM_DEVICE);

	if (rc) {
		pr_err("\nSCM Call failed..SCM Call return value = %d\n", rc);
		goto err_end;
	}

	if (resp_ptr->status) {
		rc = resp_ptr->status;
		pr_err("\nResponse status failure..status = %d\n",
							resp_ptr->status);
		goto err_end;
	}

	key_blob_len = KEY_BLOB_SIZE;
	memcpy(buf, key_blob, key_blob_len);

goto end;

err_map_resp:
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);

err_map_req:
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_FROM_DEVICE);

err_end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return rc;

end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return key_blob_len;
}

static ssize_t
store_key(struct device *dev, struct device_attribute *attr, const char *buf,
								size_t count)
{
	key_len = 0;

	if (count == 0 || count != KEY_SIZE) {
		pr_info("\nInvalid input\n");
		pr_info("Key cannot be NULL\n");
		pr_info("Key length is %lu\n", (unsigned long)count);
		pr_info("Key length must be 32 bytes\n");
		return -EINVAL;
	}

	key = memset(key, 0, KEY_SIZE);

	key_len = count;
	memcpy(key, buf, key_len);

	return count;
}

static ssize_t
import_key_blob(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct scm_cmd_buf_t scm_cmd_buf;
	struct tz_storage_service_import_key_cmd_t *req_ptr = NULL;
	struct tz_storage_service_gen_key_resp_t *resp_ptr = NULL;
	size_t req_size = 0;
	size_t resp_size = 0;
	size_t req_order = 0;
	size_t resp_order = 0;
	dma_addr_t dma_req_addr = 0;
	dma_addr_t dma_resp_addr = 0;
	dma_addr_t dma_key_blob = 0;
	dma_addr_t dma_key = 0;
	struct page *req_page = NULL;
	struct page *resp_page = NULL;

	key_blob_len = 0;

	if (key_len == 0) {
		pr_err("\nPlease provide key to import key blob\n");
		return -EINVAL;
	}

	dev = qdev;

	req_order = get_order(sizeof(struct
					tz_storage_service_import_key_cmd_t));
	req_page = alloc_pages(GFP_KERNEL, req_order);

	resp_order = get_order(sizeof(struct
					tz_storage_service_gen_key_resp_t));
	resp_page = alloc_pages(GFP_KERNEL, resp_order);

	if (!req_page || !resp_page) {
		pr_err("\nCannot allocate memory for key material\n");
		if (req_page)
			free_pages((unsigned long)page_address(req_page),
								req_order);
		if (resp_page)
			free_pages((unsigned long)page_address(resp_page),
								resp_order);
		return -ENOMEM;
	}

	req_ptr = page_address(req_page);
	resp_ptr = page_address(resp_page);

	key_blob = memset(key_blob, 0, KEY_BLOB_SIZE);

	req_ptr->cmd_id = TZ_STOR_SVC_IMPORT_KEY;

	req_ptr->input_key_len = KEY_SIZE;
	dma_key = dma_map_single(dev, key, KEY_SIZE, DMA_TO_DEVICE);
	req_ptr->input_key = (uint8_t *)dma_key;

	rc = dma_mapping_error(dev, dma_key);
	if (rc) {
		pr_err("DMA Mapping error(key)\n");
		goto err_end;
	}

	req_ptr->key_blob.key_material_len = KEY_BLOB_SIZE;
	dma_key_blob = dma_map_single(dev, key_blob, KEY_BLOB_SIZE,
							DMA_FROM_DEVICE);
	req_ptr->key_blob.key_material = (uint8_t *)dma_key_blob;

	rc = dma_mapping_error(dev, dma_key_blob);
	if (rc) {
		pr_err("DMA Mapping Error(key blob)\n");
		goto err_map_key_blob;
	}

	req_size = sizeof(struct tz_storage_service_import_key_cmd_t);
	dma_req_addr = dma_map_single(dev, req_ptr, req_size, DMA_TO_DEVICE);

	rc = dma_mapping_error(dev, dma_req_addr);
	if (rc) {
		pr_err("DMA Mapping Error(request str)\n");
		goto err_map_req;
	}

	resp_size = sizeof(struct tz_storage_service_gen_key_resp_t);
	dma_resp_addr = dma_map_single(dev, resp_ptr, resp_size,
							DMA_FROM_DEVICE);

	rc = dma_mapping_error(dev, dma_resp_addr);
	if (rc) {
		pr_err("DMA Mapping Error(response str)\n");
		goto err_map_resp;
	}

	scm_cmd_buf.req_size = req_size;
	scm_cmd_buf.req_addr = dma_req_addr;
	scm_cmd_buf.resp_size = resp_size;
	scm_cmd_buf.resp_addr = dma_resp_addr;

	rc = qcom_scm_tls_hardening(&scm_cmd_buf, sizeof(scm_cmd_buf));

	dma_unmap_single(dev, dma_resp_addr, resp_size, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_key, KEY_SIZE, DMA_TO_DEVICE);

	if (rc) {
		pr_err("\nSCM Call failed..SCM Call return value = %d\n", rc);
		goto err_end;
	}

	if (resp_ptr->status) {
		rc = resp_ptr->status;
		pr_err("\nResponse status failure..status = %d\n",
							resp_ptr->status);
		goto err_end;
	}

	key_blob_len = KEY_BLOB_SIZE;
	memcpy(buf, key_blob, key_blob_len);

goto end;

err_map_resp:
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);

err_map_req:
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_FROM_DEVICE);

err_map_key_blob:
	dma_unmap_single(dev, dma_key, KEY_SIZE, DMA_TO_DEVICE);

err_end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return rc;

end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return key_blob_len;
}

static ssize_t
store_key_blob(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	key_blob_len = 0;

	if (count == 0 || count != KEY_BLOB_SIZE) {
		pr_info("\nInvalid input\n");
		pr_info("Key blob cannot be NULL\n");
		pr_info("Key blob length is %lu\n", (unsigned long)count);
		pr_info("Key blob length must be 56 bytes\n");
		return -EINVAL;
	}

	key_blob = memset(key_blob, 0, KEY_BLOB_SIZE);

	key_blob_len = count;
	memcpy(key_blob, buf, key_blob_len);

	return count;
}

static ssize_t
store_unsealed_data(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unseal_len = 0;

	if (count == 0 || count > MAX_PLAIN_DATA_SIZE) {
		pr_info("\nInvalid input\n");
		pr_info("Plain data cannot be NULL\n");
		pr_info("Plain data length is %lu\n", (unsigned long)count);
		pr_info("Plain data must be <= 2048 bytes\n");
		return -EINVAL;
	}

	unsealed_buf = memset(unsealed_buf, 0, MAX_PLAIN_DATA_SIZE);

	unseal_len = count;
	memcpy(unsealed_buf, buf, unseal_len);

	return count;
}

static ssize_t
show_sealed_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct scm_cmd_buf_t scm_cmd_buf;
	struct tz_storage_service_seal_data_cmd_t *req_ptr = NULL;
	struct tz_storage_service_seal_data_resp_t *resp_ptr = NULL;
	size_t req_size = 0;
	size_t resp_size = 0;
	size_t req_order = 0;
	size_t resp_order = 0;
	size_t output_len = 0;
	dma_addr_t dma_req_addr = 0;
	dma_addr_t dma_resp_addr = 0;
	dma_addr_t dma_key_blob = 0;
	dma_addr_t dma_plain_data = 0;
	dma_addr_t dma_output_data = 0;
	struct page *req_page = NULL;
	struct page *resp_page = NULL;

	if (key_blob_len == 0 || unseal_len == 0) {
		pr_err("\nInvalid input\n");
		pr_info("Need key for encryption operation\n");
		pr_info("Need data for encrpyption operation\n");
		return -EINVAL;
	}

	dev = qdev;

	req_order = get_order(sizeof(struct
					tz_storage_service_seal_data_cmd_t));
	req_page = alloc_pages(GFP_KERNEL, req_order);

	resp_order = get_order(sizeof(struct
					tz_storage_service_seal_data_resp_t));
	resp_page = alloc_pages(GFP_KERNEL, resp_order);

	if (!req_page || !resp_page) {
		pr_err("\nCannot allocate memory for key material\n");
		if (req_page)
			free_pages((unsigned long)page_address(req_page),
								req_order);
		if (resp_page)
			free_pages((unsigned long)page_address(resp_page),
								resp_order);
		return -ENOMEM;
	}

	req_ptr = page_address(req_page);
	resp_ptr = page_address(resp_page);

	sealed_buf = memset(sealed_buf, 0, MAX_ENCRYPTED_DATA_SIZE);
	output_len = unseal_len + ENCRYPTED_DATA_HEADER;

	req_ptr->cmd_id = TZ_STOR_SVC_SEAL_DATA;

	req_ptr->key_blob.key_material_len = KEY_BLOB_SIZE;
	dma_key_blob = dma_map_single(dev, key_blob, KEY_BLOB_SIZE,
								DMA_TO_DEVICE);
	req_ptr->key_blob.key_material = (uint8_t *)dma_key_blob;

	rc = dma_mapping_error(dev, dma_key_blob);
	if (rc) {
		pr_err("DMA Mapping Error(key blob)\n");
		goto err_end;
	}

	req_ptr->plain_data_len = unseal_len;
	dma_plain_data = dma_map_single(dev, unsealed_buf, unseal_len,
								DMA_TO_DEVICE);
	req_ptr->plain_data = (uint8_t *)dma_plain_data;

	rc = dma_mapping_error(dev, dma_plain_data);
	if (rc) {
		pr_err("DMA Mapping Error(plain data)\n");
		goto err_map_plain_data;
	}

	req_ptr->output_len = output_len;
	dma_output_data = dma_map_single(dev, sealed_buf, output_len,
							DMA_FROM_DEVICE);
	req_ptr->output_buffer = (uint8_t *)dma_output_data;

	rc = dma_mapping_error(dev, dma_output_data);
	if (rc) {
		pr_err("DMA Mapping Error(output data)\n");
		goto err_map_output_data;
	}

	req_size = sizeof(struct tz_storage_service_seal_data_cmd_t);
	dma_req_addr = dma_map_single(dev, req_ptr, req_size, DMA_TO_DEVICE);

	rc = dma_mapping_error(dev, dma_req_addr);
	if (rc) {
		pr_err("DMA Mapping Error(request str)\n");
		goto err_map_req;
	}

	resp_size = sizeof(struct tz_storage_service_seal_data_resp_t);
	dma_resp_addr = dma_map_single(dev, resp_ptr, resp_size,
							DMA_FROM_DEVICE);

	rc = dma_mapping_error(dev, dma_resp_addr);
	if (rc) {
		pr_err("DMA Mapping Error(response str)\n");
		goto err_map_resp;
	}

	scm_cmd_buf.req_size = req_size;
	scm_cmd_buf.req_addr = dma_req_addr;
	scm_cmd_buf.resp_size = resp_size;
	scm_cmd_buf.resp_addr = dma_resp_addr;

	rc = qcom_scm_tls_hardening(&scm_cmd_buf, sizeof(scm_cmd_buf));

	dma_unmap_single(dev, dma_resp_addr, resp_size, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_output_data, output_len, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_plain_data, unseal_len, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_TO_DEVICE);

	if (rc) {
		pr_err("\nSCM Call failed..SCM Call return value = %d\n", rc);
		goto err_end;
	}

	if (resp_ptr->status != 0) {
		rc = resp_ptr->status;
		pr_err("\nResponse status failure..status = %d\n",
							resp_ptr->status);
		goto err_end;
	}

	seal_len = output_len;
	memcpy(buf, sealed_buf, seal_len);

goto end;

err_map_resp:
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);

err_map_req:
	dma_unmap_single(dev, dma_output_data, output_len, DMA_FROM_DEVICE);

err_map_output_data:
	dma_unmap_single(dev, dma_plain_data, unseal_len, DMA_TO_DEVICE);

err_map_plain_data:
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_TO_DEVICE);

err_end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return rc;

end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return seal_len;
}

static ssize_t
store_sealed_data(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	seal_len = 0;

	if (count == 0 || count > MAX_ENCRYPTED_DATA_SIZE) {
		pr_info("\nInvalid input\n");
		pr_info("Encrypted data cannot be NULL\n");
		pr_info("Encrypted data length is %lu\n", (unsigned long)count);
		pr_info("Encrypted data length must be 2072 bytes\n");
		return -EINVAL;
	}

	sealed_buf = memset(sealed_buf, 0, MAX_ENCRYPTED_DATA_SIZE);

	seal_len = count;
	memcpy(sealed_buf, buf, seal_len);

	return count;
}

static ssize_t
show_unsealed_data(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	int rc = 0;
	struct scm_cmd_buf_t scm_cmd_buf;
	struct tz_storage_service_unseal_data_cmd_t *req_ptr = NULL;
	struct tz_storage_service_unseal_data_resp_t *resp_ptr = NULL;
	size_t req_size = 0;
	size_t resp_size = 0;
	size_t req_order = 0;
	size_t resp_order = 0;
	size_t output_len = 0;
	dma_addr_t dma_req_addr = 0;
	dma_addr_t dma_resp_addr = 0;
	dma_addr_t dma_key_blob = 0;
	dma_addr_t dma_sealed_data = 0;
	dma_addr_t dma_output_data = 0;
	struct page *req_page = NULL;
	struct page *resp_page = NULL;


	if (key_blob_len == 0 || seal_len == 0) {
		pr_err("\nInvalid input\n");
		pr_info("Need key for decryption operation\n");
		pr_info("Need data for decrpyption operation\n");
		return -EINVAL;
	}

	dev = qdev;

	req_order = get_order(sizeof(struct
					tz_storage_service_unseal_data_cmd_t));
	req_page = alloc_pages(GFP_KERNEL, req_order);

	resp_order = get_order(sizeof(struct
					tz_storage_service_unseal_data_resp_t));
	resp_page = alloc_pages(GFP_KERNEL, resp_order);

	if (!req_page || !resp_page) {
		pr_err("\nCannot allocate memory for key material\n");
		if (req_page)
			free_pages((unsigned long)page_address(req_page),
								req_order);
		if (resp_page)
			free_pages((unsigned long)page_address(resp_page),
								resp_order);
		return -ENOMEM;
	}

	req_ptr = page_address(req_page);
	resp_ptr = page_address(resp_page);

	unsealed_buf = memset(unsealed_buf, 0, MAX_PLAIN_DATA_SIZE);
	output_len = seal_len - ENCRYPTED_DATA_HEADER;

	req_ptr->cmd_id = TZ_STOR_SVC_UNSEAL_DATA;

	req_ptr->key_blob.key_material_len = KEY_BLOB_SIZE;
	dma_key_blob = dma_map_single(dev, key_blob, KEY_BLOB_SIZE,
								DMA_TO_DEVICE);
	req_ptr->key_blob.key_material = (uint8_t *)dma_key_blob;

	rc = dma_mapping_error(dev, dma_key_blob);
	if (rc) {
		pr_err("DMA Mapping Error(key blob)\n");
		goto err_end;
	}

	req_ptr->sealed_dlen = seal_len;
	dma_sealed_data = dma_map_single(dev, sealed_buf, seal_len,
								DMA_TO_DEVICE);
	req_ptr->sealed_data = (uint8_t *)dma_sealed_data;

	rc = dma_mapping_error(dev, dma_sealed_data);
	if (rc) {
		pr_err("DMA Mapping Error(sealed data)\n");
		goto err_map_sealed_data;
	}

	req_ptr->output_len = output_len;
	dma_output_data = dma_map_single(dev, unsealed_buf, output_len,
							DMA_FROM_DEVICE);
	req_ptr->output_buffer = (uint8_t *)dma_output_data;

	rc = dma_mapping_error(dev, dma_output_data);
	if (rc) {
		pr_err("DMA Mapping Error(output data)\n");
		goto err_map_output_data;
	}

	req_size = sizeof(struct tz_storage_service_unseal_data_cmd_t);
	dma_req_addr = dma_map_single(dev, req_ptr, req_size, DMA_TO_DEVICE);

	rc = dma_mapping_error(dev, dma_req_addr);
	if (rc) {
		pr_err("DMA Mapping Error(request str)\n");
		goto err_map_req;
	}

	resp_size = sizeof(struct tz_storage_service_unseal_data_resp_t);
	dma_resp_addr = dma_map_single(dev, resp_ptr, resp_size,
							DMA_FROM_DEVICE);

	rc = dma_mapping_error(dev, dma_resp_addr);
	if (rc) {
		pr_err("DMA Mapping Error(response str)\n");
		goto err_map_resp;
	}

	scm_cmd_buf.req_size = req_size;
	scm_cmd_buf.req_addr = dma_req_addr;
	scm_cmd_buf.resp_size = resp_size;
	scm_cmd_buf.resp_addr = dma_resp_addr;

	rc = qcom_scm_tls_hardening(&scm_cmd_buf, sizeof(scm_cmd_buf));

	dma_unmap_single(dev, dma_resp_addr, resp_size, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_output_data, output_len, DMA_FROM_DEVICE);
	dma_unmap_single(dev, dma_sealed_data, seal_len, DMA_TO_DEVICE);
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_TO_DEVICE);

	if (rc) {
		pr_err("\nSCM Call failed..SCM Call return value = %d\n", rc);
		goto err_end;
	}

	if (resp_ptr->status != 0) {
		rc = resp_ptr->status;
		pr_err("\nResponse status failure..status = %d\n",
							resp_ptr->status);
		goto err_end;
	}

	unseal_len = output_len;
	memcpy(buf, unsealed_buf, unseal_len);

goto end;

err_map_resp:
	dma_unmap_single(dev, dma_req_addr, req_size, DMA_TO_DEVICE);

err_map_req:
	dma_unmap_single(dev, dma_output_data, output_len, DMA_FROM_DEVICE);

err_map_output_data:
	dma_unmap_single(dev, dma_sealed_data, seal_len, DMA_TO_DEVICE);

err_map_sealed_data:
	dma_unmap_single(dev, dma_key_blob, KEY_BLOB_SIZE, DMA_TO_DEVICE);

err_end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return rc;

end:
	free_pages((unsigned long)page_address(req_page), req_order);
	free_pages((unsigned long)page_address(resp_page), resp_order);
	return unseal_len;
}

struct qseecom_props {
	const int function;
	const int tz_arch;
};

const struct qseecom_props qseecom_props_ipq40xx = {
	.function = (MUL | CRYPTO | AUTH_OTP | AES_SEC_KEY),
	.tz_arch = QSEE_32,
};

const struct qseecom_props qseecom_props_ipq8064 = {
	.function = (MUL | ENC | DEC),
	.tz_arch = QSEE_32,
};

const struct qseecom_props qseecom_props_ipq807x = {
	.function = (MUL | CRYPTO),
	.tz_arch = QSEE_64,
};

static const struct of_device_id qseecom_of_table[] = {
	{	.compatible = "ipq40xx-qseecom",
		.data = (void *) &qseecom_props_ipq40xx,
	},
	{	.compatible = "ipq8064-qseecom",
		.data = (void *) &qseecom_props_ipq8064,
	},
	{	.compatible = "ipq807x-qseecom",
		.data = (void *) &qseecom_props_ipq807x,
	},
	{}
};
MODULE_DEVICE_TABLE(of, qseecom_of_table);

static DEVICE_ATTR(generate, 0644, generate_key_blob, NULL);
static DEVICE_ATTR(import, 0644, import_key_blob, store_key);
static DEVICE_ATTR(key_blob, 0644, NULL, store_key_blob);
static DEVICE_ATTR(seal, 0644, show_sealed_data, store_unsealed_data);
static DEVICE_ATTR(unseal, 0644, show_unsealed_data, store_sealed_data);

static struct attribute *sec_key_attrs[] = {
	&dev_attr_generate.attr,
	&dev_attr_import.attr,
	&dev_attr_key_blob.attr,
	&dev_attr_seal.attr,
	&dev_attr_unseal.attr,
	NULL,
};

static struct attribute_group sec_key_attr_grp = {
	.attrs = sec_key_attrs,
};

static int __init sec_key_init(void)
{
	int err = 0;
	struct page *key_page = NULL;
	struct page *key_blob_page = NULL;
	struct page *sealed_buf_page = NULL;
	struct page *unsealed_buf_page = NULL;

	key_page = alloc_pages(GFP_KERNEL, get_order(KEY_SIZE));
	key_blob_page = alloc_pages(GFP_KERNEL, get_order(KEY_BLOB_SIZE));
	sealed_buf_page = alloc_pages(GFP_KERNEL,
					get_order(MAX_ENCRYPTED_DATA_SIZE));
	unsealed_buf_page = alloc_pages(GFP_KERNEL,
						get_order(MAX_PLAIN_DATA_SIZE));

	if (!key_page || !key_blob_page || !sealed_buf_page
							|| !unsealed_buf_page) {
		pr_err("\nCannot allocate memory for secure-key operation\n");
		if (key_page)
			free_pages((unsigned long)page_address(key_page),
							get_order(KEY_SIZE));
		if (key_blob_page)
			free_pages((unsigned long)page_address(key_blob_page),
						get_order(KEY_BLOB_SIZE));
		if (sealed_buf_page)
			free_pages((unsigned long)page_address(sealed_buf_page),
					get_order(MAX_ENCRYPTED_DATA_SIZE));
		if (unsealed_buf_page)
			free_pages((unsigned long)page_address(
			unsealed_buf_page), get_order(MAX_PLAIN_DATA_SIZE));
		return -ENOMEM;
	}

	key = page_address(key_page);
	key_blob = page_address(key_blob_page);
	sealed_buf = page_address(sealed_buf_page);
	unsealed_buf = page_address(unsealed_buf_page);

	sec_kobj = kobject_create_and_add("sec_key", NULL);

	if (!sec_kobj) {
		pr_info("\nFailed to register sec_key sysfs\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(sec_kobj, &sec_key_attr_grp);

	if (err) {
		kobject_put(sec_kobj);
		sec_kobj = NULL;
		return err;
	}

	return 0;
}

/*
 * Array Length is 4096 bytes, since 4MB is the max input size
 * that can be passed to SCM call
 */
static uint8_t encrypt_text[MAX_INPUT_SIZE];
static uint8_t decrypt_text[MAX_INPUT_SIZE];

static ssize_t mdt_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t pos, size_t count)
{
	uint8_t *tmp;
	/*
	 * Position '0' means new file being written,
	 * Hence allocate new memory after freeing already allocated mem if any
	 */
	if (pos == 0) {
		kfree(mdt_file);
		mdt_file = kzalloc((count) * sizeof(uint8_t), GFP_KERNEL);
	} else {
		tmp = mdt_file;
		mdt_file = krealloc(tmp,
			(pos + count) * sizeof(uint8_t), GFP_KERNEL);
	}

	if (!mdt_file)
		return -ENOMEM;

	memcpy((mdt_file + pos), buf, count);
	mdt_size = pos + count;
	return count;
}

static ssize_t seg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t pos, size_t count)
{
	uint8_t *tmp;
	if (pos == 0) {
		kfree(seg_file);
		seg_file = kzalloc((count) * sizeof(uint8_t), GFP_KERNEL);
	} else {
		tmp = seg_file;
		seg_file = krealloc(tmp, (pos + count) * sizeof(uint8_t),
					GFP_KERNEL);
	}

	if (!seg_file)
		return -ENOMEM;

	memcpy((seg_file + pos), buf, count);
	seg_size = pos + count;
	return count;
}

static ssize_t auth_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t pos, size_t count)
{
	uint8_t *tmp = NULL;

	if (pos == 0) {
		kfree(auth_file);
		auth_file = kzalloc((count) * sizeof(uint8_t), GFP_KERNEL);
	} else {
		tmp = auth_file;
		auth_file = krealloc(tmp, (pos + count) * sizeof(uint8_t),
					GFP_KERNEL);
	}

	if (!auth_file) {
		kfree(tmp);
		return -ENOMEM;
	}

	memcpy((auth_file + pos), buf, count);
	auth_size = pos + count;

	return count;
}

struct bin_attribute mdt_attr = {
	.attr = {.name = "mdt_file", .mode = 0666},
	.write = mdt_write,
};

struct bin_attribute seg_attr = {
	.attr = {.name = "seg_file", .mode = 0666},
	.write = seg_write,
};

struct bin_attribute auth_attr = {
	.attr = {.name = "auth_file", .mode = 0666},
	.write = auth_write,
};

static int qseecom_unload_app(void)
{
	struct qseecom_unload_app_ireq req;
	struct qseecom_command_scm_resp resp;
	int ret;

	req.qsee_cmd_id = QSEOS_APP_SHUTDOWN_COMMAND;
	req.app_id = qsee_app_id;

	/* SCM_CALL to unload the app */
	ret = qcom_scm_qseecom_unload(&req,
				     sizeof(struct qseecom_unload_app_ireq),
				     &resp, sizeof(resp));
	if (ret)
		pr_err("scm_call to unload app (id = %d) failed\n", req.app_id);

	pr_info("App id %d now unloaded\n", req.app_id);
	app_state = 0;

	return 0;
}

static int tzapp_test(struct device *dev, void *input,
		      void *output, int input_len, int option)
{
	int ret = 0;
	int ret1, ret2;

	union qseecom_client_send_data_ireq send_data_req;
	struct qseecom_command_scm_resp resp;
	struct qsee_send_cmd_rsp *msgrsp; /* response data sent from QSEE */
	struct page *pg_tmp;
	unsigned long pg_addr;

	dev = qdev;

	/*
	 * Using alloc_pages to avoid colliding with input pointer's
	 * allocated page, since qsee_register_shared_buffer() in sampleapp
	 * checks if input ptr is in secure area. Page where msgreq/msgrsp
	 * is allocated is added to blacklisted area by sampleapp and added
	 * as secure memory region, hence input data (shared buffer)
	 * cannot be in that secure memory region
	 */
	pg_tmp = alloc_page(GFP_KERNEL);
	if (!pg_tmp) {
		pr_err("\nFailed to allocate page");
		return -ENOMEM;
	}
	/*
	 * Getting virtual page address. pg_tmp will be pointing to
	 * first page structure
	 */
	if (props->tz_arch != QSEE_64) {
		struct qsee_32_send_cmd *msgreq;

		msgreq = (struct qsee_32_send_cmd *) page_address(pg_tmp);

		if (!msgreq) {
			pr_err("Unable to allocate memory\n");
			return -ENOMEM;
		}
		/* pg_addr for passing to free_page */
		pg_addr = (unsigned long) msgreq;

		msgrsp = (struct qsee_send_cmd_rsp *)((uint8_t *) msgreq +
					sizeof(struct qsee_32_send_cmd));
		if (!msgrsp) {
			kfree(msgreq);
			pr_err("Unable to allocate memory\n");
			return -ENOMEM;
		}

		/*
		 * option = 1 -> Basic Multiplication, option = 2 -> Encryption,
		 * option = 3 -> Decryption, option = 4 -> Crypto Function
		 * option = 5 -> Authorized OTP fusing
		 */

		switch (option) {
		case 1:
			msgreq->cmd_id = CLIENT_CMD1_BASIC_DATA;
			msgreq->data = *((dma_addr_t *)input);
			break;
		case 2:
			msgreq->cmd_id = CLIENT_CMD8_RUN_CRYPTO_ENCRYPT;
			break;
		case 3:
			msgreq->cmd_id = CLIENT_CMD9_RUN_CRYPTO_DECRYPT;
			break;
		case 4:
			msgreq->cmd_id = CLIENT_CMD8_RUN_CRYPTO_TEST;
			break;
		case 5:
			if (!auth_file) {
				pr_err("No OTP file provided\n");
				return -ENOMEM;
			}

			msgreq->cmd_id = CLIENT_CMD_AUTH;
			msgreq->data = dma_map_single(dev, auth_file,
						      auth_size, DMA_TO_DEVICE);
			ret = dma_mapping_error(dev, msgreq->data);
			if (ret) {
				pr_err("DMA Mapping Error: otp_buffer %d",
					ret);
				return ret;
			}

			break;
		default:
			pr_err("\n Invalid Option");
			goto fn_exit_1;
		}
		if (option == 2 || option == 3) {
			msgreq->data = dma_map_single(dev, input,
					input_len, DMA_TO_DEVICE);
			msgreq->data2 = dma_map_single(dev, output,
					input_len, DMA_FROM_DEVICE);

			ret1 = dma_mapping_error(dev, msgreq->data);
			ret2 = dma_mapping_error(dev, msgreq->data2);

			if (ret1 || ret2) {
				pr_err("\nDMA Mapping Error:input:%d output:%d",
				      ret1, ret2);

				if (!ret1) {
					dma_unmap_single(dev, msgreq->data,
						input_len, DMA_TO_DEVICE);
				}

				if (!ret2) {
					dma_unmap_single(dev, msgreq->data2,
						input_len, DMA_FROM_DEVICE);
				}
				return ret1 ? ret1 : ret2;
			}

			msgreq->test_buf_size = input_len;
			msgreq->len = input_len;
		}
		send_data_req.v1.qsee_cmd_id = QSEOS_CLIENT_SEND_DATA_COMMAND;
		send_data_req.v1.app_id = qsee_app_id;

		send_data_req.v1.req_ptr = dma_map_single(dev, msgreq,
					sizeof(*msgreq), DMA_TO_DEVICE);
		send_data_req.v1.rsp_ptr = dma_map_single(dev, msgrsp,
					sizeof(*msgrsp), DMA_FROM_DEVICE);

		ret1 = dma_mapping_error(dev, send_data_req.v1.req_ptr);
		ret2 = dma_mapping_error(dev, send_data_req.v1.rsp_ptr);

		if (!ret1 && !ret2) {
			send_data_req.v1.req_len =
					sizeof(struct qsee_32_send_cmd);
			send_data_req.v1.rsp_len =
					sizeof(struct qsee_send_cmd_rsp);
			ret = qcom_scm_qseecom_send_data(&send_data_req.v1,
							sizeof(send_data_req.v1)
							, &resp, sizeof(resp));
		}

		if (option == 2 || option == 3) {
			dma_unmap_single(dev, msgreq->data,
						input_len, DMA_TO_DEVICE);
			dma_unmap_single(dev, msgreq->data2,
						input_len, DMA_FROM_DEVICE);

		}

		if (!ret1) {
			dma_unmap_single(dev, send_data_req.v1.req_ptr,
				sizeof(*msgreq), DMA_TO_DEVICE);
		}

		if (!ret2) {
			dma_unmap_single(dev, send_data_req.v1.rsp_ptr,
				sizeof(*msgrsp), DMA_FROM_DEVICE);
		}

		if (ret1 || ret2) {
			pr_err("\nDMA Mapping Error:req_ptr:%d rsp_ptr:%d",
			      ret1, ret2);
			return ret1 ? ret1 : ret2;
		}

		if (ret) {
			pr_err("qseecom_scm_call failed with err: %d\n", ret);
			goto fn_exit_1;
		}

		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			pr_err("Result incomplete\n");
			ret = -EINVAL;
			goto fn_exit_1;
		} else {
			if (resp.result != QSEOS_RESULT_SUCCESS) {
				pr_err("Response result %lu not supported\n",
								resp.result);
				ret = -EINVAL;
				goto fn_exit_1;
			} else {
				if (option == 4)
					pr_info("Crypto test success\n");
			}
		}

		if (option == 1) {
			if (msgrsp->status) {
				pr_err("Input size exceeded supported range\n");
				ret = -EINVAL;
			}
			basic_output = msgrsp->data;
		} else if (option == 5) {
			if (msgrsp->status) {
				pr_err("Auth OTP failed with response %d\n",
								msgrsp->status);
				ret = -EIO;
			} else
				pr_info("Auth and Blow Success");
		}
fn_exit_1:
		free_page(pg_addr);
		if (option == 5) {
			dma_unmap_single(dev, msgreq->data, auth_size,
								DMA_TO_DEVICE);
		}

	} else {
		struct qsee_64_send_cmd *msgreq;

		msgreq = (struct qsee_64_send_cmd *) page_address(pg_tmp);

		if (!msgreq) {
			pr_err("Unable to allocate memory\n");
			return -ENOMEM;
		}
		/* pg_addr for passing to free_page */
		pg_addr = (unsigned long) msgreq;

		msgrsp = (struct qsee_send_cmd_rsp *)((uint8_t *) msgreq +
					sizeof(struct qsee_64_send_cmd));
		if (!msgrsp) {
			kfree(msgreq);
			pr_err("Unable to allocate memory\n");
			return -ENOMEM;
		}

		/*
		 * option = 1 -> Basic Multiplication, option = 2 -> Encryption,
		 * option = 3 -> Decryption, option = 4 -> Crypto Function
		 * option = 5 -> Authorized OTP fusing
		 */

		switch (option) {
		case 1:
			msgreq->cmd_id = CLIENT_CMD1_BASIC_DATA;
			msgreq->data = *((dma_addr_t *)input);
			break;
		case 2:
			msgreq->cmd_id = CLIENT_CMD8_RUN_CRYPTO_ENCRYPT;
			break;
		case 3:
			msgreq->cmd_id = CLIENT_CMD9_RUN_CRYPTO_DECRYPT;
			break;
		case 4:
			msgreq->cmd_id = CLIENT_CMD8_RUN_CRYPTO_TEST;
			break;
		case 5:
			if (!auth_file) {
				pr_err("No OTP file provided\n");
				return -ENOMEM;
			}

			msgreq->cmd_id = CLIENT_CMD_AUTH;
			msgreq->data = dma_map_single(dev, auth_file,
						      auth_size, DMA_TO_DEVICE);
			ret = dma_mapping_error(dev, msgreq->data);
			if (ret) {
				pr_err("DMA Mapping Error: otp_buffer %d",
					ret);
				return ret;
			}

			break;
		default:
			pr_err("\n Invalid Option");
			goto fn_exit;
		}
		if (option == 2 || option == 3) {
			msgreq->data = dma_map_single(dev, input,
					input_len, DMA_TO_DEVICE);
			msgreq->data2 = dma_map_single(dev, output,
					input_len, DMA_FROM_DEVICE);

			ret1 = dma_mapping_error(dev, msgreq->data);
			ret2 = dma_mapping_error(dev, msgreq->data2);

			if (ret1 || ret2) {
				pr_err("\nDMA Mapping Error:input:%d output:%d",
				      ret1, ret2);
				if (!ret1) {
					dma_unmap_single(dev, msgreq->data,
						input_len, DMA_TO_DEVICE);
				}

				if (!ret2) {
					dma_unmap_single(dev, msgreq->data2,
						input_len, DMA_FROM_DEVICE);
				}
				return ret1 ? ret1 : ret2;
			}
			msgreq->test_buf_size = input_len;
			msgreq->len = input_len;
		}
		send_data_req.v1.qsee_cmd_id = QSEOS_CLIENT_SEND_DATA_COMMAND;
		send_data_req.v1.app_id = qsee_app_id;

		send_data_req.v1.req_ptr = dma_map_single(dev, msgreq,
					sizeof(*msgreq), DMA_TO_DEVICE);
		send_data_req.v1.rsp_ptr = dma_map_single(dev, msgrsp,
					sizeof(*msgrsp), DMA_FROM_DEVICE);

		ret1 = dma_mapping_error(dev, send_data_req.v1.req_ptr);
		ret2 = dma_mapping_error(dev, send_data_req.v1.rsp_ptr);


		if (!ret1 && !ret2) {
			send_data_req.v1.req_len =
					sizeof(struct qsee_64_send_cmd);
			send_data_req.v1.rsp_len =
					sizeof(struct qsee_send_cmd_rsp);
			ret = qcom_scm_qseecom_send_data(&send_data_req.v2,
							sizeof(send_data_req.v2)
							, &resp, sizeof(resp));
		}

		if (option == 2 || option == 3) {
			dma_unmap_single(dev, msgreq->data,
						input_len, DMA_TO_DEVICE);
			dma_unmap_single(dev, msgreq->data2,
						input_len, DMA_FROM_DEVICE);

		}

		if (!ret1) {
			dma_unmap_single(dev, send_data_req.v1.req_ptr,
				sizeof(*msgreq), DMA_TO_DEVICE);
		}

		if (!ret2) {
			dma_unmap_single(dev, send_data_req.v1.rsp_ptr,
				sizeof(*msgrsp), DMA_FROM_DEVICE);
		}

		if (ret1 || ret2) {
			pr_err("\nDMA Mapping Error:req_ptr:%d rsp_ptr:%d",
			      ret1, ret2);
			return ret1 ? ret1 : ret2;
		}

		if (ret) {
			pr_err("qseecom_scm_call failed with err: %d\n", ret);
			goto fn_exit;
		}

		if (resp.result == QSEOS_RESULT_INCOMPLETE) {
			pr_err("Result incomplete\n");
			ret = -EINVAL;
			goto fn_exit;
		} else {
			if (resp.result != QSEOS_RESULT_SUCCESS) {
				pr_err("Response result %lu not supported\n",
								resp.result);
				ret = -EINVAL;
				goto fn_exit;
			} else {
				if (option == 4)
					pr_info("Crypto test success\n");
			}
		}

		if (option == 1) {
			if (msgrsp->status) {
				pr_err("Input size exceeded supported range\n");
				ret = -EINVAL;
			}
			basic_output = msgrsp->data;
		} else if (option == 5) {
			if (msgrsp->status) {
				pr_err("Auth OTP failed with response %d\n",
								msgrsp->status);
				ret = -EIO;
			} else
				pr_info("Auth and Blow Success");
		}
fn_exit:
		free_page(pg_addr);
		if (option == 5) {
			dma_unmap_single(dev, msgreq->data, auth_size,
								DMA_TO_DEVICE);
		}
	}
	return ret;
}

static int32_t copy_files(int *img_size)
{
	uint8_t *buf;

	if (mdt_file && seg_file) {
		*img_size = mdt_size + seg_size;

		qsee_sbuffer = kzalloc(*img_size, GFP_KERNEL);
		if (!qsee_sbuffer) {
			pr_err("Error: qsee_sbuffer alloc failed\n");
			return -ENOMEM;
		}
		buf = qsee_sbuffer;

		memcpy(buf, mdt_file, mdt_size);
		buf += mdt_size;
		memcpy(buf, seg_file, seg_size);
		buf += seg_size;
	} else {
		pr_err("\nSampleapp file Inputs not provided\n");
		return -EINVAL;
	}
	return 0;
}

static int load_app(struct device *dev)
{
	struct qseecom_load_app_ireq load_req;
	struct qseecom_command_scm_resp resp;
	int ret, ret1;
	int img_size;

	ret = copy_files(&img_size);
	if (ret) {
		pr_err("Copying Files failed\n");
		return ret;
	}

	dev = qdev;

	/* Populate the structure for sending scm call to load image */
	strlcpy(load_req.app_name, "sampleapp", sizeof("sampleapp"));
	load_req.qsee_cmd_id = QSEOS_APP_START_COMMAND;
	load_req.mdt_len = mdt_size;
	load_req.img_len = img_size;
	load_req.phy_addr = dma_map_single(dev, qsee_sbuffer,
				img_size, DMA_TO_DEVICE);
	ret1 = dma_mapping_error(dev, load_req.phy_addr);
	if (ret1 == 0) {
		/* SCM_CALL to load the app and get the app_id back */
		ret = qcom_scm_qseecom_load(&load_req,
					   sizeof(struct qseecom_load_app_ireq),
					   &resp, sizeof(resp));
		dma_unmap_single(dev, load_req.phy_addr,
				img_size, DMA_TO_DEVICE);
	}
	if (ret1) {
		pr_err("\nDMA Mapping error (qsee_sbuffer)");
		return ret1;
	}
	if (ret) {
		pr_err("SCM_CALL to load app failed\n");
		return ret;
	}

	if (resp.result == QSEOS_RESULT_FAILURE) {
		pr_err("SCM_CALL rsp.result is QSEOS_RESULT_FAILURE\n");
		return -EFAULT;
	}

	if (resp.result == QSEOS_RESULT_INCOMPLETE)
		pr_err("Process_incomplete_cmd ocurred\n");

	if (resp.result != QSEOS_RESULT_SUCCESS) {
		pr_err("scm_call failed resp.result unknown, %lu\n",
				resp.result);
		return -EFAULT;
	}

	pr_info("\nLoaded Sampleapp Successfully!!!!!\n");
	app_state = 1;

	qsee_app_id = resp.data;
	return 0;
}

/* To show basic multiplication output */
static ssize_t
show_basic_output(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, (basic_data_len + 1), "%d", basic_output);
}

/* Basic multiplication App*/
static ssize_t
store_basic_input(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	dma_addr_t __aligned(sizeof(dma_addr_t) * 8) basic_input = 0;
	uint32_t ret = 0;
	basic_data_len = count;
	if ((count - 1) == 0) {
		pr_err("\n Input cannot be NULL!");
		return -EINVAL;
	}
	if (kstrtouint(buf, 10, &basic_input) || basic_input > (U32_MAX / 10))
		pr_err("\n Please enter a valid unsigned integer less than %d",
			(U32_MAX / 10));
	else
		ret = tzapp_test(dev, &basic_input, NULL, 0, 1);

	return ret ? ret : count;
}

/* To show encrypted plain text*/
static ssize_t
show_encrypt_output(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	memcpy(buf, encrypt_text, enc_len);
	return enc_len;
}

/* To Encrypt input plain text */
static ssize_t
store_encrypt_input(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int32_t ret = -EINVAL;
	uint8_t *input_pt;
	uint8_t *output_pt;

	enc_len = count;
	if (enc_len == 0) {
		pr_err("\n Input cannot be NULL!");
		return -EINVAL;
	}
	if ((enc_len % 16 != 0) || (enc_len > MAX_INPUT_SIZE)) {
		pr_info("\n Input Length must be multiple of 16 & < 4096 bytes");
		return -EINVAL;
	}

	input_pt = kzalloc(enc_len * sizeof(uint8_t *), GFP_KERNEL);
	if (!input_pt)
		return -ENOMEM;
	memcpy(input_pt, buf, count);

	output_pt = kzalloc(enc_len * sizeof(uint8_t *), GFP_KERNEL);
	if (!output_pt) {
		kfree(input_pt);
		return -ENOMEM;
	}

	ret = tzapp_test(dev, (uint8_t *)input_pt,
			 (uint8_t *)output_pt, enc_len, 2);

	if (!ret)
		memcpy(encrypt_text, output_pt, enc_len);

	kfree(input_pt);
	kfree(output_pt);
	return count;
}

/* To show decrypted cipher text */
static ssize_t
show_decrypt_output(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	memcpy(buf, decrypt_text, dec_len);
	return dec_len;
}

/* To decrypt input cipher text */
static ssize_t
store_decrypt_input(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int32_t ret = -EINVAL;
	uint8_t *input_pt;
	uint8_t *output_pt;

	dec_len = count;
	if (dec_len == 0) {
		pr_err("\n Input cannot be NULL!");
		return -EINVAL;
	}

	if ((dec_len % 16 != 0) || (dec_len > MAX_INPUT_SIZE)) {
		pr_info("\n Input Length must be multiple of 16 & < 4096 bytes");
		return -EINVAL;
	}

	input_pt = kzalloc(dec_len * sizeof(uint8_t *), GFP_KERNEL);
	if (!input_pt)
		return -ENOMEM;
	memcpy(input_pt, buf, dec_len);

	output_pt = kzalloc(dec_len * sizeof(uint8_t *), GFP_KERNEL);
	if (!output_pt) {
		kfree(input_pt);
		return -ENOMEM;
	}

	ret = tzapp_test(dev, (uint8_t *)input_pt,
			 (uint8_t *)output_pt, dec_len, 3);
	if (!ret)
		memcpy(decrypt_text, output_pt, dec_len);

	kfree(input_pt);
	kfree(output_pt);
	return count;
}

static ssize_t
store_load_start(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int load_cmd;

	if (kstrtouint(buf, 10, &load_cmd)) {
		pr_err("\n Provide valid integer input!");
		pr_err("Echo 1 to start loading app");
		pr_err("Echo 0 to start unloading app");
		return -EINVAL;
	}
	if (load_cmd == 1) {
		if (!app_state)
			load_app(dev);
		else
			pr_info("\nApp already loaded...");
	} else if (load_cmd == 0) {
		if (app_state)
			qseecom_unload_app();
		else
			pr_info("\nApp already unloaded...");
	} else {
		pr_info("\nEcho 1 to start loading app");
		pr_info("\nEcho 0 to start unloading app");
	}

	return count;
}

static ssize_t
store_crypto_input(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	tzapp_test(dev, NULL, NULL, 0, 4);
	return count;
}

static ssize_t
store_fuse_otp_input(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	tzapp_test(dev, (void *)buf, NULL, 0, 5);
	return count;
}

static DEVICE_ATTR(load_start, S_IWUSR, NULL, store_load_start);
static DEVICE_ATTR(basic_data, 0644, show_basic_output, store_basic_input);
static DEVICE_ATTR(encrypt, 0644, show_encrypt_output, store_encrypt_input);
static DEVICE_ATTR(decrypt, 0644, show_decrypt_output, store_decrypt_input);
static DEVICE_ATTR(crypto, 0644, NULL, store_crypto_input);
static DEVICE_ATTR(fuse_otp, 0644, NULL, store_fuse_otp_input);

struct kobject *tzapp_kobj;
struct attribute_group tzapp_attr_grp;

static int __init tzapp_init(void)
{
	int err;
	int i = 0;
	struct attribute **tzapp_attrs = kzalloc((hweight_long(props->function)
				+ 1) * sizeof(*tzapp_attrs), GFP_KERNEL);

	if (!tzapp_attrs) {
		pr_err("\nCannot allocate memory..tzapp");
		return -ENOMEM;
	}

	tzapp_attrs[i++] = &dev_attr_load_start.attr;

	if (props->function & MUL)
		tzapp_attrs[i++] = &dev_attr_basic_data.attr;

	if (props->function & ENC)
		tzapp_attrs[i++] = &dev_attr_encrypt.attr;

	if (props->function & DEC)
		tzapp_attrs[i++] = &dev_attr_decrypt.attr;

	if (props->function & CRYPTO)
		tzapp_attrs[i++] = &dev_attr_crypto.attr;

	if (props->function & AUTH_OTP)
		tzapp_attrs[i++] = &dev_attr_fuse_otp.attr;

	tzapp_attrs[i] = NULL;

	tzapp_attr_grp.attrs = tzapp_attrs;

	tzapp_kobj = kobject_create_and_add("tzapp", firmware_kobj);

	err = sysfs_create_group(tzapp_kobj, &tzapp_attr_grp);

	if (err) {
		kobject_put(tzapp_kobj);
		return err;
	}
	return 0;
}

static int __init qseecom_probe(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	const struct of_device_id *id;
	unsigned int start = 0, size = 0;
	struct qsee_notify_app notify_app;
	struct qseecom_command_scm_resp resp;
	int ret = 0, ret1 = 0;

	if (!of_node)
		return -ENODEV;

	qdev = &pdev->dev;
	id = of_match_device(qseecom_of_table, &pdev->dev);

	if (!id)
		return -ENODEV;

	ret = of_property_read_u32(of_node, "mem-start", &start);
	ret1 = of_property_read_u32(of_node, "mem-size", &size);
	if (ret || ret1) {
		pr_err("No mem-region specified, using default\n");
		goto load;
	}

	notify_app.cmd_id = QSEE_APP_NOTIFY_COMMAND;
	notify_app.applications_region_addr = start;
	notify_app.applications_region_size = size;

	ret = qcom_scm_qseecom_notify(&notify_app,
				     sizeof(struct qsee_notify_app),
				     &resp, sizeof(resp));
	if (ret) {
		pr_err("Notify App failed\n");
		return -1;
	}

load:
	props = ((struct qseecom_props *)id->data);

	sysfs_create_bin_file(firmware_kobj, &mdt_attr);
	sysfs_create_bin_file(firmware_kobj, &seg_attr);

	if (props->function & AUTH_OTP)
		sysfs_create_bin_file(firmware_kobj, &auth_attr);

	if (!tzapp_init())
		pr_info("\nLoaded tz app Module Successfully!\n");
	else
		pr_info("\nFailed to load tz app module\n");

	if (props->function & AES_SEC_KEY) {
		if (!sec_key_init())
			pr_info("\nLoaded Sec Key Module Successfully!\n");
		else
			pr_info("\nFailed to load Sec Key Module\n");
	}

	return 0;
}

static int __exit qseecom_remove(struct platform_device *pdev)
{
	if (app_state)
		qseecom_unload_app();

	sysfs_remove_bin_file(firmware_kobj, &mdt_attr);
	sysfs_remove_bin_file(firmware_kobj, &seg_attr);

	if (props->function & AUTH_OTP)
		sysfs_remove_bin_file(firmware_kobj, &auth_attr);

	sysfs_remove_group(tzapp_kobj, &tzapp_attr_grp);
	kobject_put(tzapp_kobj);

	if (props->function & AES_SEC_KEY) {
		free_pages((unsigned long)key, get_order(KEY_SIZE));
		free_pages((unsigned long)key_blob, get_order(KEY_BLOB_SIZE));
		free_pages((unsigned long)sealed_buf,
			   get_order(MAX_ENCRYPTED_DATA_SIZE));
		free_pages((unsigned long)unsealed_buf,
			   get_order(MAX_PLAIN_DATA_SIZE));

		sysfs_remove_group(sec_kobj, &sec_key_attr_grp);
		kobject_put(sec_kobj);
	}

	kfree(mdt_file);
	kfree(seg_file);

	if (props->function & AUTH_OTP)
		kfree(auth_file);

	kfree(qsee_sbuffer);

	return 0;
}

static struct platform_driver qseecom_driver = {
	.probe = qseecom_probe,
	.remove = qseecom_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = qseecom_of_table,
	},
};
module_platform_driver(qseecom_driver);

MODULE_DESCRIPTION("QSEECOM Driver");
MODULE_LICENSE("GPL v2");
