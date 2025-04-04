/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_CVP_INTERNAL_H_
#define _MSM_CVP_INTERNAL_H_

#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/interconnect.h>
#include <linux/kref.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include "msm_cvp_core.h"
#include <media/msm_eva_private.h>
#include "cvp_hfi_api.h"
#include "cvp_hfi_helper.h"

#define MAX_SUPPORTED_INSTANCES 16
#define MAX_CV_INSTANCES 8
#define MAX_DMM_INSTANCES 8
#define MAX_DEBUGFS_NAME 50
#define MAX_DSP_INIT_ATTEMPTS 16
#define FENCE_WAIT_SIGNAL_TIMEOUT 100
#define FENCE_WAIT_SIGNAL_RETRY_TIMES 20
#define FENCE_BIT (1ULL << 63)

#define FENCE_DMM_ICA_ENABLED_IDX 0
#define FENCE_DMM_DS_IDX 1
#define FENCE_DMM_OUTPUT_IDX 7

#define SYS_MSG_START HAL_SYS_INIT_DONE
#define SYS_MSG_END HAL_SYS_ERROR
#define SESSION_MSG_START HAL_SESSION_EVENT_CHANGE
#define SESSION_MSG_END HAL_SESSION_ERROR
#define SYS_MSG_INDEX(__msg) (__msg - SYS_MSG_START)
#define SESSION_MSG_INDEX(__msg) (__msg - SESSION_MSG_START)

#define SESSION_NAME_MAX_LEN 256

#define ARP_BUF_SIZE 0x300000

#define CVP_RT_PRIO_THRESHOLD 1

#define MAX_CVP_ERROR_COUNT 65535

struct msm_cvp_inst;

enum cvp_core_state {
	CVP_CORE_UNINIT = 0,
	CVP_CORE_INIT,
	CVP_CORE_INIT_DONE,
};

enum instance_state {
	MSM_CVP_CORE_UNINIT_DONE = 0x0001,
	MSM_CVP_CORE_INIT,
	MSM_CVP_CORE_INIT_DONE,
	MSM_CVP_OPEN,
	MSM_CVP_OPEN_DONE,
	MSM_CVP_CLOSE,
	MSM_CVP_CLOSE_DONE,
	MSM_CVP_CORE_UNINIT,
	MSM_CVP_CORE_INVALID
};

enum dsp_state {
	DSP_INVALID,
	DSP_UNINIT,
	DSP_PROBED,
	DSP_READY,
	DSP_SUSPEND,
	DSP_INACTIVE,
};

struct msm_cvp_common_data {
	char key[128];
	int value;
};

enum sku_version {
	SKU_VERSION_0 = 0,
	SKU_VERSION_1,
	SKU_VERSION_2,
};

enum vpu_version {
	VPU_VERSION_4 = 1,
	VPU_VERSION_5,
};

enum cvp_session_state {
	SESSION_NORMAL = 0x00,
	SESSION_ERROR,
	SECURE_SESSION_ERROR,
};

enum cvp_session_errorcode {
	NO_ERROR = 0x00,
	EVA_SYS_ERROR,
	EVA_SESSION_ERROR,
	EVA_SECURE_SESSION_ERROR,
	EVA_SESSION_TIMEOUT
};

struct msm_cvp_ubwc_config_data {
	struct {
		u32 max_channel_override : 1;
		u32 mal_length_override : 1;
		u32 hb_override : 1;
		u32 bank_swzl_level_override : 1;
		u32 bank_spreading_override : 1;
		u32 reserved : 27;
	} override_bit_info;

	u32 max_channels;
	u32 mal_length;
	u32 highest_bank_bit;
	u32 bank_swzl_level;
	u32 bank_spreading;
};

struct msm_cvp_qos_setting {
	u32 axi_qos;
	u32 prioritylut_low;
	u32 prioritylut_high;
	u32 urgency_low;
	u32 urgency_low_ro;
	u32 dangerlut_low;
	u32 safelut_low;
};

struct msm_cvp_platform_data {
	struct msm_cvp_common_data *common_data;
	unsigned int common_data_length;
	unsigned int sku_version;
	uint32_t vpu_ver;
	unsigned int vm_id;	/* pvm: 1; tvm: 2 */
	struct msm_cvp_ubwc_config_data *ubwc_config;
	struct msm_cvp_qos_setting *noc_qos;
};

struct cvp_kmem_cache {
	struct kmem_cache *cache;
	atomic_t nr_objs;
};

struct msm_cvp_drv {
	struct mutex lock;
	struct msm_cvp_core *cvp_core;
	struct dentry *debugfs_root;
	int thermal_level;
	u32 sku_version;
	struct cvp_kmem_cache msg_cache;
	struct cvp_kmem_cache frame_cache;
	struct cvp_kmem_cache buf_cache;
	struct cvp_kmem_cache smem_cache;
	char fw_version[CVP_VERSION_LENGTH];
};

enum profiling_points {
	SYS_INIT = 0,
	SESSION_INIT,
	LOAD_RESOURCES,
	FRAME_PROCESSING,
	FW_IDLE,
	MAX_PROFILING_POINTS,
};

struct cvp_clock_data {
	int buffer_counter;
	int load;
	int load_low;
	int load_norm;
	int load_high;
	int min_threshold;
	int max_threshold;
	unsigned long bitrate;
	unsigned long min_freq;
	unsigned long curr_freq;
	u32 ddr_bw;
	u32 sys_cache_bw;
	u32 operating_rate;
	bool low_latency_mode;
	bool turbo_mode;
};

struct cvp_profile_data {
	int start;
	int stop;
	int cumulative;
	char name[64];
	int sampling;
	int average;
};

struct msm_cvp_debug {
	struct cvp_profile_data pdata[MAX_PROFILING_POINTS];
	int profile;
	int samples;
};

enum msm_cvp_modes {
	CVP_SECURE = BIT(0),
	CVP_TURBO = BIT(1),
	CVP_THUMBNAIL = BIT(2),
	CVP_LOW_POWER = BIT(3),
	CVP_REALTIME = BIT(4),
};

#define MAX_NUM_MSGS_PER_SESSION	128
#define CVP_MAX_WAIT_TIME	2000

struct cvp_session_msg {
	struct list_head node;
	struct cvp_hfi_msg_session_hdr_ext pkt;
};

struct cvp_session_queue {
	spinlock_t lock;
	enum queue_state state;
	unsigned int msg_count;
	struct list_head msgs;
	wait_queue_head_t wq;
};

struct cvp_session_prop {
	u32 type;
	u32 kernel_mask;
	u32 priority;
	u32 is_secure;
	u32 dsp_mask;
	u32 fthread_nr;
	u32 cycles[HFI_MAX_HW_THREADS];
	u32 fw_cycles;
	u32 op_cycles[HFI_MAX_HW_THREADS];
	u32 fw_op_cycles;
	u32 ddr_bw;
	u32 ddr_op_bw;
	u32 ddr_cache;
	u32 ddr_op_cache;
	u32 fps[HFI_MAX_HW_THREADS];
	u32 dump_offset;
	u32 dump_size;
	char session_name[SESSION_NAME_MAX_LEN];
};

enum cvp_event_t {
	CVP_NO_EVENT,
	EVA_EVENT = 1,
	CVP_SYS_ERROR_EVENT,
	CVP_MAX_CLIENTS_EVENT,
	CVP_HW_UNSUPPORTED_EVENT,
	CVP_INVALID_EVENT,
	CVP_DUMP_EVENT,
};

struct cvp_session_event {
	spinlock_t lock;
	enum cvp_event_t event;
	wait_queue_head_t wq;
};

#define MAX_ENTRIES 64

struct smem_data {
	u32 size;
	u32 flags;
	u32 device_addr;
	u32 bitmap_index;
	u32 refcount;
	u32 pkt_type;
	u32 buf_idx;
};

struct cvp_buf_data {
	u32 device_addr;
	u32 size;
};

struct inst_snapshot {
	void *session;
	u32 smem_index;
	u32 dsp_index;
	u32 persist_index;
	struct smem_data smem_log[MAX_ENTRIES];
	struct cvp_buf_data dsp_buf_log[MAX_ENTRIES];
	struct cvp_buf_data persist_buf_log[MAX_ENTRIES];
};

struct cvp_noc_log {
	u32 used;
	u32 err_ctrl_swid_low;
	u32 err_ctrl_swid_high;
	u32 err_ctrl_mainctl_low;
	u32 err_ctrl_errvld_low;
	u32 err_ctrl_errclr_low;
	u32 err_ctrl_errlog0_low;
	u32 err_ctrl_errlog0_high;
	u32 err_ctrl_errlog1_low;
	u32 err_ctrl_errlog1_high;
	u32 err_ctrl_errlog2_low;
	u32 err_ctrl_errlog2_high;
	u32 err_ctrl_errlog3_low;
	u32 err_ctrl_errlog3_high;
	u32 err_core_swid_low;
	u32 err_core_swid_high;
	u32 err_core_mainctl_low;
	u32 err_core_errvld_low;
	u32 err_core_errclr_low;
	u32 err_core_errlog0_low;
	u32 err_core_errlog0_high;
	u32 err_core_errlog1_low;
	u32 err_core_errlog1_high;
	u32 err_core_errlog2_low;
	u32 err_core_errlog2_high;
	u32 err_core_errlog3_low;
	u32 err_core_errlog3_high;
	u32 arp_test_bus[16];
	u32 dma_test_bus[512];
};

struct cvp_debug_log {
	struct cvp_noc_log noc_log;
	u32 snapshot_index;
	struct inst_snapshot snapshot[16];
};

struct msm_cvp_core {
	struct mutex lock;
	struct mutex clk_lock;
	dev_t dev_num;
	struct cdev cdev;
	struct class *class;
	struct device *dev;
	struct cvp_hfi_ops *dev_ops;
	struct msm_cvp_platform_data *platform_data;
	struct msm_cvp_synx_ops *synx_ftbl;
	struct list_head instances;
	struct dentry *debugfs_root;
	enum cvp_core_state state;
	struct completion completions[SYS_MSG_END - SYS_MSG_START + 1];
	enum msm_cvp_hfi_type hfi_type;
	struct msm_cvp_platform_resources resources;
	struct msm_cvp_capability *capabilities;
	struct delayed_work fw_unload_work;
	struct work_struct ssr_work;
	enum hal_ssr_trigger_type ssr_type;
	u32 soc_version;
	u32 smmu_fault_count;
	u32 last_fault_addr;
	u32 ssr_count;
	u32 smem_leak_count;
	bool trigger_ssr;
	unsigned long curr_freq;
	unsigned long orig_core_sum;
	unsigned long bw_sum;
	atomic64_t kernel_trans_id;
	struct cvp_debug_log log;
};

struct msm_cvp_inst {
	struct list_head list;
	struct list_head dsp_list;
	struct mutex sync_lock, lock;
	struct msm_cvp_core *core;
	enum session_type session_type;
	u32 dsp_handle;
	struct task_struct *task;
	atomic_t smem_count;
	struct cvp_session_queue session_queue;
	struct cvp_session_queue session_queue_fence;
	struct cvp_session_event event_handler;
	void *session;
	enum instance_state state;
	struct msm_cvp_list freqs;
	struct msm_cvp_list persistbufs;
	struct cvp_dmamap_cache dma_cache;
	struct msm_cvp_list cvpdspbufs;
	struct msm_cvp_list cvpwnccbufs;
	struct msm_cvp_list frames;
	struct cvp_frame_bufs last_frame;
	struct cvp_frame_bufs unused_dsp_bufs;
	struct cvp_frame_bufs unused_wncc_bufs;
	u32 cvpwnccbufs_num;
	struct msm_cvp_wncc_buffer* cvpwnccbufs_table;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct dentry *debugfs_root;
	struct msm_cvp_debug debug;
	struct cvp_clock_data clk_data;
	enum msm_cvp_modes flags;
	struct msm_cvp_capability capability;
	struct kref kref;
	struct cvp_session_prop prop;
	/* error_code will be cleared after being returned to user mode */
	u32 hfi_error_code;
	/* prev_error_code saves value of error_code before it's cleared */
	u32 prev_hfi_error_code;
	/* Stores error codes of enum 'cvp_session_errorcode' queried by UMD using IOCTL  */
	u32 session_error_code;
	struct synx_session *synx_session_id;
	struct cvp_fence_queue fence_cmd_queue;
	char proc_name[TASK_COMM_LEN];
};

extern struct msm_cvp_drv *cvp_driver;

void cvp_handle_cmd_response(enum hal_command_response cmd, void *data);
int msm_cvp_trigger_ssr(struct msm_cvp_core *core,
	enum hal_ssr_trigger_type type);
int msm_cvp_noc_error_info(struct msm_cvp_core *core);
void msm_cvp_comm_handle_thermal_event(void);

void msm_cvp_ssr_handler(struct work_struct *work);
/*
 * XXX: normally should be in msm_cvp_core.h, but that's meant for public APIs,
 * whereas this is private
 */
int msm_cvp_destroy(struct msm_cvp_inst *inst);
void *cvp_get_drv_data(struct device *dev);
void *cvp_kmem_cache_zalloc(struct cvp_kmem_cache *k, gfp_t flags);
void cvp_kmem_cache_free(struct cvp_kmem_cache *k, void *obj);
bool msm_cvp_check_for_inst_overload(struct msm_cvp_core *core, u32 *instance_count);
#endif
