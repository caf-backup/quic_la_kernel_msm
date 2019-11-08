/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/soc/qcom/qmi.h>

#include "main.h"
#include "debug.h"
#include "qmi.h"
#include "pci.h"

#define WLFW_SERVICE_INS_ID_V01		1
#define WLFW_CLIENT_ID			0x4b4e454c
#define MAX_BDF_FILE_NAME		64
#define DEFAULT_BDF_FILE_NAME		"bdwlan.bin"
#define BDF_FILE_NAME_PREFIX		"bdwlan.b"

#define DEFAULT_CAL_FILE_NAME		"caldata.bin"
#define CAL_FILE_NAME_PREFIX		"caldata.b"

#ifdef CONFIG_CNSS2_DEBUG
static unsigned int qmi_timeout = 10000;
module_param(qmi_timeout, uint, 0600);
MODULE_PARM_DESC(qmi_timeout, "Timeout for QMI message in milliseconds");
EXPORT_SYMBOL(qmi_timeout);

#define QMI_WLFW_TIMEOUT_MS		qmi_timeout
#else
#define QMI_WLFW_TIMEOUT_MS		10000
#endif

#define QMI_WLFW_TIMEOUT_JF msecs_to_jiffies(QMI_WLFW_TIMEOUT_MS)
#define QMI_WLFW_MAX_RECV_BUF_SIZE      SZ_8K

bool daemon_support;
module_param(daemon_support, bool, 0600);
MODULE_PARM_DESC(daemon_support, "User space has cnss-daemon support or not");

bool cold_boot_support;
module_param(cold_boot_support, bool, 0600);
MODULE_PARM_DESC(cold_boot_support, "User space has cold_boot_support or not");

bool caldata_support = true;
module_param(caldata_support, bool, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(caldata_support, "caldata support");

unsigned int qca8074_fw_mem_mode = 0xFF;
module_param(qca8074_fw_mem_mode, uint, 0600);
MODULE_PARM_DESC(qca8074_fw_mem_mode, "qca8074_fw_mem_mode");

static bool bdf_bypass = true;
#ifdef CONFIG_CNSS2_DEBUG
module_param(bdf_bypass, bool, 0600);
MODULE_PARM_DESC(bdf_bypass, "If BDF is not found, send dummy BDF to FW");
#endif

struct qmi_history qmi_log[QMI_HISTORY_SIZE];
int qmi_history_index;
DEFINE_SPINLOCK(qmi_log_spinlock);
struct wlfw_request_mem_ind_msg_v01 ind_message = {0};

void cnss_dump_qmi_history(void)
{
	int i;

	pr_info("qmi_history_index [%d]\n", ((qmi_history_index - 1) &
		(QMI_HISTORY_SIZE - 1)));
	for (i = 0; i < QMI_HISTORY_SIZE; i++) {
		if (qmi_log[i].msg_id)
			pr_info(
			"qmi_history[%d]:timestamp[%llu] msg_id[%X] err[%X]\n",
			i, qmi_log[i].timestamp,
			qmi_log[i].msg_id,
			qmi_log[i].error_msg);
	}
}
EXPORT_SYMBOL(cnss_dump_qmi_history);

void qmi_record(u16 msg_id, u8 error_msg)
{
	spin_lock(&qmi_log_spinlock);
	qmi_log[qmi_history_index].msg_id = msg_id;
	qmi_log[qmi_history_index].error_msg = error_msg;
	qmi_log[qmi_history_index++].timestamp = ktime_to_ms(ktime_get());
	qmi_history_index &= (QMI_HISTORY_SIZE - 1);
	spin_unlock(&qmi_log_spinlock);
}

enum cnss_bdf_type {
	CNSS_BDF_BIN,
	CNSS_BDF_ELF,
};

static char *cnss_qmi_mode_to_str(enum wlfw_driver_mode_enum_v01 mode)
{
	switch (mode) {
	case QMI_WLFW_MISSION_V01:
		return "MISSION";
	case QMI_WLFW_FTM_V01:
		return "FTM";
	case QMI_WLFW_EPPING_V01:
		return "EPPING";
	case QMI_WLFW_WALTEST_V01:
		return "WALTEST";
	case QMI_WLFW_OFF_V01:
		return "OFF";
	case QMI_WLFW_CCPM_V01:
		return "CCPM";
	case QMI_WLFW_QVIT_V01:
		return "QVIT";
	case QMI_WLFW_CALIBRATION_V01:
		return "CALIBRATION";
	case QMI_WLFW_FTM_CALIBRATION_V01:
		return "FTM CALIBRATION";
	default:
		return "UNKNOWN";
	}
};

static int cnss_wlfw_ind_register_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_ind_register_req_msg_v01 req;
	struct wlfw_ind_register_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		    plat_priv->driver_state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.client_id_valid = 1;
	req.client_id = WLFW_CLIENT_ID;

	req.fw_ready_enable_valid = 1;
	req.fw_ready_enable = 1;
	req.request_mem_enable_valid = 1;
	req.request_mem_enable = 1;
	req.fw_mem_ready_enable_valid = 1;
	req.fw_mem_ready_enable = 1;
	req.fw_init_done_enable_valid = 1;
	req.fw_init_done_enable = 1;
	req.cal_done_enable_valid = 1;
	req.cal_done_enable = 1;
	req.qdss_trace_req_mem_enable_valid = 1;
	req.qdss_trace_req_mem_enable = 1;
	req.qdss_trace_save_enable_valid = 0;
	req.qdss_trace_save_enable = 0;
	req.qdss_trace_free_enable_valid = 1;
	req.qdss_trace_free_enable = 1;

	req.pin_connect_result_enable_valid = 0;
	req.pin_connect_result_enable = 0;

	qmi_record(QMI_WLFW_IND_REGISTER_REQ_V01, ret);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_ind_register_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for indication register request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_IND_REGISTER_REQ_V01,
			       WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ind_register_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send indication register request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for indication register request response, err = %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Indication register request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

	qmi_record(QMI_WLFW_IND_REGISTER_REQ_V01, ret);
	return 0;
out:
	qmi_record(QMI_WLFW_IND_REGISTER_REQ_V01, ret);
	CNSS_ASSERT(0);
	return ret;
}

static int cnss_wlfw_host_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_host_cap_req_msg_v01 req;
	struct wlfw_host_cap_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending host capability message, state: 0x%lx\n",
		    plat_priv->driver_state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.num_clients_valid = 1;
	req.num_clients = daemon_support ? 2 : 1;
	cnss_pr_dbg("Number of clients is %d\n", req.num_clients);

	req.mem_cfg_mode = plat_priv->tgt_mem_cfg_mode;
	req.mem_cfg_mode_valid = 1;
	cnss_pr_info("device_id : %lu mem mode : [%d]\n",
		     plat_priv->device_id,
		     plat_priv->tgt_mem_cfg_mode);

	req.bdf_support_valid = 1;
	req.bdf_support = 1;

	req.m3_support_valid = 0;
	req.m3_support = 0;

	req.m3_cache_support_valid = 0;
	req.m3_cache_support = 0;

	req.cal_done_valid = 1;
	req.cal_done = plat_priv->cal_done;
	cnss_pr_dbg("Calibration done is %d\n", plat_priv->cal_done);

	qmi_record(QMI_WLFW_HOST_CAP_REQ_V01, ret);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_host_cap_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_HOST_CAP_REQ_V01,
			       WLFW_HOST_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_host_cap_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for host capability request response, err = %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Host capability request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	qmi_record(QMI_WLFW_HOST_CAP_REQ_V01, ret);

	return 0;
out:
	qmi_record(QMI_WLFW_HOST_CAP_REQ_V01, ret);
	CNSS_ASSERT(0);
	return ret;
}

int cnss_wlfw_respond_mem_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_respond_mem_req_msg_v01 req;
	struct wlfw_respond_mem_resp_msg_v01 resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret = 0, i;

	cnss_pr_dbg("Sending respond memory message, state: 0x%lx\n",
		    plat_priv->driver_state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mem_seg_len = plat_priv->fw_mem_seg_len;

	for (i = 0; i < req.mem_seg_len; i++) {
		if (cold_boot_support && (!fw_mem[i].pa || !fw_mem[i].size)) {
			if (fw_mem[i].type == 0) {
				cnss_pr_err("Invalid memory for FW type, segment = %d\n",
					    i);
				ret = -EINVAL;
				goto out;
			}
			cnss_pr_err("Memory for FW is not available for type: %u\n",
				    fw_mem[i].type);
			ret = -ENOMEM;
			goto out;
		}

		cnss_pr_dbg("Memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
			    fw_mem[i].va, &fw_mem[i].pa,
			    fw_mem[i].size, fw_mem[i].type);

		req.mem_seg[i].addr = fw_mem[i].pa;
		req.mem_seg[i].size = fw_mem[i].size;
		req.mem_seg[i].type = fw_mem[i].type;
	}

	qmi_record(QMI_WLFW_RESPOND_MEM_REQ_V01, ret);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_respond_mem_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_RESPOND_MEM_REQ_V01,
			       WLFW_RESPOND_MEM_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_respond_mem_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for respond memory request response, err = %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Respond memory request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	qmi_record(QMI_WLFW_RESPOND_MEM_REQ_V01, ret);

	return 0;
out:
	qmi_record(QMI_WLFW_RESPOND_MEM_REQ_V01, ret);
	CNSS_ASSERT(0);
	return ret;
}

int cnss_wlfw_tgt_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_cap_req_msg_v01 req;
	struct wlfw_cap_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending target capability message, state: 0x%lx\n",
		    plat_priv->driver_state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_cap_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for target capability request, err: %d\n",
			    ret);
		goto out;
	}

	qmi_record(QMI_WLFW_CAP_RESP_V01, ret);

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_CAP_REQ_V01,
			       WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_cap_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send respond target capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for target capability request response, err = %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Target capability request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

	qmi_record(QMI_WLFW_CAP_RESP_V01, ret);
	if (resp.chip_info_valid)
		plat_priv->chip_info = resp.chip_info;
	if (resp.board_info_valid)
		plat_priv->board_info = resp.board_info;
	else
		plat_priv->board_info.board_id = 0xFF;
	if (resp.soc_info_valid)
		plat_priv->soc_info = resp.soc_info;
	if (resp.fw_version_info_valid)
		plat_priv->fw_version_info = resp.fw_version_info;

	cnss_pr_dbg("Target capability: chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s",
		    plat_priv->chip_info.chip_id,
		    plat_priv->chip_info.chip_family,
		    plat_priv->board_info.board_id, plat_priv->soc_info.soc_id,
		    plat_priv->fw_version_info.fw_version,
		    plat_priv->fw_version_info.fw_build_timestamp);

	return 0;
out:
	qmi_record(QMI_WLFW_CAP_RESP_V01, ret);
	CNSS_ASSERT(0);
	return ret;
}

int cnss_wlfw_load_bdf(struct wlfw_bdf_download_req_msg_v01 *req,
		struct cnss_plat_data *plat_priv, unsigned int remaining,
		uint8_t is_end, uint8_t bdf_type)
{
	int ret;
	char filename[30];
	const struct firmware *fw;
	char *bdf_addr, *folder;
	unsigned int bdf_addr_pa, location[3], board_id;
	int size;
	struct device *dev;

	dev = &plat_priv->plat_dev->dev;
	folder = (plat_priv->device_id == 0xFFFD) ? "IPQ6018/" : "IPQ8074/";

	switch (bdf_type) {
	case BDF_TYPE_GOLDEN:
		if (!of_property_read_u32(dev->of_node, "qcom,board_id",
					  &board_id)) {
			if ((board_id == 0xFF) &&
			    (plat_priv->board_info.board_id == 0xFF)) {
				snprintf(filename, sizeof(filename),
					 "%s" DEFAULT_BDF_FILE_NAME, folder);
			} else if (board_id == 0xFF) {
				snprintf(filename, sizeof(filename),
					 "%s" BDF_FILE_NAME_PREFIX "%02x",
					 folder,
					 plat_priv->board_info.board_id);
			} else {
				if (board_id != plat_priv->board_info.board_id)
					cnss_pr_info(
					"Boardid from dts:%02x,FW:%02x\n",
					board_id,
					plat_priv->board_info.board_id);

				snprintf(filename, sizeof(filename),
					 "%s" BDF_FILE_NAME_PREFIX "%02x",
					 folder, board_id);
			}
		} else {
			cnss_pr_info("No board_id entry in device tree\n");
			if (plat_priv->board_info.board_id == 0xFF)
				snprintf(filename, sizeof(filename),
					 "%s" DEFAULT_BDF_FILE_NAME, folder);
			else
				snprintf(filename, sizeof(filename),
					 "%s" BDF_FILE_NAME_PREFIX "%02x",
					 folder,
					 plat_priv->board_info.board_id);
		}
		break;
	case BDF_TYPE_CALDATA:
		if (bdf_type == BDF_TYPE_CALDATA)
			snprintf(filename, sizeof(filename),
					"%s" DEFAULT_CAL_FILE_NAME, folder);
		break;
	default:
		return -EINVAL;
	}

	ret = request_firmware(&fw, filename, &plat_priv->plat_dev->dev);
	if (ret) {
		cnss_pr_err("Failed to get BDF file %s (%d)", filename, ret);
		return ret;
	}
	size = fw->size;
	if (of_property_read_u32_array(dev->of_node, "qcom,bdf-addr", location,
				       ARRAY_SIZE(location))) {
		pr_err("Error: No bdf_addr in device_tree\n");
		CNSS_ASSERT(0);
		goto out;
	}
	CNSS_ASSERT(plat_priv->tgt_mem_cfg_mode < ARRAY_SIZE(location));
	bdf_addr_pa = location[plat_priv->tgt_mem_cfg_mode];
	bdf_addr = ioremap(bdf_addr_pa, BDF_MAX_SIZE);
	if (!bdf_addr) {
		cnss_pr_err("ERROR. not able to ioremap BDF location\n");
		ret = -EIO;
		goto out;
	}
	if (size != 0 && size <= BDF_MAX_SIZE) {
		if (bdf_type == BDF_TYPE_GOLDEN) {
			cnss_pr_info("BDF location : 0x%x\n", bdf_addr_pa);
			cnss_pr_info("BDF %s size %d\n",
				     filename, (unsigned int)fw->size);
			memcpy(bdf_addr, fw->data, fw->size);
		}
		if (bdf_type == BDF_TYPE_CALDATA) {
			cnss_pr_info("per device BDF location : 0x%x\n",
				     CALDATA_OFFSET(bdf_addr_pa));
			memcpy(CALDATA_OFFSET(bdf_addr), fw->data, fw->size);
			cnss_pr_info("CALDATA %s size %d offset 0x%x\n",
				     filename, (unsigned int)fw->size,
				     CALDATA_OFFSET(0));
		}
		req->total_size_valid = 1;
		req->total_size = size;
		req->data_valid = 0;
		req->end_valid = 1;
		req->end = 1;
		req->data_len = remaining;
		req->bdf_type = 0;
		req->bdf_type_valid = 0;
	} else {
		cnss_pr_info("bdf size %d > segsz %d\n", size, BDF_MAX_SIZE);
		req->data_len = remaining;
		req->end = is_end;
	}
	iounmap(bdf_addr);
out:
	if (fw)
		release_firmware(fw);
	return ret;
}

int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_bdf_download_req_msg_v01 *req;
	struct wlfw_bdf_download_resp_msg_v01 resp;
	struct qmi_txn txn;
	char filename[64];
	const struct firmware *fw_entry;
	const u8 *temp;
	unsigned int remaining;
	int ret = 0;
	int bdf_downloaded = 0;

	cnss_pr_dbg("Sending BDF download message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto out;
	}

	if (0xFF == plat_priv->board_info.board_id)
		snprintf(filename, sizeof(filename),
			 "QCA6290/" DEFAULT_BDF_FILE_NAME);
	else
		snprintf(filename, sizeof(filename),
			 BDF_FILE_NAME_PREFIX "%02d",
			 plat_priv->board_info.board_id);

	if (plat_priv->device_id == QCA8074_DEVICE_ID ||
	    plat_priv->device_id == QCA8074V2_DEVICE_ID ||
	    plat_priv->device_id == QCA6018_DEVICE_ID) {
		temp = filename;
		remaining = MAX_BDF_FILE_NAME;
		if (caldata_support)
			remaining = MAX_BDF_FILE_NAME * 2;
		goto bypass_bdf;
	}

	ret = request_firmware(&fw_entry, filename, &plat_priv->plat_dev->dev);
	if (ret) {
		cnss_pr_err("Failed to load BDF: %s\n", filename);
		if (bdf_bypass) {
			cnss_pr_info("bdf_bypass is enabled, sending dummy BDF\n");
			temp = filename;
			remaining = MAX_BDF_FILE_NAME;
			goto bypass_bdf;
		} else {
			goto err_req_fw;
		}
	}

	temp = fw_entry->data;
	remaining = fw_entry->size;

bypass_bdf:
	cnss_pr_dbg("Downloading BDF: %s, size: %u\n", filename, remaining);

	memset(&resp, 0, sizeof(resp));

	while (remaining) {
		req->valid = 1;
		req->file_id_valid = 1;
		req->file_id = plat_priv->board_info.board_id;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;

		if (remaining > QMI_WLFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}
		if (plat_priv->device_id == QCA8074_DEVICE_ID ||
		    plat_priv->device_id == QCA8074V2_DEVICE_ID ||
		    plat_priv->device_id == QCA6018_DEVICE_ID) {
			if (bdf_downloaded) {
				cnss_wlfw_load_bdf(req, plat_priv,
						MAX_BDF_FILE_NAME,
						1, BDF_TYPE_CALDATA);
				temp = filename;
			} else {
				cnss_wlfw_load_bdf(req, plat_priv,
						MAX_BDF_FILE_NAME,
						0, BDF_TYPE_GOLDEN);
				bdf_downloaded = 1;
			}
		}

		memcpy(req->data, temp, req->data_len);

		qmi_record(QMI_WLFW_BDF_DOWNLOAD_REQ_V01, ret);
		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				   wlfw_bdf_download_resp_msg_v01_ei, &resp);
		if (ret < 0) {
			cnss_pr_err("Failed to initialize txn for BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_send_request
			(&plat_priv->qmi_wlfw, NULL, &txn,
			 QMI_WLFW_BDF_DOWNLOAD_REQ_V01,
			 WLFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_bdf_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Failed to send respond BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
		if (ret < 0) {
			cnss_pr_err("Failed to wait for BDF download request response, err = %d\n",
				    ret);
			goto err_send;
		}

		cnss_pr_err("BDF download response , result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("BDF download request failed, result: %d, err: 0x%X\n",
				    resp.resp.result, resp.resp.error);
			ret = resp.resp.result;
			goto err_send;
		}
		qmi_record(QMI_WLFW_BDF_DOWNLOAD_REQ_V01, ret);

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

err_send:
	if (!bdf_bypass)
		release_firmware(fw_entry);
err_req_fw:
	kfree(req);
out:
	qmi_record(QMI_WLFW_BDF_DOWNLOAD_REQ_V01, ret);
	if (ret < 0)
		CNSS_ASSERT(0);
	else
		ret = 0;
	return ret;
}

int cnss_wlfw_m3_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_m3_info_req_msg_v01 req;
	struct wlfw_m3_info_resp_msg_v01 resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;
	int ret = 0;

	cnss_pr_dbg("Sending M3 information message, state: 0x%lx\n",
		    plat_priv->driver_state);

	cnss_pr_dbg("M3 memory, va: 0x%pK, pa: %pa, size: 0x%zx\n",
		    m3_mem->va, &m3_mem->pa, m3_mem->size);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	req.addr = m3_mem->pa;
	req.size = m3_mem->size;

	qmi_record(QMI_WLFW_M3_INFO_REQ_V01, ret);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_m3_info_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_M3_INFO_REQ_V01,
			       WLFW_M3_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_m3_info_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for M3 information request response, err = %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("M3 information request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	qmi_record(QMI_WLFW_M3_INFO_REQ_V01, ret);
	return 0;
out:
	qmi_record(QMI_WLFW_M3_INFO_REQ_V01, ret);
	CNSS_ASSERT(0);
	return ret;
}

int cnss_wlfw_wlan_mode_send_sync(struct cnss_plat_data *plat_priv,
				  enum wlfw_driver_mode_enum_v01 mode)
{
	struct wlfw_wlan_mode_req_msg_v01 req;
	struct wlfw_wlan_mode_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending mode message, mode: %s(%d), state: 0x%lx\n",
		    cnss_qmi_mode_to_str(mode), mode, plat_priv->driver_state);

	if (mode == QMI_WLFW_OFF_V01 &&
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_dbg("Recovery is in progress, ignore mode off request.\n");
		return 0;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mode = mode;
	req.hw_debug_valid = 1;
	req.hw_debug = 0;

	qmi_record(QMI_WLFW_WLAN_MODE_REQ_V01, ret);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wlan_mode_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WLAN_MODE_REQ_V01,
			       WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_mode_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Mode request failed, mode: %s(%d), result: %d, err: 0x%X\n",
			    cnss_qmi_mode_to_str(mode), mode, resp.resp.result,
			    resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	qmi_record(QMI_WLFW_WLAN_MODE_REQ_V01, ret);

	return 0;
out:
	qmi_record(QMI_WLFW_WLAN_MODE_REQ_V01, ret);
	if (mode == QMI_WLFW_OFF_V01) {
		cnss_pr_dbg("WLFW service is disconnected while sending mode off request.\n");
		ret = 0;
	} else {
		CNSS_ASSERT(0);
	}
	return ret;
}

int cnss_wlfw_wlan_cfg_send_sync(struct cnss_plat_data *plat_priv,
				 struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	struct wlfw_wlan_cfg_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending WLAN config message, state: 0x%lx\n",
		    plat_priv->driver_state);

	if (!plat_priv)
		return -ENODEV;

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	memcpy(&req, data, sizeof(req));

	qmi_record(QMI_WLFW_WLAN_CFG_REQ_V01, ret);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wlan_cfg_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WLAN_CFG_REQ_V01,
			       WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_cfg_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WLAN config request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	cnss_pr_dbg("Config Message response received\n");
	qmi_record(QMI_WLFW_WLAN_CFG_REQ_V01, ret);

	return 0;
out:
	qmi_record(QMI_WLFW_WLAN_CFG_REQ_V01, ret);
	CNSS_ASSERT(0);
	return ret;
}

int cnss_wlfw_athdiag_read_send_sync(struct cnss_plat_data *plat_priv,
				     u32 offset, u32 mem_type,
				     u32 data_len, u8 *data)
{
	struct wlfw_athdiag_read_req_msg_v01 req;
	struct wlfw_athdiag_read_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("athdiag read: state 0x%lx, offset %x, mem_type %x, data_len %u\n",
		    plat_priv->driver_state, offset, mem_type, data_len);

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	memset(&req, 0, sizeof(req));

	req.offset = offset;
	req.mem_type = mem_type;
	req.data_len = data_len;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_athdiag_read_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ATHDIAG_READ_REQ_V01,
			       WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_read_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of athdiag read request, err: %d\n",
			    ret);
		goto out;
	}
	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("athdiag read request failed, result: %d, err: 0x%X\n",
			    resp->resp.result, resp->resp.error);
		ret = resp->resp.result;
		goto out;
	}

	if (!resp->data_valid || resp->data_len != data_len) {
		cnss_pr_err("athdiag read data is invalid, data_valid = %u, data_len = %u\n",
			    resp->data_valid, resp->data_len);
		ret = -EINVAL;
		goto out;
	}

	memcpy(data, resp->data, resp->data_len);

out:
	kfree(resp);
	return ret;
}

int cnss_wlfw_athdiag_write_send_sync(struct cnss_plat_data *plat_priv,
				      u32 offset, u32 mem_type,
				      u32 data_len, u8 *data)
{
	struct wlfw_athdiag_write_req_msg_v01 *req;
	struct wlfw_athdiag_write_resp_msg_v01 resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("athdiag write: state 0x%lx, offset %x, mem_type %x, data_len %u, data %p\n",
		    plat_priv->driver_state, offset, mem_type, data_len, data);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	memset(&resp, 0, sizeof(resp));

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;
	memcpy(req->data, data, data_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_athdiag_write_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ATHDIAG_WRITE_REQ_V01,
			       WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_write_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of athdiag write request, err: %d\n",
			    ret);
		goto out;
	}
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("athdiag write request failed, result: %d, err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

out:
	kfree(req);
	return ret;
}

int cnss_wlfw_ini_send_sync(struct cnss_plat_data *plat_priv,
			    u8 fw_log_mode)
{
	int ret;
	struct wlfw_ini_req_msg_v01 req;
	struct wlfw_ini_resp_msg_v01 resp;
	struct qmi_txn txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log_mode: %d\n",
		    plat_priv->driver_state, fw_log_mode);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.enablefwlog_valid = 1;
	req.enablefwlog = fw_log_mode;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_ini_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_INI_REQ_V01,
			       WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ini_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("QMI INI request rejected, fw_log_mode:%d result:%d error: 0x%x\n",
			    fw_log_mode, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

	return 0;

out:
	return ret;
}

int cnss_wlfw_qdss_trace_mem_info_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_qdss_trace_mem_info_req_msg_v01 *req;
	struct wlfw_qdss_trace_mem_info_resp_msg_v01 resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int ret = 0;
	int i;

	cnss_pr_dbg("Sending QDSS trace mem info, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	memset(&resp, 0, sizeof(resp));

	req->mem_seg_len = plat_priv->qdss_mem_seg_len;
	for (i = 0; i < req->mem_seg_len; i++) {
		cnss_pr_dbg("Memory for FW, pa: 0x%x, size: 0x%x, type: %u\n",
			    (unsigned int)qdss_mem[i].pa,
			    (unsigned int)qdss_mem[i].size,
			    qdss_mem[i].type);

		req->mem_seg[i].addr = qdss_mem[i].pa;
		req->mem_seg[i].size = qdss_mem[i].size;
		req->mem_seg[i].type = qdss_mem[i].type;
	}

	qmi_record(QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01, ret);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_qdss_trace_mem_info_resp_msg_v01_ei, &resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for QDSS trace mem request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01,
			       WLFW_QDSS_TRACE_MEM_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_qdss_trace_mem_info_req_msg_v01_ei, &req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send QDSS trace mem info request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Fail to wait for response of QDSS trace mem info request, err %d\n",
			    ret);
		goto out;
	}
	cnss_pr_dbg("QDSS trace mem info response , result: %d, err: 0x%X\n",
		    resp.resp.result, resp.resp.error);
	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("QDSS trace meminfo req failed,res: %d,err: 0x%X\n",
			    resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	qmi_record(QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01, ret);

	kfree(req);
	return 0;

out:
	qmi_record(QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01, ret);
	kfree(req);
	return ret;
}

unsigned int cnss_get_qmi_timeout(void)
{
	return QMI_WLFW_TIMEOUT_MS;
}
EXPORT_SYMBOL(cnss_get_qmi_timeout);

static void cnss_wlfw_request_mem_ind_cb(struct qmi_handle *qmi_wlfw,
					 struct sockaddr_qrtr *sq,
					 struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_request_mem_ind_msg_v01 *ind_msg = data;
	int i;

	cnss_pr_dbg("Received QMI WLFW request memory indication\n");

	qmi_record(QMI_WLFW_REQUEST_MEM_IND_V01, 0);
	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}
	if (ind_msg->mem_seg_len == 0 ||
	    ind_msg->mem_seg_len > QMI_WLFW_MAX_NUM_MEM_SEG_V01)
		cnss_pr_err("Invalid memory segment length: %u\n",
			    ind_msg->mem_seg_len);

	cnss_pr_dbg("FW memory segment count is %u\n", ind_msg->mem_seg_len);
	plat_priv->fw_mem_seg_len = ind_msg->mem_seg_len;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		cnss_pr_dbg("FW requests for memory, size: 0x%x, type: %u\n",
			    ind_msg->mem_seg[i].size, ind_msg->mem_seg[i].type);
		plat_priv->fw_mem[i].type = ind_msg->mem_seg[i].type;
		plat_priv->fw_mem[i].size = ind_msg->mem_seg[i].size;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_REQUEST_MEM,
			       0, NULL);
}

static void cnss_wlfw_fw_mem_ready_ind_cb(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW memory ready indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}
	qmi_record(QMI_WLFW_FW_MEM_READY_IND_V01, 0);
	/* WAR Conditional check of driver state to hinder processing */
	/* FW_mem_ready msg i.e. received twice from QMI stack occasionally */
	if (!test_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state))
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_FW_MEM_READY,
				       0, NULL);
	else
		cnss_pr_err("FW_Mem_Ready_Ind received twice\n");
}

static void cnss_wlfw_fw_ready_ind_cb(struct qmi_handle *qmi_wlfw,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW ready indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	qmi_record(QMI_WLFW_FW_READY_IND_V01, 0);
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE,
			       0, NULL);
}

static void cnss_wlfw_fw_init_done_ind_cb(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW initialization done indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}
	qmi_record(QMI_WLFW_FW_INIT_DONE_IND_V01, 0);
	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_FW_READY,
			       0, NULL);
}

static void cnss_wlfw_pin_result_ind_cb(struct qmi_handle *qmi_wlfw,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_pin_connect_result_ind_msg_v01 *ind_msg = data;

	cnss_pr_dbg("Received QMI WLFW pin connect result indication\n");

	qmi_record(QMI_WLFW_PIN_CONNECT_RESULT_IND_V01, 0);
	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (ind_msg->pwr_pin_result_valid)
		plat_priv->pin_result.fw_pwr_pin_result =
		    ind_msg->pwr_pin_result;
	if (ind_msg->phy_io_pin_result_valid)
		plat_priv->pin_result.fw_phy_io_pin_result =
		    ind_msg->phy_io_pin_result;
	if (ind_msg->rf_pin_result_valid)
		plat_priv->pin_result.fw_rf_pin_result = ind_msg->rf_pin_result;

	cnss_pr_dbg("Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		    ind_msg->pwr_pin_result, ind_msg->phy_io_pin_result,
		    ind_msg->rf_pin_result);
}

static void cnss_wlfw_qdss_trace_req_mem_ind_cb(struct qmi_handle *qmi_wlfw,
						struct sockaddr_qrtr *sq,
						struct qmi_txn *txn,
						const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_req_mem_ind_msg_v01 *ind_msg = data;
	int i;

	cnss_pr_err("Received QMI WLFW QDSS trace request mem indication\n");
	qmi_record(QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01, 0);

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (plat_priv->qdss_mem_seg_len) {
		cnss_pr_err("Ignore double allocation for QDSS trace, current len %u\n",
			    plat_priv->qdss_mem_seg_len);
		return;
	}

	plat_priv->qdss_mem_seg_len = ind_msg->mem_seg_len;
	if (ind_msg->mem_seg_len > 1) {
		cnss_pr_err("FW requests %d segments, overwriting it with 1",
			    ind_msg->mem_seg_len);
		plat_priv->qdss_mem_seg_len = 1;
	}

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		cnss_pr_err("QDSS requests for memory, size: 0x%x, type: %u\n",
			    ind_msg->mem_seg[i].size, ind_msg->mem_seg[i].type);
		plat_priv->qdss_mem[i].type = ind_msg->mem_seg[i].type;
		plat_priv->qdss_mem[i].size = ind_msg->mem_seg[i].size;
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM,
			       0, NULL);
}

static void cnss_wlfw_qdss_trace_save_ind_cb(struct qmi_handle *qmi_wlfw,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *txn,
					     const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_save_ind_msg_v01 *ind_msg = data;
	struct cnss_qmi_event_qdss_trace_save_data *event_data;
	int i = 0;

	qmi_record(QMI_WLFW_QDSS_TRACE_SAVE_IND_V01, 0);
	cnss_pr_dbg("Received QMI WLFW QDSS trace save indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_pr_dbg("QDSS_trace_save info: source %u, total_size %u, file_name_valid %u, file_name %s\n",
		    ind_msg->source, ind_msg->total_size,
		    ind_msg->file_name_valid, ind_msg->file_name);

	if (ind_msg->source == 1)
		return;

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	if (ind_msg->mem_seg_valid) {
		if (ind_msg->mem_seg_len > QDSS_TRACE_SEG_LEN_MAX) {
			cnss_pr_err("Invalid seg len %u\n",
				    ind_msg->mem_seg_len);
			goto free_event_data;
		}
		cnss_pr_dbg("QDSS_trace_save seg len %u\n",
			    ind_msg->mem_seg_len);
		event_data->mem_seg_len = ind_msg->mem_seg_len;
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			event_data->mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			event_data->mem_seg[i].size = ind_msg->mem_seg[i].size;
			cnss_pr_dbg("seg-%d: addr 0x%llx size 0x%x\n",
				    i, ind_msg->mem_seg[i].addr,
				    ind_msg->mem_seg[i].size);
		}
	}

	event_data->total_size = ind_msg->total_size;

	if (ind_msg->file_name_valid)
		strlcpy(event_data->file_name, ind_msg->file_name,
			QDSS_TRACE_FILE_NAME_MAX + 1);
	else
		strlcpy(event_data->file_name, "qdss_trace",
			QDSS_TRACE_FILE_NAME_MAX + 1);

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE,
			       0, event_data);

	return;

free_event_data:
	kfree(event_data);
}

static void cnss_wlfw_qdss_trace_free_ind_cb(struct qmi_handle *qmi_wlfw,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *txn,
					     const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	qmi_record(QMI_WLFW_QDSS_TRACE_FREE_IND_V01, 0);
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_FREE,
			       0, NULL);
}

static struct qmi_msg_handler qmi_wlfw_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_REQUEST_MEM_IND_V01,
		.ei = wlfw_request_mem_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_request_mem_ind_msg_v01),
		.fn = cnss_wlfw_request_mem_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_MEM_READY_IND_V01,
		.ei = wlfw_fw_mem_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_mem_ready_ind_msg_v01),
		.fn = cnss_wlfw_fw_mem_ready_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_READY_IND_V01,
		.ei = wlfw_fw_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_ready_ind_msg_v01),
		.fn = cnss_wlfw_fw_ready_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_INIT_DONE_IND_V01,
		.ei = wlfw_fw_init_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_init_done_ind_msg_v01),
		.fn = cnss_wlfw_fw_init_done_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01,
		.ei = wlfw_pin_connect_result_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_pin_connect_result_ind_msg_v01),
		.fn = cnss_wlfw_pin_result_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01,
		.ei = wlfw_qdss_trace_req_mem_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_trace_req_mem_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_req_mem_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_SAVE_IND_V01,
		.ei = wlfw_qdss_trace_save_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_trace_save_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_save_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_FREE_IND_V01,
		.ei = wlfw_qdss_trace_free_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_trace_free_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_free_ind_cb,
	},
	{}
};

static int cnss_wlfw_connect_to_server(struct cnss_plat_data *plat_priv,
				       void *data)
{
	struct cnss_qmi_event_server_arrive_data *event_data = data;
	struct qmi_handle *qmi_wlfw = &plat_priv->qmi_wlfw;
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	if (!event_data) {
		cnss_pr_err("%s: event_data is NULL", __func__);
		return -EINVAL;
	}

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = event_data->node;
	sq.sq_port = event_data->port;

	ret = kernel_connect(qmi_wlfw->sock, (struct sockaddr *)&sq,
			     sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Failed to connect to QMI WLFW remote service port\n");
		goto out;
	}

	set_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service connected, state: 0x%lx\n",
		     plat_priv->driver_state);

	kfree(data);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(data);
	return ret;
}

int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv, void *data)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	ret = cnss_wlfw_connect_to_server(plat_priv, data);
	if (ret < 0)
		goto out;

	ret = cnss_wlfw_ind_register_send_sync(plat_priv);
	if (ret < 0) {
		if (ret == -EALREADY)
			ret = 0;
		goto out;
	}

	ret = cnss_wlfw_host_cap_send_sync(plat_priv);
	if (ret < 0)
		goto out;

	return 0;

out:
	return ret;
}

int cnss_wlfw_server_exit(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	clear_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service disconnected, state: 0x%lx\n",
		     plat_priv->driver_state);
	plat_priv->driver_state = 0;
	return 0;
}

static int wlfw_new_server(struct qmi_handle *qmi_wlfw,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_qmi_event_server_arrive_data *event_data;

	cnss_pr_dbg("WLFW server arriving: node %u port %u\n",
		    service->node, service->port);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return -ENOMEM;

	event_data->node = service->node;
	event_data->port = service->port;

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_SERVER_ARRIVE,
			       0, event_data);

	return 0;
}

static void wlfw_del_server(struct qmi_handle *qmi_wlfw,
			    struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("WLFW server exiting\n");

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_SERVER_EXIT,
			       0, NULL);
}

static struct qmi_ops qmi_wlfw_ops = {
	.new_server = wlfw_new_server,
	.del_server = wlfw_del_server,
};

int cnss_qmi_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;

	dev = &plat_priv->plat_dev->dev;

	if (plat_priv->device_id == QCA8074_DEVICE_ID ||
	    plat_priv->device_id == QCA8074V2_DEVICE_ID ||
	    plat_priv->device_id == QCA6018_DEVICE_ID) {
		if (qca8074_fw_mem_mode != 0xFF) {
			plat_priv->tgt_mem_cfg_mode = qca8074_fw_mem_mode;
			pr_info("Using qca8074_fw_mem_mode 0x%x\n",
				qca8074_fw_mem_mode);
		} else if (of_property_read_u32(dev->of_node,
						"qcom,tgt-mem-mode",
						&plat_priv->tgt_mem_cfg_mode)) {
			pr_info("No qca8074_tgt_mem_mode entry in dev-tree.\n");
			plat_priv->tgt_mem_cfg_mode = 0;
		}
	}

	ret = qmi_handle_init(&plat_priv->qmi_wlfw,
			      QMI_WLFW_MAX_RECV_BUF_SIZE,
			      &qmi_wlfw_ops, qmi_wlfw_msg_handlers);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize QMI handle, err: %d\n", ret);
		goto out;
	}

	ret = qmi_add_lookup(&plat_priv->qmi_wlfw, plat_priv->service_id,
			     WLFW_SERVICE_VERS_V01,
			     plat_priv->wlfw_service_instance_id);
	if (ret < 0) {
		cnss_pr_err("Failed to add QMI lookup, err: %d\n", ret);
		return ret;
	}

out:
	return ret;
}

void cnss_qmi_deinit(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->qmi_wlfw);
}
