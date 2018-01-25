/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h> /* this is for DebugFS libraries */
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/scm.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

#define BUF_LEN 0x1000
#define TZ_INFO_GET_DIAG_ID 0x2
#define TZ_KPSS	1

struct tz_log_struct {
	struct dentry *tz_dirret;
	char *ker_buf;
	uint16_t buf_len;
	char *copy_buf;
	uint16_t copy_len;
	bool flag;
};

struct tzbsp_log_pos_t {
	uint16_t wrap;		/* Ring buffer wrap-around ctr */
	uint16_t offset;	/* Ring buffer current position */
};

struct tzbsp_diag_log_t {
	struct tzbsp_log_pos_t log_pos;	/* Ring buffer position mgmt */
	uint8_t log_buf[1];		/* Open ended array to the end
					 * of the 4K IMEM buffer
					 */
};

struct tzbsp_diag_t {
	uint32_t unused[7];	/* Unused variable is to support the
				 * corresponding structure in trustzone
				 */
	uint32_t ring_off;
	uint32_t unused1[514];
	struct tzbsp_diag_log_t log;
};

struct log_read {
	uint32_t log_buf;
	uint32_t buf_size;
};

struct tzbsp_diag_t_kpss {
	uint32_t magic_num;	/* Magic Number */
	uint32_t version;	/* Major.Minor version */
	uint32_t skip1[5];
	uint32_t ring_off;	/* Ring Buffer Offset */
	uint32_t ring_len;	/* Ring Buffer Len */
	uint32_t skip2[369];
	uint8_t ring_buffer[];	/* TZ Ring Buffer */
};

/* Open file operation */
static int tz_log_open(struct inode *inode, struct file *file)
{
	int ret;
	struct log_read rdip;
	struct tzbsp_diag_t *tz_diag;
	struct tzbsp_diag_t_kpss *tz_diag_kpss;
	uint16_t offset;
	uint16_t ring;
	struct tz_log_struct *tz_log;
	char *ker_buf, *tmp_buf;
	uint16_t buf_len;

	tz_log = inode->i_private;
	file->private_data = tz_log;
	ker_buf = tz_log->ker_buf;
	tmp_buf = tz_log->copy_buf;
	buf_len = tz_log->buf_len;
	tz_log->copy_len = 0;

	rdip.buf_size = buf_len;
	rdip.log_buf = dma_map_single(NULL, ker_buf, buf_len, DMA_FROM_DEVICE);
	ret = dma_mapping_error(NULL, rdip.log_buf);
	if (ret != 0) {
		pr_err("DMA Mapping Error : %d\n", ret);
		return -EINVAL;
	}

	/* SCM call to TZ to get the tz log */
	ret = scm_call(SCM_SVC_INFO, TZ_INFO_GET_DIAG_ID,
		&rdip, sizeof(struct log_read),  NULL, 0);
	dma_unmap_single(NULL, rdip.log_buf, buf_len, DMA_FROM_DEVICE);
	if (ret != 0) {
		pr_err("Error in getting tz log : %d\n", ret);
		return -EINVAL;
	}

	if (tz_log->flag != TZ_KPSS) {
		tz_diag = (struct tzbsp_diag_t *)ker_buf;
		offset = tz_diag->log.log_pos.offset;
		ring = tz_diag->ring_off;

		if (tz_diag->log.log_pos.wrap != 0) {
			memcpy(tmp_buf, (ker_buf + offset + ring),
					(buf_len - offset - ring));
			memcpy(tmp_buf + (buf_len - offset - ring),
					(ker_buf + ring), offset);
			tz_log->copy_len = (buf_len - offset - ring) + offset;
		} else {
			memcpy(tmp_buf, (ker_buf + ring), offset);
			tz_log->copy_len = offset;
		}
	} else {
		tz_diag_kpss = (struct tzbsp_diag_t_kpss *)ker_buf;
		ring = tz_diag_kpss->ring_off;
		memcpy(tmp_buf, (ker_buf + ring), (buf_len - ring));
		tz_log->copy_len = buf_len - ring;
	}

	return 0;
}

/* Read file operation */
static ssize_t tz_log_read(struct file *fp, char __user *user_buffer,
		size_t count, loff_t *position)
{
	struct tz_log_struct *tz_log;

	tz_log = fp->private_data;
	return simple_read_from_buffer(user_buffer, count,
			position, tz_log->copy_buf,
			tz_log->copy_len);
}

static const struct file_operations fops_tz_log = {
	.open = tz_log_open,
	.read = tz_log_read,
};

static const struct of_device_id tz_log_match_table[] = {
	{	.compatible = "qca,tz_log",	},
	{	.compatible = "qca,tz_log_ipq806x",
		.data = (void *)TZ_KPSS		},
	{}
};
MODULE_DEVICE_TABLE(of, tz_log_match_table);

static int tz_log_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tz_log_struct *tz_log;
	struct dentry *tz_fileret;
	struct page *page_buf;
	const struct of_device_id *id;

	tz_log = (struct tz_log_struct *) kzalloc(sizeof(struct tz_log_struct),
			GFP_KERNEL);
	if (tz_log == NULL) {
		dev_err(&pdev->dev, "Unable to allocate memory for tz_log\n");
		return -ENOMEM;
	}

	tz_log->buf_len = BUF_LEN;
	id = of_match_device(tz_log_match_table, &pdev->dev);
	if (id)
		tz_log->flag = (bool) id->data;

	page_buf = alloc_pages(GFP_KERNEL,
			get_order(tz_log->buf_len));
	if (page_buf == NULL) {
		dev_err(&pdev->dev, "unable to get data buffer memory\n");
		ret = -ENOMEM;
		goto free_mem;
	}
	tz_log->ker_buf = page_address(page_buf);

	page_buf = alloc_pages(GFP_KERNEL,
			get_order(tz_log->buf_len));
	if (page_buf == NULL) {
		dev_err(&pdev->dev, "unable to get copy buffer memory\n");
		ret = -ENOMEM;
		goto free_mem;
	}
	tz_log->copy_buf = page_address(page_buf);

	tz_log->tz_dirret = debugfs_create_dir("qcom_debug_logs", NULL);
	if (IS_ERR(tz_log->tz_dirret) || !(tz_log->tz_dirret)) {
		dev_err(&pdev->dev, "unable to create debugfs\n");
		ret = -EIO;
		goto free_mem;
	}

	tz_fileret = debugfs_create_file("tz_log", 0444,  tz_log->tz_dirret,
			tz_log, &fops_tz_log);
	if (IS_ERR(tz_fileret) || !tz_fileret) {
		dev_err(&pdev->dev, "unable to create tz_log debugfs\n");
		ret = -EIO;
		goto remove_debugfs;
	}

	platform_set_drvdata(pdev, tz_log);
	return 0;

remove_debugfs:
	debugfs_remove_recursive(tz_log->tz_dirret);
free_mem:
	if (tz_log->copy_buf)
		__free_pages(virt_to_page(tz_log->copy_buf),
				get_order(tz_log->buf_len));
	if (tz_log->ker_buf)
		__free_pages(virt_to_page(tz_log->ker_buf),
				get_order(tz_log->buf_len));
	kfree(tz_log);

	return ret;
}

static int tz_log_remove(struct platform_device *pdev)
{
	struct tz_log_struct *tz_log = platform_get_drvdata(pdev);

	/* removing the directory recursively which
	in turn cleans all the file */
	debugfs_remove_recursive(tz_log->tz_dirret);

	__free_pages(virt_to_page(tz_log->ker_buf),
			get_order(tz_log->buf_len));
	__free_pages(virt_to_page(tz_log->copy_buf),
			get_order(tz_log->buf_len));
	kfree(tz_log);

	return 0;
}

static struct platform_driver tz_log_driver = {
	.probe = tz_log_probe,
	.remove = tz_log_remove,
	.driver = {
		.name           = "tz_log",
		.owner          = THIS_MODULE,
		.of_match_table = tz_log_match_table,
	},
};

module_platform_driver(tz_log_driver);
