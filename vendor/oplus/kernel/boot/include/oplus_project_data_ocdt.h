/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#ifndef OPLUS_PROJECT_DATA_OCDT_H
#define OPLUS_PROJECT_DATA_OCDT_H

#define MAX_OCP	6
#define MAX_LEN 8
#define FEATURE_COUNT 40
#define OCDT_VERSION_1_0	(1)
#define OCDT_VERSION_2_0	(2)

enum PCB_VERSION {
    UNKNOWN_VERSION = 0,
    PRE_EVB1,
    PRE_EVB2,
    EVB1 = 8,
    EVB2,
    EVB3,
    EVB4,
    EVB5,
    EVB6,
    T0 = 16,
    T1,
    T2,
    T3,
    T4,
    T5,
    T6,
    EVT1 = 24,
    EVT2,
    EVT3,
    EVT4,
    EVT5,
    EVT6,
    DVT1 = 32,
    DVT2,
    DVT3,
    DVT4,
    DVT5,
    DVT6,
    PVT1 = 40,
    PVT2,
    PVT3,
    PVT4,
    PVT5,
    PVT6,
    MP1 = 48,
    MP2,
    MP3,
    MP4,
    MP5,
    MP6,
    PCB_MAX,
};

#ifdef CONFIG_OPLUS_SYSTEM_KERNEL_QCOM
//Reboot modes
enum {
    UNKOWN_MODE         = 0x00,
    RECOVERY_MODE       = 0x1,
    FASTBOOT_MODE       = 0x2,
    ALARM_BOOT          = 0x3,
    DM_VERITY_LOGGING   = 0x4,
    DM_VERITY_ENFORCING = 0x5,
    DM_VERITY_KEYSCLEAR = 0x6,
    SILENCE_MODE        = 0x21,
    SAU_MODE            = 0x22,
    RF_MODE             = 0x23,
    WLAN_MODE           = 0x24,
    MOS_MODE            = 0x25,
    FACTORY_MODE        = 0x26,
    REBOOT_KERNEL       = 0x27,
    REBOOT_MODEM        = 0x28,
    REBOOT_ANDROID      = 0x29,
//Modify for PHOENIX_PROJECT and OPPO_DOCTOR use
    REBOOT_SBL_DDRTEST  = 0x2B,  //Add for agingtest
    REBOOT_SBL_DDR_CUS  = 0x2C,
    REBOOT_AGINGTEST    = 0x2D,
    REBOOT_SBLTEST_FAIL = 0x2E,
    NORMAL_MODE         = 0x3E,
    REBOOT_NORMAL       = 0x3F,
    EMERGENCY_DLOAD     = 0xFF,
};
#endif

enum OPLUS_ENG_VERSION {
    OEM_RELEASE             = 0x00,
    AGING                   = 0x01,
    CTA                     = 0x02,
    PERFORMANCE             = 0x03,
    PREVERSION              = 0x04,
    ALL_NET_CMCC_TEST       = 0x05,
    ALL_NET_CMCC_FIELD      = 0x06,
    ALL_NET_CU_TEST         = 0x07,
    ALL_NET_CU_FIELD        = 0x08,
    ALL_NET_CT_TEST         = 0x09,
    ALL_NET_CT_FIELD        = 0x0A,
    HIGH_TEMP_AGING         = 0x0B,
    FACTORY                 = 0x0C,
    CE                      = 0x0D,
    PTCRB                   = 0x0E,
};
/*
oplus board feature mask which value read from ocdt.bin design to eliminate
use of get_project.
One byte for one feature, so one feature support 256 different status
*/
typedef enum {
	OPLUS_BOARD_FEATURE_VIBRATOR_STATUS = 0,  /*add as a example */
	OPLUS_BOARD_FEATURE_END = FEATURE_COUNT,
} OplusBoardFeatureMask;

typedef  struct
{
	unsigned int ProjectNo;
	unsigned int DtsiNo;
	unsigned int AudioIdx;
	unsigned char Feature[FEATURE_COUNT];
} ProjectDataBCDT;

typedef  struct
{
	unsigned int	 Version;
	unsigned int	 Is_confidential;
} ProjectDataECDT;

typedef  struct
{
	unsigned int	  OplusBootMode;
	unsigned int	  RF;
	unsigned int	  PCB;
	unsigned char	  PmicOcp[MAX_OCP];
	unsigned char	  Operator;
	unsigned char	  Gauge;
	unsigned char	  Reserved[14];
} ProjectDataSCDT;

typedef  struct
{
  unsigned int	 Version;
  ProjectDataBCDT nDataBCDT;
  ProjectDataSCDT nDataSCDT;
  ProjectDataECDT nDataECDT;
} ProjectInfoOCDT;
#endif
