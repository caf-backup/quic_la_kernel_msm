/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/qcom_scm.h>
#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>

#include "qcom_scm.h"

#define QCOM_SCM_FNID(s, c) ((((s) & 0xFF) << 8) | ((c) & 0xFF))

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

enum qcom_scm_arg_types {
	QCOM_SCM_VAL,
	QCOM_SCM_RO,
	QCOM_SCM_RW,
	QCOM_SCM_BUFVAL,
};

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			   (((a) & 0x3) << 4) | \
			   (((b) & 0x3) << 6) | \
			   (((c) & 0x3) << 8) | \
			   (((d) & 0x3) << 10) | \
			   (((e) & 0x3) << 12) | \
			   (((f) & 0x3) << 14) | \
			   (((g) & 0x3) << 16) | \
			   (((h) & 0x3) << 18) | \
			   (((i) & 0x3) << 20) | \
			   (((j) & 0x3) << 22) | \
			   ((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/**
 * struct qcom_scm_desc
 * @arginfo:	Metadata describing the arguments in args[]
 * @args:	The array of arguments for the secure syscall
 * @res:	The values returned by the secure syscall
 */
struct qcom_scm_desc {
	u32 arginfo;
	u64 args[MAX_QCOM_SCM_ARGS];
};

static u64 qcom_smccc_convention = -1;
static DEFINE_MUTEX(qcom_scm_lock);

#define QCOM_SCM_EBUSY_WAIT_MS 30
#define QCOM_SCM_EBUSY_MAX_RETRY 20

#define N_EXT_QCOM_SCM_ARGS 7
#define FIRST_EXT_ARG_IDX 3
#define N_REGISTER_ARGS (MAX_QCOM_SCM_ARGS - N_EXT_QCOM_SCM_ARGS + 1)

/**
 * qcom_scm_call() - Invoke a syscall in the secure world
 * @dev:	device
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @desc:	Descriptor structure containing arguments and return values
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
*/
static int qcom_scm_call(struct device *dev, u32 svc_id, u32 cmd_id,
			 const struct qcom_scm_desc *desc,
			 struct arm_smccc_res *res)
{
	int arglen = desc->arginfo & 0xf;
	int retry_count = 0, i;
	u32 fn_id = QCOM_SCM_FNID(svc_id, cmd_id);
	u64 cmd, x5 = desc->args[FIRST_EXT_ARG_IDX];
	dma_addr_t args_phys = 0;
	void *args_virt = NULL;
	size_t alloc_len;

	if (unlikely(arglen > N_REGISTER_ARGS)) {
		alloc_len = N_EXT_QCOM_SCM_ARGS * sizeof(u64);
		args_virt = kzalloc(PAGE_ALIGN(alloc_len), GFP_KERNEL);

		if (!args_virt)
			return -ENOMEM;

		if (qcom_smccc_convention == ARM_SMCCC_SMC_32) {
			__le32 *args = args_virt;

			for (i = 0; i < N_EXT_QCOM_SCM_ARGS; i++)
				args[i] = cpu_to_le32(desc->args[i +
						      FIRST_EXT_ARG_IDX]);
		} else {
			__le64 *args = args_virt;

			for (i = 0; i < N_EXT_QCOM_SCM_ARGS; i++)
				args[i] = cpu_to_le64(desc->args[i +
						      FIRST_EXT_ARG_IDX]);
		}

		args_phys = dma_map_single(dev, args_virt, alloc_len,
					   DMA_TO_DEVICE);

		if (dma_mapping_error(dev, args_phys)) {
			kfree(args_virt);
			return -ENOMEM;
		}

		x5 = args_phys;
	}

	do {
		mutex_lock(&qcom_scm_lock);

		cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_STD_CALL,
					 qcom_smccc_convention,
					 ARM_SMCCC_OWNER_SIP, fn_id);

		do {
			arm_smccc_smc(cmd, desc->arginfo, desc->args[0],
				      desc->args[1], desc->args[2], x5, 0, 0,
				      res);
		} while (res->a0 == QCOM_SCM_INTERRUPTED);

		mutex_unlock(&qcom_scm_lock);

		if (res->a0 == QCOM_SCM_V2_EBUSY) {
			if (retry_count++ > QCOM_SCM_EBUSY_MAX_RETRY)
				break;
			msleep(QCOM_SCM_EBUSY_WAIT_MS);
		}
	}  while (res->a0 == QCOM_SCM_V2_EBUSY);

	if (args_virt) {
		dma_unmap_single(dev, args_phys, alloc_len, DMA_TO_DEVICE);
		kfree(args_virt);
	}

	if (res->a0 < 0)
		return qcom_scm_remap_error(res->a0);

	return 0;
}

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the cold boot address of the cpus. Any cpu outside the supported
 * range would be removed from the cpu present mask.
 */
int __qcom_scm_set_cold_boot_addr(void *entry, const cpumask_t *cpus)
{
	return 0;
}

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for cpus
 * @dev: Device pointer
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the Linux entry point for the SCM to transfer control to when coming
 * out of a power down. CPU power down may be executed on cpuidle or hotplug.
 */
int __qcom_scm_set_warm_boot_addr(struct device *dev, void *entry,
				  const cpumask_t *cpus)
{
	return -ENOTSUPP;
}

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void __qcom_scm_cpu_power_down(u32 flags)
{
}

int __qcom_scm_is_call_available(struct device *dev, u32 svc_id, u32 cmd_id)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	desc.arginfo = QCOM_SCM_ARGS(1);
	desc.args[0] = QCOM_SCM_FNID(svc_id, cmd_id) |
			(ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_INFO, QCOM_IS_CALL_AVAIL_CMD,
			    &desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_hdcp_req(struct device *dev, struct qcom_scm_hdcp_req *req,
			u32 req_cnt, u32 *resp)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	if (req_cnt > QCOM_SCM_HDCP_MAX_REQ_CNT)
		return -ERANGE;

	desc.args[0] = req[0].addr;
	desc.args[1] = req[0].val;
	desc.args[2] = req[1].addr;
	desc.args[3] = req[1].val;
	desc.args[4] = req[2].addr;
	desc.args[5] = req[2].val;
	desc.args[6] = req[3].addr;
	desc.args[7] = req[3].val;
	desc.args[8] = req[4].addr;
	desc.args[9] = req[4].val;
	desc.arginfo = QCOM_SCM_ARGS(10);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_HDCP, QCOM_SCM_CMD_HDCP, &desc,
			    &res);
	*resp = res.a1;

	return ret;
}

void __qcom_scm_init(void)
{
	u64 cmd;
	struct arm_smccc_res res;
	u32 function = QCOM_SCM_FNID(QCOM_SCM_SVC_INFO, QCOM_IS_CALL_AVAIL_CMD);

	/* First try a SMC64 call */
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
				 ARM_SMCCC_OWNER_SIP, function);

	arm_smccc_smc(cmd, QCOM_SCM_ARGS(1), cmd & (~BIT(ARM_SMCCC_TYPE_SHIFT)),
		      0, 0, 0, 0, 0, &res);

	if (!res.a0 && res.a1)
		qcom_smccc_convention = ARM_SMCCC_SMC_64;
	else
		qcom_smccc_convention = ARM_SMCCC_SMC_32;
}

bool __qcom_scm_pas_supported(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
				QCOM_SCM_PAS_IS_SUPPORTED_CMD,
				&desc, &res);

	return ret ? false : !!res.a1;
}

int __qcom_scm_pas_init_image(struct device *dev, u32 peripheral,
			      dma_addr_t metadata_phys)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	desc.args[0] = peripheral;
	desc.args[1] = metadata_phys;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_RW);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL, QCOM_SCM_PAS_INIT_IMAGE_CMD,
				&desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_pas_mem_setup(struct device *dev, u32 peripheral,
			      phys_addr_t addr, phys_addr_t size)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	desc.args[0] = peripheral;
	desc.args[1] = addr;
	desc.args[2] = size;
	desc.arginfo = QCOM_SCM_ARGS(3);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL, QCOM_SCM_PAS_MEM_SETUP_CMD,
				&desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_pas_auth_and_reset(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL,
				QCOM_SCM_PAS_AUTH_AND_RESET_CMD,
				&desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL, QCOM_SCM_PAS_SHUTDOWN_CMD,
			&desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_pas_mss_reset(struct device *dev, bool reset)
{
	struct qcom_scm_desc desc = {0};
	struct arm_smccc_res res;
	int ret;

	desc.args[0] = reset;
	desc.args[1] = 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, QCOM_SCM_SVC_PIL, QCOM_SCM_PAS_MSS_RESET, &desc,
			    &res);

	return ret ? : res.a1;
}

int __qcom_qfprom_show_authenticate(struct device *dev, char *buf)
{
	int ret;
	struct arm_smccc_res res;
	struct qcom_scm_desc desc = {0};
	dma_addr_t auth_phys;
	void *auth_buf;

	auth_buf = dma_alloc_coherent(dev, sizeof(*buf),
					&auth_phys, GFP_KERNEL);
	if (!auth_buf) {
		dev_err(dev, "Allocation for auth buffer failed\n");
		return -ENOMEM;
	}
	desc.args[0] = (u64)auth_phys;
	desc.args[1] = sizeof(char);
	desc.arginfo = SCM_ARGS(2, SCM_RO);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_FUSE,
				QCOM_QFPROM_IS_AUTHENTICATE_CMD, &desc, &res);
	memcpy(buf, auth_buf, sizeof(char));
	dma_free_coherent(dev, sizeof(*buf), auth_buf, auth_phys);
	return ret ? : res.a1;
}

int __qcom_qfprom_write_version(struct device *dev, void *wrip, int size)
{
	return -ENOTSUPP;
}

int __qcom_qfprom_read_version(struct device *dev, uint32_t sw_type,
			uint32_t value, uint32_t qfprom_ret_ptr)
{
	int ret;
	struct arm_smccc_res res;
	struct qcom_scm_desc desc = {0};
	struct qfprom_xtra {
		uint32_t qfprom_ret_ptr;
		uint32_t size;
	} *xtra;
	dma_addr_t xtra_phys;

	xtra = (struct qfprom_xtra *)dma_alloc_coherent(dev,
		sizeof(struct qfprom_xtra), &xtra_phys, GFP_KERNEL);
	if (!xtra) {
		dev_err(dev, "Allocation for xtraargs buffer failed\n");
		return -ENOMEM;
	}

	xtra->qfprom_ret_ptr = qfprom_ret_ptr;
	xtra->size = sizeof(uint32_t);

	desc.args[0] = sw_type;
	desc.args[1] = (u64)value;
	desc.args[2] = sizeof(uint32_t);
	desc.args[3] = (u64)xtra_phys;

	desc.arginfo = SCM_ARGS(5, SCM_VAL, SCM_RW, SCM_VAL, SCM_RW,
						SCM_VAL);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_FUSE,
				QCOM_QFPROM_ROW_READ_CMD, &desc, &res);
	dma_free_coherent(dev, sizeof(struct qfprom_xtra), xtra,
				xtra_phys);
	return ret ? : res.a1;
}

int __qcom_scm_regsave(struct device *dev, u32 svc_id, u32 cmd_id,
			void *scm_regsave, unsigned int buf_size)
{
	struct scm_desc desc = {0};
	struct arm_smccc_res res;
	int ret;

	desc.args[0] = (u64)virt_to_phys(scm_regsave);
	desc.args[1] = buf_size;
	desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_REGSAVE, QCOM_SCM_REGSAVE_CMD,
				&desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_tcsr(struct device *dev, u32 svc_id, u32 cmd_id,
			struct qcom_scm_tcsr_req *tcsr_cmd)
{
	return -ENOTSUPP;
}

int __qcom_scm_dload(struct device *dev, u32 svc_id, u32 cmd_id, void *cmd_buf)
{
	struct scm_desc desc = {0};
	struct arm_smccc_res res;
	int ret;
	unsigned int enable = *((unsigned int *)cmd_buf);

#define TCSR_BOOT_MISC_REG	0x193d100ull
#define DLOAD_MODE_ENABLE	0x10ull
#define DLOAD_MODE_DISABLE	0x00ull

	desc.args[0] = TCSR_BOOT_MISC_REG;
	desc.args[1] = enable ? DLOAD_MODE_ENABLE : DLOAD_MODE_DISABLE;
	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	ret = qcom_scm_call(dev, SCM_SVC_IO_ACCESS, SCM_IO_WRITE,
				&desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_sdi(struct device *dev, u32 svc_id, u32 cmd_id)
{
	struct scm_desc desc = {0};
	struct arm_smccc_res res;
	int ret;

	desc.args[0] = 1ull; /* Disable wdog debug */
	desc.args[1] = 0ull; /* SDI Enable */
	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	ret = qcom_scm_call(dev, QCOM_SCM_SVC_BOOT,
		SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID, &desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_tzsched(struct device *dev, const void *req, size_t req_size,
			void *resp, size_t resp_size)
{
	return -ENOTSUPP;
}

int __qcom_scm_pinmux_read(u32 svc_id, u32 cmd_id, u32 arg1)
{
	return -ENOTSUPP;
}

int __qcom_scm_pinmux_write(u32 svc_id, u32 cmd_id, u32 arg1, u32 arg2)
{
	return -ENOTSUPP;
}

int __qcom_scm_cache_dump(u32 cpu)
{
	return -ENOTSUPP;
}

int __qcom_scm_get_cache_dump_size(struct device *dev, u32 cmd_id,
					void *cmd_buf, u32 size)
{
	return -ENOTSUPP;
}

int __qcom_scm_send_cache_dump_addr(struct device *dev, u32 cmd_id,
					void *cmd_buf, u32 size)
{
	return -ENOTSUPP;
}

static int __qcom_scm_tz_log_v8(struct device *dev, u32 svc_id, u32 cmd_id,
					u32 log_buf, u32 buf_size)
{
	struct scm_desc desc = {0};
	struct arm_smccc_res res;
	int ret;

	desc.args[0] = log_buf;
	desc.args[1] = buf_size;
	desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);

	ret = qcom_scm_call(dev, svc_id, cmd_id, &desc, &res);

	return ret ? : res.a1;
}

int __qcom_scm_tz_log(struct device *dev, u32 svc_id, u32 cmd_id,
				void *ker_buf, u32 *buf_len, u32 **ring_off,
				struct tzbsp_diag_log_t **log)
{
	int ret;
	struct tzbsp_diag_t_v8 *tz_diag_v8;
	dma_addr_t log_buf;

	log_buf = dma_map_single(dev, ker_buf, BUF_LEN_V8, DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, log_buf);

	if (ret != 0) {
		pr_err("DMA Mapping Error : %d\n", ret);
		return -EINVAL;
	}

	ret = __qcom_scm_tz_log_v8(dev, svc_id, cmd_id,
					log_buf, BUF_LEN_V8);
	dma_unmap_single(dev, log_buf, BUF_LEN_V8, DMA_FROM_DEVICE);

	if (ret == 0) {
		tz_diag_v8 = (struct tzbsp_diag_t_v8 *) ker_buf;
		*ring_off = (uint32_t *)&(tz_diag_v8->ring_off);
		*log = (struct tzbsp_diag_log_t *) &tz_diag_v8->log;
		*buf_len = BUF_LEN_V8;
	}

	return ret;
}

int __qcom_los_scm_call(struct device *dev, u32 svc_id, u32 cmd_id,
					void *cmd_buf, size_t size)
{
	return -ENOTSUPP;
}
