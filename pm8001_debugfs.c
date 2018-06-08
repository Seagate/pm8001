/*
 * PMC-Sierra SPC 8001 SAS/SATA based host adapters driver
 *
 * Copyright (c) 2011 Xyratex International Inc.,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include "pm8001_sas.h"
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/nmi.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
#ifndef IS_ERR_OR_NULL
#define IS_ERR_OR_NULL(ptr) (!ptr || IS_ERR(ptr))
#endif
#endif

#ifdef CONFIG_SCSI_PM8001_DEBUG_FS

#define SIZEOF_HEX4BYTE_STRING	(sizeof("0xabcdef01\n") - 1)
#define SIZEOF_REGISTER_FORMAT	(sizeof("[0x000] : 0xabcdef01\n") - 1)

/*
 *	debugfs interface
 *
 * To access this interface the user should:
 * # mount -t debugfs none /sys/kernel/debug
 */

static int pm8001_debugfs_enable = 1;
module_param_named(debugfs_enable, pm8001_debugfs_enable, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debugfs_enable, "Enable debugfs sevices");

/* Debug File System Platform Base Class Functions */

struct pm8001_debug {
	struct debugfs_blob_wrapper blob;
	union { /* Mutually Exclusive */
		unsigned long size;
		unsigned long offset;
	} allocation;
	ssize_t (*write)(struct file *file, loff_t pos, size_t nbytes);
	char buffer[0];
};

/*
 *	pm8001_debugfs_lseek - Seek through a debugfs file
 *	@file: The file pointer to attach the feature.
 *	@off: The offset to seek to or the amount to seek by.
 *	@whence: Indicates how to seek.
 *
 *	Description:
 *	This routine is the entry point for the debugfs lseek file operation.
 *	The @whence paramter indicates whether @off is the offset to directly
 *	seek to, or if it is a value to seek forward or reverse by. This
 *	function figures out what the new offset of the debugfs file fill be
 *	and assigns that value to the f_pos field of @file.
 *
 *	Returns:
 *	This function returns the new offset if successful and returns a
 *	negative error if unable to process the seek.
 */
static loff_t
pm8001_debugfs_lseek(struct file *file, loff_t off, int whence)
{
	struct pm8001_debug *debug;
	loff_t pos = -1;

	debug = file->private_data;

	switch (whence) {
	case 0:
		pos = off;
		break;
	case 1:
		pos = file->f_pos + off;
		break;
	case 2:
		pos = debug->blob.size - off;
	}
	return (pos < 0 || pos > debug->blob.size) ?
		-EINVAL :
		(file->f_pos = pos);
}

/*
 *	pm8001_debugfs_read - Read a debugfs file
 *	@file: The file pointer to attach the feature.
 *	@buf: The buffer to copy the data to.
 *	@nbytes: The number of bytes to read.
 *	@ppos: The position in the file to start reading from.
 *
 *	Description:
 *	This routine is the entry point for the debugfs read file operation.
 *	Reading data from the buffer indicated in the private_data field of
 *	@file, starting at @ppos and copy up to @nbytes of data to @buf.
 *
 *	Returns:
 *	This function returns the amount of data that was read (this could be
 *	less than @nbytes if the end of the file was reached) or a negative
 *	error value.
 */
static ssize_t
pm8001_debugfs_read(
	struct file *file,
	char __user *buf,
	size_t nbytes,
	loff_t *ppos)
{
	struct pm8001_debug *debug;

	debug = file->private_data;

	return simple_read_from_buffer(buf, nbytes, ppos, debug->blob.data,
					debug->blob.size);
}

/*
 *	pm8001_debugfs_write - Write a debugfs file
 *	@file: The file pointer to attach the feature.
 *	@buf: The buffer to copy the data to.
 *	@nbytes: The number of bytes to read.
 *	@ppos: The position in the file to start reading from.
 *
 *	Description:
 *	This routine is the entry point for the debugfs read file operation.
 *	Reading data from the buffer indicated in the private_data field of
 *	@file, starting at @ppos and copy up to @nbytes of data to @buf.
 *
 *	Returns:
 *	This function returns the amount of data that was read (this could be
 *	less than @nbytes if the end of the file was reached) or a negative
 *	error value.
 */
static ssize_t
pm8001_debugfs_write(
	struct file *file,
	const char __user *buf,
	size_t nbytes,
	loff_t *ppos)
{
	struct pm8001_debug *debug;
	size_t ret;
	loff_t pos = *ppos;

	if (pos < 0)
		return -EINVAL;
	debug = file->private_data;
	if (pos >= debug->allocation.size || !nbytes)
		return 0;
	if (nbytes > debug->allocation.size - pos)
		nbytes = debug->allocation.size - pos;

	ret = copy_from_user(debug->blob.data + pos, buf, nbytes);
	if (ret == nbytes)
		return -EFAULT;
	nbytes -= ret;
	*ppos = pos + nbytes;

	if (debug->write) /* = NULL CANT HAPPEN, feedback in read */
		nbytes = (*(debug->write))(file, pos, nbytes);

	return nbytes;
}

/*
 *	pm8001_debugfs_release - Release the buffer used to store debugfs file
 *	                         data.
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the feature.
 *
 *	Description:
 *	This routine is the entry point for the debugfs release file operation.
 *	Frees the buffer that was allocated when the debugfs file was opened.
 *
 *	Returns:
 *	zero
 */
static int
pm8001_debugfs_release(struct inode *inode, struct file *file)
{
	struct pm8001_debug *debug;

	debug = file->private_data;

	kfree(debug);
	file->private_data = NULL;

	return 0;
}

struct pm8001_header_operations {
	const char name[15]; /* MAKE SURE THIS IS LARGE ENOUGH */
	const unsigned char type;
#define PM8001_OP_DIR      0
#define PM8001_OP_FILE_RO  1
#define PM8001_OP_FILE_RW  2
#define PM8001_OP_WRAP     3
};

struct pm8001_file_operations {
	const struct pm8001_header_operations header;
	const struct file_operations fop;
};

struct pm8001_dir_operations {
	const struct pm8001_header_operations header;
	const struct pm8001_header_operations *children[];
};

struct pm8001_wrap_operations {
	const struct pm8001_header_operations header;
	struct dentry *(*const function)(
		const char *name,
		struct dentry *root,
		struct pm8001_hba_info *pm8001_ha,
		const char *path);
};

/*
 *	pm8001_debugfs_build_tree - Create a debugfs tree based on the
 *	                            operations instructions.
 *	@op: const struct pm8001_header_operations pointer with tree
 *	     operations instructions.
 *	@root: The dentry pointer to attach the tree to.
 *	@pm8001_ha: Hba reference
 *	@path upstream path
 *
 *	Description:
 *	This routine is the recursive entry point for the debugfs tree creation
 *	instructions.
 *
 *	Returns:
 *	zero on success, negative for error code, as much of the tree is
 *	created as possible.
 */
static int
pm8001_debugfs_build_tree(
	const struct pm8001_header_operations *op,
	struct dentry *root,
	struct pm8001_hba_info *pm8001_ha,
	const char *path)
{
	struct dentry *entry;
	const struct pm8001_file_operations *fop;
	const struct pm8001_dir_operations *dop;
	const struct pm8001_wrap_operations *wop;
	char *new_path;
	int i, rc = -EINVAL;

	switch (op->type) {
	case PM8001_OP_FILE_RO:
	case PM8001_OP_FILE_RW:
		rc = 0;
		fop = (const struct pm8001_file_operations *)op;
		entry = debugfs_create_file(
			fop->header.name,
			((fop->header.type == PM8001_OP_FILE_RO) ?
			 (S_IFREG|S_IRUGO) :
			 (S_IFREG|S_IRUGO|S_IWUSR)),
			root, root, &fop->fop);
		if (IS_ERR_OR_NULL(entry)) {
			pm8001_printk("Cannot create %s/%s\n",
				path, fop->header.name);
			rc = PTR_ERR(entry);
			if (!rc)
				rc = -EBADF;
			break;
		}
		entry->d_fsdata = pm8001_ha;
		break;
	case PM8001_OP_DIR:
		rc = 0;
		dop = (const struct pm8001_dir_operations *)op;
		entry = debugfs_create_dir(dop->header.name, root);
		if (IS_ERR_OR_NULL(entry)) {
			pm8001_printk("Cannot create %s/%s\n",
				path, dop->header.name);
			rc = PTR_ERR(entry);
			if (!rc)
				rc = -ENOTDIR;
			break;
		}
		entry->d_fsdata = pm8001_ha;
		i = strlen(path) + strlen(dop->header.name) + 2;
		new_path = kmalloc(i, GFP_KERNEL);
		if (new_path)
			snprintf(new_path, i, "%s/%s", path, dop->header.name);
		for (i = 0; dop->children[i]; ++i) {
			int ret = pm8001_debugfs_build_tree(dop->children[i],
				entry, pm8001_ha, (new_path ? new_path : path));
			if (ret < 0)
				rc = ret;
		}
		kfree(new_path);
		break;
	case PM8001_OP_WRAP:
		rc = 0;
		wop = (const struct pm8001_wrap_operations *)op;
		entry = (*wop->function)(
			wop->header.name, root, pm8001_ha, path);
		if (IS_ERR_OR_NULL(entry)) {
			pm8001_printk("Cannot create %s/%s\n",
				path, wop->header.name);
			rc = PTR_ERR(entry);
			if (!rc)
				rc = -ENOTDIR;
			break;
		}
		entry->d_fsdata = pm8001_ha;
		break;
	}
	return rc;
}

/* 1.gsm_memory */

/*
 *	pm8001_debugfs_forensic_gsm_memory_read - Read a gms memory from window
 *	@file: The file pointer to attach the feature.
 *	@buf: The buffer to copy the data to.
 *	@nbytes: The number of bytes to read.
 *	@ppos: The position in the file to start reading from.
 *
 *	Description:
 *	This routine is the entry point for the debugfs read file operation.
 *	Reading data as referenced by an hba pointer (abstracted by data) at
 *	a shift offset allocation.offset and makes incremental arrangements to
 *	transfer from the translation window to @file, starting at @ppos and
 *	copy up to @nbytes of data to @buf.
 *
 *	Returns:
 *	This function returns the amount of data that was read (this could be
 *	less than @nbytes if the end of the file was reached) or a negative
 *	error value.
 */
static ssize_t
pm8001_debugfs_forensic_gsm_memory_read(
	struct file *file,
	char __user *buf,
	size_t nbytes,
	loff_t *ppos)
{
	struct pm8001_debug *debug = file->private_data;
	struct pm8001_hba_info *pm8001_ha;
	char * cp;
	ssize_t retval = 0;
	size_t size = debug->blob.size;

	if (size < *ppos)
		return retval;

	size -= *ppos;
	pm8001_ha = debug->blob.data;
	cp = pm8001_ha->io_mem[2].memvirtaddr;

	while (nbytes) {
		unsigned long flags;
		int err;
		ssize_t rc = -EFAULT;
		u32 shift = (debug->allocation.offset + *ppos) & 0xFFFF0000;
		u32 offset = (debug->allocation.offset + *ppos) & 0xFFFF;
		size_t xfer = 0x0010000 - offset;
		const size_t max_xfer = 1024;

		if (xfer > max_xfer)
			xfer = max_xfer;
		if (xfer > nbytes)
			xfer = nbytes;
		if (xfer > size)
			xfer = size;

		if (xfer == 0)
			break;

		while (unlikely(!spin_trylock_irqsave(&pm8001_ha->lock,
							flags))) {
			yield();
			touch_nmi_watchdog();
		}
		err = pm8001_bar4_shift(pm8001_ha, shift);
		
		if (-1 != err) {
			loff_t position = 0;
			rc = simple_read_from_buffer(buf, xfer,
				&position, cp + offset, xfer);
		}

		pm8001_bar4_shift(pm8001_ha, 0);
		spin_unlock_irqrestore(&pm8001_ha->lock, flags);
		/* Be nice */
                yield();

		if (-1 == err)
			break;

		if (rc <= 0) {
			if (retval == 0)
				retval = rc;
			break;
		}

		retval += rc;
		*ppos += rc;
		buf += rc;
		cp += rc;
		nbytes -= rc;
		size -= rc;
	}
	return retval;
}

/*
 *	pm8001_debugfs_forensic_gsm_memory_open - Open the gsm memory
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *	@off: Offset within BAR3
 *	@size: Size of region in BAR3
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	populates the data and returns a pointer to that data in the
 *	private_data field in @file.
 */
static int
pm8001_debugfs_forensic_gsm_memory_open(
	struct inode *inode,
	struct file *file,
	loff_t off,
	size_t size)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	struct pm8001_debug *debug;
	int rc = -ENOMEM;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.offset = off; /* Shift Offset */
	debug->blob.size = size;
	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	debug->blob.data = pm8001_ha; /* HBA */
	debug->write = NULL;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

/*
 *	pm8001_debugfs_forensic_gsm_memory_io_status_open - Open the gsm
 *	                                                    memory io_status
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory io_status
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gsm_memory_io_status_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_gsm_memory_open(inode, file,
							0x640000, 256 * 1024);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_memory_io_status = {
	{
		.name = "0.io_status",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_memory_io_status_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_forensic_gsm_memory_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_memory_rb_storage_open - Open the gsm
 *	                                                     memory rb_storage
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory rb_storage
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gsm_memory_rb_storage_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_gsm_memory_open(inode, file,
							0x680000, 128 * 1024);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_memory_rb_storage = {
	{
		.name = "1.rb_storage",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_memory_rb_storage_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_forensic_gsm_memory_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_memory_rb_pointers_open - Open the gsm
 *	                                                      memory rb_pointers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory rb_pointers
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gsm_memory_rb_pointers_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_gsm_memory_open(inode, file,
							0x6A1000, 4 * 1024);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_memory_rb_pointers = {
	{
		.name = "2.rb_pointers",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_memory_rb_pointers_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_forensic_gsm_memory_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_memory_rb_configure_open - Open the gsm
 *	                                                    memory rb_configure
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory rb_configure
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gsm_memory_rb_configure_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_gsm_memory_open(inode, file,
							0x6A2000, 1 * 1024);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_memory_rb_configure = {
	{
		.name = "3.rb_configure",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_memory_rb_configure_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_forensic_gsm_memory_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_memory_gsm_sm_open - Open the gsm
 *	                                                 memory gsm_sm
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory gsm_sm
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gsm_memory_gsm_sm_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_gsm_memory_open(inode, file,
							0x400000, 1024 * 1024);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_memory_gsm_sm = {
	{
		.name = "4.gsm_sm",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_memory_gsm_sm_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_forensic_gsm_memory_read,
		.release = pm8001_debugfs_release,
	}
};

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op_gsm_memory = {
	{
		.name = "1.gsm_memory",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_gsm_memory_io_status.header,
		&pm8001_debugfs_forensic_op_gsm_memory_rb_storage.header,
		&pm8001_debugfs_forensic_op_gsm_memory_rb_pointers.header,
		&pm8001_debugfs_forensic_op_gsm_memory_rb_configure.header,
		&pm8001_debugfs_forensic_op_gsm_memory_gsm_sm.header,
		NULL
	}
};

/* 2.queue */

/*
 *	atoi - convert a string to a number
 *	@string: the utf8 encoded numerical string
 *
 *	Description:
 *	return the numerical value
 */
static inline unsigned atoi(const char *string)
{
	unsigned val = 0;

	if (sscanf(string, "%u", &val) != 1)
		val = 0;
	return val;
}

/*
 *	pm8001_debugfs_forensic_queue_cipi_open - Open the ci pi header
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the queue headers
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_queue_cipi_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	struct pm8001_debug *debug;
	int ind, num, i, len, rc = -ENOMEM;

	parent = inode->i_private;
#if defined(PM8001_DEBUGFS_DEBUG)
	pm8001_printk("%s->%s->%s\n",
		parent->d_parent->d_parent->d_name.name,
		parent->d_parent->d_name.name, parent->d_name.name);
#endif

	pm8001_ha = parent->d_fsdata;
	/* Calculate index for number of Queues */
	ind = CI;
	if (strncmp(parent->d_name.name, "oq", 2) == 0)
		ind += PI - CI;
	/* Total number of queues */
	num = pm8001_ha->memoryMap.region[ind].num_elements;

	/* Calculate Size of debug buffer */
	len = (num * SIZEOF_REGISTER_FORMAT) + 1;
	debug = kmalloc(sizeof(*debug) + len, GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.size = len;
	debug->blob.data = debug->buffer;
	debug->blob.size = 0;
	debug->write = NULL;

	for (i = 0; i < num; ++i) {
		/* For i'th queue */
		ind = IB + i;
		if (strncmp(parent->d_name.name, "oq", 2) == 0)
			ind += OB - IB;

		debug->blob.size += snprintf(debug->buffer,
		debug->allocation.size,
		"[0x%04x] : 0x%x\n",
		i,
		((strncmp(file->f_dentry->d_name.name, "ci", 2) == 0) ?
		(((OB > IB) ? (ind < OB) : (ind >= IB)) ?
		 *((uint32_t *)(pm8001_ha->memoryMap.region[CI].virt_ptr)) :
		 pm8001_ha->outbnd_q_tbl[0].consumer_idx) :
		(((OB > IB) ? (ind < OB) : (ind >= IB)) ?
		 pm8001_ha->inbnd_q_tbl[0].producer_idx :
		 *((uint32_t *)(pm8001_ha->memoryMap.region[PI].virt_ptr)))));
	}
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

/*
 *	pm8001_debugfs_forensic_dump - convert data to append UTF8 dump
 *	@debug: buffer reference
 *	@type: ascii format
 *	@qp: Binary data to dump (assume already null terminated)
 *	@size: Size of binary data to dump
 *	@off: Address designation to present as header.
 *
 *	Description:
 *	This routine is the entry point for the debugfs HEX conversion.
 *	fills the data.
 */
#define QUEUE_FORMAT    0
#define GSM_FORMAT      1
#define REGISTER_FORMAT 2
#define OP_TRUNCATED_WARN "Warning: debug->blob.size exceeded " \
			"debug->allocation.size. Output might have got truncated\n"

static void pm8001_debugfs_forensic_dump(
	struct pm8001_debug *debug,
	int type,
	void *p,
	size_t size,
	loff_t off)
{
	uint32_t *qp;
	int i;
	const char *prefix = NULL;

	for (qp = p, i = 0; i < (size / sizeof(uint32_t)); ++i) {
		if (debug->blob.size > debug->allocation.size) {
			debug->blob.size = debug->allocation.size;
#if defined(PM8001_DEBUGFS_DEBUG)
			pm8001_printk(OP_TRUNCATED_WARN);
#endif
		}

		if (!prefix)
			switch (type) {
			case QUEUE_FORMAT:
				prefix = "{[";
				/* FALLTHRU */
			case REGISTER_FORMAT:
				debug->blob.size += snprintf(
				  debug->blob.data + debug->blob.size,
				  debug->allocation.size - debug->blob.size,
				  "[0x%03llx] : ", off + (i * sizeof(*qp)));
				break;
			case GSM_FORMAT:
				prefix = "{MEMBASE_III,";
				break;
		}
		if (debug->blob.size > debug->allocation.size) {
			debug->blob.size = debug->allocation.size;
#if defined(PM8001_DEBUGFS_DEBUG)
			pm8001_printk(OP_TRUNCATED_WARN);
#endif
		}
		debug->blob.size += snprintf(
			debug->blob.data + debug->blob.size,
			debug->allocation.size - debug->blob.size,
			prefix ? prefix : "");
		if (prefix)
			prefix = ",";
		if (debug->blob.size > debug->allocation.size) {
			debug->blob.size = debug->allocation.size;
#if defined(PM8001_DEBUGFS_DEBUG)
			pm8001_printk(OP_TRUNCATED_WARN);
#endif
		}
		debug->blob.size += snprintf(
			debug->blob.data + debug->blob.size,
			debug->allocation.size - debug->blob.size,
			((type == REGISTER_FORMAT) ? "0x%08x\n" : "0x%08x"),
			*(qp++));
	}
	if (debug->blob.size > debug->allocation.size) {
		debug->blob.size = debug->allocation.size;
#if defined(PM8001_DEBUGFS_DEBUG)
		pm8001_printk(OP_TRUNCATED_WARN);
#endif
	}

	switch (type) {
	case REGISTER_FORMAT:
		return;

	case QUEUE_FORMAT:
		debug->blob.size += snprintf(
			debug->blob.data + debug->blob.size,
			debug->allocation.size - debug->blob.size,
			"]}\n");
		break;

	case GSM_FORMAT:
		debug->blob.size += snprintf(
			debug->blob.data + debug->blob.size,
			debug->allocation.size - debug->blob.size,
			"}\n");
		break;
	}
	if (debug->blob.size > debug->allocation.size) {
		debug->blob.size = debug->allocation.size;
#if defined(PM8001_DEBUGFS_DEBUG)
		pm8001_printk(OP_TRUNCATED_WARN);
#endif
	}
}

/*
 *	pm8001_debugfs_forensic_queue_open - Open the queue
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the queue
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_queue_open(struct inode *inode, struct file *file)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	struct pm8001_debug *debug;
	unsigned ind, len;
	int rc = -ENOMEM;
	size_t size;
	int cur_iomb, num;

	parent = inode->i_private;
#if defined(PM8001_DEBUGFS_DEBUG)
	pm8001_printk("%s->%s->%s\n",
		parent->d_parent->d_parent->d_name.name,
		parent->d_parent->d_name.name, parent->d_name.name);
#endif
	ind = IB + atoi(parent->d_name.name);
	if (strncmp(parent->d_parent->d_name.name, "oq", 2) == 0)
		ind += OB - IB;
	pm8001_ha = parent->d_fsdata;
	/* Cal. no. of IOMBs in the 'ind'th Queue and Size of this Queue */
	num  = pm8001_ha->memoryMap.region[ind].num_elements;
	size = pm8001_ha->memoryMap.region[ind].element_size;

#if defined(PM8001_DEBUGFS_DEBUG)
	pm8001_printk("pm8001_ha->memoryMap.region[%d].num_elements=%d",
				  ind, num);
	pm8001_printk("pm8001_ha->memoryMap.region[%d].element_size=%zu\n",
				  ind, size);
#endif

	/* Dump format Length for one IOMB in the queue	*/
	len = ((size / sizeof(uint32_t)) * SIZEOF_HEX4BYTE_STRING) +
			(sizeof("[0x0000] : {[") - 1) + (sizeof("}\n") - 1);
	len = (len * num) + 1; /* For 'num' number of IOMBs */
	debug = kmalloc(sizeof(*debug) + len, GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.size = len;
	debug->blob.data = debug->buffer;
	debug->blob.size = 0;
	for (cur_iomb = 0; cur_iomb < num; ++cur_iomb) {
		pm8001_debugfs_forensic_dump(debug,	QUEUE_FORMAT,
			((char *)pm8001_ha->memoryMap.region[ind].virt_ptr) +
				(cur_iomb * size), size, cur_iomb);
	}
	debug->write = NULL;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

static const struct file_operations
pm8001_debugfs_forensic_queue_fop = {
	.owner =   THIS_MODULE,
	.open =	   pm8001_debugfs_forensic_queue_open,
	.llseek =  pm8001_debugfs_lseek,
	.read =	   pm8001_debugfs_read,
	.release = pm8001_debugfs_release,
};

/*
 *	pm8001_debugfs_forensic_queue_create - Create file
 *	@name: Root name (format)
 *	@root: parent directory
 *	@hba: hba reference
 *	@path: Path to this node
 *
 *	Description:
 *	This routine is the entry point for the debugfs create directory
 *	operation.
 */
static struct dentry *pm8001_debugfs_forensic_queue_create(
	const char *name,
	struct dentry *root,
	struct pm8001_hba_info *pm8001_ha,
	const char *path)
{
	struct dentry *entry;

#if defined(PM8001_DEBUGFS_DEBUG)
	pm8001_printk("%s->%s->%s\n",
		root->d_parent->d_parent->d_name.name,
		root->d_parent->d_name.name, root->d_name.name);
#endif

	entry = debugfs_create_file("iomb", (S_IFREG|S_IRUGO),
			root, root,
			&pm8001_debugfs_forensic_queue_fop);
	if (!IS_ERR_OR_NULL(entry))
		entry->d_fsdata = pm8001_ha;

	return entry;
}

static const struct pm8001_wrap_operations
pm8001_debugfs_forensic_op_queue_entry = {
	{
		"%03u",
		.type = PM8001_OP_WRAP
	},
	pm8001_debugfs_forensic_queue_create
};

/*
 *	pm8001_debugfs_forensic_queue_dir - Create each queue
 *	@name: Root name (format)
 *	@root: parent directory
 *	@hba: hba reference
 *	@path: A path to this node
 *
 *	Description:
 *	This routine is the entry point for the debugfs create directory
 *	operation.
 */
static struct dentry *pm8001_debugfs_forensic_queue_dir(
	const char *name,
	struct dentry *root,
	struct pm8001_hba_info *pm8001_ha,
	const char *path)
{
	int i, ind, num;
	struct dentry *entry;

#if defined(PM8001_DEBUGFS_DEBUG)
	pm8001_printk("%s->%s->%s\n",
		root->d_parent->d_name.name, root->d_name.name, name);
#endif
	ind = CI;
	if (strncmp(root->d_name.name, "oq", 2) == 0)
		ind += PI - CI;
	num = pm8001_ha->memoryMap.region[ind].num_elements;
#if defined(PM8001_DEBUGFS_DEBUG)
	pm8001_printk(
		"pm8001_ha->memoryMap.region[%d].num_elements=%d\n",
		ind, num);
#endif
	for (entry = NULL, i = 0; i < num; ++i) {
		char buffer[16];
		int j, ret;
		char *new_path;

		snprintf(buffer, sizeof(buffer), name, i);
		entry = debugfs_create_dir(buffer, root);
		if (IS_ERR_OR_NULL(entry))
			break;
		entry->d_fsdata = pm8001_ha;
		j = strlen(path) + strlen(buffer) + 2;
		new_path = kmalloc(i, GFP_KERNEL);
		if (new_path)
			snprintf(new_path, i, "%s/%s", path, buffer);
		ret = pm8001_debugfs_build_tree(
			&pm8001_debugfs_forensic_op_queue_entry.header, entry,
			pm8001_ha, (new_path ?  new_path : path));
		kfree(new_path);
		if (ret < 0) {
			entry = NULL;
			break;
		}
	}
	return entry;
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_queue_ci = {
	{
		.name = "ci",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_queue_cipi_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};
static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_queue_pi = {
	{
		.name = "pi",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_queue_cipi_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};


static const struct pm8001_wrap_operations
pm8001_debugfs_forensic_op_queue_num = {
	{
		.name = "%02u",
		.type = PM8001_OP_WRAP
	},
	pm8001_debugfs_forensic_queue_dir
};

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op_queue_input = {
	{
		.name = "iq",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_queue_num.header,
		&pm8001_debugfs_forensic_op_queue_ci.header,
		&pm8001_debugfs_forensic_op_queue_pi.header,
		NULL
	}
};

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op_queue_output = {
	{
		.name = "oq",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_queue_num.header,
		&pm8001_debugfs_forensic_op_queue_ci.header,
		&pm8001_debugfs_forensic_op_queue_pi.header,
		NULL
	}
};

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op_queue = {
	{
		.name = "2.queue",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_queue_input.header,
		&pm8001_debugfs_forensic_op_queue_output.header,
		NULL
	}
};

/* 3.gsm */

/*
 *	pm8001_debugfs_forensic_next_list_entry - return the next list entry
 *	@entry: current list entry (first one necessarily -1)
 *
 *	Description:
 *	This routine returns the next list entry. Returns NULL when list is
 *	exhausted.
 */
struct pm8001_debugfs_forensic_mem_list {
	loff_t offset;
	size_t size;
};

static struct pm8001_debugfs_forensic_mem_list *
pm8001_debugfs_forensic_next_list_entry(
	struct pm8001_debugfs_forensic_mem_list *entry)
{
	if (((++entry)->offset == 0) && (entry->size == 0))
		return NULL;
	return entry;
}

/*
 *	pm8001_debugfs_forensic_memory_open - Open the registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the device memory
 *	@function: Function that returns a list entry.
 *	@arg: First argument to list entry function to start recursion.
 *	@bar: Bar to utilize for offset address.
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_memory_open(
	struct inode *inode,
	struct file *file,
	struct pm8001_debugfs_forensic_mem_list *(*function)(
		struct pm8001_debugfs_forensic_mem_list *),
	struct pm8001_debugfs_forensic_mem_list *arg,
	int bar)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	struct pm8001_debug *debug;
	int len, rc = -ENOMEM;
	struct pm8001_debugfs_forensic_mem_list *next;
	unsigned char type;
	unsigned int dwords;

	/* Determine printing size of the list to allocate a buffer */
	for (len = 1, next = arg; next; next = (*function)(next)) {

		if ((next->offset == 0) && (next->size == 0))
			continue;
		dwords = ((next->size + sizeof(uint32_t) - 1) /
				sizeof(uint32_t));

		if (bar == 2)
			len += (SIZEOF_HEX4BYTE_STRING * dwords) +
					(sizeof("{MEMBASE_III,\n") - 1);
		else
			len += (SIZEOF_HEX4BYTE_STRING +
					(sizeof("[0x000] : ") - 1)) * dwords;
	}

	debug = kmalloc(sizeof(*debug) + len, GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.size = len;
	debug->blob.size = 0;
	debug->blob.data = debug->buffer;
	debug->buffer[0] = '\0';
	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	type = REGISTER_FORMAT;
	/* Populate */
	for (next = arg; next; next = (*function)(next)) {
		u32 offset;
		unsigned long flags;
		if ((next->offset == 0) && (next->size == 0))
			continue;
		offset = next->offset;
		if (bar == 2) {
			type = GSM_FORMAT;
			while (unlikely(!spin_trylock_irqsave(&pm8001_ha->lock,
								flags))) {
				yield();
				touch_nmi_watchdog();
			}
			if (-1 == pm8001_bar4_shift(pm8001_ha,
					offset & 0xFFFF0000)) {
				spin_unlock_irqrestore(&pm8001_ha->lock, flags);
				kfree(debug);
				rc = -EINVAL;
				goto out;
			}
			offset &= 0xFFFF;
		}
		pm8001_debugfs_forensic_dump(
			debug,
			type,
			((char *)pm8001_ha->io_mem[bar].memvirtaddr) + offset,
			next->size, next->offset);
		if (bar == 2) {
			pm8001_bar4_shift(pm8001_ha, 0);
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			/* Be nice */
	                yield();
		}
	}
	debug->write = NULL;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

/*
 *	pm8001_debugfs_forensic_gsm_spc_open - Open the gsm spc registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_spc_list[] = {
	{ 0x000000, 256 },
	{ 0x050000, 48 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_spc_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_spc_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_spc = {
	{
		.name = "01.SPC",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_spc_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_bdma_open - Open the gsm bdma registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_bdma_list[] = {
	{ 0x010000, 12 },
	{ 0x010010, 4 },
	{ 0x010020, 100 },
	{ 0x010084, 8 },
	{ 0x010090, 8 },
	{ 0x01009C, 8 },
	{ 0x0100A8, 8 },
	{ 0x0100D0, 8 },
	{ 0x0100F0, 12 },
	{ 0x010100, 12 },
	{ 0x010110, 12 },
	{ 0x010120, 12 },
	{ 0x010130, 12 },
	{ 0x010140, 12 },
	{ 0x010150, 12 },
	{ 0x010160, 12 },
	{ 0x010200, 128 },
	{ 0x010300, 4 },
	{ 0x010340, 4 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_bdma_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_bdma_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_bdma = {
	{
		.name = "02.BDMA",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_bdma_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_app_open - Open the gsm app registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_app_list[] = {
	{ 0x013000, 12 },
	{ 0x013010, 64 },
	{ 0x013060, 12 },
	{ 0x013070, 76 },
	{ 0x013100, 124 },
	{ 0x013200, 124 },
	{ 0x013280, 36 },
	{ 0x013300, 36 },
	{ 0x013328, 120 },
	{ 0x0133C0, 8 },
	{ 0x013400, 256 },
	{ 0x013800, 4 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_app_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_app_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_app = {
	{
		.name = "03.APP",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_app_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_phy_open - Open the gsm phy registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_phy_list[] = {
	{ 0x014000, 28 },
	{ 0x014020, 8 },
	{ 0x01402C, 4 },
	{ 0x014040, 8 },
	{ 0x01405C, 4 },
	{ 0x014064, 12 },
	{ 0x0140A0, 20 },
	{ 0x0140C0, 32 },
	{ 0x014100, 40 },
	{ 0x014130, 16 },
	{ 0x014180, 4 },
	{ 0x0141B0, 16 },
	{ 0x0141D0, 24 },
	{ 0x0141EC, 4 },
	{ 0x0142F8, 48 },
	{ 0x014330, 16 },
	{ 0x014380, 4 },
	{ 0x0143B0, 16 },
	{ 0x0143D0, 24 },
	{ 0x0143EC, 4 },
	{ 0x0143F8, 48 },
	{ 0x014430, 16 },
	{ 0x014480, 4 },
	{ 0x0144B0, 16 },
	{ 0x0144D0, 24 },
	{ 0x0144EC, 4 },
	{ 0x0144F8, 48 },
	{ 0x014530, 16 },
	{ 0x014580, 4 },
	{ 0x0145B0, 16 },
	{ 0x0145D0, 24 },
	{ 0x0145EC, 4 },
	{ 0x0145F8, 48 },
	{ 0x014630, 16 },
	{ 0x014680, 4 },
	{ 0x0146B0, 16 },
	{ 0x0146D0, 24 },
	{ 0x0146EC, 4 },
	{ 0x0146F8, 48 },
	{ 0x014730, 16 },
	{ 0x014780, 4 },
	{ 0x0147B0, 16 },
	{ 0x0147D0, 24 },
	{ 0x0147EC, 4 },
	{ 0x0147F8, 48 },
	{ 0x014830, 16 },
	{ 0x014880, 4 },
	{ 0x0148B0, 16 },
	{ 0x0148D0, 24 },
	{ 0x0148EC, 4 },
	{ 0x0148F8, 84 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_phy_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_phy_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_phy = {
	{
		.name = "04.PHY",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_phy_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_core_open - Open the gsm core registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_core_list[] = {
	{ 0x018000, 76 },
	{ 0x018050, 16 },
	{ 0x018070, 28 },
	{ 0x018094, 36 },
	{ 0x018100, 44 },
	{ 0x019010, 24 },
	{ 0x019030, 4 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_core_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_core_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_core = {
	{
		.name = "05.CORE",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_core_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_ossp_open - Open the gsm ossp registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_ossp_list[] = {
	{ 0x020000, 28 },
	{ 0x020028, 8 },
	{ 0x02003C, 8 },
	{ 0x020050, 8 },
	{ 0x020064, 8 },
	{ 0x020078, 8 },
	{ 0x02008C, 8 },
	{ 0x0200B0, 48 },
	{ 0x0200F0, 16 },
	{ 0x020120, 8 },
	{ 0x020220, 8 },
	{ 0x020320, 8 },
	{ 0x020420, 8 },
	{ 0x020520, 8 },
	{ 0x020620, 8 },
	{ 0x020720, 8 },
	{ 0x020820, 8 },
	{ 0x020908, 4 },
	{ 0x020910, 8 },
	{ 0x020920, 8 },
	{ 0x020930, 16 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_ossp_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_ossp_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_ossp = {
	{
		.name = "06.OSSP",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_ossp_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_sspa_open - Open the gsm sspa registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_sspa_list[] = {
	{ 0x032000, 52 },
	{ 0x032074, 56 },
	{ 0x0320B8, 24 },
	{ 0x0320E0, 4 },
	{ 0x036000, 52 },
	{ 0x036074, 56 },
	{ 0x0360B8, 24 },
	{ 0x0360E0, 4 },
	{ 0x03A000, 52 },
	{ 0x03A074, 56 },
	{ 0x03A0B8, 24 },
	{ 0x03A0E0, 4 },
	{ 0x03E000, 52 },
	{ 0x03E074, 56 },
	{ 0x03E0B8, 24 },
	{ 0x03E0E0, 4 },
	{ 0x042000, 52 },
	{ 0x042074, 56 },
	{ 0x0420B8, 24 },
	{ 0x0420E0, 4 },
	{ 0x046000, 52 },
	{ 0x046074, 56 },
	{ 0x0460B8, 24 },
	{ 0x0460E0, 4 },
	{ 0x04A000, 52 },
	{ 0x04A074, 56 },
	{ 0x04A0B8, 24 },
	{ 0x04A0E0, 4 },
	{ 0x04E000, 52 },
	{ 0x04E074, 56 },
	{ 0x04E0B8, 24 },
	{ 0x04E0E0, 4 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_sspa_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_sspa_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_sspa = {
	{
		.name = "07.SSPA",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_sspa_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_hsst_open - Open the gsm hsst registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
 pm8001_debugfs_forensic_gsm_hsst_list[] = {
	{ 0x021000, 76 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_hsst_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_hsst_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_hsst = {
	{
		.name = "08.HSST",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_hsst_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_lms_dss_open - Open the gsm lms_dss
 *	                                           registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_lms_dss_list[] = {
	{ 0x030000, 108 },
	{ 0x034000, 108 },
	{ 0x038000, 108 },
	{ 0x03C000, 108 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_lms_dss_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_lms_dss_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_lms_dss = {
	{
		.name = "09.LMS_DSS",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_lms_dss_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_sspl_6g_open - Open the gsm sspl_6g
 *	                                           registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_sspl_6g_list[] = {
	{ 0x031000, 92 },
	{ 0x031060, 32 },
	{ 0x0310A0, 12 },
	{ 0x035000, 92 },
	{ 0x035060, 32 },
	{ 0x0350A0, 12 },
	{ 0x039000, 92 },
	{ 0x039060, 32 },
	{ 0x0390A0, 12 },
	{ 0x03D000, 92 },
	{ 0x03D060, 32 },
	{ 0x031DA0, 12 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_sspl_6g_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_sspl_6g_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_sspl_6g = {
	{
		.name = "10.SPL_6G",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_sspl_6g_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_hsst1_open - Open the gsm hsst1 registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_hsst1_list[] = {
	{ 0x033000, 28 },
	{ 0x033020, 88 },
	{ 0x0330B4, 52 },
	{ 0x037000, 28 },
	{ 0x037020, 88 },
	{ 0x0370B4, 52 },
	{ 0x03B000, 28 },
	{ 0x03B020, 88 },
	{ 0x03B0B4, 52 },
	{ 0x03F000, 28 },
	{ 0x03F020, 88 },
	{ 0x03F0B4, 52 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_hsst1_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_hsst1_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_hsst1 = {
	{
		.name = "11.HSST",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_hsst1_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_lms_dss1_open - Open the gsm lms_dss1
 *	                                            registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_lms_dss1_list[] = {
	{ 0x040000, 108 },
	{ 0x044000, 108 },
	{ 0x048000, 108 },
	{ 0x04C000, 108 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_lms_dss1_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_lms_dss1_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_lms_dss1 = {
	{
		.name = "12.LMS_DSS",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_lms_dss1_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_sspl_6g1_open - Open the gsm sspl_6g1
 *	                                            registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_sspl_6g1_list[] = {
	{ 0x041000, 92 },
	{ 0x041060, 32 },
	{ 0x0410A0, 12 },
	{ 0x045000, 92 },
	{ 0x045060, 32 },
	{ 0x0450A0, 12 },
	{ 0x049000, 92 },
	{ 0x049060, 32 },
	{ 0x0490A0, 12 },
	{ 0x04D000, 92 },
	{ 0x04D060, 32 },
	{ 0x041DA0, 12 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_sspl_6g1_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_sspl_6g1_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_sspl_6g1 = {
	{
		.name = "13.SPL_6G",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_sspl_6g1_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_hsst2_open - Open the gsm hsst2 registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_hsst2_list[] = {
	{ 0x043000, 28 },
	{ 0x043020, 88 },
	{ 0x0430B4, 52 },
	{ 0x047000, 28 },
	{ 0x047020, 88 },
	{ 0x0470B4, 52 },
	{ 0x04B000, 28 },
	{ 0x04B020, 88 },
	{ 0x04B0B4, 52 },
	{ 0x04F000, 28 },
	{ 0x04F020, 88 },
	{ 0x04F0B4, 52 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_hsst2_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_hsst2_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_hsst2 = {
	{
		.name = "14.HSST",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_hsst2_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_mbic_iop_open - Open the gsm mbic_iop
 *	                                            registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_mbic_iop_list[] = {
	{ 0x060000, 188 },
	{ 0x0600C0, 4 },
	{ 0x0600C8, 4 },
	{ 0x0600D0, 4 },
	{ 0x0600D8, 4 },
	{ 0x0600E0, 4 },
	{ 0x0600E8, 4 },
	{ 0x0600F0, 4 },
	{ 0x0600F8, 4 },
	{ 0x060100, 44 },
	{ 0x060130, 12 },
	{ 0x060140, 40 },
	{ 0x060180, 32 },
	{ 0x060400, 12 },
	{ 0x060410, 244 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_mbic_iop_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_mbic_iop_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_mbic_iop = {
	{
		.name = "15.MBIC_IOP",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_mbic_iop_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_mbic_aap1_open - Open the gsm mbic_aap1
 *	                                             registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_mbic_aap1_list[] = {
	{ 0x070000, 188 },
	{ 0x0700C0, 4 },
	{ 0x0700C8, 4 },
	{ 0x0700D0, 4 },
	{ 0x0700D8, 4 },
	{ 0x0700E0, 4 },
	{ 0x0700E8, 4 },
	{ 0x0700F0, 4 },
	{ 0x0700F8, 4 },
	{ 0x070100, 44 },
	{ 0x070130, 12 },
	{ 0x070140, 40 },
	{ 0x070180, 32 },
	{ 0x070400, 12 },
	{ 0x070410, 244 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_mbic_aap1_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_mbic_aap1_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_mbic_aap1 = {
	{
		.name = "16.MBIC_AAP1",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_mbic_aap1_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_spbc_open - Open the gsm spbc registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_gsm_spbc_list[] = {
	{ 0x090000, 48 },
	{ 0x09003C, 16 },
	{ 0x09005C, 20 },
	{ 0x09007C, 20 },
	{ 0x09009C, 20 },
	{ 0x090100, 164 },
	{ 0x0901C0, 140 },
	{ 0x090260, 140 },
	{ 0x090360, 12 },
	{ 0x091014, 32 },
	{ 0x091054, 32 },
	{ 0x091094, 20 },
	{ 0x091400, 32 },
	{ 0x091800, 32 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_gsm_spbc_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_gsm_spbc_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_spbc = {
	{
		.name = "17.SPBC",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_spbc_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_gsm_next_gsm - return the next list entry
 *	@entry: current list entry
 *
 *	Description:
 *	This routine returns the next gsm list entry. Returns NULL when list is
 *	exhausted.
 */
static struct pm8001_debugfs_forensic_mem_list *
pm8001_debugfs_forensic_gsm_next_gsm(
	struct pm8001_debugfs_forensic_mem_list *entry)
{
	/* First entry? */
	if ((entry->offset == 0) && (entry->size == 0)) {
		entry->offset = 0x070000;
		entry->size = 4;
		return entry;
	}
	/* Increment to next */
	entry->offset += 8;
	if (entry->offset < 0x072400) {
		/* Deal with holes */
		switch (entry->offset) {
		case 0x070010:
			entry->offset = 0x070020;
			break;
		case 0x070030:
			entry->offset = 0x070070;
			break;
		case 0x0700A0:
			entry->offset = 0x0700A8;
			break;
		case 0x0700B0:
			entry->offset = 0x070100;
			break;
		case 0x070500:
			entry->offset = 0x070800;
			break;
		case 0x070C40:
			entry->offset = 0x071000;
			break;
		case 0x071400:
			entry->offset = 0x071800;
			break;
		case 0x071C00:
			entry->offset = 0x072000;
			break;
		}
		return entry;
	}
	/* Clear to be sure */
	entry->offset = 0;
	entry->size = 0;
	return NULL;
}

/*
 *	pm8001_debugfs_forensic_gsm_gsm_open - Open the gsm gsm registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */

static int
pm8001_debugfs_forensic_gsm_gsm_open(
	struct inode *inode,
	struct file *file)
{
	struct pm8001_debugfs_forensic_mem_list entry[] = {
		{ 0, 0 }
	};

	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_gsm_next_gsm,
		entry, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gsm_gsm = {
	{
		.name = "18.GSM",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gsm_gsm_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op_gsm = {
	{
		.name = "3.gsm",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_gsm_spc.header,
		&pm8001_debugfs_forensic_op_gsm_bdma.header,
		&pm8001_debugfs_forensic_op_gsm_app.header,
		&pm8001_debugfs_forensic_op_gsm_phy.header,
		&pm8001_debugfs_forensic_op_gsm_core.header,
		&pm8001_debugfs_forensic_op_gsm_ossp.header,
		&pm8001_debugfs_forensic_op_gsm_sspa.header,
		&pm8001_debugfs_forensic_op_gsm_hsst.header,
		&pm8001_debugfs_forensic_op_gsm_lms_dss.header,
		&pm8001_debugfs_forensic_op_gsm_sspl_6g.header,
		&pm8001_debugfs_forensic_op_gsm_hsst1.header,
		&pm8001_debugfs_forensic_op_gsm_lms_dss1.header,
		&pm8001_debugfs_forensic_op_gsm_sspl_6g1.header,
		&pm8001_debugfs_forensic_op_gsm_hsst2.header,
		&pm8001_debugfs_forensic_op_gsm_mbic_iop.header,
		&pm8001_debugfs_forensic_op_gsm_mbic_aap1.header,
		&pm8001_debugfs_forensic_op_gsm_spbc.header,
		&pm8001_debugfs_forensic_op_gsm_gsm.header,
		NULL
	}
};

/* 4.msgu */

/*
 *	pm8001_debugfs_forensic_msgu_open - Open the msgu registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_msgu_list[] = {
	{ 0x000004, 4 },
	{ 0x000020, 4 },
	{ 0x000040, 56 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_msgu_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_msgu_list, 0);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_msgu = {
	{
		.name = "4.msgu",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_msgu_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/* 5.eventlog */

/*
 *	pm8001_debugfs_forensic_eventlog_value_open - Open the value handler
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the gsm memory
 *	@write: write handler
 *	@form: Print format
 *	@val: value location
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_eventlog_value_open(
	struct inode *inode,
	struct file *file,
	ssize_t (*write)(struct file * file, loff_t pos, size_t nbytes),
	const char *form,
	int val)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	struct pm8001_debug *debug;
	int len, rc = -ENOMEM;

	len = strlen(form) + 9;
	debug = kmalloc(sizeof(*debug) + len, GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.size = len;
	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	debug->blob.data = debug->buffer;
	debug->blob.size = snprintf(debug->blob.data, len, form, val);
	debug->write = write;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

/**
 * pm8001_writelog - generic routine to write a single event log entry
 * @header: a pointer to the event log base header
 * @entry: a pointer to copy the latest event log into
 * @return: log insertion, zero for success.
 */
static int pm8001_writelog(
	struct eventlog_header *header,
	struct eventlog_entry *entry)
{
	unsigned maximum_index;
	u32 producer_index;

	/* Validity of header */
	if ((header->offset != sizeof(*header)) ||
	    (header->entry_size != sizeof(*entry)) ||
	    ((header->signature != EVENTLOG_HEADER_SIGNATURE_AAP1) &&
	     (header->signature != EVENTLOG_HEADER_SIGNATURE_IOP)))
		return -EINVAL;
	/* Validity of hardware indexii */
	maximum_index = header->size / sizeof(*entry);
	producer_index = header->producer_index;
	if ((maximum_index < producer_index) ||
	    (maximum_index < header->consumer_index))
		return -EINVAL;
	((struct eventlog_entry *)(header + 1))[producer_index] = *entry;
	++producer_index;
	if (producer_index >= maximum_index)
		producer_index = 0;
	header->producer_index = producer_index;
	return 0;
}

/*
 *	pm8001_debugfs_forensic_eventlog_realloc - allocate a replacement memory
 *	                                           for the eventlogs.
 *	@pm8001_ha: The pm8001 hba information structure.
 *	@nbytes: the new size of the regions.
 *
 *	Description:
 *	This routine is the wrapper for pm8001_mem_alloc that reallocates and
 *	preserves the content of the event logs.
 */
static inline int
pm8001_debugfs_forensic_eventlog_realloc(
	struct pm8001_hba_info *pm8001_ha,
	size_t nbytes)
{
	int i, rc = 0;
	struct mpi_mem m_aap1;
	struct mpi_mem m_iop;
	struct mpi_mem s_aap1;
	struct mpi_mem s_iop;
	struct eventlog_header *mh_aap1;
	struct eventlog_header *mh_iop;
	struct eventlog_header *sh_aap1;
	struct eventlog_header *sh_iop;
	struct eventlog_entry entry;
	u32 consumer;

	if ((pm8001_ha->memoryMap.region[AAP1].total_len == nbytes)
	 && (pm8001_ha->memoryMap.region[IOP].total_len == nbytes))
		goto out;
	rc = -EINVAL;
	if (nbytes >= 4294967295)
		/* NOTREACHED */ goto out;
	rc = -ENOMEM;
	memset(&m_aap1, 0, sizeof(m_aap1));
	memset(&m_iop, 0, sizeof(m_iop));
	m_aap1.total_len = m_iop.total_len = nbytes;
	m_aap1.alignment = pm8001_ha->memoryMap.region[AAP1].alignment;
	m_iop.alignment = pm8001_ha->memoryMap.region[IOP].alignment;
	if (pm8001_ha->memoryMap.region[AAP1].num_elements == 1) {
		m_aap1.element_size = nbytes;
		m_aap1.num_elements = 1;
	} else {
		m_aap1.element_size =
			pm8001_ha->memoryMap.region[AAP1].element_size;
		m_aap1.num_elements = m_aap1.total_len / m_aap1.element_size;
	}
	if (pm8001_ha->memoryMap.region[IOP].num_elements == 1) {
		m_iop.element_size = nbytes;
		m_iop.num_elements = 1;
	} else {
		m_iop.element_size =
			pm8001_ha->memoryMap.region[IOP].element_size;
		m_iop.num_elements = m_iop.total_len / m_iop.element_size;
	}
	if (pm8001_mem_alloc(pm8001_ha->pdev,
			&m_aap1.virt_ptr,
			&m_aap1.phys_addr,
			&m_aap1.phys_addr_hi,
			&m_aap1.phys_addr_lo,
			m_aap1.total_len,
			m_aap1.alignment,
			&m_aap1.real_addr,
			&m_aap1.real_len) != 0) {
		PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("AAP1 realloc failed\n"));
		goto out;
	}
	if (pm8001_mem_alloc(pm8001_ha->pdev,
			&m_iop.virt_ptr,
			&m_iop.phys_addr,
			&m_iop.phys_addr_hi,
			&m_iop.phys_addr_lo,
			m_iop.total_len,
			m_iop.alignment,
			&m_iop.real_addr,
			&m_iop.real_len) != 0) {
		PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("IOP realloc failed\n"));
		pci_free_consistent(pm8001_ha->pdev,
			m_aap1.real_len,
			m_aap1.real_addr,
			m_aap1.phys_addr);
		goto out;
	}

	/* Preserve Events */

	/* AAP1 */
	s_aap1 = pm8001_ha->memoryMap.region[AAP1];
	mh_aap1 = m_aap1.virt_ptr;
	sh_aap1 = s_aap1.virt_ptr;
	*mh_aap1 = *sh_aap1;
	mh_aap1->size = m_aap1.total_len - sizeof(*mh_aap1);
	memset(mh_aap1 + 1, 0, mh_aap1->size);
	mh_aap1->consumer_index = 0;
	mh_aap1->producer_index = 0;
	consumer = sh_aap1->consumer_index;
	pm8001_ha->aap1_consumer = 0;
	do {
		if (consumer == pm8001_ha->aap1_consumer)
			pm8001_ha->aap1_consumer = mh_aap1->consumer_index;
	} while (((i = pm8001_readlog(sh_aap1, &entry, &consumer)) >= 0) &&
		!pm8001_writelog(mh_aap1, &entry) &&
		(i == 0));
	pm8001_ha->memoryMap.region[AAP1] = m_aap1;
	pci_free_consistent(pm8001_ha->pdev,
		s_aap1.real_len,
		s_aap1.real_addr,
		s_aap1.phys_addr);

	/* IOP */
	s_iop = pm8001_ha->memoryMap.region[IOP];
	mh_iop = m_iop.virt_ptr;
	sh_iop = s_iop.virt_ptr;
	*mh_iop = *sh_iop;
	mh_iop->size = m_iop.total_len - sizeof(*mh_iop);
	memset(mh_iop + 1, 0, mh_iop->size);
	mh_iop->consumer_index = 0;
	mh_iop->producer_index = 0;
	consumer = sh_iop->consumer_index;
	pm8001_ha->iop_consumer = 0;
	do {
		if (consumer == pm8001_ha->iop_consumer)
			pm8001_ha->iop_consumer = mh_iop->consumer_index;
	} while (((i = pm8001_readlog(sh_iop, &entry, &consumer)) >= 0) &&
		!pm8001_writelog(mh_iop, &entry) &&
		(i == 0));
	pm8001_ha->memoryMap.region[IOP] = m_iop;
	pci_free_consistent(pm8001_ha->pdev,
		s_iop.real_len,
		s_iop.real_addr,
		s_iop.phys_addr);

	rc = 0;
out:
	return rc;
}

/*
 *	pm8001_debugfs_forensic_eventlog_write - value writer
 *	@file: The file pointer attached to the write operation
 *	@pos: first offset written
 *	@nbytes: number of bytes written
 *	@type: 0 -> level, 1 -> size
 *
 *	Description:
 *	This routine is the entry point for the debugfs write file operation.
 *	It takes action based on the data writen.
 */
static ssize_t
pm8001_debugfs_forensic_eventlog_write(
	struct file *file,
	loff_t pos,
	size_t nbytes,
	unsigned type)
{
	unsigned val, original;
	struct pm8001_debug *debug;
	struct pm8001_hba_info *pm8001_ha;
	unsigned char *cp;
	const char *form;

	if (pos != 0)
		return -EINVAL;

	debug = file->private_data;
	debug->buffer[debug->allocation.size - 1] = '\0';
	cp = debug->buffer;
	while (*cp && ((*cp < '0') || ('9' < *cp)))
		++cp;
	if (!*cp)
		return -EINVAL;
	val = atoi(cp);
	if (((type == 0) ? 5 : (8 * 1024 * 1024)) < val)
		return -EINVAL;
	pm8001_ha = file->f_dentry->d_fsdata;
	switch (type) {
	case 0:
		form = "Eventlog Level=%d\n";
		if (val > 5)
			val = 5;
		pm8001_ha->main_cfg_tbl.event_log_option =
		pm8001_ha->main_cfg_tbl.iop_event_log_option = val;
		pm8001_update_main_config_table(pm8001_ha);
		break;
	case 1:
		form = "Eventlog Size=%d\n";
		original = pm8001_ha->main_cfg_tbl.event_log_size;
		if (original < pm8001_ha->main_cfg_tbl.iop_event_log_size)
			original = pm8001_ha->main_cfg_tbl.iop_event_log_size;
		/* Actions to resize */
		if (pm8001_debugfs_forensic_eventlog_realloc(pm8001_ha, val)) {
			val = original;
			break;
		}
		pm8001_ha->main_cfg_tbl.event_log_size =
		pm8001_ha->main_cfg_tbl.iop_event_log_size = val;
		pm8001_ha->main_cfg_tbl.upper_event_log_addr		=
			pm8001_ha->memoryMap.region[AAP1].phys_addr_hi;
		pm8001_ha->main_cfg_tbl.lower_event_log_addr		=
			pm8001_ha->memoryMap.region[AAP1].phys_addr_lo;
		pm8001_ha->main_cfg_tbl.event_log_size			=
			pm8001_ha->memoryMap.region[AAP1].total_len;
		pm8001_ha->main_cfg_tbl.upper_iop_event_log_addr	=
			pm8001_ha->memoryMap.region[IOP].phys_addr_hi;
		pm8001_ha->main_cfg_tbl.lower_iop_event_log_addr	=
			pm8001_ha->memoryMap.region[IOP].phys_addr_lo;
		pm8001_ha->main_cfg_tbl.iop_event_log_size		=
			pm8001_ha->memoryMap.region[IOP].total_len;
		pm8001_update_main_config_table(pm8001_ha);
		break;
	default:
		/* NOTREACHED */
		form = "\n";
	}
	/* Submit update to controller */
	debug->blob.size = snprintf(debug->blob.data, debug->allocation.size,
		form, val);

	return nbytes;
}

/*
 *	pm8001_debugfs_forensic_eventlog_level_write - level writer
 *	@file: The file pointer attached to the write operation
 *	@pos: first offset written
 *	@nbytes: number of bytes written
 *
 *	Description:
 *	This routine is the entry point for the debugfs write file operation.
 *	It takes action based on the data writen.
 */
static ssize_t
pm8001_debugfs_forensic_eventlog_level_write(
	struct file *file,
	loff_t pos,
	size_t nbytes)
{
	return pm8001_debugfs_forensic_eventlog_write(file, pos, nbytes, 0);
}

/*
 *	pm8001_debugfs_forensic_eventlog_level_open - Open the level handler
 *	@inode: The inode pointer
 *	@file: The file pointer to attach event log level variable
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_eventlog_level_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent = inode->i_private;
	struct pm8001_hba_info *pm8001_ha = parent->d_fsdata;
	unsigned val = pm8001_ha->main_cfg_tbl.event_log_option;
	if (val < pm8001_ha->main_cfg_tbl.iop_event_log_option)
		val = pm8001_ha->main_cfg_tbl.iop_event_log_option;
	return pm8001_debugfs_forensic_eventlog_value_open(
		inode, file, pm8001_debugfs_forensic_eventlog_level_write,
		"Eventlog Level=%d\n", val);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_eventlog_level = {
	{
		.name = "0.level",
		.type = PM8001_OP_FILE_RW
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_eventlog_level_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =    pm8001_debugfs_read,
		.write =   pm8001_debugfs_write,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_eventlog_size_write - size writer
 *	@file: The file pointer attached to the write operation
 *	@pos: first offset written
 *	@nbytes: number of bytes written
 *
 *	Description:
 *	This routine is the entry point for the debugfs write file operation.
 *	It takes action based on the data writen.
 */
static ssize_t
pm8001_debugfs_forensic_eventlog_size_write(
	struct file *file,
	loff_t pos,
	size_t nbytes)
{
	return pm8001_debugfs_forensic_eventlog_write(file, pos, nbytes, 1);
}

/*
 *	pm8001_debugfs_forensic_eventlog_size_open - Open the size handler
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the event log size variable
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_eventlog_size_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent = inode->i_private;
	struct pm8001_hba_info *pm8001_ha = parent->d_fsdata;
	size_t val = pm8001_ha->main_cfg_tbl.event_log_size;
	if (val < pm8001_ha->main_cfg_tbl.iop_event_log_size)
		val = pm8001_ha->main_cfg_tbl.iop_event_log_size;
	return pm8001_debugfs_forensic_eventlog_value_open(
		inode, file, pm8001_debugfs_forensic_eventlog_size_write,
		"Eventlog Size=%d\n", val);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_eventlog_size = {
	{
		.name = "1.size",
		.type = PM8001_OP_FILE_RW
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_eventlog_size_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =    pm8001_debugfs_read,
		.write =   pm8001_debugfs_write,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_eventlog_open - Open the event log handler
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the event log
 *	@ind: eventlog index
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_eventlog_open(
	struct inode *inode,
	struct file *file,
	int ind)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	struct pm8001_debug *debug;
	int rc = -ENOMEM;

	debug = kmalloc(sizeof(*debug), GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.size = 0;
	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	debug->blob.size = pm8001_ha->memoryMap.region[ind].element_size;
	debug->blob.data = pm8001_ha->memoryMap.region[ind].virt_ptr;
	debug->write = NULL;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}

/*
 *	pm8001_debugfs_forensic_eventlog_aap1_open - Open the aap1 event log
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the app1 event log
 *	@ind: eventlog index
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_eventlog_aap1_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_eventlog_open(inode, file, AAP1);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_eventlog_aap1 = {
	{
		.name = "3.aap1",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_eventlog_aap1_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =    pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/*
 *	pm8001_debugfs_forensic_eventlog_iop_open - Open the iop event log
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the iop event log
 *	@ind: eventlog index
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_eventlog_iop_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_eventlog_open(inode, file, IOP);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_eventlog_iop = {
	{
		.name = "4.iop",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_eventlog_iop_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =    pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op_eventlog = {
	{
		.name = "5.eventlog",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_eventlog_level.header,
		&pm8001_debugfs_forensic_op_eventlog_size.header,
		&pm8001_debugfs_forensic_op_eventlog_aap1.header,
		&pm8001_debugfs_forensic_op_eventlog_iop.header,
		NULL
	}
};

/* 6.mpi_config */

/*
 *	pm8001_debugfs_forensic_gstmpi_open - Open the MPI Config,
 *						GST table and MPI Queues
 *	@file: The file pointer to attach the queue
 *	@p: Address of the Data to dump.
 *	@size: Size of binary data to dump
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gstmpi_open(
	struct file *file,
	void *p,
	size_t size)
{
	struct pm8001_debug *debug;
	int len, rc = -ENOMEM;

	len = ((size / sizeof(uint32_t)) * SIZEOF_REGISTER_FORMAT) + 1;
	debug = kmalloc(sizeof(*debug) + len, GFP_KERNEL);
	if (!debug)
		goto out;

	debug->allocation.size = len;
	debug->blob.data = debug->buffer;

	debug->blob.size = 0;
	pm8001_debugfs_forensic_dump(debug, REGISTER_FORMAT,
				p , size, 0);

	debug->write = NULL;
	file->private_data = debug;

	rc = 0;
out:
	return rc;
}
/*
 *	pm8001_debugfs_forensic_mpi_configuration_open - Open the MPI
 *	                                                 configuration table
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the queue
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_mpi_configuration_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	size_t size;

	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	size = pm8001_ha->general_stat_tbl_addr -
				pm8001_ha->main_cfg_tbl_addr;

	return pm8001_debugfs_forensic_gstmpi_open(file,
				pm8001_ha->main_cfg_tbl_addr, size);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_mpi_configuration = {
	{
		.name = "6.mpi_config",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_mpi_configuration_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/* 7.gst */

/*
 *	pm8001_debugfs_forensic_gst_open - Open the General Status Table
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the queue
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_gst_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	size_t size;

	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	size = pm8001_ha->inbnd_q_tbl_addr -
				pm8001_ha->general_stat_tbl_addr;

	return pm8001_debugfs_forensic_gstmpi_open(file,
				pm8001_ha->general_stat_tbl_addr, size);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_gst = {
	{
		.name = "7.gst",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_gst_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/* 8.mpi_iqueue */

/*
 *	pm8001_debugfs_forensic_mpi_inbound_queue_open - Open the MPI
 *                                                inbound queue table
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the queue
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_mpi_inbound_queue_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	size_t size;

	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	size = 0x20; /* As hard-coded into companion code in driver */

	return pm8001_debugfs_forensic_gstmpi_open(file,
				pm8001_ha->inbnd_q_tbl_addr, size);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_mpi_inbound_queue = {
	{
		.name = "8.mpi_iqueue",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_mpi_inbound_queue_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/* 8.mpi_oqueue */

/*
 *	pm8001_debugfs_forensic_mpi_outbound_queue_open - Open the MPI
 *                                                outbound queue table
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the queue
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static int
pm8001_debugfs_forensic_mpi_outbound_queue_open(
	struct inode *inode,
	struct file *file)
{
	struct dentry *parent;
	struct pm8001_hba_info *pm8001_ha;
	size_t size;

	parent = inode->i_private;
	pm8001_ha = parent->d_fsdata;
	size = 0x24; /* As hard-coded into companion code in driver */

	return pm8001_debugfs_forensic_gstmpi_open(file,
				pm8001_ha->outbnd_q_tbl_addr, size);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_mpi_outbound_queue = {
	{
		.name = "8.mpi_oqueue",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_mpi_outbound_queue_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/* 9.analog */

/*
 *	pm8001_debugfs_forensic_analog_open - Open the analog configuration
 *	                                      registers
 *	@inode: The inode pointer
 *	@file: The file pointer to attach the analog memory
 *
 *	Description:
 *	This routine is the entry point for the debugfs open file operation. It
 *	fills the data and returns a pointer to that data in the private_data
 *	field in @file.
 */
static struct pm8001_debugfs_forensic_mem_list
pm8001_debugfs_forensic_analog_list[] = {
	{ 0x020100, 12 },
	{ 0x020110, 8 },
/*	{ 0x020120, 4 }, */
	{ 0x020128, 4 },
	{ 0x020130, 8 },
	{ 0x020200, 12 },
	{ 0x020210, 8 },
/*	{ 0x020220, 4 }, */
	{ 0x020228, 4 },
	{ 0x020230, 8 },
	{ 0x020300, 12 },
	{ 0x020310, 8 },
/*	{ 0x020320, 4 }, */
	{ 0x020328, 4 },
	{ 0x020330, 8 },
	{ 0x020400, 12 },
	{ 0x020410, 8 },
/*	{ 0x020420, 4 }, */
	{ 0x020428, 4 },
	{ 0x020430, 8 },
	{ 0x020500, 12 },
	{ 0x020510, 8 },
/*	{ 0x020520, 4 }, */
	{ 0x020528, 4 },
	{ 0x020530, 8 },
	{ 0x020600, 12 },
	{ 0x020610, 8 },
/*	{ 0x020620, 4 }, */
	{ 0x020628, 4 },
	{ 0x020630, 8 },
	{ 0x020700, 12 },
	{ 0x020710, 8 },
/*	{ 0x020720, 4 }, */
	{ 0x020728, 4 },
	{ 0x020730, 8 },
	{ 0x020800, 12 },
	{ 0x020810, 8 },
/*	{ 0x020820, 4 }, */
	{ 0x020828, 4 },
	{ 0x020830, 8 },
	{ 0, 0 },
};

static int
pm8001_debugfs_forensic_analog_open(
	struct inode *inode,
	struct file *file)
{
	return pm8001_debugfs_forensic_memory_open(
		inode, file,
		pm8001_debugfs_forensic_next_list_entry,
		pm8001_debugfs_forensic_analog_list, 2);
}

static const struct pm8001_file_operations
pm8001_debugfs_forensic_op_analog = {
	{
		.name = "9.analog",
		.type = PM8001_OP_FILE_RO
	},
	{
		.owner =   THIS_MODULE,
		.open =	   pm8001_debugfs_forensic_analog_open,
		.llseek =  pm8001_debugfs_lseek,
		.read =	   pm8001_debugfs_read,
		.release = pm8001_debugfs_release,
	}
};

/* forensic root */

static const struct pm8001_dir_operations
pm8001_debugfs_forensic_op = {
	{
		.name = "forensic",
		.type = PM8001_OP_DIR
	},
	{
		&pm8001_debugfs_forensic_op_gsm_memory.header,
		&pm8001_debugfs_forensic_op_queue.header,
		&pm8001_debugfs_forensic_op_gsm.header,
		&pm8001_debugfs_forensic_op_msgu.header,
		&pm8001_debugfs_forensic_op_eventlog.header,
		&pm8001_debugfs_forensic_op_mpi_configuration.header,
		&pm8001_debugfs_forensic_op_gst.header,
		&pm8001_debugfs_forensic_op_mpi_outbound_queue.header,
		&pm8001_debugfs_forensic_op_mpi_inbound_queue.header,
		&pm8001_debugfs_forensic_op_analog.header,
		NULL
	}
};

static struct dentry *pm8001_debugfs_root;
static atomic_t pm8001_debugfs_hba_count;
#endif

/*
 *	pm8001_debugfs_initialize - Initialize debugfs
 *	@pm8001_ha: Hba information structure
 *
 *	Description:
 *	When Debugfs is configured this rotuine sets up the pm8001 debugfs
 *	file system. If not already created, this routine will create the
 *	pm8001 directory, and pm8001.X directory for each HBA. It will also
 *	create each file used to access pm8001 specific debugfs features.
 */
void pm8001_debugfs_initialize(struct pm8001_hba_info *pm8001_ha)
{
#ifdef CONFIG_SCSI_PM8001_DEBUG_FS
	char name[64];

	if (!pm8001_debugfs_enable)
		return;

	/* Setup pm8001 root directory */
	if (!pm8001_debugfs_root) {
		pm8001_debugfs_root = debugfs_create_dir("pm8001", NULL);
		atomic_set(&pm8001_debugfs_hba_count, 0);
		if (!pm8001_debugfs_root) {
			pm8001_printk("Cannot create debugfs root\n");
			goto debug_failed;
		}
		pm8001_debugfs_root->d_fsdata = pm8001_ha;
	}

	/* Setup pm8001.X directory for specific HBA */
	if (!pm8001_ha->hba_debugfs_root) {
		snprintf(name, sizeof(name), "pm8001.%d", pm8001_ha->id);
		pm8001_ha->hba_debugfs_root =
			debugfs_create_dir(name, pm8001_debugfs_root);
		if (!pm8001_ha->hba_debugfs_root) {
			pm8001_printk("Cannot create debugfs hba\n");
			goto debug_failed;
		}
		atomic_inc(&pm8001_debugfs_hba_count);
		pm8001_ha->hba_debugfs_root->d_fsdata = pm8001_ha;

		if (pm8001_debugfs_build_tree(
				&pm8001_debugfs_forensic_op.header,
				pm8001_ha->hba_debugfs_root, pm8001_ha, name)) {
			goto debug_failed;
		}
	}
debug_failed:
	return;
#endif
}

#if (defined(CONFIG_SCSI_PM8001_DEBUG_FS) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)))
/* Pulled-in from future kernels, ported slightly, racy */

/**
 * debugfs_remove_recursive - recursively removes a directory
 * @dentry: a pointer to a the dentry of the directory to be removed.
 *
 * This function recursively removes a directory tree in debugfs that
 * was previously created with a call to another debugfs function
 * (like debugfs_create_file() or variants thereof.)
 *
 * This function is required to be called in order for the file to be
 * removed, no automatic cleanup of files will happen when a module is
 * removed, you are responsible here.
 */
static void debugfs_remove_recursive(struct dentry *dentry)
{
	struct dentry *child;
	struct dentry *parent;

	if (!dentry)
		return;

	parent = dentry->d_parent;
	if (!parent || !parent->d_inode)
		return;

	parent = dentry;
	mutex_lock(&parent->d_inode->i_mutex);

	while (1) {
		/*
		 * When all dentries under "parent" has been removed,
		 * walk up the tree until we reach our starting point.
		 */
		if (list_empty(&parent->d_subdirs)) {
			mutex_unlock(&parent->d_inode->i_mutex);
			if (parent == dentry)
				break;
			parent = parent->d_parent;
			mutex_lock(&parent->d_inode->i_mutex);
		}
		child = list_entry(parent->d_subdirs.next, struct dentry,
				d_u.d_child);
 next_sibling:

		/*
		 * If "child" isn't empty, walk down the tree and
		 * remove all its descendants first.
		 */
		if (!list_empty(&child->d_subdirs)) {
			mutex_unlock(&parent->d_inode->i_mutex);
			parent = child;
			mutex_lock(&parent->d_inode->i_mutex);
			continue;
		}
		mutex_unlock(&parent->d_inode->i_mutex);
		debugfs_remove(child);
		mutex_lock(&parent->d_inode->i_mutex);
		if (parent->d_subdirs.next == &child->d_u.d_child) {
			/*
			 * Try the next sibling.
			 */
			if (child->d_u.d_child.next != &parent->d_subdirs) {
				child = list_entry(child->d_u.d_child.next,
						   struct dentry,
						   d_u.d_child);
				goto next_sibling;
			}

			/*
			 * Avoid infinite loop if we fail to remove
			 * one dentry.
			 */
			mutex_unlock(&parent->d_inode->i_mutex);
			break;
		}
	}

	debugfs_remove(dentry);
}
#endif

/*
 *	pm8001_debugfs_terminate - Tear down debugfs infrastructure.
 *	@pm8001_ha: Hba information structure
 *
 *	Description:
 *	When Debugfs is configured this routine removes debugfs file system
 *	elements that are specific to this hba. If also checks to see if there
 *	are any users left for the debugfs directories associated with the HBA
 *	and driver. If this is the last user of the HBA directory or driver
 *	directory then it will remove those from the debugfs infrastructure as
 *	well.
 */
void pm8001_debugfs_terminate(struct pm8001_hba_info *pm8001_ha)
{
#ifdef CONFIG_SCSI_PM8001_DEBUG_FS
	if (pm8001_ha->hba_debugfs_root) {
		debugfs_remove_recursive(pm8001_ha->hba_debugfs_root);
		pm8001_ha->hba_debugfs_root = NULL;
		atomic_dec(&pm8001_debugfs_hba_count);
	}
	if (atomic_read(&pm8001_debugfs_hba_count) == 0) {
		debugfs_remove(pm8001_debugfs_root);
		pm8001_debugfs_root = NULL;
	}
#endif
	return;
}
