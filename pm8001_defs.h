/*
 * PMC-Sierra SPC 8001 SAS/SATA based host adapters driver
 *
 * Copyright (c) 2008-2009 USI Co., Ltd.
 * All rights reserved.
 * Copyright (c) 2010 Xyratex International Inc.,
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

#ifndef _PM8001_DEFS_H_
#define _PM8001_DEFS_H_

#include <linux/version.h>
#include <scsi/scsi.h>

#ifndef	PCI_VENDOR_ID_PMC_Sierra
#define PCI_VENDOR_ID_PMC_Sierra        0x11f8
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#define	PMCS_SYSFS_DEV_ATTR	class_device_attribute
#else
#define	PMCS_SYSFS_DEV_ATTR	device_attribute
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#define	PMCS_FUNCDATA_ARG	
#define	PMCS_FUNCDATA_VAL	NULL
#define	PMCS_GFP_T		unsigned long
#define	SAS_PROTOCOL_ALL	0xe
#include <linux/workqueue.h>
#define EXTRA_IRQ_ARGS      , struct pt_regs *unused
// typedef irqreturn_t (*irq_handler_t)(int, void * EXTRA_IRQ_ARGS);
#else
#define	PMCS_FUNCDATA_ARG	, void *funcdata
#define	PMCS_FUNCDATA_VAL	funcdata
#define	PMCS_GFP_T		gfp_t
#define EXTRA_IRQ_ARGS  
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
#define PMCS_WORK_ARG                void *
static inline void INIT_WORK_compat(struct work_struct *work, void *func)
{
        INIT_WORK(work, func, work);
}
#undef INIT_WORK
#define INIT_WORK(_work, _func) INIT_WORK_compat(_work, _func)
#else
#define PMCS_WORK_ARG                struct work_struct *
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define sg_page(_sg) ((_sg)->page)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#include <linux/kernel.h>
#include <scsi/sas.h>
#include <scsi/libsas.h>
static __inline void sas_ssp_task_response(struct device *, struct sas_task *, struct ssp_response_iu *);
static __inline void sas_ssp_task_response(struct device *dev, struct sas_task *task, struct ssp_response_iu *iu)
{
        struct task_status_struct *tstat = &task->task_status;

        tstat->resp = SAS_TASK_COMPLETE;

        if (iu->datapres == 0)
                tstat->stat = iu->status;
        else if (iu->datapres == 1)
                tstat->stat = iu->resp_data[3];
        else if (iu->datapres == 2) {
                tstat->stat = SAM_CHECK_COND;
                tstat->buf_valid_size =
                        min_t(int, SAS_STATUS_BUF_SIZE,
                              be32_to_cpu(iu->sense_data_len));
                memcpy(tstat->buf, iu->sense_data, tstat->buf_valid_size);

                if (iu->status != SAM_CHECK_COND)
                        dev_printk(KERN_WARNING, dev,
                                   "dev %llx sent sense data, but "
                                   "stat(%x) is not CHECK CONDITION\n",
                                   SAS_ADDR(task->dev->sas_addr),
                                   iu->status);
        }
        else
                /* when datapres contains corrupt/unknown value... */
                tstat->stat = SAM_CHECK_COND;
}
#endif

enum chip_flavors {
	chip_8001,
};
#define PM8001_MAX_DMA_SG		SG_ALL
enum phy_speed {
	PHY_SPEED_15 = 0x01,
	PHY_SPEED_30 = 0x02,
	PHY_SPEED_60 = 0x04,
};

enum data_direction {
	DATA_DIR_NONE = 0x0,	/* NO TRANSFER */
	DATA_DIR_IN = 0x01,	/* INBOUND */
	DATA_DIR_OUT = 0x02,	/* OUTBOUND */
	DATA_DIR_BYRECIPIENT = 0x04, /* UNSPECIFIED */
};

enum port_type {
	PORT_TYPE_SAS = (1L << 1),
	PORT_TYPE_SATA = (1L << 0),
};

/* driver compile-time configuration */
#define	PM8001_MAX_CCB_ARRAY	 1
#if (PM8001_MAX_CCB_ARRAY == 1)
#define	PM8001_MAX_CCB		 512	/* max ccbs supported */
#else
#define	PM8001_CCB_PER_ARRAY	 512
#define	PM8001_MAX_CCB		 (PM8001_CCB_PER_ARRAY * PM8001_MAX_CCB_ARRAY)
#endif
/* maximum mpi queue entries */
#define PM8001_MPI_QUEUE         ((PM8001_MAX_CCB) * 2)

#define	PM8001_MAX_INB_NUM	 1
#define	PM8001_MAX_OUTB_NUM	 1
#define PM8001_RESERVED_CCB      176
/* SCSI Queue depth */
#define	PM8001_CAN_QUEUE	 (PM8001_MAX_CCB - PM8001_RESERVED_CCB)
#define PM8001_MAX_HW_SECTORS	 32768  /* Max 512 byte sectors per transfer */

/* unchangeable hardware details */
#define	PM8001_MAX_PHYS		 8	/* max. possible phys */
#define	PM8001_MAX_PORTS	 8	/* max. possible ports */
#define	PM8001_MAX_DEVICES	 1024	/* max supported device */

#define USI_MAX_MEMCNT		 (9 + PM8001_MAX_CCB_ARRAY - 1)
enum memory_region_num {
	AAP1 = 0x0, /* application acceleration processor */
	IOP,	    /* IO processor */
	CI,	    /* consumer index */
	PI,	    /* producer index */
	IB,	    /* inbound queue */
	OB,	    /* outbound queue */
	NVMD,	    /* NVM device */
	DEV_MEM,    /* memory for devices */
	CCB_MEM,    /* memory for command control block */
};
#define	PM8001_EVENT_LOG_SIZE	 (128 * 1024)

/*error code*/
enum mpi_err {
	MPI_IO_STATUS_SUCCESS = 0x0,
	MPI_IO_STATUS_BUSY = 0x01,
	MPI_IO_STATUS_FAIL = 0x02,
};

/**
 * Phy Control constants
 */
enum phy_control_type {
	PHY_LINK_RESET = 0x01,
	PHY_HARD_RESET = 0x02,
	PHY_NOTIFY_ENABLE_SPINUP = 0x10,
};

enum pm8001_hba_info_flags {
	PM8001F_INIT_TIME	= (1U << 0),
	PM8001F_RUN_TIME	= (1U << 1),
};

// #define PMDEBUG 0
#define PMDEBUG 1

#if PMDEBUG > 0
#define PMALLOC(a, b)   pmalloc(a, b, __func__, __LINE__)
#define PMFREE(a, b)    pmfree(a, b, __func__, __LINE__)
extern size_t pmallocation;
static __inline void *pmalloc(size_t, gfp_t, const char *, const int);
static __inline void pmfree(void *, size_t, const char *, const int);

static __inline void *
pmalloc(size_t amt, gfp_t flags, const char *func, const int lno)
{
    void * ptr = kzalloc(amt, flags);
    if (ptr) {
        pmallocation += amt;
#if PMDEBUG > 1
        printk("PMALLOC: %p/%ld from %s:%d (total=0x%lx)\n", ptr, amt, func, lno, pmallocation);
#endif
    }
    return (ptr);
}

static __inline void
pmfree(void *ptr, size_t amt, const char *func, const int lno)
{
    kfree(ptr);
    pmallocation -= amt;
#if PMDEBUG > 1
    printk("PMFREE: %p/%ld from %s:%d (total=0x%lx)\n", ptr, amt, func, lno, pmallocation);
#endif
}

#else
#define PMALLOC         kzalloc
#define PMFREE(a, b)    kfree(a)
#endif
#endif
