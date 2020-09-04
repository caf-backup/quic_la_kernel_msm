/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include <linux/msi.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#define to_qgic2_msi(n) container_of(n, struct qgic2_msi, dev)

/*
 * Maximum number of MSI IRQs per qgicm address.
 */
#define MAX_MSI_IRQS                    32

struct qgic2_msi_controller;

struct qgic2_msi {
        struct device           *dev;
        int                     msi[MAX_MSI_IRQS];
        unsigned int            irq;
        u32                     msi_gicm_addr;
        u32                     msi_gicm_base;
        DECLARE_BITMAP(msi_irq_in_use, MAX_MSI_IRQS);
        struct msi_desc         *desc;
	struct qgic2_msi_controller *chip;
};

struct qgic2_msi_controller {
        struct module *owner;
        struct device *dev;
        struct device_node *of_node;
        struct list_head list;

        int (*setup_irq)(struct qgic2_msi_controller *chip, struct qgic2_msi *qgic);
        int (*setup_irqs)(struct qgic2_msi_controller *chip, struct qgic2_msi *qgic, int nvec);
        void (*teardown_irq)(struct qgic2_msi *qgic, unsigned int irq);
        void (*teardown_irqs)(struct qgic2_msi *qgic);
};

struct qgic2_msi *get_qgic2_struct(int gicm_id);
