/****
  include for mxm1120
**/
#ifndef __MXM1120_H__
#define __MXM1120_H__

#include "../abstract/magnetic_cover.h"

/* ********************************************************* */
/* feature of ic revision */
/* ********************************************************* */
#define M1120_REV_0_2                       (0x02)
#define M1120_REV_1_0                       (0x10)
#define M1120_REV                           M1120_REV_1_0
#define M1120_DRIVER_VERSION                "Ver1.04-140226"
/* ********************************************************* */

/* ********************************************************* */
/* property of driver */
/* ********************************************************* */
#define M1120_DRIVER_NAME_UP                   "hall_m1120_up"
#define M1120_DRIVER_NAME_MIDDLE                 "m1120_middle"
#define M1120_DRIVER_NAME_DOWN                 "hall_m1120_down"
#define M1120_IRQ_NAME                      "m1120-irq"
#define M1120_PATH                          "/dev/m1120"

#define M1120_DEF_HIGH    500
#define M1120_DEF_LOW    -500

/*
 *SAD1 SAD0 == 00  0001100 R/W   (7bits)0x0C  (8bits)0x18
 *SAD1 SAD0 == 01  0001101 R/W   (7bits)0x0D  (8bits)0x1A
 *SAD1 SAD0 == 10  0001110 R/W   (7bits)0x0E  (8bits)0x1C
 *SAD1 SAD0 == 11  0001111 R/W   (7bits)0x0F  (8bits)0x1E
 */
#define M1120_SLAVE_ADDR                    (0x18)
/* ********************************************************* */

/* ********************************************************* */
/* register map */
/* ********************************************************* */
#define M1120_REG_PERSINT                   (0x00)
#define M1120_VAL_PERSINT_COUNT(n)          (n << 4)
#define M1120_VAL_PERSINT_INTCLR            (0x01)
/*
 *[7:4]   PERS        : interrupt persistence count
 *[0]     INTCLR  = 1 : interrupt clear
 */
/* --------------------------------------------------------- */
#define M1120_REG_INTSRS                    (0x01)
#define M1120_VAL_INTSRS_INT_ON             (0x80)
#define M1120_DETECTION_MODE_INTERRUPT      M1120_VAL_INTSRS_INT_ON
#define M1120_VAL_INTSRS_INT_OFF            (0x00)
#define M1120_DETECTION_MODE_POLLING        M1120_VAL_INTSRS_INT_OFF
#define M1120_VAL_INTSRS_INTTYPE_BESIDE     (0x00)
#define M1120_VAL_INTSRS_INTTYPE_WITHIN     (0x10)

/*
 *M1120_VAL_INTSRS_X1BIT_X2_X3
 *X1: 10BIT or 8BIT
 *X2: Measuring range
 *X3: Resolution
 */
#define M1120_VAL_INTSRS_10BIT_0P08_40P8  (0x00)
#define M1120_VAL_INTSRS_10BIT_0P04_20P4  (0x01)

/*
 *[7]     INTON   = 0 : disable interrupt
 *[7]     INTON   = 1 : enable interrupt
 *[4]     INT_TYP  = 0 : generate interrupt when raw data is beside range of threshold
 *[4]     INT_TYP  = 1 : generate interrupt when raw data is within range of threshold
 *[2:0]   SRS         : select sensitivity type when M1120_VAL_OPF_BIT_10
 *000     : 0.068 (mT/LSB)
 *001     : 0.034 (mT/LSB)
 *010     : 0.017 (mT/LSB)
 *011     : 0.009 (mT/LSB)
 *100     : 0.004 (mT/LSB)
 *101     : 0.017 (mT/LSB)
 *110     : 0.017 (mT/LSB)
 *111     : 0.017 (mT/LSB)
 *[2:0]   SRS         : select sensitivity type when M1120_VAL_OPF_BIT_8
 *000     : 0.272 (mT/LSB)
 *001     : 0.136 (mT/LSB)
 *010     : 0.068 (mT/LSB)
 *011     : 0.036 (mT/LSB)
 *100     : 0.016 (mT/LSB)
 *101     : 0.068 (mT/LSB)
 *110     : 0.068 (mT/LSB)
 *111     : 0.068 (mT/LSB)
 */
/* --------------------------------------------------------- */
#define M1120_REG_LTHL                      (0x02)
/*
 *[7:0]   LTHL        : low byte of low threshold value
 */
/* --------------------------------------------------------- */
#define M1120_REG_LTHH                      (0x03)
/*
 *[7:6]   LTHH        : high 2bits of low threshold value with sign
 */
/* --------------------------------------------------------- */
#define M1120_REG_HTHL                      (0x04)
/*
 *[7:0]   HTHL        : low byte of high threshold value
 */
/* --------------------------------------------------------- */
#define M1120_REG_HTHH                      (0x05)
/*
 *[7:6]   HTHH        : high 2bits of high threshold value with sign
 */
/* --------------------------------------------------------- */
#define M1120_REG_I2CDIS                    (0x06)
#define M1120_VAL_I2CDISABLE                (0x37)
/*
 *[7:0]   I2CDIS      : disable i2c
 */
/* --------------------------------------------------------- */
#define M1120_REG_SRST                      (0x07)
#define M1120_VAL_SRST_RESET                (0x01)
/*
 *[0]     SRST    = 1 : soft reset
 */
/* --------------------------------------------------------- */
#define M1120_REG_OPF                       (0x08)
#define M1120_VAL_OPF_FREQ_20HZ             (0x00)
#define M1120_VAL_OPF_FREQ_10HZ             (0x10)
#define M1120_VAL_OPF_FREQ_6_7HZ            (0x20)
#define M1120_VAL_OPF_FREQ_5HZ              (0x30)
#define M1120_VAL_OPF_FREQ_80HZ             (0x40)
#define M1120_VAL_OPF_FREQ_40HZ             (0x50)
#define M1120_VAL_OPF_FREQ_26_7HZ           (0x60)
#define M1120_VAL_OPF_EFRD_ON               (0x08)
#define M1120_VAL_OPF_BIT_8                 (0x02)
#define M1120_VAL_OPF_BIT_10                (0x00)
#define M1120_VAL_OPF_HSSON_ON              (0x01)
#define M1120_VAL_OPF_HSSON_OFF             (0x00)
/*
 *[6:4]   OPF         : operation frequency
 *000     : 20    (Hz)
 *001     : 10    (Hz)
 *010     : 6.7   (Hz)
 *011     : 5     (Hz)
 *100     : 80    (Hz)
 *101     : 40    (Hz)
 *110     : 26.7  (Hz)
 *111     : 20    (Hz)
 *[3]     EFRD    = 0 : keep data without accessing eFuse
 *[3]     EFRD    = 1 : update data after accessing eFuse
 *[1]     BIT     = 0 : 10 bit resolution
 *[1]     BIT     = 1 : 8 bit resolution
 *[0]     HSSON   = 0 : Off power down mode
 *[0]     HSSON   = 1 : On power down mode
 */
/* --------------------------------------------------------- */
#define M1120_REG_DID                       (0x09)
#define M1120_VAL_DID                       (0x9C)
/*
 *[7:0]   DID         : Device ID
 */
/* --------------------------------------------------------- */
#define M1120_REG_INFO                      (0x0A)
/*
 *[7:0]   INFO        : Information about IC
 */
/* --------------------------------------------------------- */
#define M1120_REG_ASA                       (0x0B)
/*
 *[7:0]   ASA         : Hall Sensor sensitivity adjustment
 */
/* --------------------------------------------------------- */
#define M1120_REG_ST1                       (0x10)
#define M1120_VAL_ST1_DRDY                  (0x01)
/*
 *[4] INTM    : status of interrupt mode
 *[1] BITM    : status of resolution
 *[0] DRDY    : status of data ready
 */
/* --------------------------------------------------------- */
#define M1120_REG_HSL                       (0x11)

/*[7:0]   HSL  : low byte of hall sensor measurement data*/

/* --------------------------------------------------------- */
#define M1120_REG_HSH                       (0x12)

/*[7:6]   HSL  : high 2bits of hall sensor measurement data with sign*/

/* ********************************************************* */
/* ioctl command */
/* ********************************************************* */
#define M1120_IOCTL_BASE                    (0x80)
#define M1120_IOCTL_SET_ENABLE              _IOW(M1120_IOCTL_BASE, 0x00, int)
#define M1120_IOCTL_GET_ENABLE              _IOR(M1120_IOCTL_BASE, 0x01, int)
#define M1120_IOCTL_SET_DELAY               _IOW(M1120_IOCTL_BASE, 0x02, int)
#define M1120_IOCTL_GET_DELAY               _IOR(M1120_IOCTL_BASE, 0x03, int)
#define M1120_IOCTL_SET_CALIBRATION         _IOW(M1120_IOCTL_BASE, 0x04, int*)
#define M1120_IOCTL_GET_CALIBRATED_DATA     _IOR(M1120_IOCTL_BASE, 0x05, int*)
#define M1120_IOCTL_SET_INTERRUPT           _IOW(M1120_IOCTL_BASE, 0x06, unsigned int)
#define M1120_IOCTL_GET_INTERRUPT           _IOR(M1120_IOCTL_BASE, 0x07, unsigned int*)
#define M1120_IOCTL_SET_THRESHOLD_HIGH      _IOW(M1120_IOCTL_BASE, 0x08, unsigned int)
#define M1120_IOCTL_GET_THRESHOLD_HIGH      _IOR(M1120_IOCTL_BASE, 0x09, unsigned int*)
#define M1120_IOCTL_SET_THRESHOLD_LOW       _IOW(M1120_IOCTL_BASE, 0x0A, unsigned int)
#define M1120_IOCTL_GET_THRESHOLD_LOW       _IOR(M1120_IOCTL_BASE, 0x0B, unsigned int*)
#define M1120_IOCTL_SET_REG                 _IOW(M1120_IOCTL_BASE, 0x0C, int)
#define M1120_IOCTL_GET_REG                 _IOR(M1120_IOCTL_BASE, 0x0D, int)
/* ********************************************************* */
/* event property */
/* ********************************************************* */
#define DEFAULT_EVENT_TYPE                  EV_ABS
#define DEFAULT_EVENT_CODE                  ABS_X
#define DEFAULT_EVENT_DATA_CAPABILITY_MIN   (-32768)
#define DEFAULT_EVENT_DATA_CAPABILITY_MAX   (32767)
/* ********************************************************* */
/* delay property */
/* ********************************************************* */
#define M1120_DELAY_MAX                     (200)   /* ms */
#define M1120_DELAY_MIN                     (20)    /* ms */
#define M1120_DELAY_FOR_READY               (10)    /* ms */
/* ********************************************************* */


/* ********************************************************* */
/* data type for driver */
/* ********************************************************* */

enum {
	OPERATION_MODE_POWERDOWN_M,
	OPERATION_MODE_MEASUREMENT_M,
	OPERATION_MODE_FUSEROMACCESS_M
};

/* ohter define */
#define M1120_DBG_ENABLE
#define M1120_DETECTION_MODE             M1120_DETECTION_MODE_INTERRUPT
#define M1120_INTERRUPT_TYPE             M1120_VAL_INTSRS_INTTYPE_BESIDE
/*#define M1120_INTERRUPT_TYPE           M1120_VAL_INTSRS_INTTYPE_WITHIN*/
#define M1120_SENSITIVITY_TYPE           M1120_VAL_INTSRS_10BIT_0P04_20P4
#define M1120_PERSISTENCE_COUNT          M1120_VAL_PERSINT_COUNT(15)
#define M1120_OPERATION_FREQUENCY        M1120_VAL_OPF_FREQ_80HZ
#define M1120_OPERATION_RESOLUTION       M1120_VAL_OPF_BIT_10
#define M1120_DETECT_RANGE_HIGH          (60)/*Need change via test.*/
#define M1120_DETECT_RANGE_LOW           (50)/*Need change via test.*/
#define M1120_RESULT_STATUS_A            (0x01)/*result status A 180Degree*/
#define M1120_RESULT_STATUS_B            (0x02)/*result status B 180Degree*/
#define M1120_EVENT_TYPE                 EV_ABS
#define M1120_EVENT_CODE                 ABS_X
#define M1120_EVENT_DATA_CAPABILITY_MIN  (-32768)
#define M1120_EVENT_DATA_CAPABILITY_MAX  (32767)
#define M1120_I2C_BUF_SIZE               (17)

#define M1120_REG_NUM                    (15)
#define MAX_I2C_RETRY_TIME               (4)

union m1120_reg_t {
	struct {
		unsigned char persint;
		unsigned char intsrs;
		unsigned char lthl;
		unsigned char lthh;
		unsigned char hthl;
		unsigned char hthh;
		unsigned char i2cdis;
		unsigned char srst;
		unsigned char opf;
		unsigned char did;
		unsigned char info;
		unsigned char asa;
		unsigned char st1;
		unsigned char hsl;
		unsigned char hsh;
	} map;
	unsigned char array[M1120_REG_NUM];
};

struct mxm1120_chip_info {
	struct i2c_client               *client;
	int                             irq;
	struct magnetic_cover_info      *magcvr_info;
	struct mutex                    data_lock;
	union m1120_reg_t               reg;

	int                             calibrated_data;
	int                 last_data;
	short               thrhigh;
	short               thrlow;
};

#endif  /* __MXM1120_H__ */
