/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/seq_file.h>

#include <asm/setup.h>

#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h>

#include <mach/msm_iomap.h>

#include <mach/board.h>
#ifdef CONFIG_MSM_SMD
#include "smd_private.h"


static struct proc_dir_entry *boot_info_dir;

static int boot_kernel_success_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->boot_kernel_success);

	return 0;
}

static int boot_kernel_success_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_kernel_success_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations boot_kernel_success_proc_fw_upgrade_ops = {
	.open		= boot_kernel_success_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int update_completed_on_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->update_completed_on);

	return 0;
}

static int update_completed_on_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, update_completed_on_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations update_completed_on_proc_fw_upgrade_ops = {
	.open		= update_completed_on_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int update_started_on_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->update_started_on);

	return 0;
}

static int update_started_on_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, update_started_on_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations update_started_on_proc_fw_upgrade_ops = {
	.open		= update_started_on_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int active_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->active);

	return 0;
}

static int active_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, active_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations active_proc_fw_upgrade_ops = {
	.open		= active_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int magic_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->magic);

	return 0;
}

static int magic_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, magic_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations magic_proc_fw_upgrade_ops = {
	.open		= magic_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_bootconfig_partition(void)
{
	struct sbl_if_dualboot_info_type *sbl_info_t;

	sbl_info_t = (struct sbl_if_dualboot_info_type *)
	    smem_alloc(SMEM_BOOT_DUALPARTINFO,
		       sizeof(struct sbl_if_dualboot_info_type));
	if (!sbl_info_t) {
		 printk(KERN_WARNING "%s: no dual boot info in shared "
			"memory\n", __func__);
	} else {
		 boot_info_dir = proc_mkdir("boot_info", NULL);
		 if (!boot_info_dir)
			return 0;


		 printk("SMEM boot info %x, %x: %x %x %x\n",sbl_info_t->magic,
			   sbl_info_t->active, sbl_info_t->update_started_on,
			   sbl_info_t->update_completed_on, sbl_info_t->boot_kernel_success);
		 proc_create_data("magic", S_IRUGO, boot_info_dir,
			   &magic_proc_fw_upgrade_ops, sbl_info_t);
		 proc_create_data("active", S_IRUGO, boot_info_dir,
			   &active_proc_fw_upgrade_ops, sbl_info_t);
		 proc_create_data("update_started_on", S_IRUGO, boot_info_dir,
			   &update_started_on_proc_fw_upgrade_ops, sbl_info_t);
		 proc_create_data("update_completed_on", S_IRUGO, boot_info_dir,
			   &update_completed_on_proc_fw_upgrade_ops, sbl_info_t);
		 proc_create_data("boot_kernel_success", S_IRUGO, boot_info_dir,
			   &boot_kernel_success_proc_fw_upgrade_ops, sbl_info_t);
	}

	return 0;
}
#else
static int get_bootconfig_partition(void)
{

	printk(KERN_WARNING "%s: no partition table found!", __func__);

	return -ENODEV;
}
#endif

device_initcall(get_bootconfig_partition);
