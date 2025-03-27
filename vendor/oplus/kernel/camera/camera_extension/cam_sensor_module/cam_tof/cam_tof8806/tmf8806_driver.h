/*
 *****************************************************************************
 * Copyright by ams OSRAM AG                                                 *
 * All rights are reserved.                                                  *
 *                                                                           *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
 * THE SOFTWARE.                                                             *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
 *****************************************************************************
 */

/*
 * History of Versions:
 *    1.0 ... initial version
 *    1.1 ... clock correction changed, output fifo buffer increased
 *        ... histogram scaled for output data in uint_32
 *        ... check output buffer size before histograms are dumped
 *        ... app0_apply_fac_calib_store wait for interrupt
 *    1.2 ... histogram dumping not scaled
 *        ... output data with header
 *        ... fix serial number readout
 *    1.3 ... fix in switch apps
 *        ... firmware download
 *        ... cat driver_debug interpretation in hex
 *        ... distance_thresholds interpretation in dec
 *        ... show attributes of hex values in 0x shown
 */

/*! \file tmf8806_driver.h - TMF8806 driver
 * \brief Device driver for measuring Distance in mm.
 */

#ifndef TMF8806_DRIVER_H
#define TMF8806_DRIVER_H

/* -------------------------------- includes -------------------------------- */
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>

#include "tmf8806.h"
#include "tmf8806_shim.h"
#include "tmf8806_regs.h"
#include "tof_pdrv.h"
/* -------------------------------- defines --------------------------------- */
#define MAX_REGS 256                                // i2c max accessable registers
#define TOF_GPIO_INT_NAME     "irq"                 // pin from overlay file
#define TOF_GPIO_ENABLE_NAME  "enable"              // pin from overlay file
#define TOF_PROP_NAME_POLLIO  "tof,tof_poll_period" // property in overlay file
#define APP_LOG_LEVEL               TMF8806_LOG_LEVEL_ERROR // how much logging we want at initialization

#define TOF8806_APP_ID_REG               0x00
#define TOF8806_REQ_APP_ID_REG           0x02
#define TOF8806_CMD_DATA9_REG            0x06
#define TOF8806_APP_ID_BOOTLOADER        0x80
#define TOF8806_APP_ID_APP0              0xC0
#define TOF8806_RESULT_NUMBER_OFFSET     0x20
#define TOF_XTALK_AVERAGE_COUNT          5

#define TOF8806_APP0_CMD_IDX             11
#define TOF8806_FACTORY_CAL_CMD_SIZE     14
#define TMF8806_FACTORY_CALIB_KITERS     40960 // kilo iterations for factory calibration

#define TMF8806_OSC_MIN_TRIM_VAL  0
#define TMF8806_OSC_MAX_TRIM_VAL  511
#define TMF8806_IMAGE_START 0x20000000
//#define AMS_OEM_MUTEX_DEBUG
#ifdef AMS_OEM_MUTEX_DEBUG
#define AMS_MUTEX_OEM_LOCK(m) { \
	pr_info("%s: OEMMutex Lock\n", __func__); \
	mutex_lock_interruptible(m); \
}
#define AMS_MUTEX_OEM_UNLOCK(m) { \
	pr_info("%s: OEMMutex Unlock\n", __func__); \
	mutex_unlock(m); \
}
#else
#define AMS_MUTEX_OEM_LOCK(m) { \
	mutex_lock(m); \
}
#define AMS_MUTEX_OEM_UNLOCK(m) { \
	mutex_unlock(m); \
}
#endif

/* -------------------------------- macros ---------------------------------- */
#ifdef AMS_MUTEX_DEBUG
#define AMS_MUTEX_LOCK(m) { \
	pr_info("%s: Mutex Lock\n", __func__); \
	mutex_lock_interruptible(m); \
  }
#define AMS_MUTEX_UNLOCK(m) { \
	pr_info("%s: Mutex Unlock\n", __func__); \
	mutex_unlock(m); \
	}
#else
#define AMS_MUTEX_LOCK(m) { \
	mutex_lock(m); \
	}
#define AMS_MUTEX_TRYLOCK(m) ({ \
	mutex_trylock(m); \
})
#define AMS_MUTEX_UNLOCK(m) { \
	mutex_unlock(m); \
	}
#endif

/* -------------------------------- structures ------------------------------ */
struct tmf8806_output {
	char frameId;
	char frameNumber;
	char payload_lsb;
	char payload_msb;
	char payload[TMF8806_NUMBER_HISTOGRAMS*TMF8806_DATA_BUFFER_SIZE*2]; /* 2 bytes per bin */
};
enum tof8806_power_state {
	TOF8806_POWER_ON,
	TOF8806_POWER_OFF,
};

union tmf8806_output_frame {
	struct tmf8806_output frame;
	char buf[sizeof(struct tmf8806_output)];
}__attribute__((packed));

struct tmf8806_platform_data {
	const char *tof_name;
	struct gpio_desc *gpiod_interrupt;
	struct gpio_desc *gpiod_enable;
	struct i2c_client *client;
	const char *ram_patch_fname[];
};

struct tof8806_registered_driver_t {
	bool platform_driver;
	bool i2c_driver;
	bool sysfs_registered;
	bool dev_registered;
};

typedef struct _tmf8806_chip
{
	tmf8806Driver tof_core;

	struct i2c_client *client;
	struct mutex lock;
	int driver_debug;
	struct mutex power_lock;
	struct mutex oem_lock;
	struct mutex state_lock;
	struct mutex data_lock;
	struct input_dev *obj_input_dev;
	struct tmf8806_platform_data *pdata;
	u8 shadow[256];
	STRUCT_KFIFO_REC_2(PAGE_SIZE*4) tof_output_fifo;
	struct task_struct *app0_poll_irq;
	int poll_period;
	union tmf8806_output_frame tof_output_frame;
	enum tof8806_power_state   power_status;
	unsigned int xtalk_peak;
	unsigned int xtalk_count;

}tmf8806_chip;

/* -------------------------------- functions ------------------------------- */
/**
 * tof_queue_frame - queue data of buffer into output fifo
 *
 * @tmf8806_chip: tmf8806_chip pointer
 *
 * Returns 0 for no Error, -1 for Error
 */
int tof8806_queue_frame(tmf8806_chip *chip);
extern int cam_tmf8806_driver_init(void);
extern void cam_tmf8806_driver_exit(void);
int do_tmf8806_power_down(tmf8806_chip *chip);
extern int wait_for_tmf8806_ready(void);
extern int tmf8806_stop(void);
extern void tmf8806_clean(void);
int tmf8806_switch_apps(tmf8806_chip *chip, char req_app_id);
int tmf8806_oem_start(void);
#endif
