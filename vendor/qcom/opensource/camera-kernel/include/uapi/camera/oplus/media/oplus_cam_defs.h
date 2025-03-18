/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _OPLUS_CAM_DEFS_H
#define _OPLUS_CAM_DEFS_H
#define CAM_OEM_COMMON_OPCODE_BASE              0x8000
#define CAM_GET_OIS_EIS_HALL                    (CAM_COMMON_OPCODE_BASE + 0xD)
#define CAM_WRITE_CALIBRATION_DATA              (CAM_OEM_COMMON_OPCODE_BASE + 0x2)
#define CAM_CHECK_CALIBRATION_DATA              (CAM_OEM_COMMON_OPCODE_BASE + 0x3)
#define CAM_WRITE_AE_SYNC_DATA                  (CAM_OEM_COMMON_OPCODE_BASE + 0x4)
#define CAM_OEM_IO_CMD                          (CAM_OEM_COMMON_OPCODE_BASE + 0x5)
#define CAM_OEM_GET_ID                          (CAM_OEM_COMMON_OPCODE_BASE + 0x6)
#define CAM_GET_DPC_DATA                        (CAM_OEM_COMMON_OPCODE_BASE + 0x7)
#define CAM_STORE_DUALOIS_GYRO_GAIN             (CAM_OEM_COMMON_OPCODE_BASE + 0x8)
#define CAM_WRITE_DUALOIS_GYRO_GAIN             (CAM_OEM_COMMON_OPCODE_BASE + 0x9)
#define CAM_DO_DUALOIS_GYRO_OFFSET              (CAM_OEM_COMMON_OPCODE_BASE + 0xA)
#define CAM_QUERY_DUALOIS_SMA_WIRE_STATUS       (CAM_OEM_COMMON_OPCODE_BASE + 0xB)
#define CAM_GET_DUALOIS_INITIAL_GYRO_GAIN       (CAM_OEM_COMMON_OPCODE_BASE + 0xC)
#define CAM_WRITE_SHIFT_OIS_REGISTER            (CAM_OEM_COMMON_OPCODE_BASE + 0xD)
#define CAM_FIRMWARE_CALI_GYRO_OFFSET           (CAM_OEM_COMMON_OPCODE_BASE + 0xE)
#define CAM_TELE_OIS_USE_MONITOR                (CAM_OEM_COMMON_OPCODE_BASE + 0xF)
#define CAM_DUMP_SENSOR_OTP                     (CAM_OEM_COMMON_OPCODE_BASE + 0x10)
#define CAM_SET_CDR_VALUE                       (CAM_OEM_COMMON_OPCODE_BASE + 0x11)
#define CAM_GET_TEMPERATURE                     (CAM_OEM_COMMON_OPCODE_BASE + 0x12)
#define CAM_WRITE_VSYNC_DATA                    (CAM_OEM_COMMON_OPCODE_BASE + 0x13)

#define CAM_OEM_CMD_READ_DEV                    0
#define CAM_OEM_CMD_WRITE_DEV                   1
#define CAM_OEM_OIS_CALIB                       2
#define CAM_OEM_RW_SIZE_MAX                     128
#define CAM_OEM_INITSETTINGS_SIZE_MAX           3000
#define CAM_OEM_OTP_DATA_MAX_LENGTH             256

struct cam_oem_i2c_reg_array {
	unsigned int    reg_addr;
	unsigned int    reg_data;
	unsigned int    delay;
	unsigned int    data_mask;
	unsigned short  operation;
};

enum  CCIOperationType
{
    CCI_WRITE            = 0,
    CCI_WRITE_BURST      = 1,
    CCI_WRITE_SEQUENTIAL = 2,
    CCI_READ             = 3,
    CCI_POLL             = 4,
    MAX                  = 5
};

struct cam_oem_rw_ctl {
	signed int                cmd_code;
	unsigned long long        cam_regs_ptr;
	unsigned int              slave_addr;
	unsigned int              reg_data_type;
	signed int                reg_addr_type;
	signed short              num_bytes;
};

struct cam_oem_initsettings {
	struct cam_oem_i2c_reg_array reg_setting[CAM_OEM_INITSETTINGS_SIZE_MAX];
	signed short size;
	unsigned short addr_type;
	unsigned short data_type;
};

/*add for get hall dat for EIS*/
#define HALL_MAX_NUMBER 12
struct ois_hall_type {
	unsigned int    dataNum;
	unsigned int    mdata[HALL_MAX_NUMBER];
	unsigned int    timeStamp;
};

typedef struct dual_ois_calibration_t
{
	int LsGyroGain_X;
	int LsGyroGain_Y;
	int SsGyroGain_A;
	int SsGyroGain_B;
	int SsGyroGain_C;
	int mode;
	int successed;
} DUAL_OIS_CALI_RESULTS;

typedef struct ois_gyrogain_t
{
	int32_t GyroGain_X;
	int32_t GyroGain_Y;
} OIS_GYROGAIN;

typedef struct dual_ois_gyrogain_t
{
	unsigned int LsGyroGain_X;
	unsigned int LsGyroGain_Y;
	unsigned int SsGyroGain_A;
	unsigned int SsGyroGain_B;
	unsigned int SsGyroGain_C;
} DUAL_OIS_GYROGAIN;

#define VIDIOC_CAM_FTM_POWNER_UP 0
#define VIDIOC_CAM_FTM_POWNER_DOWN 1
#define VIDIOC_CAM_AON_POWNER_UP                  0x5000
#define VIDIOC_CAM_AON_POWNER_DOWN                0x5001
#define VIDIOC_CAM_AON_QUERY_INFO                 0x5002

#define VIDIOC_CAM_SENSOR_STATR 0x9000
#define VIDIOC_CAM_SENSOR_STOP 0x9001

enum camera_extension_feature_type {
	CAM_EXTENSION_FEATURE_TYPE_INVALID,
	CAM_ACTUATOR_NORMAL_POWER_UP_TYPE,
	CAM_ACTUATOR_SDS_POWER_UP_TYPE,
	CAM_ACTUATOR_UPDATE_PID_POWER_UP_TYPE,
	CAM_ACTUATOR_DELAY_POWER_DOWN_TYPE,
	CAM_SENSOR_ADVANCE_POWER_UP_TYPE,
	CAM_SENSOR_NORMAL_POWER_UP_TYPE,
	CAM_OIS_NORMAL_POWER_UP_TYPE,
	CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
	CAM_OIS_DELAY_POWER_DOWN_TYPE,
	CAM_OIS_PUSH_CENTER_POWER_UP_TYPE,
	CAM_TOF_NORMAL_POWER_UP_TYPE,
	CAM_DEV_EXCEPTION_POWER_DOWN_TYPE,
	CAMERA_POWER_UP_TYPE_MAX,
};
#define FD_DFCT_MAX_NUM 5
#define SG_DFCT_MAX_NUM 299
#define FD_DFCT_NUM_ADDR 0x7678
#define SG_DFCT_NUM_ADDR 0x767A
#define FD_DFCT_ADDR 0x8B00
#define SG_DFCT_ADDR 0x8B10
#define V_ADDR_SHIFT 12
#define H_DATA_MASK 0xFFF80000
#define V_DATA_MASK 0x0007FF80

struct sony_dfct_tbl_t {
	/*---- single static defect ----*/
	int sg_dfct_num;                         /* the number of single static defect*/
	int sg_dfct_addr[SG_DFCT_MAX_NUM];       /* [ u25 ( upper-u13 = x-addr, lower-u12 = y-addr ) ]*/
	/*---- FD static defect ----*/
	int fd_dfct_num;                         /* the number of FD static defect*/
	int fd_dfct_addr[FD_DFCT_MAX_NUM];       /* [ u25 ( upper-u13 = x-addr, lower-u12 = y-addr ) ]*/
} __attribute__ ((packed));

#define CALIB_DATA_LENGTH         1689
#define WRITE_DATA_MAX_LENGTH     16
#define WRITE_DATA_DELAY          3
#define EEPROM_NAME_LENGTH        64

struct cam_write_eeprom_t {
    unsigned int    cam_id;
    unsigned int    baseAddr;
    unsigned int    calibDataSize;
    unsigned int    isWRP;
    unsigned int    WRPaddr;
    unsigned int    CloseWRP;
    unsigned int    OpenWRP;
    unsigned char calibData[CALIB_DATA_LENGTH];
    char eepromName[EEPROM_NAME_LENGTH];
} __attribute__ ((packed));

#define EEPROM_CHECK_DATA_MAX_SIZE 196
struct check_eeprom_data_t{
    unsigned int    cam_id;
    unsigned int    checkDataSize;
    unsigned int    startAddr;
    unsigned int    eepromData_checksum;
} __attribute__ ((packed));


/*add for sensor power up in advance*/
enum cam_sensor_power_state {
        CAM_SENSOR_POWER_OFF,
        CAM_SENSOR_POWER_ON,
};

enum cam_sensor_setting_state {
        CAM_SENSOR_SETTING_WRITE_INVALID,
        CAM_SENSOR_SETTING_WRITE_SUCCESS,
};

#define ENABLE_OIS_DELAY_POWER_DOWN
#ifdef ENABLE_OIS_DELAY_POWER_DOWN
#define OIS_POWER_DOWN_DELAY 200//ms
enum cam_ois_power_down_thread_state {
	CAM_OIS_POWER_DOWN_THREAD_RUNNING,
	CAM_OIS_POWER_DOWN_THREAD_STOPPED,
};

enum cam_ois_power_state {
	CAM_OIS_POWER_ON,
	CAM_OIS_POWER_OFF,
};
#endif

enum cam_ois_type_vendor {
	CAM_OIS_MASTER,
	CAM_OIS_SLAVE,
	CAM_OIS_TYPE_MAX,
};

enum cam_ois_state_vendor {
	CAM_OIS_INVALID,
	CAM_OIS_FW_DOWNLOADED,
	CAM_OIS_READY,
};

enum cam_ois_control_cmd {
	CAM_OIS_START_POLL_THREAD,
	CAM_OIS_STOP_POLL_THREAD,
};

enum cam_ois_close_state {
	CAM_OIS_IS_OPEN,
	CAM_OIS_IS_DOING_CLOSE,
	CAM_OIS_IS_CLOSE,
};

enum cam_ois_download_fw_state {
	CAM_OIS_FW_NOT_DOWNLOAD,
	CAM_OIS_FW_DOWNLOAD_DONE,
};
#endif /* _OPLUS_CAM_DEFS_H */
