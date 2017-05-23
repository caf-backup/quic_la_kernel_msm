/* Copyright (c) 2016 The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/remoteproc.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include "remoteproc_internal.h"
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/qcom_scm.h>
#include <linux/elf.h>
#include <soc/qcom/subsystem_restart.h>

#define WCSS_CRASH_REASON_SMEM 421
#define WCNSS_PAS_ID		6
#define QCOM_MDT_TYPE_MASK      (7 << 24)
#define QCOM_MDT_TYPE_HASH      (2 << 24)
#define STOP_ACK_TIMEOUT_MS 10000

struct q6v5_rtable {
	struct resource_table rtable;
	struct fw_rsc_hdr last_hdr;
};

static struct q6v5_rtable q6v5_rtable = {
	.rtable = {
		.ver = 1,
		.num = 0,
	},
	.last_hdr = {
		.type = RSC_LAST,
	},
};

struct q6v5_rproc_pdata {
	void __iomem *q6_base;
	void __iomem *tcsr_base;
	void __iomem *mpm_base;
	void __iomem *gcc_bcr_base;
	void __iomem *gcc_misc_base;
	struct rproc *rproc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct completion stop_done;
	struct qcom_smem_state *state;
	unsigned stop_bit;
	unsigned shutdown_bit;
	bool running;
	int emulation;
	int secure;
};

static struct q6v5_rproc_pdata *q6v5_rproc_pdata;

#define subsys_to_pdata(d) container_of(d, struct q6v5_rproc_pdata, subsys_desc)

static struct resource_table *q6v5_find_loaded_rsc_table(struct rproc *rproc,
	const struct firmware *fw)
{
	q6v5_rtable.rtable.offset[0] = sizeof(struct resource_table);
	return &(q6v5_rtable.rtable);
}

static struct resource_table *q6v5_find_rsc_table(struct rproc *rproc,
	const struct firmware *fw, int *tablesz)
{
	*tablesz = sizeof(struct q6v5_rtable);

	q6v5_rtable.rtable.offset[0] = sizeof(struct resource_table);
	return &(q6v5_rtable.rtable);
}

static int q6_rproc_stop(struct rproc *rproc)
{
	struct device *dev = rproc->dev.parent;
	struct platform_device *pdev = to_platform_device(dev);
	struct q6v5_rproc_pdata *pdata = platform_get_drvdata(pdev);
	unsigned long val = 0;
	int ret = 0;
	unsigned int nretry = 0;

	pdata->running = false;
	if (pdata->secure) {
		ret = qcom_scm_pas_shutdown(WCNSS_PAS_ID);
		if (ret)
			dev_err(dev, "failted to shutdown %d\n", ret);
		return ret;
	}

	/* Non secure */
#define TCSR_HALTREQ 0x0
#define TCSR_HALTACK 0x4
#define QDSP6SS_INTF_HALTREQ 0x88
#define QDSP6SS_INTF_HALTACK 0x8c
#define SSCAON_CONFIG 0x8
#define GCC_WCSS_BCR 0x0
#define GCC_WCSS_Q6_BCR 0x100
#define GCC_MISC_RESET_ADDR 0x8
#define HALTACK BIT(0)

	/* Assert WCSS/Q6 HALTREQ */
	val = readl(pdata->tcsr_base + TCSR_HALTREQ);
	val |= BIT(0);
	writel(val, pdata->tcsr_base + TCSR_HALTREQ);

	val = readl(pdata->q6_base + QDSP6SS_INTF_HALTREQ);
	val |= BIT(0);
	writel(val, pdata->q6_base + QDSP6SS_INTF_HALTREQ);

	/* Check HALTACK */
	if (pdata->emulation == 1) {
		val = readl(pdata->tcsr_base + TCSR_HALTACK);
		mdelay(100);
		val = readl(pdata->q6_base + QDSP6SS_INTF_HALTACK);
		mdelay(100);
	} else {
		while (1) {
			val = readl(pdata->tcsr_base + TCSR_HALTACK);
			if (val & HALTACK)
				break;
			mdelay(1);
			nretry++;
			if (nretry >= 10) {
				pr_err("Can't bring q6 to halt\n");
				return -EIO;
			}
		}
		nretry = 0;
		while (1) {
			val = readl(pdata->q6_base + QDSP6SS_INTF_HALTACK);
			if (val & HALTACK)
				break;
			mdelay(1);
			nretry++;
			if (nretry >= 10) {
				pr_err("Can't bring q6 to halt\n");
				return -EIO;
			}
		}
	}

	/* Set MPM_SSCAON_CONFIG 1 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val |= BIT(1);
	writel(val, pdata->mpm_base + SSCAON_CONFIG);
	mdelay(100);

	/* Set MPM_SSCAON_CONFIG 0 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val &= ~(BIT(0));
	writel(val, pdata->mpm_base + SSCAON_CONFIG);
	mdelay(100);

	/* Set MPM_SSCAON_CONFIG 1 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val &= ~(BIT(1));
	writel(val, pdata->mpm_base + SSCAON_CONFIG);
	mdelay(100);

	/* Enable Q6/WCSS BLOCK ARES */
	val = readl(pdata->gcc_bcr_base + GCC_WCSS_BCR);
	val |= BIT(0);
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_BCR);

	val = readl(pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);
	val |= BIT(0);
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);

	/* Enable A2AB/ACMT/ECHAB ARES */
	val = readl(pdata->gcc_misc_base + GCC_MISC_RESET_ADDR);
	val |= 0xe;
	writel(val, pdata->gcc_misc_base + GCC_MISC_RESET_ADDR);

	/* De-assert WCSS/Q6 HALTREQ */
	val = readl(pdata->tcsr_base + TCSR_HALTREQ);
	val &= ~(BIT(0));
	writel(val, pdata->tcsr_base + TCSR_HALTREQ);

	val = readl(pdata->q6_base + QDSP6SS_INTF_HALTREQ);
	val &= ~(BIT(0));
	writel(val, pdata->q6_base + QDSP6SS_INTF_HALTREQ);

	/* Release A2AB/ACMT/ECHAB ARES */
	val = readl(pdata->gcc_misc_base + GCC_MISC_RESET_ADDR);
	val &= (~0xf);
	writel(val, pdata->gcc_misc_base + GCC_MISC_RESET_ADDR);

	/* Release Q6/WCSS BLOCK ARES */
	val = readl(pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);
	val &= ~(BIT(0));
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_Q6_BCR);

	val = readl(pdata->gcc_bcr_base + GCC_WCSS_BCR);
	val &= ~(BIT(0));
	writel(val, pdata->gcc_bcr_base + GCC_WCSS_BCR);

	/* Set MPM_SSCAON_CONFIG 0 */
	val = readl(pdata->mpm_base + SSCAON_CONFIG);
	val |= BIT(0);
	writel(val, pdata->mpm_base + SSCAON_CONFIG);
	mdelay(100);

	return ret;
}

static int q6_rproc_start(struct rproc *rproc)
{
	struct device *dev = rproc->dev.parent;
	struct platform_device *pdev = to_platform_device(dev);
	struct q6v5_rproc_pdata *pdata = platform_get_drvdata(pdev);
	int temp = 19;
	unsigned long val = 0;
	unsigned int nretry = 0;
	int ret = 0;

	if (pdata->secure) {
		ret = qcom_scm_pas_auth_and_reset(WCNSS_PAS_ID);
		if (ret) {
			dev_err(dev, "q6-wcss reset failed\n");
			return ret;
		}
		goto skip_reset;
	}

#define QDSP6SS_RST_EVB 0x10
#define QDSP6SS_RESET 0x14
#define QDSP6SS_DBG_CFG 0x18
#define QDSP6SS_GFMUX_CTL 0x20
#define QDSP6SS_XO_CBCR 0x38
#define QDSP6SS_PWR_CTL 0x30
#define QDSP6SS_MEM_PWR_CTL 0xb0
#define QDSP6SS_SLEEP_CBCR 0x3C
#define QDSP6SS_BHS_STATUS 0x78
#define BHS_EN_REST_ACK BIT(0)

	/* Write bootaddr to EVB so that Q6WCSS will jump there after reset */
	writel(rproc->bootaddr >> 4, pdata->q6_base + QDSP6SS_RST_EVB);
	/* Turn on XO clock. It is required for BHS and memory operation */
	writel(0x1, pdata->q6_base + QDSP6SS_XO_CBCR);
	/* Turn on BHS */
	writel(0x1700000, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* Wait till BHS Reset is done */
	if (pdata->emulation == 1) {
		mdelay(100);
	} else {
		while (1) {
			val = readl(pdata->q6_base + QDSP6SS_BHS_STATUS);
			if (val & BHS_EN_REST_ACK)
				break;
			mdelay(1);
			nretry++;
			if (nretry >= 10) {
				pr_err("Can't bring q6 out of reset\n");
				return -EIO;
			}
		}
	}
	/* Put LDO in bypass mode */
	writel(0x3700000, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* De-assert QDSP6 complier memory clamp */
	writel(0x3300000, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* De-assert memory peripheral sleep and L2 memory standby */
	writel(0x33c0000, pdata->q6_base + QDSP6SS_PWR_CTL);

	/* turn on QDSP6 memory foot/head switch one bank at a time */
	while  (temp >= 0) {
		val = readl(pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		val = val | 1 << temp;
		writel(val, pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		val = readl(pdata->q6_base + QDSP6SS_MEM_PWR_CTL);
		udelay(2);
		temp -= 1;
	}
	/* Remove the QDSP6 core memory word line clamp */
	writel(0x31FFFFF, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* Remove QDSP6 I/O clamp */
	writel(0x30FFFFF, pdata->q6_base + QDSP6SS_PWR_CTL);
	/* Bring Q6 out of reset and stop the core */
	writel(0x5, pdata->q6_base + QDSP6SS_RESET);
	mdelay(10);
	/* Retain debugger state during next QDSP6 reset */
	writel(0x0, pdata->q6_base + QDSP6SS_DBG_CFG);
	/* Turn on the QDSP6 core clock */
	writel(0x102, pdata->q6_base + QDSP6SS_GFMUX_CTL);
	/* Enable the core to run */
	writel(0x4, pdata->q6_base + QDSP6SS_RESET);
	/* Enable QDSP6SS Sleep clock */
	writel(0x1, pdata->q6_base + QDSP6SS_SLEEP_CBCR);

skip_reset:
	pdata->running = true;

	return 0;
}

static void *q6_da_to_va(struct rproc *rproc, u64 da, int len)
{
	unsigned long addr = (__force unsigned long)(da & 0xFFFFFFFF);

	return ioremap(addr, len);
}

static struct rproc_ops q6v5_rproc_ops = {
	.start		= q6_rproc_start,
	.da_to_va	= q6_da_to_va,
	.stop		= q6_rproc_stop,
};

static struct rproc_fw_ops q6_fw_ops;

static irqreturn_t wcss_err_fatal_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(dev_id);
	char *msg;
	size_t len;

	if (!pdata->running)
		return IRQ_HANDLED;

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, WCSS_CRASH_REASON_SMEM, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		pr_err("Fatal error received from wcss software!: %s\n", msg);
	else
		pr_err("Fatal error received no message!\n");

	subsys_set_crash_status(pdata->subsys, CRASH_STATUS_ERR_FATAL);
	subsystem_restart_dev(pdata->subsys);
	return IRQ_HANDLED;
}

static irqreturn_t wcss_stop_ack_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(dev_id);

	pr_info("Received stop ack interrupt from wcss\n");
	complete(&pdata->stop_done);
	return IRQ_HANDLED;
}

static irqreturn_t wcss_wdog_bite_intr_handler(int irq, void *dev_id)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(dev_id);
	char *msg;
	size_t len;

	if (!pdata->running) {
		complete(&pdata->stop_done);
		return IRQ_HANDLED;
	}

	msg = qcom_smem_get(QCOM_SMEM_HOST_ANY, WCSS_CRASH_REASON_SMEM, &len);
	if (!IS_ERR(msg) && len > 0 && msg[0])
		pr_err("Watchdog bite received from wcss software!: %s\n", msg);
	else
		pr_err("Watchdog bit received no message!\n");

	subsys_set_crash_status(pdata->subsys, CRASH_STATUS_WDOG_BITE);
	subsystem_restart_dev(pdata->subsys);

	return IRQ_HANDLED;
}


static int start_q6(const struct subsys_desc *subsys)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(subsys);
	struct rproc *rproc = pdata->rproc;
	int ret = 0;

	reinit_completion(&pdata->stop_done);
	pdata->subsys_desc.ramdump_disable = 1;
	ret = rproc_add(rproc);
	if (ret)
		return ret;

	wait_for_completion(&rproc->firmware_loading_complete);

	ret = rproc_boot(rproc);
	if (ret) {
		pr_err("couldn't boot q6v5: %d\n", ret);
	}

	return ret;
}

static int stop_q6(const struct subsys_desc *subsys, bool force_stop)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(subsys);
	struct rproc *rproc = pdata->rproc;
	int ret = 0;

	if (!subsys_get_crash_status(pdata->subsys) && force_stop) {
		qcom_smem_state_update_bits(pdata->state,
			BIT(pdata->stop_bit), BIT(pdata->stop_bit));

		ret = wait_for_completion_timeout(&pdata->stop_done,
			msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));

		if (ret == 0)
			pr_err("Timedout waiting for stop-ack\n");

		qcom_smem_state_update_bits(pdata->state,
			BIT(pdata->stop_bit), 0);
	}

	rproc_shutdown(rproc);
	rproc_del(rproc);

	return 0;
}

static void wcss_panic_handler(const struct subsys_desc *subsys)
{
	struct q6v5_rproc_pdata *pdata = subsys_to_pdata(subsys);

	pdata->running = false;

	if (!subsys_get_crash_status(pdata->subsys)) {
		qcom_smem_state_update_bits(pdata->state,
			BIT(pdata->shutdown_bit), BIT(pdata->shutdown_bit));
		mdelay(STOP_ACK_TIMEOUT_MS);
	}
	return;
}

static int q6v5_load(struct rproc *rproc, const struct firmware *fw)
{
	int ret = 0;
	const char *name = rproc->firmware;
	size_t name_len;
	char *segment_name;
	struct device *dev_rproc = rproc->dev.parent;
	struct elf32_hdr *ehdr;
	int i = 0;
	const u8 *elf_data = fw->data;
	struct elf32_phdr *phdr;
	struct platform_device *pdev = to_platform_device(dev_rproc);
	struct q6v5_rproc_pdata *pdata = platform_get_drvdata(pdev);

	name_len = strlen(name);
	if (name_len <= 4) {
		dev_err(dev_rproc, "Firmware name should be >4bytes (*.mdt)\n");
		return -EINVAL;
	}

	segment_name = kstrdup(name, GFP_KERNEL);
	if (!segment_name)
		return -ENOMEM;

	if (!fw) {
		dev_err(dev_rproc, "failed to load %s\n", name);
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	if (pdata->secure) {
		ret = qcom_scm_pas_init_image(WCNSS_PAS_ID, fw->data, fw->size);
		if (ret) {
			dev_err(dev_rproc, "image authentication failed\n");
			return ret;
		}
	}
	pr_info("Sanity check passed for the image\n");

	/* go through the available ELF segments and load it */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 pa = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		void *ptr;

		if ((phdr->p_type != PT_LOAD) ||
				((phdr->p_flags & QCOM_MDT_TYPE_MASK)
				 == QCOM_MDT_TYPE_HASH) || (!phdr->p_memsz))
			continue;

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rproc, pa, memsz);
		if (!ptr) {
			dev_err(dev_rproc, "bad phdr pa 0x%x mem 0x%x\n", pa,
					memsz);
			ret = -EINVAL;
			return ret;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz) {
			/* The firmware file has <name>.mdt as the ELF header +
			 * hash segment, followed by <name>.b00, <name>.b01, etc
			 * for every ELF segment of the firmware file. The
			 * rproc loads the first <name>.mdt file, and for the
			 * ELF segments that we need to load, we make the
			 * filename as <name>.b"segment_number"
			 */
			sprintf(segment_name + name_len - 3,  "b%02d", i);
			ret = request_firmware(&fw, segment_name, dev_rproc);
			if (ret) {
				dev_err(dev_rproc, "can't to load %s\n",
						segment_name);
				break;
			}
			memcpy(ptr, fw->data, fw->size);
			release_firmware(fw);
		}

		/* for .bss and sections that needs to be memset to zero */
		if (memsz > filesz)
			memset(ptr + filesz, 0, memsz - filesz);

	}
	kfree(segment_name);
	return ret;
}

static int q6_rproc_probe(struct platform_device *pdev)
{
	const char *firmware_name;
	struct rproc *rproc;
	int ret;
	struct resource *resource;
	struct qcom_smem_state *state;
	unsigned stop_bit;

	state = qcom_smem_state_get(&pdev->dev, "stop",
					&stop_bit);
	if (IS_ERR(state)) {
		/* Wait till SMP2P is registered and up */
		return -EPROBE_DEFER;
	}

	ret = dma_set_coherent_mask(&pdev->dev,
			DMA_BIT_MASK(sizeof(dma_addr_t) * 8));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_coherent_mask: %d\n", ret);
		return ret;
	}

	ret = of_property_read_string(pdev->dev.of_node, "firmware",
			&firmware_name);
	if (ret) {
		dev_err(&pdev->dev, "couldn't read firmware name: %d\n", ret);
		return ret;
	}

	rproc = rproc_alloc(&pdev->dev, "q6v5-wcss", &q6v5_rproc_ops,
				firmware_name,
				sizeof(*q6v5_rproc_pdata));
	if (unlikely(!rproc))
		return -ENOMEM;

	q6v5_rproc_pdata = rproc->priv;
	q6v5_rproc_pdata->rproc = rproc;
	q6v5_rproc_pdata->emulation = of_property_read_bool(pdev->dev.of_node,
					"qca,emulation");
	q6v5_rproc_pdata->secure = of_property_read_bool(pdev->dev.of_node,
					"qca,secure");
	rproc->has_iommu = false;

	q6_fw_ops = *(rproc->fw_ops);
	q6_fw_ops.find_rsc_table = q6v5_find_rsc_table;
	q6_fw_ops.find_loaded_rsc_table = q6v5_find_loaded_rsc_table;
	q6_fw_ops.load = q6v5_load;

	if (!q6v5_rproc_pdata->secure) {
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"wcss-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->q6_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->q6_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"tcsr-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->tcsr_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->tcsr_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"mpm-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->mpm_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->mpm_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"gcc-wcss-bcr-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->gcc_bcr_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->gcc_bcr_base)
			goto free_rproc;

		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"gcc-wcss-misc-base");
		if (unlikely(!resource))
			goto free_rproc;

		q6v5_rproc_pdata->gcc_misc_base = ioremap(resource->start,
				resource_size(resource));
		if (!q6v5_rproc_pdata->gcc_misc_base)
			goto free_rproc;

	}

	platform_set_drvdata(pdev, q6v5_rproc_pdata);

	rproc->fw_ops = &q6_fw_ops;

	q6v5_rproc_pdata->state = qcom_smem_state_get(&pdev->dev, "stop",
			&q6v5_rproc_pdata->stop_bit);
	if (IS_ERR(q6v5_rproc_pdata->state)) {
		pr_err("Can't get stop bit status fro SMP2P\n");
		goto free_rproc;
	}

	q6v5_rproc_pdata->state = qcom_smem_state_get(&pdev->dev, "shutdown",
			&q6v5_rproc_pdata->shutdown_bit);
	if (IS_ERR(q6v5_rproc_pdata->state)) {
		pr_err("Can't get shutdown bit status fro SMP2P\n");
		goto free_rproc;
	}

	q6v5_rproc_pdata->subsys_desc.is_not_loadable = 0;
	q6v5_rproc_pdata->subsys_desc.name = "q6v5-wcss";
	q6v5_rproc_pdata->subsys_desc.dev = &pdev->dev;
	q6v5_rproc_pdata->subsys_desc.owner = THIS_MODULE;
	q6v5_rproc_pdata->subsys_desc.shutdown = stop_q6;
	q6v5_rproc_pdata->subsys_desc.powerup = start_q6;
	q6v5_rproc_pdata->subsys_desc.ramdump = NULL;
	q6v5_rproc_pdata->subsys_desc.crash_shutdown = wcss_panic_handler;
	q6v5_rproc_pdata->subsys_desc.err_fatal_handler =
				wcss_err_fatal_intr_handler;
	q6v5_rproc_pdata->subsys_desc.stop_ack_handler =
				wcss_stop_ack_intr_handler;
	q6v5_rproc_pdata->subsys_desc.wdog_bite_handler =
				wcss_wdog_bite_intr_handler;

	q6v5_rproc_pdata->subsys = subsys_register(
					&q6v5_rproc_pdata->subsys_desc);
	if (IS_ERR(q6v5_rproc_pdata->subsys)) {
		ret = PTR_ERR(q6v5_rproc_pdata->subsys);
		goto free_rproc;
	}

	init_completion(&q6v5_rproc_pdata->stop_done);
	q6v5_rproc_pdata->running = false;

	return 0;

free_rproc:
	if (q6v5_rproc_pdata->gcc_misc_base)
		iounmap(q6v5_rproc_pdata->gcc_misc_base);

	if (q6v5_rproc_pdata->gcc_bcr_base)
		iounmap(q6v5_rproc_pdata->gcc_bcr_base);

	if (q6v5_rproc_pdata->mpm_base)
		iounmap(q6v5_rproc_pdata->mpm_base);

	if (q6v5_rproc_pdata->tcsr_base)
		iounmap(q6v5_rproc_pdata->tcsr_base);

	if (q6v5_rproc_pdata->q6_base)
		iounmap(q6v5_rproc_pdata->q6_base);

	rproc_put(rproc);
	return -EIO;
}

static int q6_rproc_remove(struct platform_device *pdev)
{
	struct q6v5_rproc_pdata *pdata;
	struct rproc *rproc;

	pdata = platform_get_drvdata(pdev);
	rproc = q6v5_rproc_pdata->rproc;

	rproc_del(rproc);
	rproc_put(rproc);

	subsys_unregister(pdata->subsys);

	return 0;
}

static const struct of_device_id q6_match_table[] = {
	{ .compatible = "qca,q6v5-wcss-rproc" },
	{}
};

static struct platform_driver q6_rproc_driver = {
	.probe = q6_rproc_probe,
	.remove = q6_rproc_remove,
	.driver = {
		.name = "q6v5-wcss",
		.of_match_table = q6_match_table,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(q6_rproc_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("QCA Remote Processor control driver");
