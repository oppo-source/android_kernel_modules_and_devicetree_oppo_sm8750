/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _KGSL_SNAPSHOT_H_
#define _KGSL_SNAPSHOT_H_

#include <linux/types.h>

/* Snapshot header */

/* High word is static, low word is snapshot version ID */
#define SNAPSHOT_MAGIC 0x504D0002

#define DEBUG_SECTION_SZ(_dwords) (((_dwords) * sizeof(unsigned int)) \
				   + sizeof(struct kgsl_snapshot_debug))

/* GPU ID scheme:
 * [16:31] - core identifer (0x0002 for 2D or 0x0003 for 3D)
 * [00:16] - GPU specific identifier
 */

struct kgsl_snapshot_header {
	__u32 magic; /* Magic identifier */
	__u32 gpuid; /* GPU ID - see above */
	/* Added in snapshot version 2 */
	__u32 chipid; /* Chip ID from the GPU */
} __packed;

/* Section header */
#define SNAPSHOT_SECTION_MAGIC 0xABCD

struct kgsl_snapshot_section_header {
	__u16 magic; /* Magic identifier */
	__u16 id;    /* Type of section */
	__u32 size;  /* Size of the section including this header */
} __packed;

/* Section identifiers */
#define KGSL_SNAPSHOT_SECTION_OS           0x0101
#define KGSL_SNAPSHOT_SECTION_REGS         0x0201
#define KGSL_SNAPSHOT_SECTION_REGS_V2      0x0202
#define KGSL_SNAPSHOT_SECTION_RB_V2        0x0302
#define KGSL_SNAPSHOT_SECTION_IB_V2        0x0402
#define KGSL_SNAPSHOT_SECTION_INDEXED_REGS 0x0501
#define KGSL_SNAPSHOT_SECTION_INDEXED_REGS_V2 0x0502
#define KGSL_SNAPSHOT_SECTION_DEBUG        0x0901
#define KGSL_SNAPSHOT_SECTION_DEBUGBUS     0x0A01
#define KGSL_SNAPSHOT_SECTION_GPU_OBJECT_V2 0x0B02
#define KGSL_SNAPSHOT_SECTION_MEMLIST_V2   0x0E02
#define KGSL_SNAPSHOT_SECTION_SHADER       0x1201
#define KGSL_SNAPSHOT_SECTION_SHADER_V2    0x1202
#define KGSL_SNAPSHOT_SECTION_SHADER_V3    0x1203
#define KGSL_SNAPSHOT_SECTION_MVC          0x1501
#define KGSL_SNAPSHOT_SECTION_MVC_V2       0x1502
#define KGSL_SNAPSHOT_SECTION_MVC_V3       0x1503
#define KGSL_SNAPSHOT_SECTION_GMU_MEMORY   0x1701
#define KGSL_SNAPSHOT_SECTION_SIDE_DEBUGBUS 0x1801
#define KGSL_SNAPSHOT_SECTION_TRACE_BUFFER 0x1901
#define KGSL_SNAPSHOT_SECTION_EVENTLOG     0x1A01

#define KGSL_SNAPSHOT_SECTION_END          0xFFFF

/* OS sub-section header */
#define KGSL_SNAPSHOT_OS_LINUX_V4          0x00000203

/* Linux OS specific information */
struct kgsl_snapshot_linux_v4 {
	int osid;		/* subsection OS identifier */
	__u32 seconds;		/* Unix timestamp for the snapshot */
	__u32 power_flags;	/* Current power flags */
	__u32 power_level;	/* Current power level */
	__u32 power_interval_timeout;	/* Power interval timeout */
	__u32 grpclk;		/* Current GP clock value */
	__u32 busclk;		/* Current busclk value */
	__u64 ptbase;		/* Current ptbase */
	__u64 ptbase_lpac;	/* Current LPAC ptbase */
	__u32 pid;		/* PID of the process that owns the PT */
	__u32 pid_lpac;		/* PID of the LPAC process that owns the PT */
	__u32 current_context;	/* ID of the current context */
	__u32 current_context_lpac;	/* ID of the current LPAC context */
	__u32 ctxtcount;	/* Number of contexts appended to section */
	unsigned char release[32];	/* kernel release */
	unsigned char version[32];	/* kernel version */
	unsigned char comm[16];		/* Name of the process that owns the PT */
	unsigned char comm_lpac[16];	/* Name of the LPAC process that owns the PT */
} __packed;

/*
 * This structure contains a record of an active context.
 * These are appended one after another in the OS section below
 * the header above
 */
struct kgsl_snapshot_linux_context_v2 {
	__u32 id;			/* The context ID */
	__u32 timestamp_queued;		/* The last queued timestamp */
	__u32 timestamp_consumed;	/* The last timestamp consumed by HW */
	__u32 timestamp_retired;	/* The last timestamp retired by HW */
};

/* Ringbuffer sub-section header */
struct kgsl_snapshot_rb_v2 {
	int start;  /* dword at the start of the dump */
	int end;    /* dword at the end of the dump */
	int rbsize; /* Size (in dwords) of the ringbuffer */
	int wptr;   /* Current index of the CPU write pointer */
	int rptr;   /* Current index of the GPU read pointer */
	int count;  /* Number of dwords in the dump */
	__u32 timestamp_queued; /* The last queued timestamp */
	__u32 timestamp_retired; /* The last timestamp retired by HW */
	__u64 gpuaddr; /* The GPU address of the ringbuffer */
	__u32 id; /* Ringbuffer identifier */
} __packed;

/* Replay or Memory list section, both sections have same header */
struct kgsl_snapshot_mem_list_v2 {
	/*
	 * Number of IBs to replay for replay section or
	 * number of memory list entries for mem list section
	 */
	int num_entries;
	/* Pagetable base to which the replay IBs or memory entries belong */
	__u64 ptbase;
} __packed;

/* Indirect buffer sub-section header (v2) */
struct kgsl_snapshot_ib_v2 {
	__u64 gpuaddr; /* GPU address of the IB */
	__u64 ptbase;  /* Base for the pagetable the GPU address is valid in */
	__u64 size;    /* Size of the IB */
} __packed;

/* GMU memory ID's */
#define SNAPSHOT_GMU_MEM_UNKNOWN	0x00
#define SNAPSHOT_GMU_MEM_HFI		0x01
#define SNAPSHOT_GMU_MEM_LOG		0x02
#define SNAPSHOT_GMU_MEM_BWTABLE	0x03
#define SNAPSHOT_GMU_MEM_DEBUG		0x04
#define SNAPSHOT_GMU_MEM_BIN_BLOCK	0x05
#define SNAPSHOT_GMU_MEM_CONTEXT_QUEUE	0x06
#define SNAPSHOT_GMU_MEM_HW_FENCE	0x07
#define SNAPSHOT_GMU_MEM_WARMBOOT	0x08
#define SNAPSHOT_GMU_MEM_VRB		0x09
#define SNAPSHOT_GMU_MEM_TRACE		0x0a

/* GMU memory section data */
struct kgsl_snapshot_gmu_mem {
	int type;
	uint64_t hostaddr;
	uint64_t gmuaddr;
	uint64_t gpuaddr;
} __packed;

/* Register sub-section header */
struct kgsl_snapshot_regs {
	__u32 count; /* Number of register pairs in the section */
} __packed;

/* Indexed register sub-section header */
struct kgsl_snapshot_indexed_regs {
	__u32 index_reg; /* Offset of the index register for this section */
	__u32 data_reg;  /* Offset of the data register for this section */
	int start;     /* Starting index */
	int count;     /* Number of dwords in the data */
} __packed;

struct kgsl_snapshot_indexed_regs_v2 {
	u32 index_reg;	/* Offset of the index register for this section */
	u32 data_reg;	/* Offset of the data register for this section */
	u32 start;	/* Starting index */
	u32 count;	/* Number of dwords in the data */
	u32 pipe_id;	/* Id of pipe, BV, Br etc */
	u32 slice_id;	/* Slice ID to be dumped */
} __packed;

/* MVC register sub-section header */
struct kgsl_snapshot_mvc_regs {
	int ctxt_id;
	int cluster_id;
} __packed;

struct kgsl_snapshot_mvc_regs_v2 {
	int ctxt_id;
	int cluster_id;
	int pipe_id;
	int location_id;
} __packed;

struct kgsl_snapshot_mvc_regs_v3 {
	u32 ctxt_id;
	u32 cluster_id;
	u32 pipe_id;
	u32 location_id;
	u32 slice_id;
	u32 sp_id;
	u32 usptp_id;
} __packed;

/* Debug data sub-section header */

/* A5XX debug sections */
#define SNAPSHOT_DEBUG_CP_MEQ     7
#define SNAPSHOT_DEBUG_CP_PM4_RAM 8
#define SNAPSHOT_DEBUG_CP_PFP_RAM 9
#define SNAPSHOT_DEBUG_CP_ROQ     10
#define SNAPSHOT_DEBUG_SHADER_MEMORY 11
#define SNAPSHOT_DEBUG_CP_MERCIU 12
#define SNAPSHOT_DEBUG_SQE_VERSION 14

/* GMU Version information */
#define SNAPSHOT_DEBUG_GMU_CORE_VERSION 15
#define SNAPSHOT_DEBUG_GMU_CORE_DEV_VERSION 16
#define SNAPSHOT_DEBUG_GMU_PWR_VERSION 17
#define SNAPSHOT_DEBUG_GMU_PWR_DEV_VERSION 18
#define SNAPSHOT_DEBUG_GMU_HFI_VERSION 19
#define SNAPSHOT_DEBUG_AQE_VERSION 20

struct kgsl_snapshot_debug {
	int type;    /* Type identifier for the attached tata */
	int size;   /* Size of the section in dwords */
} __packed;

struct kgsl_snapshot_debugbus {
	int id;	   /* Debug bus ID */
	int count; /* Number of dwords in the dump */
} __packed;

struct kgsl_snapshot_side_debugbus {
	int id;   /* Debug bus ID */
	int size; /* Number of dwords in the dump */
	int valid_data;  /* Mask of valid bits of the side debugbus */
} __packed;

struct kgsl_snapshot_shader {
	int type;  /* SP/TP statetype */
	int index; /* SP/TP index */
	int size;  /* Number of dwords in the dump */
} __packed;

struct kgsl_snapshot_shader_v2 {
	int type;  /* SP/TP statetype */
	int index; /* SP/TP index */
	int usptp; /* USPTP index */
	int pipe_id; /* Pipe id */
	int location; /* Location value */
	u32 size;  /* Number of dwords in the dump */
} __packed;

struct kgsl_snapshot_shader_v3 {
	u32 type;  /* SP/TP statetype */
	u32 slice_id; /* Slice ID */
	u32 sp_index; /* SP/TP index */
	u32 usptp; /* USPTP index */
	u32 pipe_id; /* Pipe id */
	u32 location; /* Location value */
	u32 ctxt_id; /* Context ID */
	u32 size;  /* Number of dwords in the dump */
} __packed;

#define TRACE_BUF_NUM_SIG 4

/**
 * enum trace_buffer_source - Bits to identify the source block of trace buffer information
 * GX_DBGC : Signals captured from GX block
 * CX_DBGC : Signals captured from CX block
 */
enum trace_buffer_source {
	GX_DBGC = 1,
	CX_DBGC = 2,
};

/**
 * kgsl_snapshot_trace_buffer: Header Information for the tracebuffer in snapshot.
 */
struct kgsl_snapshot_trace_buffer {
	/** @dbgc_ctrl: Identify source for trace */
	__u16 dbgc_ctrl;
	/** @dbgc_ctrl: Identify source for trace */
	__u16 segment;
	/** @granularity: The total number of segments in each packet */
	__u16 granularity;
	/** @ping_blk: Signal block */
	__u16 ping_blk[TRACE_BUF_NUM_SIG];
	/** @ping_idx: Signal Index */
	__u16 ping_idx[TRACE_BUF_NUM_SIG];
	/** @size: Number of bytes in the dump */
	__u32 size;
} __packed;

#define SNAPSHOT_GPU_OBJECT_SHADER  1
#define SNAPSHOT_GPU_OBJECT_IB      2
#define SNAPSHOT_GPU_OBJECT_GENERIC 3
#define SNAPSHOT_GPU_OBJECT_DRAW    4
#define SNAPSHOT_GPU_OBJECT_GLOBAL  5

struct kgsl_snapshot_gpu_object_v2 {
	int type;      /* Type of GPU object */
	__u64 gpuaddr; /* GPU address of the object */
	__u64 ptbase;  /* Base for the pagetable the GPU address is valid in */
	__u64 size;    /* Size of the object (in dwords) */
} __packed;

struct kgsl_snapshot_eventlog {
	/** @type: Type of the event log buffer */
	__u16 type;
	/** @version: Version of the event log buffer */
	__u16 version;
	/** @size: Size of the eventlog buffer in bytes */
	u32 size;
} __packed;

struct kgsl_snapshot_gmu_version {
	/** @type: Type of the GMU version buffer */
	u32 type;
	/** @value: GMU FW version value */
	u32 value;
} __packed;

struct kgsl_device;
struct kgsl_process_private;

void kgsl_snapshot_push_object(struct kgsl_device *device,
		struct kgsl_process_private *process,
		uint64_t gpuaddr, uint64_t dwords);
#endif
