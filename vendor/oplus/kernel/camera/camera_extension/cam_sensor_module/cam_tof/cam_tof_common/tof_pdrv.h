/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
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

/***** tof8801_pdrv.h *****/

#ifndef TOF_PDRV_H
#define TOF_PDRV_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/kfifo.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include "tof8801.h"
#include "tmf8806.h"
#include "cam_context.h"
#include "cam_sensor_io_custom.h"
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>


#define TOF_ASSERT_PARA(expr) \
	{ \
		if (!(expr)) \
		{ \
			CAM_EXT_ERR(CAM_EXT_TOF, "null pointer or invalid para"); \
			return -EINVAL; \
		} \
	}
/*
#define EXIT_ON_ACQUIRE_TOF_FAILURE \
	CAM_ERR(CAM_EXT_TOF, "%s EXIT_ON_ACQUIRE_TOF \n",__func__); \
	if (tof_cci_i2c_acquire()) \
	{ \
		CAM_ERR(CAM_EXT_TOF, "%s fail to acquire tof cci i2c.\n",__func__); \
		return -EIO; \
	}

#define EXIT_ON_RELEASE_TOF_FAILURE \
	CAM_ERR(CAM_EXT_TOF, "%s EXIT_ON_RELEASE_TOF \n",__func__); \
	if (tof_cci_i2c_release()) \
	{ \
		CAM_ERR(CAM_EXT_TOF, "fail to release tof cci i2c.\n"); \
		return -EIO; \
	}
*/

#define MAX_CCI_WRITE_DATA_NUM 128
extern int32_t cam_cci_control_interface(void*);
extern int cam_soc_util_get_dt_properties(struct cam_hw_soc_info *soc_info);
enum tof_cci_operations {
	CAMERA_CCI_INIT,
	CAMERA_CCI_RELEASE,
	CAMERA_CCI_READ,
	CAMERA_CCI_WRITE,
};

struct tof_cci_transfer {
	int cmd;
	uint16_t addr;
	uint8_t *data;
	uint16_t count;
};
enum cam_tof_state {
	CAM_TOF_ACQUIRED,
	CAM_TOF_RELEASED,
};

enum tof_chip_id {
	TOF8806_CHIP_ID = 0x09,
	TOF8801_CHIP_ID = 0x07,
};

struct cam_tof_ctrl_t {
	char device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct mutex mutex;
	struct mutex cci_i2c_mutex;
	struct platform_device *pdev;
	struct camera_io_master io_master_info;
	struct cam_hw_soc_info soc_info;
	struct cam_sensor_power_ctrl_t power_info;
	enum cam_tof_state state;
	struct regulator *vdd;
	/* continous cci write buf, use a static allocated buf
	to avoid repeating alloc/free */
	uint32_t w_buf[sizeof(struct cam_sensor_i2c_reg_array) * MAX_CCI_WRITE_DATA_NUM];

};

extern struct platform_driver tof8801_pltf_driver;
extern struct platform_driver tof8806_pltf_driver;

extern int is_tof_use_cci_i2c(void);
extern int tof_cci_i2c_acquire(void);
extern int tof_cci_i2c_release(void);
extern int tof_cci_i2c_read(char reg, char *buf, int len);
extern int tof_cci_i2c_write(char reg, char *buf, int len);
extern int tof_power_up(void);
extern int tof_power_down(void);

#endif  /* __TOF_PDRV_H */

