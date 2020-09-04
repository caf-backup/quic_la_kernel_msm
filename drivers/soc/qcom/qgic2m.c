/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <soc/qcom/qgic2m.h>

#define MAX_QGICM 2

struct qgic2_msi *qgic2_msi_drv[MAX_QGICM];

/* get_qgic2_struct() - to pass the qgic2_msi struct that
 * contains the msi_desc.
 * gicm_id - 1, set 1 MSI vectors(APCS_ALIAS0_1_QGIC2M_NS)
 * 	   - 2, set 2 MSI vecotrs(APCS_ALIAS0_2_QGIC2M_NS)
 * each set will have 0 to 31 irq vectors.
 */
struct qgic2_msi *get_qgic2_struct(int gicm_id)
{
	return qgic2_msi_drv[gicm_id-1];
}
EXPORT_SYMBOL(get_qgic2_struct);

static int create_qgic2_msi_irq(struct qgic2_msi *qgic)
{
	int irq, pos;

again:
	pos = find_first_zero_bit(qgic->msi_irq_in_use, MAX_MSI_IRQS);
	if (pos >= MAX_MSI_IRQS)
		return -ENOSPC;

	if (test_and_set_bit(pos, qgic->msi_irq_in_use))
		goto again;

	if (pos >= MAX_MSI_IRQS) {
		pr_err("QGIC2-MSI: pos %d is not less than %d\n",
			pos, MAX_MSI_IRQS);
		return ENOSPC;
	}

	irq = qgic->msi[pos];
	if (!irq) {
		pr_err("QGIC2-MSI: failed to create QGIC MSI IRQ.\n");
		return -EINVAL;
	}
	return irq;
}

static int qgic2_setup_msi_irq(struct qgic2_msi *qgic,
		struct msi_desc *desc, int nvec)
{
	int irq, index, firstirq = 0;
	struct msi_msg msg;

	for (index = 0; index < nvec; index++) {
		irq = create_qgic2_msi_irq(qgic);

		if (irq < 0)
			return irq;

		if (index == 0)
			firstirq = irq;

		irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
	}

	/* write msi vector and data */
	irq_set_msi_desc(firstirq, desc);

	msg.address_hi = 0;
	msg.address_lo = qgic->msi_gicm_addr;
	msg.data = qgic->msi_gicm_base + (firstirq - qgic->msi[0]);
	desc->msg = msg;

	return 0;
}

static int msi_setup_irq(struct qgic2_msi_controller *chip, struct qgic2_msi *qgic)
{
	return qgic2_setup_msi_irq(qgic, qgic->desc, 1);
}

int qgic2_setup_msi_irqs(struct qgic2_msi *qgic, int nvec)
{
	struct device *dev = qgic->dev;
	struct msi_desc *entry;
	int ret = 0;

	list_for_each_entry(entry, &dev->msi_list, list) {
		entry->msi_attrib.multiple = order_base_2(nvec);
		ret = qgic2_setup_msi_irq(qgic, entry, nvec);
	}

	return ret;
}

static int msi_setup_irqs(struct qgic2_msi_controller *chip, struct qgic2_msi *qgic, int nvec)
{
	return qgic2_setup_msi_irqs(qgic, nvec);
}

void destroy_qgic2_msi_irq(unsigned int irq, struct qgic2_msi *qgic)
{
	int pos;

	pos = irq - qgic->msi[0];
	clear_bit(pos, qgic->msi_irq_in_use);
}

static void msi_teardown_irq(struct qgic2_msi *qgic, unsigned int irq)
{
	struct irq_data *data = irq_get_irq_data(irq);

	if (data)
		destroy_qgic2_msi_irq(irq, qgic);
}

static void msi_teardown_irqs(struct qgic2_msi *qgic)
{
	struct device *dev = qgic->dev;
	struct msi_desc *entry;

	list_for_each_entry(entry, &dev->msi_list, list) {

		int i, nvec;
		if (entry->irq == 0)
			continue;

		nvec = 1 << entry->msi_attrib.multiple;
		for (i = 0; i < nvec; i++)
			destroy_qgic2_msi_irq(entry->irq + i, qgic);
	}
}

static struct qgic2_msi_controller qgic2_msi_chip = {
	.setup_irq = msi_setup_irq,
	.setup_irqs = msi_setup_irqs,
	.teardown_irq = msi_teardown_irq,
	.teardown_irqs = msi_teardown_irqs,
};

static int qti_qgic2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct qgic2_msi *qgic;
	static int gicm_idx = 0;
	char irq_name[20];
	int i;

	qgic = devm_kzalloc(dev, sizeof(*qgic), GFP_KERNEL);
	if (!qgic)
		return -ENOMEM;

	qgic->dev = dev;

	qgic->msi_gicm_addr = 0;
	qgic->msi_gicm_base = 0;
	of_property_read_u32(np, "qti,msi-gicm-addr", &qgic->msi_gicm_addr);
	of_property_read_u32(np, "qti,msi-gicm-base", &qgic->msi_gicm_base);

	for (i = 0; i < MAX_MSI_IRQS; i++) {
		snprintf(irq_name, sizeof(irq_name), "msi_%d", i);
		qgic->msi[i] = platform_get_irq_byname(pdev, irq_name);
		if (qgic->msi[i] < 0)
			break;
	}

	qgic2_msi_chip.dev = qgic->dev;
	qgic->chip = &qgic2_msi_chip;

	platform_set_drvdata(pdev, qgic);

	qgic2_msi_drv[gicm_idx++] = qgic;

	return 0;
}

static int qti_qgic2_remove(struct platform_device *pdev)
{
	struct qgic2_msi *qgic = platform_get_drvdata(pdev);
	struct qgic2_msi_controller *chip = qgic->chip;

	chip->teardown_irqs(qgic);
	qgic = NULL;

	return 0;
}

static const struct of_device_id qti_qgic2_of_match[] = {
	{ .compatible = "qti,qgic2m-msi" },
	{}
};
MODULE_DEVICE_TABLE(of, qti_qgic2_of_match);

static struct platform_driver qti_qgic2_driver = {
	.probe = qti_qgic2_probe,
	.remove = qti_qgic2_remove,
	.driver  = {
		.name  = "qti_qgic2_msi",
		.of_match_table = qti_qgic2_of_match,
	},
};

static int __init qgic2_msi_init(void)
{
	int ret;
	ret = platform_driver_register(&qti_qgic2_driver);
	if(ret == 0) {
		printk(KERN_ALERT "Registered Sucessfully \n");
	} else {
		printk(KERN_ALERT "Registered Failed \n");
	}

	return 0;
}
module_init(qgic2_msi_init);

static void __exit qgic2_msi_exit(void)
{
	platform_driver_unregister(&qti_qgic2_driver);
	return;
}
module_exit(qgic2_msi_exit);

MODULE_DESCRIPTION("QTI Global Interrupt Controller Version 2");
MODULE_LICENSE("GPL v2");
