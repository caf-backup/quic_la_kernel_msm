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
#include <linux/uaccess.h>
#include <mach/board.h>
#ifdef CONFIG_MSM_SMD
#include "smd_private.h"


static struct proc_dir_entry *boot_info_dir;
static struct proc_dir_entry *uboot_dir;
static struct proc_dir_entry *kernel_dir;

int get_partition_idx(const char *part_name, struct sbl_if_dualboot_info_type *sbl_info_t)
{
	int i;

	for (i = 0; i < sbl_info_t->numaltpart; i++) {
		if (strncmp(part_name, sbl_info_t->per_part_entry[i].name,
			     ALT_PART_NAME_LENGTH) == 0)
			return i;
	}
	return -1; /* cannot find partition, should not happen */
}

static int getbinary_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	memcpy(m->buf + m->count, sbl_info_t, sizeof(struct sbl_if_dualboot_info_type ));
	m->count += sizeof(struct sbl_if_dualboot_info_type);

	return 0;
}

static int getbinary_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, getbinary_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations getbinary_proc_fw_upgrade_ops = {
	.open		= getbinary_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t kernel_upgraded_proc_write(struct file *file, const char __user *user, size_t count, loff_t *data)
{
	int i;
	char optstr[64];
	unsigned int upgraded = 0;
	struct sbl_if_dualboot_info_type *sbl_info_t = PDE(file->f_path.dentry->d_inode)->data;

	if (count == 0 || count > sizeof(optstr))
		return -EINVAL;
	if (copy_from_user(optstr, user, count))
		return -EFAULT;
	optstr[count - 1] = '\0';
	upgraded = simple_strtoul(optstr, NULL, 0);
	i = get_partition_idx("rootfs", sbl_info_t);
	if (i >= 0)
		sbl_info_t->per_part_entry[i].upgraded = upgraded;

	return count;
}

static int kernel_upgraded_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	int i;

	i = get_partition_idx("rootfs", sbl_info_t);
	if (i >= 0)
		seq_printf(m, "%x\n", sbl_info_t->per_part_entry[i].upgraded);
	else
		seq_printf(m, "Unknown Partition\n");

	return 0;
}

static int kernel_upgraded_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, kernel_upgraded_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations kernel_upgraded_proc_fw_upgrade_ops = {
	.open		= kernel_upgraded_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= kernel_upgraded_proc_write,
};

static ssize_t kernel_primaryboot_proc_write(struct file *file, const char __user *user, size_t count, loff_t *data)
{
	int i;
	char optstr[64];
	unsigned int primaryboot = 0;
	struct sbl_if_dualboot_info_type *sbl_info_t = PDE(file->f_path.dentry->d_inode)->data;

	if (count == 0 || count > sizeof(optstr))
		return -EINVAL;
	if (copy_from_user(optstr, user, count))
		return -EFAULT;
	optstr[count - 1] = '\0';
	primaryboot = simple_strtoul(optstr, NULL, 0);
	i = get_partition_idx("rootfs", sbl_info_t);
	if (i >= 0)
		sbl_info_t->per_part_entry[i].primaryboot = primaryboot;

	return count;
}

static int kernel_primaryboot_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	int i;

	i = get_partition_idx("rootfs", sbl_info_t);
	if (i >= 0)
		seq_printf(m, "%x\n", sbl_info_t->per_part_entry[i].primaryboot);
	else
		seq_printf(m, "Unknown Partition\n");

	return 0;
}

static int kernel_primaryboot_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, kernel_primaryboot_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations kernel_primaryboot_proc_fw_upgrade_ops = {
	.open		= kernel_primaryboot_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= kernel_primaryboot_proc_write,
};

static int kernel_name_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	int i;

	i = get_partition_idx("rootfs", sbl_info_t);
	if (i >= 0)
		seq_printf(m, "%s\n", sbl_info_t->per_part_entry[i].name);
	else
		seq_printf(m, "Unknown Partition\n");

	return 0;
}

static int kernel_name_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, kernel_name_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations kernel_name_proc_fw_upgrade_ops = {
	.open		= kernel_name_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t uboot_upgraded_proc_write(struct file *file, const char __user *user, size_t count, loff_t *data)
{
	int i;
	char optstr[64];
	unsigned int upgraded = 0;
	struct sbl_if_dualboot_info_type *sbl_info_t = PDE(file->f_path.dentry->d_inode)->data;

	if (count == 0 || count > sizeof(optstr))
		return -EINVAL;
	if (copy_from_user(optstr, user, count))
		return -EFAULT;
	optstr[count - 1] = '\0';
	upgraded = simple_strtoul(optstr, NULL, 0);
	i = get_partition_idx("0:APPSBL", sbl_info_t);
	if (i >= 0)
		sbl_info_t->per_part_entry[i].upgraded = upgraded;

	return count;
}

static int uboot_upgraded_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	int i;

	i = get_partition_idx("0:APPSBL", sbl_info_t);
	if (i >= 0)
		seq_printf(m, "%x\n", sbl_info_t->per_part_entry[i].upgraded);
	else
		seq_printf(m, "Unknown Partition\n");

	return 0;
}

static int uboot_upgraded_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_upgraded_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations uboot_upgraded_proc_fw_upgrade_ops = {
	.open		= uboot_upgraded_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= uboot_upgraded_proc_write,
};

static ssize_t uboot_primaryboot_proc_write(struct file *file, const char __user *user, size_t count, loff_t *data)
{
	int i;
	char optstr[64];
	unsigned int primaryboot = 0;
	struct sbl_if_dualboot_info_type *sbl_info_t = PDE(file->f_path.dentry->d_inode)->data;

	if (count == 0 || count > sizeof(optstr))
		return -EINVAL;
	if (copy_from_user(optstr, user, count))
		return -EFAULT;
	optstr[count - 1] = '\0';
	primaryboot = simple_strtoul(optstr, NULL, 0);
	i = get_partition_idx("0:APPSBL", sbl_info_t);
	if (i >= 0)
		sbl_info_t->per_part_entry[i].primaryboot = primaryboot;

	return count;
}

static int uboot_primaryboot_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	int i;

	i = get_partition_idx("0:APPSBL", sbl_info_t);
	if (i >= 0)
		seq_printf(m, "%x\n", sbl_info_t->per_part_entry[i].primaryboot);
	else
		seq_printf(m, "Unknown Partition\n");

	return 0;
}

static int uboot_primaryboot_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_primaryboot_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations uboot_primaryboot_proc_fw_upgrade_ops = {
	.open		= uboot_primaryboot_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= uboot_primaryboot_proc_write,
};

static int uboot_name_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;
	int i;

	i = get_partition_idx("0:APPSBL", sbl_info_t);
	if (i >= 0)
		seq_printf(m, "%s\n", sbl_info_t->per_part_entry[i].name);
	else
		seq_printf(m, "Unknown Partition\n");

	return 0;
}

static int uboot_name_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_name_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations uboot_name__proc_fw_upgrade_ops = {
	.open		= uboot_name_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int numaltpart_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->numaltpart);

	return 0;
}

static int numaltpart_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, numaltpart_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations numaltpart_proc_fw_upgrade_ops = {
	.open		= numaltpart_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t upg_in_prog_proc_write(struct file *file, const char __user *user, size_t count, loff_t *data)
{
	char optstr[64];
	unsigned int upg_in_prg = 0;
	struct sbl_if_dualboot_info_type *sbl_info_t = PDE(file->f_path.dentry->d_inode)->data;

	if (count == 0 || count > sizeof(optstr))
		return -EINVAL;
	if (copy_from_user(optstr, user, count))
		return -EFAULT;
	optstr[count - 1] = '\0';
	upg_in_prg = simple_strtoul(optstr, NULL, 0);
	sbl_info_t->upgradeinprogress = upg_in_prg;

	return count;
}

static int upg_in_prog_proc_fw_upgrade_show(struct seq_file *m, void *v)
{
	struct sbl_if_dualboot_info_type *sbl_info_t = m->private;

	seq_printf(m, "%x\n", sbl_info_t->upgradeinprogress);

	return 0;
}

static int upg_in_prog_proc_fw_upgrade_open(struct inode *inode, struct file *file)
{
	return single_open(file, upg_in_prog_proc_fw_upgrade_show, PDE(inode)->data);
}

static const struct file_operations upg_in_prog_proc_fw_upgrade_ops = {
	.open		= upg_in_prog_proc_fw_upgrade_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= upg_in_prog_proc_write,
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
	int i;

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


		printk("SMEM boot info %x, %x: %x \n",sbl_info_t->magic,
				sbl_info_t->upgradeinprogress, sbl_info_t->numaltpart);
		proc_create_data("magic", S_IRUGO, boot_info_dir,
				&magic_proc_fw_upgrade_ops, sbl_info_t);
		proc_create_data("upgradeinprogress", S_IRUGO, boot_info_dir,
				&upg_in_prog_proc_fw_upgrade_ops, sbl_info_t);
		proc_create_data("numaltpart", S_IRUGO, boot_info_dir,
				&numaltpart_proc_fw_upgrade_ops, sbl_info_t);

		i = get_partition_idx("0:APPSBL", sbl_info_t);
		if (i >= 0) {
			uboot_dir = proc_mkdir(sbl_info_t->per_part_entry[i].name, boot_info_dir);
			if (uboot_dir != NULL) {
				proc_create_data("primaryboot", S_IRUGO, uboot_dir,
						&uboot_primaryboot_proc_fw_upgrade_ops, sbl_info_t);
				proc_create_data("upgraded", S_IRUGO, uboot_dir,
						&uboot_upgraded_proc_fw_upgrade_ops, sbl_info_t);
			}
		}

		i = get_partition_idx("rootfs", sbl_info_t);
		if (i >= 0) {
			kernel_dir = proc_mkdir(sbl_info_t->per_part_entry[i].name, boot_info_dir);
			if (kernel_dir != NULL) {
				proc_create_data("primaryboot", S_IRUGO, kernel_dir,
						&kernel_primaryboot_proc_fw_upgrade_ops, sbl_info_t);
				proc_create_data("upgraded", S_IRUGO, kernel_dir,
						&kernel_upgraded_proc_fw_upgrade_ops, sbl_info_t);
			}
		}

		proc_create_data("getbinary", S_IRUGO, boot_info_dir,
				&getbinary_proc_fw_upgrade_ops, sbl_info_t);

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
