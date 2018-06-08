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

#ifndef _PM8001_SAS_H_
#define _PM8001_SAS_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <scsi/scsi.h>
#include <scsi/libsas.h>
#include <scsi/scsi_tcq.h>
#include <scsi/sas_ata.h>
#include <asm/atomic.h>
#include "pm8001_defs.h"

#define DRV_NAME		"pm8001"
#define DRV_MAJ_VERSION "0.1.36"
#define DRV_BUILD_VER "F08.ss13"
#define DRV_VERSION     DRV_MAJ_VERSION "-" DRV_BUILD_VER
#define PM8001_FAIL_LOGGING	0x01 /* Error message logging */
#define PM8001_INIT_LOGGING	0x02 /* driver init logging */
#define PM8001_DISC_LOGGING	0x04 /* discovery layer logging */
#define PM8001_IO_LOGGING	0x08 /* I/O path logging */
#define PM8001_EH_LOGGING	0x10 /* libsas EH function logging*/
#define PM8001_IOCTL_LOGGING	0x20 /* IOCTL message logging */
#define PM8001_MSG_LOGGING	0x40 /* misc message logging */
#define PM8001_MSG_LOGGING2	0x80 /* misc message logging (2) */
#define PM8001_EVT_LOGGING	0x100 /* event logging  */
#define pm8001_printk(format, arg...)	printk(KERN_INFO "%s %d:%s:" format,\
				__func__, __LINE__, DRV_BUILD_VER, ## arg)
#define PM8001_CHECK_LOGGING(HBA, LEVEL, CMD)	\
do {						\
	if (unlikely(HBA->logging_level & LEVEL))	\
		do {					\
			CMD;				\
		} while (0);				\
} while (0);

#define PM8001_EH_DBG(HBA, CMD)			\
	PM8001_CHECK_LOGGING(HBA, PM8001_EH_LOGGING, CMD)

#define PM8001_INIT_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_INIT_LOGGING, CMD)

#define PM8001_DISC_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_DISC_LOGGING, CMD)

#define PM8001_IO_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_IO_LOGGING, CMD)

#define PM8001_FAIL_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_FAIL_LOGGING, CMD)

#define PM8001_IOCTL_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_IOCTL_LOGGING, CMD)

#define PM8001_MSG_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_MSG_LOGGING, CMD)

#define PM8001_MSG_DBG2(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_MSG_LOGGING2, CMD)

#define PM8001_EVT_DBG(HBA, CMD)		\
	PM8001_CHECK_LOGGING(HBA, PM8001_EVT_LOGGING, CMD)

#if (PM8001_MAX_CCB_ARRAY == 1)
#define	FOR_ALL_CCB(ccb)						     \
	for (i = 0; ccb = &pm8001_ha->ccb_info[i], i < PM8001_MAX_CCB; i++)
#else
#define	FOR_ALL_CCB(ccb)						     \
	for (i = 0; ccb = &pm8001_ha->ccb_info[i / PM8001_CCB_PER_ARRAY]     \
	[i % PM8001_CCB_PER_ARRAY],					     \
	i < (PM8001_MAX_CCB_ARRAY * PM8001_CCB_PER_ARRAY); i++)
#endif	

#define PM8001_USE_TASKLET
#define PM8001_USE_MSIX
#define PM8001_READ_VPD

#define DEV_IS_EXPANDER(type)	((type == EDGE_DEV) || (type == FANOUT_DEV))

#define PM8001_NAME_LENGTH		32/* generic length of strings */
extern struct list_head hba_list;
extern const struct pm8001_dispatch pm8001_8001_dispatch;

struct pm8001_hba_info;
struct pm8001_ccb_info;
struct pm8001_device;
/* define task management IU */
struct pm8001_tmf_task {
	u8	tmf;
	u32	tag_of_task_to_be_managed;
};
struct pm8001_ioctl_payload {
	u32	signature;
	u16	major_function;
	u16	minor_function;
	u16	length;
	u16	status;
	u16	offset;
	u16	id;
	u8	*func_specific;
};

struct pm8001_dispatch {
	char *name;
	int (*chip_in_hda_mode)(struct pm8001_hba_info *pm8001_ha);
	int (*chip_hda_mode)(struct pm8001_hba_info *pm8001_ha);
	int (*chip_init)(struct pm8001_hba_info *pm8001_ha);
	int (*chip_soft_rst)(struct pm8001_hba_info *pm8001_ha, u32 signature);
	void (*chip_rst)(struct pm8001_hba_info *pm8001_ha);
	int (*chip_ioremap)(struct pm8001_hba_info *pm8001_ha);
	void (*chip_iounmap)(struct pm8001_hba_info *pm8001_ha);
	irqreturn_t (*isr)(struct pm8001_hba_info *pm8001_ha);
	u32 (*is_our_interupt)(struct pm8001_hba_info *pm8001_ha);
	int (*isr_process_oq)(struct pm8001_hba_info *pm8001_ha);
	void (*interrupt_enable)(struct pm8001_hba_info *pm8001_ha);
	void (*interrupt_disable)(struct pm8001_hba_info *pm8001_ha);
	void (*make_prd)(struct scatterlist *scatter, int nr, void *prd);
	int (*smp_req)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_ccb_info *ccb);
	int (*ssp_io_req)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_ccb_info *ccb);
	int (*sata_req)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_ccb_info *ccb);
	int (*phy_start_req)(struct pm8001_hba_info *pm8001_ha,	u8 phy_id);
	int (*phy_stop_req)(struct pm8001_hba_info *pm8001_ha, u8 phy_id);
	int (*reg_dev_req)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_device *pm8001_dev, u32 flag);
	int (*dereg_dev_req)(struct pm8001_hba_info *pm8001_ha, u32 device_id);
	int (*phy_ctl_req)(struct pm8001_hba_info *pm8001_ha,
		u32 phy_id, u32 phy_op);
	int (*task_abort)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_device *pm8001_dev, u8 flag, u32 task_tag,
		u32 cmd_tag);
	int (*ssp_tm_req)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_ccb_info *ccb, struct pm8001_tmf_task *tmf);
	int (*get_nvmd_req)(struct pm8001_hba_info *pm8001_ha, void *payload);
	int (*set_nvmd_req)(struct pm8001_hba_info *pm8001_ha, void *payload);
	int (*fw_flash_update_req)(struct pm8001_hba_info *pm8001_ha,
		void *payload);
	int (*set_dev_state_req)(struct pm8001_hba_info *pm8001_ha,
		struct pm8001_device *pm8001_dev, u32 state);
	int (*sas_diag_start_end_req)(struct pm8001_hba_info *pm8001_ha,
		u32 state);
	int (*sas_diag_execute_req)(struct pm8001_hba_info *pm8001_ha,
		u32 state);
	int (*sas_re_init_req)(struct pm8001_hba_info *pm8001_ha);
};

struct pm8001_chip_info {
	u32	n_phy;
	const struct pm8001_dispatch	*dispatch;
};
#define PM8001_CHIP_DISP	(pm8001_ha->chip->dispatch)

struct pm8001_port {
	struct asd_sas_port	sas_port;
	u8			port_attached;
	u8			wide_port_phymap;
	u8			port_state;
	struct list_head	list;
};

struct pm8001_phy {
	struct pm8001_hba_info	*pm8001_ha;
	struct pm8001_port	*port;
	struct asd_sas_phy	sas_phy;
	struct sas_identify	identify;
	struct scsi_device	*sdev;
	u64			dev_sas_addr;
	u32			phy_type;
	struct completion	*enable_completion;
	u32			frame_rcvd_size;
	u8			frame_rcvd[32];
	u8			phy_attached;
	u8			phy_state;
	enum sas_linkrate	minimum_linkrate;
	enum sas_linkrate	maximum_linkrate;
};

struct pm8001_device {
	enum sas_dev_type	dev_type;
	struct domain_device	*sas_device;
	u32			attached_phy;
	u32			id;
	struct completion	*dcompletion;
	struct completion	*setds_completion;
	u32			device_id;
	u32			running_req;
	int dying;
	int orej;
};
#define	INC_REQ(d, h)										\
	(d)->running_req++;									\
	PM8001_MSG_DBG2(h, pm8001_printk("%p %u requests now running\n", d, (d)->running_req));	\
	do { ; } while (0)

#define	DEC_REQ(d, h)											\
	if (d) {											\
		BUG_ON((d)->running_req == 0);								\
		(d)->running_req--;									\
		PM8001_MSG_DBG2(h, pm8001_printk("%p %u requests now running\n", d, (d)->running_req));	\
	}												\
	do { } while (0)

struct pm8001_prd_imt {
	__le32			len;
	__le32			e;
};

struct pm8001_prd {
	__le64			addr;		/* 64-bit buffer address */
	struct pm8001_prd_imt	im_len;		/* 64-bit length */
} __attribute__ ((packed));
/*
 * CCB(Command Control Block)
 */
struct pm8001_ccb_info {
	struct list_head	entry;
	struct sas_task		*task;
	u32			n_elem;
	u32			ccb_tag;
	dma_addr_t		ccb_dma_handle;
	struct pm8001_device	*device;
	struct pm8001_prd	buf_prd[PM8001_MAX_DMA_SG];
	struct fw_control_ex	*fw_control_context;
	u32			opCode;
	u8			cmd[60];
	u8			aborting;
	u8			open_retry;
};

struct mpi_mem {
	void			*virt_ptr;
	dma_addr_t		phys_addr;
	u32			phys_addr_hi;
	u32			phys_addr_lo;
	u32			total_len;
	u32			num_elements;
	u32			element_size;
	u32			alignment;
	void *      		real_addr;
	size_t      		real_len;
};

struct mpi_mem_req {
	/* The number of element in the  mpiMemory array */
	u32			count;
	/* The array of structures that define memroy regions*/
	struct mpi_mem		region[USI_MAX_MEMCNT];
};

struct main_cfg_table {
	u32			signature;
	u32			interface_rev;
	u32			firmware_rev;
	u32			max_out_io;
	u32			max_sgl;
	u32			ctrl_cap_flag;
	u32			gst_offset;
	u32			inbound_queue_offset;
	u32			outbound_queue_offset;
	u32			inbound_q_nppd_hppd;
	u32			outbound_hw_event_pid0_3;
	u32			outbound_hw_event_pid4_7;
	u32			outbound_ncq_event_pid0_3;
	u32			outbound_ncq_event_pid4_7;
	u32			outbound_tgt_ITNexus_event_pid0_3;
	u32			outbound_tgt_ITNexus_event_pid4_7;
	u32			outbound_tgt_ssp_event_pid0_3;
	u32			outbound_tgt_ssp_event_pid4_7;
	u32			outbound_tgt_smp_event_pid0_3;
	u32			outbound_tgt_smp_event_pid4_7;
	u32			upper_event_log_addr;
	u32			lower_event_log_addr;
	u32			event_log_size;
	u32			event_log_option;
	u32			upper_iop_event_log_addr;
	u32			lower_iop_event_log_addr;
	u32			iop_event_log_size;
	u32			iop_event_log_option;
	u32			fatal_err_interrupt;
	u32			fatal_err_dump_offset0;
	u32			fatal_err_dump_length0;
	u32			fatal_err_dump_offset1;
	u32			fatal_err_dump_length1;
	u32			hda_mode_flag;
	u32			analog_setup_table_offset;
};
struct general_status_table {
	u32			gst_len_mpistate;
	u32			iq_freeze_state0;
	u32			iq_freeze_state1;
	u32			msgu_tcnt;
	u32			iop_tcnt;
	u32			reserved;
	u32			phy_state[8];
	u32			reserved1;
	u32			reserved2;
	u32			reserved3;
	u32			recover_err_info[8];
};
enum t_mode_5_2 {
	t_mode_5_2_430mV,
	t_mode_5_2_480mV,
	t_mode_5_2_530mV,
	t_mode_5_2_580mV,
	t_mode_5_2_620mV,
	t_mode_5_2_670mV,
	t_mode_5_2_720mV,
	t_mode_5_2_770mV,
	t_mode_5_2_820mV,
	t_mode_5_2_860mV,
	t_mode_5_2_910mV,
	t_mode_5_2_1010mV,
	t_mode_5_2_1100mV,
	t_mode_5_2_1200mV,
	t_mode_5_2_1350mV,
	t_mode_5_2_1500mV
};
enum t_mode_9_6 {
	t_mode_9_6_0dB,
	t_mode_9_6_0_8dB,
	t_mode_9_6_1_6dB,
	t_mode_9_6_2_5dB,
	t_mode_9_6_3_5dB,
	t_mode_9_6_4_7dB,
	t_mode_9_6_6_0dB,
	t_mode_9_6_7_6dB,
	t_mode_9_6_9_5dB,
	t_mode_9_6_12_0dB,
	t_mode_9_6_15_6dB,
	t_mode_9_6_21_6dB,
	t_mode_9_6_squelch
};
enum t_ctrl_7_6 {
	t_ctrl_7_6_100uA,
	t_ctrl_7_6_80uA,
	t_ctrl_7_6_70uA,
	t_ctrl_7_6_60uA,
	t_ctrl_7_6_90uA = t_ctrl_7_6_100uA,
	t_ctrl_7_6_110uA = t_ctrl_7_6_80uA,
	t_ctrl_7_6_120uA = t_ctrl_7_6_70uA,
	t_ctrl_7_6_130uA = t_ctrl_7_6_60uA
};
enum t_mode_1_0 {
	t_mode_1_0_57ps,
	t_mode_1_0_68ps,
	t_mode_1_0_82ps,
	t_mode_1_0_97ps
};
enum t_mode_15 {
	t_mode_15_100uA = 0,
	t_mode_15_80uA = 0,
	t_mode_15_70uA = 0,
	t_mode_15_60uA = 0,
	t_mode_15_90uA = 1,
	t_mode_15_110uA = 1,
	t_mode_15_120uA = 1,
	t_mode_15_130uA = 1
};
struct analog_setup_table {
	struct per_phy_analog_setup_table {
		struct tx_g1 {
			u32	t_mode_5_2_sas_g1:4;
			u32	t_mode_9_6_sas_g1:4;
			u32	t_ctrl_5_4_sas_g1:2;
			u32	t_ctrl_7_6_sas_g1:2;
			u32	trs_preen_sas_g1:1;
			u32	trs_sscen_sas_g1:1;
			u32	t_mode_13_12_sas_g1:2;
			u32	t_mode_5_2_sata_g1:4;
			u32	t_mode_9_6_sata_g1:4;
			u32	t_ctrl_5_4_sata_g1:2;
			u32	t_ctrl_7_6_sata_g1:2;
			u32	trs_preen_sata_g1:1;
			u32	trs_sscen_sata_g1:1;
			u32	t_mode_13_12_sata_g1:2;
		}		tx_g1;
		struct tx_g2 {
			u32	t_mode_5_2_sas_g2:4;
			u32	t_mode_9_6_sas_g2:4;
			u32	t_ctrl_5_4_sas_g2:2;
			u32	t_ctrl_7_6_sas_g2:2;
			u32	trs_preen_sas_g2:1;
			u32	trs_sscen_sas_g2:1;
			u32	t_mode_13_12_sas_g2:2;
			u32	t_mode_5_2_sata_g2:4;
			u32	t_mode_9_6_sata_g2:4;
			u32	t_ctrl_5_4_sata_g2:2;
			u32	t_ctrl_7_6_sata_g2:2;
			u32	trs_preen_sata_g2:1;
			u32	trs_sscen_sata_g2:1;
			u32	t_mode_13_12_sata_g2:2;
		}		tx_g2;
		struct tx_g3 {
			u32	t_mode_5_2_sas_g3:4;
			u32	t_mode_9_6_sas_g3:4;
			u32	t_ctrl_5_4_sas_g3:2;
			u32	t_ctrl_7_6_sas_g3:2;
			u32	trs_preen_sas_g3:1;
			u32	trs_sscen_sas_g3:1;
			u32	t_mode_13_12_sas_g3:2;
			u32	t_mode_5_2_sata_g3:4;
			u32	t_mode_9_6_sata_g3:4;
			u32	t_ctrl_5_4_sata_g3:2;
			u32	t_ctrl_7_6_sata_g3:2;
			u32	trs_preen_sata_g3:1;
			u32	trs_sscen_sata_g3:1;
			u32	t_mode_13_12_sata_g3:2;
		}		tx_g3;
		struct tx_cfg1 {
			u32	t_mode_1_0_sas_g1:2;
			u32	t_mode_1_0_sas_g2:2;
			u32	t_mode_1_0_sas_g3:2;
			u32	reserved1:2;
			u32	t_mode_1_0_sata_g1:2;
			u32	t_mode_1_0_sata_g2:2;
			u32	t_mode_1_0_sata_g3:2;
			u32	reserved2:2;
			u32	reserved3:2;
			u32	reserved4:1;
			u32	reserved5:1;
			u32	t_mode_15_sas_g1:1;
			u32	t_mode_15_sas_g2:1;
			u32	t_mode_15_sas_g3:1;
			u32	reserved6:1;
			u32	t_mode_15_sata_g1:1;
			u32	t_mode_15_sata_g2:1;
			u32	t_mode_15_sata_g3:1;
			u32	reserved7:1;
			u32	reserved8:4;
		}		tx_cfg1;
		struct rx_g1g2 {
			u32	reserved1:1;
			u32	r_rxmode_8_5_sas_g1g2:4;
			u32	r_rxmode_12_9_sas_g1g2:4;
			u32	r_rxmode_14_13_sas_g1g2:2;
			u32	reserved2:1;
			u32	reserved3:1;
			u32	reserved4:3;
			u32	reserved5:1;
			u32	r_rxmode_8_5_sata_g1g2:4;
			u32	r_rxmode_12_9_sata_g1t2:4;
			u32	r_rxmode_14_13_sata_g1g2:2;
			u32	reserved6:1;
			u32	reserved7:1;
			u32	reserved8:3;
		}		rx_g1g2;
		struct rx_g3 {
			u32	reserved1:1;
			u32	r_rxmode_8_5_sas_g3:4;
			u32	r_rxmode_12_9_sas_g3:4;
			u32	r_rxmode_14_13_sas_g3:2;
			u32	trs_mode_0_sas_g3:1;
			u32	trs_mode_0_sas2:1;
			u32	dfe_mode_2_0_sas_g3:3;
			u32	reserved2:1;
			u32	r_rxmode_8_5_sata_g3:4;
			u32	r_rxmode_12_9_sata_g1t2:4;
			u32	r_rxmode_14_13_sata_g3:2;
			u32	trs_mode_0_sata_g3:1;
			u32	trs_mode_4_sas2:1;
			u32	reserved3:3;
		}		rx_g3;
		struct rx_cfg1 {
			u32	r_rxmode_3_0:4;
			u32	reserved1:11;
			u32	reserved2:6;
			u32	reserved3:3;
			u32	reserved4:1;
			u32	reserved5:7;
		}		rx_cfg1;
		struct rx_cfg2 {
			u32	r_losmode_3_0_sas:4;
			u32	r_losmode_3_0_sata:4;
			u32	r_losmode_11_4:8;
			u32	reserved1:8;
			u32	r_enb:1;
			u32	r_atmsb:1;
			u32	reserved2:6;
		}		rx_cfg2;
		u32		reserved1;
		u32		reserved2;
	}	per_phy[10];
};
struct inbound_queue_table {
	u32			element_pri_size_cnt;
	u32			upper_base_addr;
	u32			lower_base_addr;
	u32			ci_upper_base_addr;
	u32			ci_lower_base_addr;
	u32			pi_pci_bar;
	u32			pi_offset;
	u32			total_length;
	void			*base_virt;
	void			*ci_virt;
	u32			reserved;
	__le32			consumer_index;
	u32			producer_idx;
};
struct outbound_queue_table {
	u32			element_size_cnt;
	u32			upper_base_addr;
	u32			lower_base_addr;
	void			*base_virt;
	u32			pi_upper_base_addr;
	u32			pi_lower_base_addr;
	u32			ci_pci_bar;
	u32			ci_offset;
	u32			total_length;
	void			*pi_virt;
	u32			interrup_vec_cnt_delay;
	u32			dinterrup_to_pci_offset;
	__le32			producer_index;
	u32			consumer_idx;
};
struct eventlog_header {
	__le32			signature;
#define EVENTLOG_HEADER_SIGNATURE_AAP1 0x1234AAAA
#define EVENTLOG_HEADER_SIGNATURE_IOP 0x5678CCCC
	__le32			offset; /* sizeof(eventlog_header) */
	u32			reserved1;
	__le32			size;
	u32			reserved2;
	__le32			consumer_index;
	__le32			producer_index;
	__le32			entry_size;
};
struct eventlog_entry {
	u8			size:3;
	u8			reserved1:5;
	u8			reserved2[2];
	u8			reserved:4;
	u8			severity:4;
	__le32			timestamp_upper;
	__le32			timestamp_lower;
	__le32			sequence;
	__le32			log[4];
};
struct pm8001_hba_memspace {
	void __iomem  		*memvirtaddr;
	u64			membase;
	u32			memsize;
};
struct pm8001_hba_info {
	char			name[PM8001_NAME_LENGTH];
	struct list_head	list;
	unsigned long		flags;
	spinlock_t		lock;/* host-wide lock */
	struct pci_dev		*pdev;/* our device */
	struct device		*dev;
	struct pm8001_hba_memspace io_mem[6];
	struct mpi_mem_req	memoryMap;
	void __iomem	*msg_unit_tbl_addr;/*Message Unit Table Addr*/
	void __iomem	*main_cfg_tbl_addr;/*Main Config Table Addr*/
	void __iomem	*general_stat_tbl_addr;/*General Status Table Addr*/
	void __iomem	*inbnd_q_tbl_addr;/*Inbound Queue Config Table Addr*/
	void __iomem	*outbnd_q_tbl_addr;/*Outbound Queue Config Table Addr*/
	struct main_cfg_table	main_cfg_tbl;
	struct general_status_table	gs_tbl;
	struct inbound_queue_table	inbnd_q_tbl[PM8001_MAX_INB_NUM];
	struct outbound_queue_table	outbnd_q_tbl[PM8001_MAX_OUTB_NUM];
	u8			sas_addr[PM8001_MAX_PHYS][SAS_ADDR_SIZE];
	u64			sas_addr_def[PM8001_MAX_PHYS];
	u8			sas_addr_set;
	struct sas_ha_struct	*sas;/* SCSI/SAS glue */
	struct Scsi_Host	*shost;
	u32			chip_id;
	const struct pm8001_chip_info	*chip;
	struct completion	*nvmd_completion;
	u16			tags_serno;
	int			tags_alloc;
	int			tags_num;
	unsigned long		*tags;
#define	TAG_IDX_MASK(x)	(x & 0xffff)
#define	TAG_MAKE(h, t)	((((((h)->tags_serno++) & 0x7fff) | 0x8000) << 16) | t)
	struct pm8001_phy	phy[PM8001_MAX_PHYS];
	struct pm8001_port	port[PM8001_MAX_PHYS];
	u32			id;
	u32			irq;
	struct pm8001_device	*devices;
#if (PM8001_MAX_CCB_ARRAY == 1)
	struct pm8001_ccb_info	*ccb_info;
#else
	struct pm8001_ccb_info	*ccb_info[PM8001_MAX_CCB_ARRAY];	
#endif	
#ifdef PM8001_USE_MSIX
	struct msix_entry	msix_entries[16];/*for msi-x interrupt*/
	int			number_of_intr;/*will be used in remove()*/
#endif
#ifdef PM8001_USE_TASKLET
	struct tasklet_struct	tasklet;
#endif
	u32			brcvd;
	u32			logging_level;
	u32			logging_option;
	u32			fw_status;
	const struct firmware 	*fw_image;
	u32			rst_signature;
#ifdef _CONFIG_SCSI_PM8001_DEBUG_FS
# undef CONFIG_SCSI_PM8001_DEBUG_FS
# define CONFIG_SCSI_PM8001_DEBUG_FS
#endif
#ifndef CONFIG_DEBUG_FS
# undef CONFIG_SCSI_PM8001_DEBUG_FS
#endif
#ifdef CONFIG_SCSI_PM8001_DEBUG_FS
	struct dentry		*hba_debugfs_root;
#endif
	/* Local consumer indexes in support of sysfs event log node */
	u32			aap1_consumer;
	u32			iop_consumer;
};

struct pm8001_work {
	struct work_struct work;
	struct pm8001_hba_info *pm8001_ha;
	void *data;
	int handler;
};

struct pm8001_fw_image_header {
	u8 vender_id[8];
	u8 product_id;
	u8 hardware_rev;
	u8 dest_partition;
	u8 reserved;
	u8 fw_rev[4];
	__be32  image_length;
	__be32 image_crc;
	__be32 startup_entry;
} __attribute__((packed, aligned(4)));


/**
 * FW Flash Update status values
 */
#define FLASH_UPDATE_COMPLETE_PENDING_REBOOT	0x00
#define FLASH_UPDATE_IN_PROGRESS		0x01
#define FLASH_UPDATE_HDR_ERR			0x02
#define FLASH_UPDATE_OFFSET_ERR			0x03
#define FLASH_UPDATE_CRC_ERR			0x04
#define FLASH_UPDATE_LENGTH_ERR			0x05
#define FLASH_UPDATE_HW_ERR			0x06
#define FLASH_UPDATE_DNLD_NOT_SUPPORTED		0x10
#define FLASH_UPDATE_DISABLED			0x11

/**
 * brief param structure for firmware flash update.
 */
struct fw_flash_updata_info {
	u32			cur_image_offset;
	u32			cur_image_len;
	u32			total_image_len;
	struct pm8001_prd	sgl;
};

struct fw_control_info {
	u32			retcode;/*ret code (status)*/
	u32			phase;/*ret code phase*/
	u32			phaseCmplt;/*percent complete for the current
	update phase */
	u32			version;/*Hex encoded firmware version number*/
	u32			offset;/*Used for downloading firmware	*/
	u32			len; /*len of buffer*/
	u32			size;/* Used in OS VPD and Trace get size
	operations.*/
	u32			reserved;/* padding required for 64 bit
	alignment */
	u8			buffer[1];/* Start of buffer */
};
struct fw_control_ex {
	struct fw_control_info *fw_control;
	void			*buffer;/* keep buffer pointer to be
	freed when the response comes*/
	void			*virtAddr;/* keep virtual address of the data */
	void			*usrAddr;/* keep virtual address of the
	user data */
	dma_addr_t		phys_addr;
	u32			len; /* len of buffer  */
	void			*payload; /* pointer to IOCTL Payload */
	u8			inProgress;/*if 1 - the IOCTL request is in
	progress */
	void			*param1;
	void			*param2;
	void			*param3;
};

/* pm8001 workqueue */
extern struct workqueue_struct *pm8001_wq;

/* Find the ccb array */
static __inline struct pm8001_ccb_info *get_ccb_array(
				struct pm8001_hba_info *pm8001_ha, u32 tag);
static __inline struct pm8001_ccb_info *get_ccb_array(
				struct pm8001_hba_info *pm8001_ha, u32 tag)
{
	struct pm8001_ccb_info *ccb;
#if (PM8001_MAX_CCB_ARRAY == 1)
	ccb = &pm8001_ha->ccb_info[TAG_IDX_MASK(tag)];
#else
	u32 array_index, tag_index;

	/* Finding the array of ccb */
	array_index = ((TAG_IDX_MASK(tag)) / PM8001_CCB_PER_ARRAY);
	/* Finding the index in the ccb array*/
	tag_index = ((TAG_IDX_MASK(tag)) % PM8001_CCB_PER_ARRAY);
	ccb = &pm8001_ha->ccb_info[array_index][tag_index];
#endif
	return ccb;
}

/******************** function prototype *********************/
void pm8001_tag_free(struct pm8001_hba_info *pm8001_ha, u32 tag);
int pm8001_tag_alloc(struct pm8001_hba_info *pm8001_ha, u32 *tag_out);
void pm8001_tag_init(struct pm8001_hba_info *pm8001_ha);
u32 pm8001_get_ncq_tag(struct sas_task *task, u32 *tag);
void pm8001_ccb_free(struct pm8001_hba_info *pm8001_ha, u32 ccb_idx);
void pm8001_ccb_task_free(struct pm8001_hba_info *pm8001_ha,
	struct sas_task *task, struct pm8001_ccb_info *ccb, u32 ccb_idx);
int pm8001_phy_control(struct asd_sas_phy *sas_phy, enum phy_func func
	PMCS_FUNCDATA_ARG);
int pm8001_slave_alloc(struct scsi_device *scsi_dev);
int pm8001_slave_configure(struct scsi_device *sdev);
void pm8001_scan_start(struct Scsi_Host *shost);
int pm8001_scan_finished(struct Scsi_Host *shost, unsigned long time);
int pm8001_queue_command(struct sas_task *task, const int num,
	PMCS_GFP_T gfp_flags);
int pm8001_abort_task(struct sas_task *task);
int pm8001_abort_task_set(struct domain_device *dev, u8 *lun);
int pm8001_clear_aca(struct domain_device *dev, u8 *lun);
int pm8001_clear_task_set(struct domain_device *dev, u8 *lun);
int pm8001_dev_found(struct domain_device *dev);
void pm8001_dev_gone(struct domain_device *dev);
int pm8001_lu_reset(struct domain_device *dev, u8 *lun);
int pm8001_I_T_nexus_reset(struct domain_device *dev);
int pm8001_query_task(struct sas_task *task);
void pm8001_open_reject_retry(
	struct pm8001_hba_info *pm8001_ha,
	struct sas_task *task_to_close,
	struct pm8001_device *device_to_close);
int pm8001_eh_bus_reset_handler(struct scsi_cmnd *cmnd);
int pm8001_eh_host_reset_handler(struct scsi_cmnd *cmnd);
int pm8001_clear_nexus_ha(struct sas_ha_struct *ha);
int pm8001_mem_alloc(struct pci_dev *pdev, void **virt_addr,
	dma_addr_t *pphys_addr, u32 *pphys_addr_hi, u32 *pphys_addr_lo,
	u32 mem_size, u32 align, void **real_va, size_t *real_len);
void pm8001_update_main_config_table(struct pm8001_hba_info *pm8001_ha);
int pm8001_readlog(
	struct eventlog_header *header,
	struct eventlog_entry *entry,
	u32 *consumer_index);
void pm8001_debugfs_initialize(struct pm8001_hba_info *pm8001_ha);
void pm8001_debugfs_terminate(struct pm8001_hba_info *pm8001_ha);
int pm8001_bar4_shift(struct pm8001_hba_info *pm8001_ha, u32 shiftValue);

/* ctl shared API */
extern struct PMCS_SYSFS_DEV_ATTR *pm8001_host_attrs[];

#endif

