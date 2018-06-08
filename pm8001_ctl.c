/*
 * PMC-Sierra SPC 8001 SAS/SATA based host adapters driver
 *
 * Copyright (c) 2008-2009 USI Co., Ltd.
 * All rights reserved.
 * Copyright (c) 2010-2011 Xyratex International Inc.,
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
#include <linux/firmware.h>
#include <linux/slab.h>
#include "pm8001_sas.h"
#include "pm8001_ctl.h"


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#define	PMCS_SYSFS_DEV		class_device
#define	PMCS_DEVICE_ATTR	CLASS_DEVICE_ATTR
#define	PMCS_ATTR_ARG
#else
#define	PMCS_SYSFS_DEV		device
#define	PMCS_DEVICE_ATTR	DEVICE_ATTR
#define	PMCS_ATTR_ARG		struct device_attribute *attr,
#endif

/* scsi host attributes */

/**
 * pm8001_ctl_mpi_interface_rev_show - MPI interface revision number
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_mpi_interface_rev_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		pm8001_ha->main_cfg_tbl.interface_rev);
}
static
PMCS_DEVICE_ATTR(interface_rev, S_IRUGO, pm8001_ctl_mpi_interface_rev_show, NULL);

/**
 * pm8001_ctl_tags_alloc_show - tags active
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_tags_alloc_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%d\n",
		pm8001_ha->tags_alloc);
}
static
PMCS_DEVICE_ATTR(tags_alloc, S_IRUGO, pm8001_ctl_tags_alloc_show, 0);

/**
 * pm8001_ctl_fw_version_show - firmware version
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_fw_version_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%02x.%02x.%02x.%02x\n",
		       (u8)(pm8001_ha->main_cfg_tbl.firmware_rev >> 24),
		       (u8)(pm8001_ha->main_cfg_tbl.firmware_rev >> 16),
		       (u8)(pm8001_ha->main_cfg_tbl.firmware_rev >> 8),
		       (u8)(pm8001_ha->main_cfg_tbl.firmware_rev));
}
static PMCS_DEVICE_ATTR(fw_version, S_IRUGO, pm8001_ctl_fw_version_show, NULL);
/**
 * pm8001_ctl_max_out_io_show - max outstanding io supported
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_max_out_io_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			pm8001_ha->main_cfg_tbl.max_out_io);
}
static PMCS_DEVICE_ATTR(max_out_io, S_IRUGO, pm8001_ctl_max_out_io_show, NULL);
/**
 * pm8001_ctl_max_devices_show - max devices support
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_max_devices_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%04d\n",
			(u16)(pm8001_ha->main_cfg_tbl.max_sgl >> 16));
}
static PMCS_DEVICE_ATTR(max_devices, S_IRUGO, pm8001_ctl_max_devices_show, NULL);
/**
 * pm8001_ctl_max_sg_list_show - max sg list supported iff not 0.0 for no
 * hardware limitation
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_max_sg_list_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%04d\n",
			pm8001_ha->main_cfg_tbl.max_sgl & 0x0000FFFF);
}
static PMCS_DEVICE_ATTR(max_sg_list, S_IRUGO, pm8001_ctl_max_sg_list_show, NULL);

#define SAS_1_0 0x1
#define SAS_1_1 0x2
#define SAS_2_0 0x4

static ssize_t
show_sas_spec_support_status(unsigned int mode, char *buf)
{
	ssize_t len = 0;

	if (mode & SAS_1_1)
		len = sprintf(buf, "%s", "SAS1.1");
	if (mode & SAS_2_0)
		len += sprintf(buf + len, "%s%s", len ? ", " : "", "SAS2.0");
	len += sprintf(buf + len, "\n");

	return len;
}

/**
 * pm8001_ctl_sas_spec_support_show - sas spec supported
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_sas_spec_support_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	unsigned int mode;
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;
	mode = (pm8001_ha->main_cfg_tbl.ctrl_cap_flag & 0xfe000000)>>25;
	return show_sas_spec_support_status(mode, buf);
}
static PMCS_DEVICE_ATTR(sas_spec_support, S_IRUGO,
		   pm8001_ctl_sas_spec_support_show, NULL);

/**
 * pm8001_ctl_sas_address_show - sas address
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * This is the controller sas address
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_host_sas_address_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;
	char *cp = buf;
	ssize_t r, l = 0;
	int i;

	for (i = 0;
	     i < (sizeof(pm8001_ha->sas_addr)/sizeof(pm8001_ha->sas_addr[0]));
	     ++i) {
		if (i) {
			if (l >= PAGE_SIZE)
				break;
			*(cp++) = ' ';
			++l;
		}
		r = snprintf(cp, PAGE_SIZE - l, "0x%016llx",
			be64_to_cpu(*(__be64 *)pm8001_ha->sas_addr[i]));
		l += r;
		cp += r;
	}
	if (l < PAGE_SIZE) {
		*(cp++) = '\n';
		++l;
	}
	return l;
}
static ssize_t pm8001_ctl_host_sas_address_store(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;
	u64 val[8];
	int i;
	DECLARE_COMPLETION_ONSTACK(completion);
	struct pm8001_ioctl_payload payload;
	static const unsigned char map[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };

	memset(val, 0, sizeof(val));
	i = sscanf(buf,
		"0x%016llx 0x%016llx 0x%016llx 0x%016llx "
		"0x%016llx 0x%016llx 0x%016llx 0x%016llx",
		&val[0], &val[1], &val[2], &val[3],
		&val[4], &val[5], &val[6], &val[7]);
	if (i != 8)
		return -EINVAL;

	for (i = 0;
	     i < (sizeof(pm8001_ha->sas_addr)/sizeof(pm8001_ha->sas_addr[0]));
	     ++i)
		*(__be64 *)pm8001_ha->sas_addr[i] = cpu_to_be64(val[i]);
	pm8001_ha->nvmd_completion = &completion;
	payload.minor_function = 0;	/* TWI devices */
	payload.length = 128;
	payload.func_specific = PMALLOC(128, GFP_KERNEL);
	memset(payload.func_specific, 0xff, 128);
	for (i = 0; i < sizeof(map); i++)
		memcpy(&payload.func_specific[map[i] * SAS_ADDR_SIZE],
			&pm8001_ha->sas_addr[i],
			SAS_ADDR_SIZE);
	PM8001_CHIP_DISP->set_nvmd_req(pm8001_ha, &payload);
	wait_for_completion(&completion);
	PMFREE(payload.func_specific, 128);
	return strlen(buf);
}
static PMCS_DEVICE_ATTR(host_sas_address, S_IRUGO | S_IWUSR,
		   pm8001_ctl_host_sas_address_show,
		   pm8001_ctl_host_sas_address_store);

/**
 * pm8001_ctl_logging_level_show - logging level
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read/write' shost attribute.
 */
static ssize_t pm8001_ctl_logging_level_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	return snprintf(buf, PAGE_SIZE, "%08xh\n", pm8001_ha->logging_level);
}
static ssize_t pm8001_ctl_logging_level_store(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;
	int val = 0;

	if (sscanf(buf, "%x", &val) != 1)
		return -EINVAL;

	pm8001_ha->logging_level = val;
	pm8001_update_main_config_table(pm8001_ha);
	return strlen(buf);
}

static PMCS_DEVICE_ATTR(logging_level, S_IRUGO | S_IWUSR,
	pm8001_ctl_logging_level_show, pm8001_ctl_logging_level_store);

#if	PMDEBUG > 0
/**
 * pm8001_ctl_allocation_show - memory allocation amount
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read' shost attribute.
 */
size_t pmallocation = 0;
static ssize_t pm8001_ctl_allocation_show(struct PMCS_SYSFS_DEV *cdev, PMCS_ATTR_ARG char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lx\n", (unsigned long)pmallocation);
}
static PMCS_DEVICE_ATTR(allocation, S_IRUGO, pm8001_ctl_allocation_show, 0);
#endif

/**
 * pm8001_readlog - generic routine to read a single event log entry
 * @header: a pointer to the event log base header
 * @entry: a pointer to copy the latest event log into
 * @consumer_index: a current local index pointing to the last oldest entry
 *                  retrieved. Automatically incremented and wrapped.
 * @return: A copied entry content and a status. Status return of zero means
 *          more entries available, and the consumer index is incremented and
 *          wrapped. A positive status return means last entry, the consumer
 *          index may be incremented depending of approached or at the last
 *          entry. Status can also return a negative error code. It is the
 *          responsibility of the caller to confirm the validity of the copied
 *          event information.
 */
int pm8001_readlog(
	struct eventlog_header *header,
	struct eventlog_entry *entry,
	u32 *consumer_index)
{
	unsigned maximum_index;
	unsigned producer_index;

	/* Validity of header */
	if ((header->offset != sizeof(*header)) ||
	    (header->entry_size != sizeof(*entry)) ||
	    ((header->signature != EVENTLOG_HEADER_SIGNATURE_AAP1) &&
	     (header->signature != EVENTLOG_HEADER_SIGNATURE_IOP)))
		return -EINVAL;
	/* Validity of hardware indexii */
	maximum_index = header->size / sizeof(*entry);
	if ((maximum_index <= header->producer_index) ||
	    (maximum_index <= header->consumer_index))
		return -EINVAL;
	producer_index = header->producer_index;
	if (producer_index == 0)
		producer_index = maximum_index - 1;
	else
		--producer_index;
	/* Validity of current consumer index */
	if (((producer_index < header->consumer_index) ?
	     ((producer_index < *consumer_index) &&
	      (*consumer_index < header->consumer_index)) :
	     ((producer_index < *consumer_index) ||
	      (*consumer_index < header->consumer_index))) ||
	    (*consumer_index >= maximum_index))
		*consumer_index = header->consumer_index;
	*entry = ((struct eventlog_entry *)(header + 1))[*consumer_index];
	if (*consumer_index != producer_index) {
		++(*consumer_index);
		if (*consumer_index >= maximum_index)
			*consumer_index = 0;
		/* More entries? */
		return *consumer_index == producer_index;
	}
	return 2; /* Already at Last Entry */
}

/**
 * pm8001_validlog - generic routine to validate a single event log entry
 * @entry: a pointer to the event log entry
 * @return: zero if not valid
 */
static inline int pm8001_validlog(struct eventlog_entry *entry)
{
	return ((entry->size <= (sizeof(entry->log) / sizeof(entry->log[0]))) &&
		((entry->timestamp_upper != 0) ||
		 (entry->timestamp_lower != 0)));
}

/**
 * pm8001_ctl_log_show - event log
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 * @ind: The region index (AAP1 or IOP)
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_log_show(struct PMCS_SYSFS_DEV *cdev,
	char *buf, int ind)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;
	struct eventlog_entry entry;
	char *str = buf;
	int i;

	memset(&entry, 0, sizeof(entry));
	do {
		i = pm8001_readlog(
			(struct eventlog_header *)(
				pm8001_ha->memoryMap.region[ind].virt_ptr),
			&entry,
			(ind == AAP1) ?
				&pm8001_ha->aap1_consumer :
				&pm8001_ha->iop_consumer);
	} while ((i == 0) && !pm8001_validlog(&entry));
	if ((i >= 0) && pm8001_validlog(&entry)) {
		u32 *lp;
		unsigned long long timestamp =
			(((unsigned long long)entry.timestamp_upper) << 32) |
			entry.timestamp_lower, remainder;
		remainder = do_div(timestamp, 1000000000 / 8) * 8;
		str += snprintf(str, PAGE_SIZE - (str - buf),
			"%u %llu.%09llu %u",
			entry.severity,
			timestamp,
			remainder,
			entry.sequence);
		for (lp = entry.log, i = entry.size; i > 0; --i)
			str += snprintf(str, PAGE_SIZE - (str - buf),
				" 0x%08x", *(lp++));
		/* Here is where we would interpret the log */

		str += snprintf(str, PAGE_SIZE - (str - buf), "\n");
	}
	return str - buf;
}

/**
 * pm8001_ctl_aap_log_show - aap1 event log
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_aap_log_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	return pm8001_ctl_log_show(cdev, buf, AAP1);
}
static PMCS_DEVICE_ATTR(aap_log, S_IRUGO, pm8001_ctl_aap_log_show, NULL);
/**
 * pm8001_ctl_aap_log_show - IOP event log
 * @cdev: pointer to embedded class device
 * @buf: the buffer returned
 *
 * A sysfs 'read-only' shost attribute.
 */
static ssize_t pm8001_ctl_iop_log_show(struct PMCS_SYSFS_DEV *cdev,
	PMCS_ATTR_ARG char *buf)
{
	return pm8001_ctl_log_show(cdev, buf, IOP);
}
static PMCS_DEVICE_ATTR(iop_log, S_IRUGO, pm8001_ctl_iop_log_show, NULL);

#define FLASH_CMD_NONE      0x00
#define FLASH_CMD_UPDATE    0x01
#define FLASH_CMD_SET_NVMD    0x02

struct flash_command {
     u8      command[8];
     int     code;
};

static struct flash_command flash_command_table[] =
{
     {"set_nvmd",    FLASH_CMD_SET_NVMD},
     {"update",      FLASH_CMD_UPDATE},
     {"",            FLASH_CMD_NONE} /* Last entry should be NULL. */
};

struct error_fw {
     char    *reason;
     int     err_code;
};

static struct error_fw flash_error_table[] =
{
     {"Failed to open fw image file",	FAIL_OPEN_BIOS_FILE},
     {"image header mismatch",		FLASH_UPDATE_HDR_ERR},
     {"image offset mismatch",		FLASH_UPDATE_OFFSET_ERR},
     {"image CRC Error",		FLASH_UPDATE_CRC_ERR},
     {"image length Error.",		FLASH_UPDATE_LENGTH_ERR},
     {"Failed to program flash chip",	FLASH_UPDATE_HW_ERR},
     {"Flash chip not supported.",	FLASH_UPDATE_DNLD_NOT_SUPPORTED},
     {"Flash update disabled.",		FLASH_UPDATE_DISABLED},
     {"Flash in progress",		FLASH_IN_PROGRESS},
     {"Image file size Error",		FAIL_FILE_SIZE},
     {"Input parameter error",		FAIL_PARAMETERS},
     {"Out of memory",			FAIL_OUT_MEMORY},
     {"OK", 0}	/* Last entry err_code = 0. */
};

static int pm8001_set_nvmd(struct pm8001_hba_info *pm8001_ha)
{
	struct pm8001_ioctl_payload	*payload;
	DECLARE_COMPLETION_ONSTACK(completion);
	u8		*ioctlbuffer = NULL;
	u32		length = 0;
	u32		ret = 0;

	length = 1024 * 5 + sizeof(*payload) - 1;
	ioctlbuffer = PMALLOC(length, GFP_KERNEL);
	if (!ioctlbuffer)
		return -ENOMEM;
	if ((pm8001_ha->fw_image->size <= 0) ||
	    (pm8001_ha->fw_image->size > 4096)) {
		ret = FAIL_FILE_SIZE;
		goto out;
	}
	payload = (struct pm8001_ioctl_payload *)ioctlbuffer;
	memcpy((u8 *)payload->func_specific, (u8 *)pm8001_ha->fw_image->data,
				pm8001_ha->fw_image->size);
	payload->length = pm8001_ha->fw_image->size;
	payload->id = 0;
	pm8001_ha->nvmd_completion = &completion;
	ret = PM8001_CHIP_DISP->set_nvmd_req(pm8001_ha, payload);
	wait_for_completion(&completion);
out:
	PMFREE(ioctlbuffer, length);
	return ret;
}

static int pm8001_update_flash(struct pm8001_hba_info *pm8001_ha)
{
	struct pm8001_ioctl_payload	*payload;
	DECLARE_COMPLETION_ONSTACK(completion);
	u8		*ioctlbuffer = NULL;
	u32		length = 0;
	struct fw_control_info	*fwControl;
	u32		loopNumber, loopcount = 0;
	u32		sizeRead = 0;
	u32		partitionSize, partitionSizeTmp;
	u32		ret = 0;
	u32		partitionNumber = 0;
	struct pm8001_fw_image_header *image_hdr;

	length = 1024 * 16 + sizeof(*payload) - 1;
	ioctlbuffer = PMALLOC(length, GFP_KERNEL);
	image_hdr = (struct pm8001_fw_image_header *)pm8001_ha->fw_image->data;
	if (!ioctlbuffer)
		return -ENOMEM;
	if (pm8001_ha->fw_image->size < 28) {
		ret = FAIL_FILE_SIZE;
		goto out;
	}

	while (sizeRead < pm8001_ha->fw_image->size) {
		partitionSizeTmp =
			*(u32 *)((u8 *)&image_hdr->image_length + sizeRead);
		partitionSize = be32_to_cpu(partitionSizeTmp);
		loopcount = (partitionSize + HEADER_LEN)/IOCTL_BUF_SIZE;
		if (loopcount % IOCTL_BUF_SIZE)
			loopcount++;
		if (loopcount == 0)
			loopcount++;
		for (loopNumber = 0; loopNumber < loopcount; loopNumber++) {
			payload = (struct pm8001_ioctl_payload *)ioctlbuffer;
			payload->length = 1024*16;
			payload->id = 0;
			fwControl =
			      (struct fw_control_info *)payload->func_specific;
			fwControl->len = IOCTL_BUF_SIZE;   /* IN */
			fwControl->size = partitionSize + HEADER_LEN;/* IN */
			fwControl->retcode = 0;/* OUT */
			fwControl->offset = loopNumber * IOCTL_BUF_SIZE;/*OUT */

		/* for the last chunk of data in case file size is not even with
		4k, load only the rest*/
		if (((loopcount-loopNumber) == 1) &&
			((partitionSize + HEADER_LEN) % IOCTL_BUF_SIZE)) {
			fwControl->len =
				(partitionSize + HEADER_LEN) % IOCTL_BUF_SIZE;
			memcpy((u8 *)fwControl->buffer,
				(u8 *)pm8001_ha->fw_image->data + sizeRead,
				(partitionSize + HEADER_LEN) % IOCTL_BUF_SIZE);
			sizeRead +=
				(partitionSize + HEADER_LEN) % IOCTL_BUF_SIZE;
		} else {
			memcpy((u8 *)fwControl->buffer,
				(u8 *)pm8001_ha->fw_image->data + sizeRead,
				IOCTL_BUF_SIZE);
			sizeRead += IOCTL_BUF_SIZE;
		}

		pm8001_ha->nvmd_completion = &completion;
		ret = PM8001_CHIP_DISP->fw_flash_update_req(pm8001_ha, payload);
		wait_for_completion(&completion);
		if (ret || (fwControl->retcode > FLASH_UPDATE_IN_PROGRESS)) {
			ret = fwControl->retcode;
			PMFREE(ioctlbuffer, length);
			ioctlbuffer = NULL;
			break;
		}
	}
	if (ret)
		break;
	partitionNumber++;
}
out:
	if (ioctlbuffer)
		PMFREE(ioctlbuffer, length);
	return ret;
}
static ssize_t pm8001_store_update_fw(struct PMCS_SYSFS_DEV *cdev,
				      PMCS_ATTR_ARG
				      const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;
	char *cmd_ptr, *filename_ptr;
	int res, i;
	int flash_command = FLASH_CMD_NONE;
	int err = 0;
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	cmd_ptr = PMALLOC(count*2, GFP_KERNEL);

	if (!cmd_ptr) {
		err = FAIL_OUT_MEMORY;
		goto out;
	}

	filename_ptr = cmd_ptr + count;
	res = sscanf(buf, "%s %s", cmd_ptr, filename_ptr);
	if (res != 2) {
		err = FAIL_PARAMETERS;
		goto out1;
	}

	for (i = 0; flash_command_table[i].code != FLASH_CMD_NONE; i++) {
		if (!memcmp(flash_command_table[i].command,
				 cmd_ptr, strlen(cmd_ptr))) {
			flash_command = flash_command_table[i].code;
			break;
		}
	}
	if (flash_command == FLASH_CMD_NONE) {
		err = FAIL_PARAMETERS;
		goto out1;
	}

	if (pm8001_ha->fw_status == FLASH_IN_PROGRESS) {
		err = FLASH_IN_PROGRESS;
		goto out1;
	}
	err = request_firmware(&pm8001_ha->fw_image,
			       filename_ptr,
			       pm8001_ha->dev);

	if (err) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Failed to load firmware image file %s,"
			" error %d\n", filename_ptr, err));
		err = FAIL_OPEN_BIOS_FILE;
		goto out1;
	}

	switch (flash_command) {
	case FLASH_CMD_UPDATE:
		pm8001_ha->fw_status = FLASH_IN_PROGRESS;
		err = pm8001_update_flash(pm8001_ha);
		break;
	case FLASH_CMD_SET_NVMD:
		pm8001_ha->fw_status = FLASH_IN_PROGRESS;
		err = pm8001_set_nvmd(pm8001_ha);
		break;
	default:
		pm8001_ha->fw_status = FAIL_PARAMETERS;
		err = FAIL_PARAMETERS;
		break;
	}
	release_firmware(pm8001_ha->fw_image);
out1:
	PMFREE(cmd_ptr, count*2);
out:
	pm8001_ha->fw_status = err;

	if (!err)
		return count;
	else
		return -err;
}

static ssize_t pm8001_show_update_fw(struct PMCS_SYSFS_DEV *cdev,
				     PMCS_ATTR_ARG char *buf)
{
	int i;
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct sas_ha_struct *sha = SHOST_TO_SAS_HA(shost);
	struct pm8001_hba_info *pm8001_ha = sha->lldd_ha;

	for (i = 0; flash_error_table[i].err_code != 0; i++) {
		if (flash_error_table[i].err_code == pm8001_ha->fw_status)
			break;
	}
	if (pm8001_ha->fw_status != FLASH_IN_PROGRESS)
		pm8001_ha->fw_status = FLASH_OK;

	return snprintf(buf, PAGE_SIZE, "status=%x %s\n",
			flash_error_table[i].err_code,
			flash_error_table[i].reason);
}

static PMCS_DEVICE_ATTR(update_fw, S_IRUGO|S_IWUGO,
	pm8001_show_update_fw, pm8001_store_update_fw);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
struct PMCS_SYSFS_DEV_ATTR *pm8001_host_attrs[] = {
	&class_device_attr_interface_rev,
	&class_device_attr_tags_alloc,
	&class_device_attr_fw_version,
	&class_device_attr_update_fw,
#if	PMDEBUG > 0
	&class_device_attr_allocation,
#endif
	&class_device_attr_aap_log,
	&class_device_attr_iop_log,
	&class_device_attr_max_out_io,
	&class_device_attr_max_devices,
	&class_device_attr_max_sg_list,
	&class_device_attr_sas_spec_support,
	&class_device_attr_logging_level,
	&class_device_attr_host_sas_address,
	NULL,
};
#else
struct PMCS_SYSFS_DEV_ATTR *pm8001_host_attrs[] = {
	&dev_attr_interface_rev,
	&dev_attr_tags_alloc,
	&dev_attr_fw_version,
	&dev_attr_update_fw,
	&dev_attr_allocation,
	&dev_attr_aap_log,
	&dev_attr_iop_log,
	&dev_attr_max_out_io,
	&dev_attr_max_devices,
	&dev_attr_max_sg_list,
	&dev_attr_sas_spec_support,
	&dev_attr_logging_level,
	&dev_attr_host_sas_address,
	NULL,
};
#endif

