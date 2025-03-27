#ifndef _CAM_EXTENSION_UAPI_H_
#define _CAM_EXTENSION_UAPI_H_

#define CAM_STATE_MONITOR_MAX_ENTRIES		600
#define CAM_EXTENSION_MONITOR_DUMP_STR_MAX	((CAM_STATE_MONITOR_MAX_ENTRIES + 200) * 152)

struct extension_control {
	__u32        op_code;
	__u32        size;
	__u64        reserved;
	__u64        handle;
};

struct monitor_check {
	__u32				camera_feature_inuse_mask;	// Functions in use.
	__u32        			count_inuse_clock;		// Reference count of clock.
	__u32        			count_inuse_regulator;		// Reference count of rgltr.
	__u32        			count_enabled_regulator;	// Actual num of rgltr enabled.
	bool				check_pass;
};

#define CAM_EXTENSION_GENERIC_BASE			0x10
#define CAM_EXTENSION_OPCODE_MONITOR_CHECK		(CAM_EXTENSION_GENERIC_BASE + 0x1)
#define CAM_EXTENSION_OPCODE_MONITOR_DUMP		(CAM_EXTENSION_GENERIC_BASE + 0x2)

// ioctl type
#define	CAM_EXTENSION_IOC_MAGIC			'X'

// ioctl nr
#define	IOC_GENERIC				(0)

#define	CAM_EXTENSION_IOC_GENERIC		_IOR(CAM_EXTENSION_IOC_MAGIC, IOC_GENERIC, struct extension_control)

// #define CAM_EXTENSION_CMD_DUMP			(_IOC(_IOC_READ, CAM_EXTENSION_IOC_MAGIC, IOC_NR_RD_MONITOR, 0))

#endif
