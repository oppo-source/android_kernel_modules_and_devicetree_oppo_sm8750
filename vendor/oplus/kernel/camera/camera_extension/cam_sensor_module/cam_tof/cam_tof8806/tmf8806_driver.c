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

/*! \file tmf8806_driver.c - TMF8806 driver
 * \brief Device driver for measuring Distance in mm.
 */

/* -------------------------------- includes -------------------------------- */
#include <linux/module.h>
#include <linux/moduleparam.h>
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
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/kfifo.h>
#include <linux/printk.h>
#include <linux/poll.h>

#include "tmf8806_driver.h"
#include "tmf8806_hex_interpreter.h"
#include "tof_pdrv.h"

/* -------------------------------- variables ------------------------------- */
static struct tmf8806_platform_data tmf8806_pdata = {
	.tof_name = "tmf8806",
	.ram_patch_fname = { "mainapp_PATCH_Maxwell.hex", },
};

bool is_8806_alread_probe = 0;
bool is_8806chipid_matched = 0;
bool irq_thread_status = 0;
static bool g_is_alread_runing = 0;
static struct kobject *cam_tof_kobj;
struct gpio_desc *common_gpiod_enable;
static tmf8806_chip * g_tof8806_sensor_chip = NULL;

struct tof8806_registered_driver_t tof8806_registered_driver = { 0, 0, 0, 0 };

/**************************************************************************/
/*  Functions                                                             */
/**************************************************************************/
/**
 * tmf8806_switch_apps - Switch to Bootloader/App0
 *
 * @tmf8806_chip: tmf8806_chip pointer
 * @req_app_id: required application
 *
 * Returns 0 for No Error, -EIO for Error
 */
int tmf8806_switch_apps(tmf8806_chip *chip, char req_app_id)
{
	int error = 0;
	if ((req_app_id != TOF8806_APP_ID_BOOTLOADER) && (req_app_id != TOF8806_APP_ID_APP0))
		return -EIO;

	i2cRxReg(chip, 0, TOF8806_APP_ID_REG, 1, chip->tof_core.dataBuffer);

	if (req_app_id == chip->tof_core.dataBuffer[0]) //if req_app is the current app
		return 0;

	error = i2cTxReg(chip, 0, TOF8806_REQ_APP_ID_REG, 1, &req_app_id);
	if (error) {
		dev_err(&chip->client->dev, "Error setting REQ_APP_ID register.\n");
		error = -EIO;
	}

	switch (req_app_id) {
		case TOF8806_APP_ID_APP0:
			if (error)
			{
				dev_err(&chip->client->dev, "Error switch to App0.\n");
				/* Hard reset back to bootloader if error */
				enablePinLow(chip);
				delayInMicroseconds(5 * 1000);
				enablePinHigh(chip);
				error = tmf8806IsCpuReady(&chip->tof_core, CPU_READY_TIME_MS);
				if (error) {
				  dev_err(&chip->client->dev, "Error waiting for CPU ready flag.\n");
				}
				tmf8806ReadDeviceInfo(&chip->tof_core);
			}
			else
			{
				error = tmf8806IsCpuReady(&chip->tof_core, CPU_READY_TIME_MS);
				if (error == 0) {
					dev_err(&chip->client->dev, "Cpu not Ready.\n");
				}
				error = tmf8806ReadDeviceInfo(&chip->tof_core);
				dev_info(&chip->client->dev, "Running app_id: 0x%02x\n", chip->tof_core.device.appVersion[0]);
				if (chip->tof_core.device.appVersion[0]!= TOF8806_APP_ID_APP0)
				{
				  dev_err(&chip->client->dev, "Error: Unrecognized application.\n");
				  return APP_ERROR_CMD;
				}
			}
			break;
		case TOF8806_APP_ID_BOOTLOADER:
			if (!error)
			{
				error = tmf8806IsCpuReady(&chip->tof_core, CPU_READY_TIME_MS);
				if (error == 0) {
					dev_err(&chip->client->dev, "Cpu not Ready.\n");
				}
				i2cRxReg(chip, 0, TOF8806_APP_ID_REG, 1, chip->tof_core.dataBuffer);

				(chip->tof_core.dataBuffer[0] == TOF8806_APP_ID_BOOTLOADER) ? (error = APP_SUCCESS_OK)
																			: (error = APP_ERROR_CMD);
			}
			break;
		default:
			error = -EIO;
			break;
	}
	return error;
}

/**************************************************************************/
/* Sysfs callbacks                                                        */
/**************************************************************************/
/******** Common show/store functions ********/
#define OPLUS_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute oplus_attr_##_name = __ATTR(_name, _mode, _show, _store)

static ssize_t program_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int error;
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);

	AMS_MUTEX_LOCK(&chip->lock);
	error = i2cRxReg(chip, 0, TOF8806_APP_ID_REG, 1, chip->tof_core.dataBuffer);
	AMS_MUTEX_UNLOCK(&chip->lock);
	if (error) {
		return -EIO;
	}
	return scnprintf(buf, PAGE_SIZE, "%#x\n", chip->tof_core.dataBuffer[0]);
}

static ssize_t program_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	char req_app_id;
	int error;
	sscanf(buf, "%hhx", &req_app_id);
	CAM_EXT_INFO(CAM_EXT_TOF, "%s: requested app: %#x\n", __func__, req_app_id);

	AMS_MUTEX_LOCK(&chip->lock);
	error = tmf8806_switch_apps(chip, req_app_id);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return error ? -EIO : count;
}

static ssize_t chip_enable_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int state;
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (!chip->pdata->gpiod_enable) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -EIO;
	}
	state = gpiod_get_value(chip->pdata->gpiod_enable) ? 1 : 0;
	AMS_MUTEX_UNLOCK(&chip->lock);
	return scnprintf(buf, PAGE_SIZE, "%d\n", state);
}

static ssize_t chip_enable_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int capture;
	int error;

	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	error = sscanf(buf, "%d", &capture);
	if (error != 1)
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);
	if (!chip->pdata->gpiod_enable) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -EIO;
	}
	CAM_EXT_INFO(CAM_EXT_TOF, "capture enable =  %d\n", capture);

	if (capture == 0) {
		if(g_is_alread_runing == 0) {
			AMS_MUTEX_UNLOCK(&chip->lock);
			return 0;
		}
		g_is_alread_runing=0;
		AMS_MUTEX_UNLOCK(&chip->lock);

		AMS_MUTEX_LOCK(&g_tof8806_sensor_chip->state_lock);
		if (irq_thread_status) {
			(void)kthread_stop(chip->app0_poll_irq);
		}
		irq_thread_status = 0;
		AMS_MUTEX_UNLOCK(&g_tof8806_sensor_chip->state_lock);

		AMS_MUTEX_LOCK(&chip->lock);
		if (chip->tof_core.device.appVersion[0] == TOF8806_APP_ID_APP0) {
			if (tmf8806StopMeasurement(&chip->tof_core) != APP_SUCCESS_OK) {
				CAM_EXT_ERR(CAM_EXT_TOF, "Stop Measurement fail");
			}
		}
		// enablePinLow(chip);
		tmf8806_stop();
	}else {
		g_tof8806_sensor_chip->tof_core.measureConfig.data.command = 0;
		if(g_is_alread_runing) {
			AMS_MUTEX_UNLOCK(&chip->lock);
			return 0;
		}
		g_is_alread_runing=1;
		AMS_MUTEX_LOCK(&g_tof8806_sensor_chip->state_lock);
		error = tmf8806_oem_start();
		AMS_MUTEX_UNLOCK(&g_tof8806_sensor_chip->state_lock);
		if(error != 0) {
			CAM_EXT_INFO(CAM_EXT_TOF, "start tof failed");
		}
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return count;
}

static ssize_t driver_debug_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%#hhx\n", chip->tof_core.logLevel);
}

static ssize_t driver_debug_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	sscanf(buf, "%hhx", &chip->tof_core.logLevel);
	return count;
}

static ssize_t program_version_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int len = -1;
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);

	if (tmf8806ReadDeviceInfo(&chip->tof_core) == APP_SUCCESS_OK) {
		len = scnprintf(buf, PAGE_SIZE, "%#x %#x %#x %#x %#x %#x\n",
						chip->tof_core.device.appVersion[0], chip->tof_core.device.appVersion[1],
						chip->tof_core.device.appVersion[2], chip->tof_core.device.appVersion[3],
						chip->tof_core.device.chipVersion[0], chip->tof_core.device.chipVersion[1]);
	}

	AMS_MUTEX_UNLOCK(&chip->lock);
	return len;
}

static ssize_t register_write_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	char preg;
	char pval;
	char pmask = -1;
	int numparams;
	int rc = 0;

	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);

	numparams = sscanf(buf, "%hhx:%hhx:%hhx", &preg, &pval, &pmask);
	if ((numparams < 2) || (numparams > 3))
		return -EINVAL;
	if ((numparams >= 1) && (preg < 0))
		return -EINVAL;
	if ((numparams >= 2) && (preg < 0))
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);
	if (pmask == -1) {
		rc = i2cTxReg(chip, 0, preg,1, &pval);
	} else {
		rc = i2c_write_mask(chip->client, preg, pval, pmask);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return rc ? rc : count;
}

static ssize_t registers_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int per_line = 4; // dumped register values per line
	int len = 0;
	int idx, per_line_idx;
	int bufsize = PAGE_SIZE;
	int error;

	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);

	memset(chip->shadow, 0, MAX_REGS);
	error = i2cRxReg(chip, 0, 0x00, MAX_REGS, chip->shadow);

	if (error) {
		CAM_EXT_ERR(CAM_EXT_TOF, "Read all registers failed: %d\n", error);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return error;
	}

	for (idx = 0; idx < MAX_REGS; idx += per_line) {
		len += scnprintf(buf + len, bufsize - len, "%#02x:", idx);
		for (per_line_idx = 0; per_line_idx < per_line; per_line_idx++) {
			len += scnprintf(buf + len, bufsize - len, " ");
			len += scnprintf(buf + len, bufsize - len, "%#02x", chip->shadow[idx+per_line_idx]);
		}
		len += scnprintf(buf + len, bufsize - len, "\n");
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return len;
}
#if 0
static ssize_t request_ram_patch_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int error = -1;
	const struct firmware *cfg = NULL;
	const u8 *line;
	const u8 *line_end;
	u32 patch_size = 0;
	AMS_MUTEX_OEM_LOCK(&chip->power_lock);
	if(chip->power_status == TOF8806_POWER_OFF) {
		error=tof_power_up();
		if(error != 0) {
			CAM_EXT_ERR(CAM_EXT_TOF, "power up failed.\n");
			AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
			return error;
		} else {
			chip->power_status = TOF8806_POWER_ON;
			CAM_EXT_INFO(CAM_EXT_TOF, "power up success.\n");
		}
	} else {
		CAM_EXT_INFO(CAM_EXT_TOF, "tof have power up.\n");
	}
	AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);

	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);

	// Check if bootloader mode
	i2cRxReg(chip, 0, TOF8806_APP_ID_REG, 1, chip->tof_core.dataBuffer);
	if (chip->tof_core.dataBuffer[0] != TOF8806_APP_ID_BOOTLOADER) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s Download only in Bootloader mode\n", __func__);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}

	// Read in Hex Data
	CAM_EXT_INFO(CAM_EXT_TOF, "Trying firmware: \'%s\'...\n", chip->pdata->ram_patch_fname[0]);
	error = request_firmware_direct(&cfg,chip->pdata->ram_patch_fname[0], kobj_to_dev(kobj));
	if (error || !cfg) {
		CAM_EXT_WARN(CAM_EXT_TOF," FW not available: %d\n", error);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -1;
	}

	intelHexInterpreterInitialise8806( );
	line = cfg->data;
	line_end = line;
	while ((line_end - cfg->data) < cfg->size) {
		line_end = strchrnul(line, '\n');
		patch_size += ((line_end - line) > INTEL_HEX_MIN_RECORD_SIZE) ?
						((line_end - line - INTEL_HEX_MIN_RECORD_SIZE) / 2) : 0;
		error = intelHexHandleRecord8806(chip, line_end - line, line);
		if (error) {
			dev_err(&chip->client->dev, "%s: Ram patch failed: %d\n", __func__, error);
			goto err_fmwdwnl;
		}
		line = ++line_end;
	}

	//  Download Data
	error = tmf8806DownloadFirmware(&chip->tof_core, TMF8806_IMAGE_START, patchImage, imageSize);
	if ( error != BL_SUCCESS_OK) {
		CAM_EXT_ERR(CAM_EXT_TOF, "Download Error %d\n", error);
		goto err_fmwdwnl;
	}

	// Read Device Info
	error = tmf8806ReadDeviceInfo(&chip->tof_core);
	if ( error != APP_SUCCESS_OK) {
		CAM_EXT_ERR(CAM_EXT_TOF, "Read Device Info Error %d\n", error);
		goto err_fmwdwnl;
	}
	tmf8806ClrAndEnableInterrupts(&chip->tof_core, TMF8806_INTERRUPT_RESULT| TMF8806_INTERRUPT_DIAGNOSTIC);
	CAM_EXT_INFO(CAM_EXT_TOF, "Download done\n");

err_fmwdwnl:
	AMS_MUTEX_UNLOCK(&chip->lock);
	return error ? -EIO : count;
}
#endif
// App0 cmd show/store functions
static ssize_t app0_command_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int error, i;
	int len = 0;
	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	error = i2cRxReg(chip, 0, TOF8806_CMD_DATA9_REG, TOF8806_APP0_CMD_IDX, chip->tof_core.dataBuffer);
	AMS_MUTEX_UNLOCK(&chip->lock);
	if (error) {
		return -EIO;
	}
	for (i = 0; i < TOF8806_APP0_CMD_IDX; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%#02x:%#02x\n", i, chip->tof_core.dataBuffer[i]);
	}

	return len;
}

ssize_t tmf8806_app0_meas_cmd_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	ssize_t ret = 0;
	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		 CAM_EXT_INFO(CAM_EXT_TOF, "%s cmd: %s buf %s \n", __func__, attr->attr.name, buf);
	}

	AMS_MUTEX_LOCK(&chip->data_lock);
	if (!strncmp(attr->attr.name,"capture", strlen(attr->attr.name))) {
		ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", chip->tof_core.measureConfig.data.command);
	}
	else if (!strncmp(attr->attr.name,"iterations", strlen(attr->attr.name))) {
		ret += scnprintf(buf, PAGE_SIZE, "%hu\n", chip->tof_core.measureConfig.data.kIters);
	}
	else if (!strncmp(attr->attr.name,"period", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%hhu\n", chip->tof_core.measureConfig.data.repetitionPeriodMs);
	}
	else if (!strncmp(attr->attr.name,"snr", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", *((char *)&chip->tof_core.measureConfig.data.snr));
	}
	else if (!strncmp(attr->attr.name,"capture_delay", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%hhu\n", chip->tof_core.measureConfig.data.daxDelay100us);
	}
	else if (!strncmp(attr->attr.name,"gpio_setting", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", *((char *)&chip->tof_core.measureConfig.data.gpio));
	}
	else if (!strncmp(attr->attr.name,"alg_setting", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", *((char *)&chip->tof_core.measureConfig.data.algo));
	}
	else if (!strncmp(attr->attr.name,"data_setting", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", *((char *)&chip->tof_core.measureConfig.data.data));
	}
	else if (!strncmp(attr->attr.name,"spreadSpecVcsel", strlen(attr->attr.name))) {
		ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", *((char *)&chip->tof_core.measureConfig.data.spreadSpecVcselChp));
	}
	else if (!strncmp(attr->attr.name,"spreadSpecSpad", strlen(attr->attr.name))) {
		 ret += scnprintf(buf, PAGE_SIZE, "%#hhx\n", *((char *)&chip->tof_core.measureConfig.data.spreadSpecSpadChp));
	}
	else {
		ret += scnprintf(buf, PAGE_SIZE, "wrong cmd \n");
	}
	AMS_MUTEX_UNLOCK(&chip->data_lock);
	return ret;
}

static ssize_t capture_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
 {
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t iterations_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t period_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t snr_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t capture_delay_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t gpio_setting_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t alg_setting_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}
/*
static ssize_t data_setting_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}
*/
static ssize_t spreadSpecVcsel_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t spreadSpecSpad_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	return tmf8806_app0_meas_cmd_show(kobj, attr, buf);
}

static ssize_t app0_command_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	int len = 0;
	int error = 0;
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	char bytes[TOF8806_APP0_CMD_IDX];
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);

	len = sscanf(buf, "%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		  &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5],
		  &bytes[6], &bytes[7], &bytes[8], &bytes[9], &bytes[10]);


	if ((len < 1) || (len > TOF8806_APP0_CMD_IDX)) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -EINVAL;
	}


	error = i2cTxReg(chip, 0, TMF8806_COM_CMD_REG + 1 - len, len, &bytes[0]);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return  count;
}

ssize_t tmf8806_app0_cmd_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	ssize_t ret = count;
	int8_t error = APP_SUCCESS_OK;
	char bytes[TOF8806_APP0_CMD_IDX];
	uint32_t tmpIterations = 0;  //Temporary variable to store iterations from userspace
	CAM_EXT_INFO(CAM_EXT_TOF, "%s cmd: %s buf: %s \n", __func__, attr->attr.name, buf);

	AMS_MUTEX_LOCK(&chip->data_lock);

	if (!strncmp(attr->attr.name,"capture", strlen(attr->attr.name)))
	{
		//only for capture attr start and stop measurement
		uint8_t capture_status = 0;
		ret = sscanf(buf, "%hhx", &capture_status);

		//ret = sscanf(buf, "%hhx", &chip->tof_core.measureConfig.data.command);
		CAM_EXT_INFO(CAM_EXT_TOF, "sscanf ret %d capture_status:%d", ret, capture_status);
		if (ret == 1) {
			if (capture_status) {
				if (chip->tof_core.measureConfig.data.command == 0) {
					chip->xtalk_peak = 0;
					chip->xtalk_count = 0;
					chip->tof_core.measureConfig.data.command = capture_status;
					error = tmf8806StartMeasurement(&chip->tof_core);
					CAM_EXT_INFO(CAM_EXT_TOF, "tmf8806StartMeasurement result %d", error);
				} else {
					error = APP_SUCCESS_OK;
					CAM_EXT_INFO(CAM_EXT_TOF, "tmf8806StartMeasurement return ok directly");
				}
			} else {
				chip->tof_core.measureConfig.data.command = 0;
				error = tmf8806StopMeasurement(&chip->tof_core);
				CAM_EXT_INFO(CAM_EXT_TOF, "tmf8806StopMeasurement result %d", error);
			}
			if (error != APP_SUCCESS_OK) {
				AMS_MUTEX_UNLOCK(&chip->data_lock);
				return -EIO;
			}
		}
	}
	else if (!strncmp(attr->attr.name,"iterations", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%u",  &tmpIterations);
		chip->tof_core.measureConfig.data.kIters = tmpIterations/1000;
	}
	else if (!strncmp(attr->attr.name,"period", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhu", &chip->tof_core.measureConfig.data.repetitionPeriodMs);
	}
	else if (!strncmp(attr->attr.name,"snr", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhx", &bytes[0]);
		memcpy(&chip->tof_core.measureConfig.data.snr, bytes, 1);
	}
	else if (!strncmp(attr->attr.name,"capture_delay", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhu", &chip->tof_core.measureConfig.data.daxDelay100us);
	}
	else if (!strncmp(attr->attr.name,"gpio_setting", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhx", &bytes[0]);
		memcpy(&chip->tof_core.measureConfig.data.gpio, bytes, 1);
	}
	else if (!strncmp(attr->attr.name,"alg_setting", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhx", &bytes[0]);
		memcpy(&chip->tof_core.measureConfig.data.algo, bytes, 1);
	}
	else if (!strncmp(attr->attr.name,"data_setting", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhx", &bytes[0]);
		memcpy(&chip->tof_core.measureConfig.data.data, bytes, 1);
	}
	else if (!strncmp(attr->attr.name,"spreadSpecVcsel", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhx", &bytes[0]);
		memcpy(&chip->tof_core.measureConfig.data.spreadSpecVcselChp, bytes, 1);
	}
	else if (!strncmp(attr->attr.name,"spreadSpecSpad", strlen(attr->attr.name))) {
		ret = sscanf(buf, "%hhx", &bytes[0]);
		memcpy(&chip->tof_core.measureConfig.data.spreadSpecSpadChp, bytes, 1);

	}
	else {
		CAM_EXT_INFO(CAM_EXT_TOF, " cmd not found\n");
	}

	AMS_MUTEX_UNLOCK(&chip->data_lock);
	return (ret != 1) ? -EINVAL : count;
}


static ssize_t capture_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t iterations_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t period_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t snr_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t capture_delay_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t gpio_setting_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t alg_setting_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}
/*
static ssize_t data_setting_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}
*/
static ssize_t spreadSpecVcsel_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

static ssize_t spreadSpecSpad_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	return tmf8806_app0_cmd_store(kobj, attr, buf,count);
}

//App0 functions

static ssize_t app0_fac_calib_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	ssize_t ret = -1;
	char byte[TOF8806_FACTORY_CAL_CMD_SIZE];
	uint32_t timeout = 500;
	uint8_t irqs = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	ret = tmf8806FactoryCalibration(&chip->tof_core, TMF8806_FACTORY_CALIB_KITERS);

	if (ret == APP_SUCCESS_OK) {
		delayInMicroseconds(1000000); // needs more then 1 sec.
		while ( irqs == 0 && timeout-- > 0 )
		{
			delayInMicroseconds(10000);
			irqs = tmf8806GetAndClrInterrupts( &chip->tof_core, TMF8806_INTERRUPT_RESULT );
		}

		ret = tmf8806ReadFactoryCalibration(&chip->tof_core);
		if (ret != APP_SUCCESS_OK) {
			CAM_EXT_ERR(CAM_EXT_TOF, "No factory calibration page \n");
		}
		if (timeout == 0) {
			ret = -1;
			CAM_EXT_ERR(CAM_EXT_TOF, " calibration timeout \n");

		}
	}

	if(ret != APP_SUCCESS_OK){
		return ret;
	}

	memcpy(byte, &chip->tof_core.factoryCalib, TOF8806_FACTORY_CAL_CMD_SIZE);
	ret = scnprintf(buf, PAGE_SIZE, "%#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx %#hhx\n",
					byte[0], byte[1], byte[2], byte[3], byte[4], byte[5], byte[6],
					byte[7], byte[8], byte[9], byte[10], byte[11], byte[12], byte[13]);

	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}
/*
static ssize_t distance_thresholds_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	ssize_t ret = -EIO;
	uint8_t pers = 0;
	uint16_t lowThr,highThr = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	if (APP_SUCCESS_OK == tmf8806GetThresholds(&chip->tof_core, &pers, &lowThr, &highThr)) {
		ret = scnprintf(buf, PAGE_SIZE, "%hhu %hu %hu \n", pers, lowThr, highThr);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t app0_fac_calib_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int ret;
	char byte[TOF8806_FACTORY_CAL_CMD_SIZE] = {0};

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	ret = sscanf(buf, "%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		  &byte[0], &byte[1], &byte[2], &byte[3], &byte[4], &byte[5], &byte[6],
		  &byte[7], &byte[8], &byte[9], &byte[10], &byte[11], &byte[12], &byte[13]);

	if (ret == TOF8806_FACTORY_CAL_CMD_SIZE) {
		tmf8806SetFactoryCalibration(&chip->tof_core, (tmf8806FactoryCalibData *) byte);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return (ret != TOF8806_FACTORY_CAL_CMD_SIZE) ? -EINVAL : count;
}

static ssize_t app0_state_data_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int ret;
	char byte[TMF8806_COM_STATE_DATA_COMPRESSED];

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	ret = sscanf(buf, "%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		  &byte[0], &byte[1], &byte[2], &byte[3], &byte[4], &byte[5], &byte[6], &byte[7], &byte[8], &byte[9], &byte[10]);

	if (ret == TMF8806_COM_STATE_DATA_COMPRESSED) {
		tmf8806SetStateData(&chip->tof_core, (tmf8806StateData *) byte);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return (ret != TMF8806_COM_STATE_DATA_COMPRESSED) ? -EINVAL : count;
}

static ssize_t distance_thresholds_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
 {
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int ret;
	uint8_t pers = 0;
	uint16_t lowThr, highThr = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	ret = sscanf(buf, "%hhu %hu %hu", &pers, &lowThr, &highThr);
	if (ret != 3)
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);
	ret = tmf8806SetThresholds(&chip->tof_core, pers, lowThr, highThr);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return (ret != APP_SUCCESS_OK) ? -EIO : count;
}
*/
static ssize_t app0_apply_fac_calib_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
 {
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int8_t ret;
	char byte[TOF8806_FACTORY_CAL_CMD_SIZE] = {0};

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	ret = sscanf(buf, "%hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
		  &byte[0], &byte[1], &byte[2], &byte[3], &byte[4], &byte[5], &byte[6],
		  &byte[7], &byte[8], &byte[9], &byte[10], &byte[11], &byte[12], &byte[13]);

	if (ret == TOF8806_FACTORY_CAL_CMD_SIZE) {
		tmf8806SetFactoryCalibration(&chip->tof_core, (tmf8806FactoryCalibData *) byte);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return (ret != TOF8806_FACTORY_CAL_CMD_SIZE) ? -EINVAL : count;
}
/*
static ssize_t app0_clk_correction_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int ret;
	uint8_t clkenable = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	ret = sscanf(buf, "%hhx ", &clkenable);
	if (ret != 1)
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);
	tmf8806ClkCorrection(&chip->tof_core, clkenable);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return count;
}

static ssize_t app0_histogram_readout_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int ret;
	uint8_t histograms = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	ret = sscanf(buf, "%hhx", &histograms);
	if (ret != 1)
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);
	ret = tmf8806ConfigureHistograms(&chip->tof_core, histograms);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return (ret != APP_SUCCESS_OK) ? -EIO : count;
}
*/
static ssize_t app0_ctrl_reg_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int error, i;
	int len = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	error = i2cRxReg(chip, 0, TOF8806_APP_ID_REG, TOF8806_RESULT_NUMBER_OFFSET, chip->tof_core.dataBuffer);
	AMS_MUTEX_UNLOCK(&chip->lock);
	if (error) {
		return -EIO;
	}
	for (i = 0; i < TOF8806_RESULT_NUMBER_OFFSET; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%#02x:%#02x\n", i, chip->tof_core.dataBuffer[i]);
	}

	return len;
}

static ssize_t app0_osc_trim_show(struct kobject *kobj, struct kobj_attribute * attr, char * buf)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int error, trim = 0 ;
	int len = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	AMS_MUTEX_LOCK(&chip->lock);
	error = tmf8806_oscillator_trim(&chip->tof_core, &trim , 0);
	AMS_MUTEX_UNLOCK(&chip->lock);

	if (error) {
		return -EIO;
	}

	len += scnprintf(buf, PAGE_SIZE, "%d\n", trim);

	return len;
}

static ssize_t app0_osc_trim_store(struct kobject *kobj, struct kobj_attribute * attr, const char * buf, size_t count)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int ret;
	int trim = 0;

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	}

	ret = sscanf(buf, "%d", &trim);
	if (ret != 1)
		return -EINVAL;

	if ((trim > TMF8806_OSC_MAX_TRIM_VAL) || (trim < TMF8806_OSC_MIN_TRIM_VAL)) {
		CAM_EXT_ERR(CAM_EXT_TOF, "%s: Error clk trim setting is out of range [%d,%d]\n", __func__, 1, 511);
		return -EINVAL;
	}

	AMS_MUTEX_LOCK(&chip->lock);
	ret = tmf8806_oscillator_trim(&chip->tof_core, &trim , 1);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return (ret != APP_SUCCESS_OK) ? -EIO : count;
}

static ssize_t app0_read_peak_crosstalk_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	tmf8806_chip *chip = g_tof8806_sensor_chip;

	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->tof_core.device.appVersion[0] != TOF8806_APP_ID_APP0) {
		CAM_EXT_ERR(CAM_EXT_TOF, "%s: Error ToF chip app_id: %#x",
				__func__, chip->tof_core.device.appVersion[0]);
		AMS_MUTEX_UNLOCK(&chip->lock);
		return -EIO;
	}

	CAM_EXT_INFO(CAM_EXT_TOF, "xtalk=%d \n", chip->xtalk_peak);
	len = scnprintf(buf, PAGE_SIZE, "%u\n", chip->xtalk_peak);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return len;
}

// OUTPUT DATA
static ssize_t app0_tof_output_read(struct file * fp, struct kobject * kobj, struct bin_attribute * attr,
									char *buf, loff_t off, size_t size)
{
	struct device *dev = kobj_to_dev(kobj);
	tmf8806_chip *chip = g_tof8806_sensor_chip;
	int read;
	u32 elem_len = 0;

	AMS_MUTEX_LOCK(&chip->lock);
	//elem_len = kfifo_peek_len(&chip->tof_output_fifo);
	dev_dbg(dev, "%s size: %u\n", __func__, (unsigned int) size);
	if (kfifo_len(&chip->tof_output_fifo)) {
		dev_dbg(dev, "fifo read elem_len: %u\n", elem_len);
		read = kfifo_out(&chip->tof_output_fifo, buf, elem_len);
		dev_dbg(dev, "fifo_len: %u\n", kfifo_len(&chip->tof_output_fifo));
		AMS_MUTEX_UNLOCK(&chip->lock);
		return elem_len;
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return 0;
}
/****************************************************************************
 * Common Sysfs Attributes
 * **************************************************************************/
/******* READ-WRITE attributes ******/
static OPLUS_ATTR(program,0644, program_show, program_store);
static OPLUS_ATTR(chip_enable,0644, chip_enable_show, chip_enable_store);
static OPLUS_ATTR(driver_debug,0644, driver_debug_show, driver_debug_store);
//static OPLUS_ATTR(stop_data,0644, stop_data_show, stop_data_store);

/******* READ-ONLY attributes ******/
static OPLUS_ATTR(program_version,0644, program_version_show,NULL);
static OPLUS_ATTR(registers,0644, registers_show,NULL);
/******* WRITE-ONLY attributes ******/
static OPLUS_ATTR(register_write,0644, NULL,register_write_store);
//static OPLUS_ATTR(request_ram_patch,0644, NULL,request_ram_patch_store);

/****************************************************************************
 * Bootloader Sysfs Attributes
 * **************************************************************************/


/****************************************************************************
 * APP0 Sysfs Attributes
 * *************************************************************************/
/******* READ-WRITE attributes ******/
static OPLUS_ATTR(app0_command,0644, app0_command_show, app0_command_store);
static OPLUS_ATTR(capture,0644, capture_show, capture_store);
static OPLUS_ATTR(period,0644, period_show, period_store);
//static OPLUS_ATTR(noise_threshold,0644, noise_threshold_show, noise_threshold_store);
static OPLUS_ATTR(iterations,0644, iterations_show, iterations_store);
static OPLUS_ATTR(capture_delay,0644, capture_delay_show, capture_delay_store);
static OPLUS_ATTR(alg_setting,0644, alg_setting_show, alg_setting_store);
static OPLUS_ATTR(snr,0644, snr_show, snr_store);
static OPLUS_ATTR(spreadSpecVcsel,0644, spreadSpecVcsel_show, spreadSpecVcsel_store);
static OPLUS_ATTR(spreadSpecSpad,0644, spreadSpecSpad_show, spreadSpecSpad_store);
static OPLUS_ATTR(gpio_setting,0644, gpio_setting_show, gpio_setting_store);
//static OPLUS_ATTR(app0_clk_iterations,0644, app0_clk_iterations_show, app0_clk_iterations_store);
//static OPLUS_ATTR(app0_clk_trim_enable,0644, app0_clk_trim_enable_show, app0_clk_trim_enable_store);
static OPLUS_ATTR(app0_clk_trim_set,0644, app0_osc_trim_show, app0_osc_trim_store);
static OPLUS_ATTR(app0_apply_fac_calib,0644, NULL, app0_apply_fac_calib_store);
//static OPLUS_ATTR(app0_apply_config_calib,0644, app0_apply_config_calib_show, app0_apply_config_calib_store);
//static OPLUS_ATTR(app0_apply_state_data,0644, app0_apply_state_data_show, app0_apply_state_data_store);
/******* READ-ONLY attributes ******/
//static OPLUS_ATTR(app0_general_configuration,0644, app0_general_configuration_show,NULL);
static OPLUS_ATTR(app0_ctrl_reg,0644, app0_ctrl_reg_show,NULL);
//static OPLUS_ATTR(app0_temp,0644, app0_temp_show,NULL);
//static OPLUS_ATTR(app0_diag_state_mask,0644, app0_diag_state_mask_show,NULL);
//static OPLUS_ATTR(app0_reflectivity_count,0644, app0_reflectivity_count_show,NULL);
static OPLUS_ATTR(app0_get_fac_calib,0644, app0_fac_calib_show,NULL);
//static OPLUS_ATTR(app0_get_distance,0644, app0_get_distance_show,NULL);
static OPLUS_ATTR(app0_read_peak_crosstalk,0644, app0_read_peak_crosstalk_show,NULL);

/******* WRITE-ONLY attributes ******/
/******* READ-ONLY BINARY attributes ******/
static BIN_ATTR_RO(app0_tof_output, 0);

static struct attribute *tmf8806_common_attrs[] = {
	&oplus_attr_program.attr,
	&oplus_attr_chip_enable.attr,
	&oplus_attr_driver_debug.attr,
	//&oplus_attr_stop_data.attr,
	&oplus_attr_program_version.attr,
	&oplus_attr_registers.attr,
	&oplus_attr_register_write.attr,
	//&oplus_attr_request_ram_patch.attr,
	NULL,
};

static struct attribute *tmf8806_app0_attrs[] = {
	&oplus_attr_app0_command.attr,
	&oplus_attr_capture.attr,
	&oplus_attr_period.attr,
	&oplus_attr_iterations.attr,
	//&oplus_attr_noise_threshold.attr,
	&oplus_attr_capture_delay.attr,
	&oplus_attr_alg_setting.attr,
	&oplus_attr_gpio_setting.attr,
	&oplus_attr_snr.attr,
	&oplus_attr_spreadSpecVcsel.attr,
	&oplus_attr_spreadSpecSpad.attr,
	//&oplus_attr_app0_clk_iterations.attr,
	//&oplus_attr_app0_clk_trim_enable.attr,
	&oplus_attr_app0_clk_trim_set.attr,
	//&oplus_attr_app0_diag_state_mask.attr,
	//&oplus_attr_app0_general_configuration.attr,
	&oplus_attr_app0_ctrl_reg.attr,
	//&oplus_attr_app0_temp.attr,
	//&oplus_attr_app0_reflectivity_count.attr,
	&oplus_attr_app0_get_fac_calib.attr,
	&oplus_attr_app0_apply_fac_calib.attr,
	//&oplus_attr_app0_apply_config_calib.attr,
	//&oplus_attr_app0_apply_state_data.attr,
	&oplus_attr_app0_read_peak_crosstalk.attr,
	//&oplus_attr_app0_get_distance.attr,
	NULL,
};
static struct bin_attribute *tof_app0_bin_attrs[] = {
	&bin_attr_app0_tof_output,
	NULL,
};
static const struct attribute_group tmf8806_common_attr_group = {
	.attrs = tmf8806_common_attrs,
};

static const struct attribute_group tmf8806_app0_attr_group = {
	.name = "app0",
	.attrs = tmf8806_app0_attrs,
	.bin_attrs = tof_app0_bin_attrs,
};
static const struct attribute_group *tmf8806_attr_groups[] = {
	&tmf8806_common_attr_group,
	&tmf8806_app0_attr_group,
	NULL,
};


/**************************************************************************/
/* Probe Remove Functions                                                 */
/**************************************************************************/
int tof8806_queue_frame(tmf8806_chip *chip)
{
	int result = 0;
	int size = (chip->tof_output_frame.frame.payload_msb << 8) + chip->tof_output_frame.frame.payload_lsb + 4 ;

	result = kfifo_in(&chip->tof_output_fifo, chip->tof_output_frame.buf, size);
	if (chip->tof_core.logLevel >= TMF8806_LOG_LEVEL_INFO) {

		dev_info(&chip->client->dev, "Size %x\n",size);
		dev_info(&chip->client->dev, "Fr Num %x\n",chip->tof_output_frame.frame.frameNumber);
	}
	if (result == 0) {
		if (chip->tof_core.logLevel >= TMF8806_LOG_LEVEL_INFO) {
			dev_info(&chip->client->dev, "Reset output frame.\n");
		}
		kfifo_reset(&chip->tof_output_fifo);
		result = kfifo_in(&chip->tof_output_fifo, chip->tof_output_frame.buf, size);
		if (result == 0) {
			dev_err(&chip->client->dev, "Error: queueing ToF output frame.\n");
		}
		if (result != size) {
			dev_err(&chip->client->dev, "Error: queueing ToF output frame Size.\n");
		}
	}
	chip->tof_output_frame.frame.frameId = 0;
	chip->tof_output_frame.frame.payload_lsb = 0;
	chip->tof_output_frame.frame.payload_msb = 0;
	chip->tof_output_frame.frame.frameNumber++;

	return (result == size) ? 0 : -1;
}

/**
 * tof_get_gpio_config - Get GPIO config from Device-Managed API
 *
 * @tmf8806_chip: tmf8806_chip pointer
 */
static int tof_get_gpio_config(tmf8806_chip *tof_chip)
{
	int error;
	struct device *dev;
	struct gpio_desc *gpiod;

	if (!tof_chip->client) {
		return -EINVAL;
	}
	dev = &tof_chip->client->dev;

	/* Get the enable line GPIO pin number */
	gpiod = devm_gpiod_get_optional(dev, TOF_GPIO_ENABLE_NAME, GPIOD_OUT_HIGH);
	if (IS_ERR(gpiod)) {
		error = PTR_ERR(gpiod);
		return error;
	}
	tof_chip->pdata->gpiod_enable = gpiod;

	/* Get the interrupt GPIO pin number */
	gpiod = devm_gpiod_get_optional(dev, TOF_GPIO_INT_NAME, GPIOD_IN);
/*
	if (IS_ERR(gpiod)) {
		error = PTR_ERR(gpiod);
		dev_info(&tof_chip->client->dev, "Error: Irq. %d \n", error);
		if (PTR_ERR(gpiod) != -EBUSY) { //for ICAM on this pin there is an Error (from ACPI), but seems to be working
			return error;
		}
	}
	tof_chip->pdata->gpiod_interrupt = gpiod;
*/
	return 0;
}

/**
 * tof_irq_handler - The IRQ handler
 *
 * @irq: interrupt number.
 * @dev_id: private data pointer.
 *
 * Returns IRQ_HANDLED
 */
static irqreturn_t tof_irq_handler(int irq, void *dev_id)
{
	tmf8806_chip *chip = (tmf8806_chip *)dev_id;
	int size;
	AMS_MUTEX_LOCK(&chip->lock);

	if (chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE) {
		CAM_EXT_INFO(CAM_EXT_TOF, "irq_handler\n");
	}

	size = tmf8806_app_process_irq(&chip->tof_core);

	if (size == 0) {
		AMS_MUTEX_UNLOCK(&chip->lock);
		return IRQ_HANDLED;
	}

	/* Alert user space of changes */
	sysfs_notify(&chip->client->dev.kobj, tmf8806_app0_attr_group.name, bin_attr_app0_tof_output.attr.name);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return IRQ_HANDLED;
}

/**
 * tof_request_irq - request IRQ for given gpio
 *
 * @tof_chip: tmf8806_chip pointer
 *
 * Returns status of function devm_request_threaded_irq
 */
#if 0
static int tof_request_irq(tmf8806_chip *tof_chip)
{
	int irq = tof_chip->client->irq;
	unsigned long default_trigger = irqd_get_trigger_type(irq_get_irq_data(irq));
	dev_info(&tof_chip->client->dev, "irq: %d, trigger_type: %lu", irq, default_trigger);
	return devm_request_threaded_irq(&tof_chip->client->dev, tof_chip->client->irq, NULL, tof_irq_handler,
									default_trigger | IRQF_SHARED | IRQF_ONESHOT, tof_chip->client->name, tof_chip);
}
#endif
int do_tmf8806_power_down(tmf8806_chip *chip)
{
	int rc=0;

	CAM_EXT_INFO(CAM_EXT_TOF, "tof_power_down E");

	AMS_MUTEX_OEM_LOCK(&chip->power_lock);
	if(chip->power_status == TOF8806_POWER_ON) {
		gpiod_set_value(chip->pdata->gpiod_enable, 0);
		msleep(1);
		rc = tof_power_down();
		if(rc != 0) {
			CAM_EXT_ERR(CAM_EXT_TOF, "power down failed");
			AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
			return rc;
		}
		CAM_EXT_INFO(CAM_EXT_TOF, "power down success");
		chip->power_status = TOF8806_POWER_OFF;
	} else {
		CAM_EXT_INFO(CAM_EXT_TOF, "donot need do power down,power status=%d",chip->power_status);
	}
	AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
	return 0;
}

int tmf8806_stop(void)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;

	if (!chip) {
		CAM_EXT_ERR(CAM_EXT_TOF, "g_tof_sensor_chip is NULL");
		return -EINVAL;
	}

	if (is_8806_alread_probe == 1) {
		AMS_MUTEX_OEM_LOCK(&chip->oem_lock);
		tmf8806StopMeasurement(&chip->tof_core);
		CAM_EXT_INFO(CAM_EXT_TOF, "tof stop capture ");
		AMS_MUTEX_OEM_UNLOCK(&chip->oem_lock);
		g_is_alread_runing=0;
	}

	do_tmf8806_power_down(chip);

	return 0;
}
EXPORT_SYMBOL(tmf8806_stop);

void tmf8806_clean(void)
{
	tmf8806_chip *chip = g_tof8806_sensor_chip;

	if (NULL != chip && is_8806_alread_probe) {

		if (chip->tof_core.measureConfig.data.command != 0) {
			chip->tof_core.measureConfig.data.command = 0;
			tmf8806StopMeasurement(&chip->tof_core);
		}

		if (chip->poll_period && irq_thread_status) {
			(void)kthread_stop(chip->app0_poll_irq);
			irq_thread_status = 0;
		}

		g_is_alread_runing = 0;

		do_tmf8806_power_down(chip);
	}
}
EXPORT_SYMBOL(tmf8806_clean);

static int tmf8806_input_dev_open(struct input_dev *dev)
{
	tmf8806_chip *chip = input_get_drvdata(dev);
	int error = 0;
	CAM_EXT_INFO(CAM_EXT_TOF, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->pdata->gpiod_enable && (gpiod_get_value(chip->pdata->gpiod_enable) == 0)) {
		/* enable the chip */
		error = gpiod_direction_output(chip->pdata->gpiod_enable, 1);
		if (error) {
			CAM_EXT_ERR(CAM_EXT_TOF, "Chip enable failed.\n");
			AMS_MUTEX_UNLOCK(&chip->lock);
			return -EIO;
		}
	}
	AMS_MUTEX_UNLOCK(&chip->lock);
	return error;
}
int wait_for_tmf8806_ready(void)
{
	struct i2c_client *client = tmf8806_pdata.client;
	tmf8806_chip *chip = (tmf8806_chip *)i2c_get_clientdata(client);

	int error = 0;
	uint8_t app_id[1] = {TOF8806_APP_ID_APP0};

	AMS_MUTEX_OEM_LOCK(&chip->power_lock);
	if(chip->power_status == TOF8806_POWER_OFF) {
		error=tof_power_up();
		if(error != 0) {
			CAM_EXT_ERR(CAM_EXT_TOF, "power up failed.\n");
			AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
			return error;
		} else {
			chip->power_status = TOF8806_POWER_ON;
			CAM_EXT_INFO(CAM_EXT_TOF, "power up success.\n");
		}
	} else {
		CAM_EXT_INFO(CAM_EXT_TOF, "tof have power up.\n");
	}
	AMS_MUTEX_OEM_UNLOCK(&chip->power_lock);
	if (chip != NULL
		&& tmf8806_pdata.client != NULL) {
		is_8806_alread_probe = 0 ;
		if (writePin( chip->pdata->gpiod_enable, 1)) {
			CAM_EXT_ERR(CAM_EXT_TOF, "Chip enable failed.\n");
			goto gpio_err;
		}
		msleep(20);
		CAM_EXT_INFO(CAM_EXT_TOF, "wait_for_tof_ready ok.\n");
		/***** Wait until ToF is ready for commands *****/
		tmf8806Initialise(&chip->tof_core, APP_LOG_LEVEL);
		delayInMicroseconds(ENABLE_TIME_MS * 1000);
		tmf8806Wakeup(&chip->tof_core);

		if (tmf8806IsCpuReady(&chip->tof_core, CPU_READY_TIME_MS) == 0) {
			CAM_EXT_ERR(CAM_EXT_TOF, "I2C communication failure,CPU is not ready.\n");
			goto gen_err;
		}
		if(is_8806chipid_matched){
			is_8806_alread_probe = 1 ;
			CAM_EXT_INFO(CAM_EXT_TOF, "Caemra Tof iic communication ok.\n");
			//set input device
			if(tof8806_registered_driver.dev_registered == FALSE){
				chip->obj_input_dev = devm_input_allocate_device(&client->dev);
				if (chip->obj_input_dev == NULL) {
					CAM_EXT_ERR(CAM_EXT_TOF, "Error allocating input_dev.\n");
					goto input_dev_alloc_err;
				}
				chip->obj_input_dev->name = chip->pdata->tof_name;
				chip->obj_input_dev->id.bustype = BUS_I2C;
				input_set_drvdata(chip->obj_input_dev, chip);
				chip->obj_input_dev->open = tmf8806_input_dev_open;
				set_bit(EV_ABS, chip->obj_input_dev->evbit);
				input_set_abs_params(chip->obj_input_dev, ABS_DISTANCE, 0, 0xFF, 0, 0);
				error = input_register_device(chip->obj_input_dev);
				tof8806_registered_driver.dev_registered = TRUE;
				if (error) {
					CAM_EXT_ERR(CAM_EXT_TOF, "Error registering input_dev.\n");
					goto input_reg_err;
				}
				CAM_EXT_INFO(CAM_EXT_TOF, " register input_dev.\n");
			}
			if (i2cTxReg(chip, 0, TOF8806_REQ_APP_ID_REG, 1, app_id)) {
			  dev_err(&chip->client->dev, "Error setting REQ_APP_ID register.\n");

			}

			if (tmf8806IsApp0Ready(&chip->tof_core, 20) != 1) {
				dev_err(&client->dev, "App0 not ready.\n");
				error = -EIO; // in case of error
				goto gen_err;
			}
			if(tof8806_registered_driver.sysfs_registered == FALSE){
			//set sysfs group
				cam_tof_kobj = kobject_create_and_add("tof_control", kernel_kobj);

				error = sysfs_create_groups(cam_tof_kobj, tmf8806_attr_groups);
				tof8806_registered_driver.sysfs_registered = TRUE;
				if (error) {
					CAM_EXT_ERR(CAM_EXT_TOF, "Error creating sysfs attribute group.\n");
					goto sysfs_err;
				}
				CAM_EXT_INFO(CAM_EXT_TOF, " register sysfs attribute group.\n");
			}
		}else{
			/*if(tof8806_registered_driver.sysfs_registered){
				sysfs_remove_groups(&client->dev.kobj, (const struct attribute_group **)&tmf8806_attr_groups);
				tof8806_registered_driver.sysfs_registered = FALSE;
			}
			if(tof8806_registered_driver.dev_registered){
				input_unregister_device(chip->obj_input_dev);
				tof8806_registered_driver.dev_registered = FALSE;
			}*/
			CAM_EXT_ERR(CAM_EXT_TOF, "chipid not matched, input_dev and sysfs attribute group are not registed.\n");
			error = -EIO; // in case of error
			goto gen_err;
		}
	} else {
		CAM_EXT_INFO(CAM_EXT_TOF, "No probe tof8806 i2c device or have probe done ,check it\n");
	}
	do_tmf8806_power_down(chip);

	return 0;

sysfs_err:
	if(tof8806_registered_driver.sysfs_registered){
		sysfs_remove_groups(cam_tof_kobj, tmf8806_attr_groups);
		tof8806_registered_driver.sysfs_registered = FALSE;
	}
input_dev_alloc_err:
input_reg_err:
	if(tof8806_registered_driver.dev_registered == TRUE){
		input_unregister_device(chip->obj_input_dev);
		tof8806_registered_driver.dev_registered = FALSE;
	}
gen_err:
gpio_err:
	do_tmf8806_power_down(chip);
	i2c_set_clientdata(client, NULL);
	enablePinLow(chip);
	CAM_EXT_ERR(CAM_EXT_TOF, "Probe failed.\n");

	return error;
}
EXPORT_SYMBOL(wait_for_tmf8806_ready);

/**
 * tof8806_app0_poll_irq_thread -
 *
 * @tof_chip: tmf8806_chip pointer
 *
 * Returns 0
 */
static int tof8806_app0_poll_irq_thread(void *tof_chip)
{
	tmf8806_chip *chip = (tmf8806_chip *)tof_chip;
	int us_sleep = 0;
	int rc = 0;
	AMS_MUTEX_LOCK(&chip->data_lock);
	// Poll period is interpreted in units of 100 usec
	us_sleep = chip->tof_core.measureConfig.data.repetitionPeriodMs * 1000;
	dev_info(&chip->client->dev, "Starting ToF irq polling thread, period: %u us\n", us_sleep);
	AMS_MUTEX_UNLOCK(&chip->data_lock);
	while (!kthread_should_stop()) {
		rc = tof_irq_handler(0, tof_chip);
		delayInMicroseconds(us_sleep);
		if(rc == -EINVAL){
			CAM_EXT_INFO(CAM_EXT_TOF, "tof iic error but umd thread not exit \n");
			input_event(chip->obj_input_dev, EV_ABS, ABS_DISTANCE,0);
			input_sync(chip->obj_input_dev);
			input_event(chip->obj_input_dev, EV_ABS, ABS_DISTANCE,1);
			input_sync(chip->obj_input_dev);
		}
		usleep_range(us_sleep, us_sleep + us_sleep/10);
		CAM_EXT_INFO(CAM_EXT_TOF, "ToF irq polling thread running");
	}
	return 0;
}
int tmf8806_oem_start(void)
{
	int error = 0;
	if(g_tof8806_sensor_chip->power_status == TOF8806_POWER_OFF) {
		AMS_MUTEX_OEM_LOCK(&g_tof8806_sensor_chip->power_lock);
		error=tof_power_up();
		gpiod_set_value(g_tof8806_sensor_chip->pdata->gpiod_enable, 1);
		msleep(10);

		if(error != 0) {
			CAM_EXT_ERR(CAM_EXT_TOF, "power up failed.\n");
			AMS_MUTEX_OEM_UNLOCK(&g_tof8806_sensor_chip->power_lock);
			return error;
		} else {
			g_tof8806_sensor_chip->power_status = TOF8806_POWER_ON;
			CAM_EXT_INFO(CAM_EXT_TOF, "power up success.\n");
		}
	} else {
		CAM_EXT_INFO(CAM_EXT_TOF, "tof have power up.\n");
	}
	AMS_MUTEX_OEM_UNLOCK(&g_tof8806_sensor_chip->power_lock);

	enablePinHigh(g_tof8806_sensor_chip) ;
	delayInMicroseconds(ENABLE_TIME_MS * 1000);
	tmf8806Wakeup(&g_tof8806_sensor_chip->tof_core);
	error = tmf8806IsCpuReady(&g_tof8806_sensor_chip->tof_core, CPU_READY_TIME_MS);

	if (error == 0) {
		CAM_EXT_ERR(CAM_EXT_TOF, "CPU not ready");
		AMS_MUTEX_UNLOCK(&g_tof8806_sensor_chip->lock);
		return -EIO;
	}

	if(tmf8806SwitchToRomApplication(&g_tof8806_sensor_chip->tof_core) != BL_SUCCESS_OK){
		CAM_EXT_ERR(CAM_EXT_TOF, "tmf8806SwitchToRomApplication fail");
		AMS_MUTEX_UNLOCK(&g_tof8806_sensor_chip->lock);
		return -EIO;
	}

	if (irq_thread_status == 0) {
		g_tof8806_sensor_chip->app0_poll_irq = kthread_run(tof8806_app0_poll_irq_thread, (void *)g_tof8806_sensor_chip, "tof-irq_poll");
		if (IS_ERR(g_tof8806_sensor_chip->app0_poll_irq)) {
			CAM_EXT_ERR(CAM_EXT_TOF, "Error starting IRQ polling thread.\n");
			error = PTR_ERR(g_tof8806_sensor_chip->app0_poll_irq);
			return -EIO;
		}
	}else {
		CAM_EXT_INFO(CAM_EXT_TOF, "starting IRQ polling thread, thread already start \n");
	}
	irq_thread_status = 1;
	return 0;
}

static int tmf8806_probe(struct i2c_client *client)
{
	tmf8806_chip *chip;
	int error = 0;
	void *poll_prop_ptr = NULL;

	/* Check I2C functionality */
	CAM_EXT_INFO(CAM_EXT_TOF, "I2C Address: %#04x\n", client->addr);
	client->addr = 0x41;
	CAM_EXT_INFO(CAM_EXT_TOF, "I2C Address overrided: %#04x\n", client->addr);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
	  CAM_EXT_ERR(CAM_EXT_TOF, "I2C check functionality failed.\n");
	  return -ENXIO;
	}

	/* Memory Alloc */
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	g_tof8806_sensor_chip = chip;
	if (!chip) {
		  CAM_EXT_ERR(CAM_EXT_TOF, "Mem kzalloc failed. \n");
		  return -ENOMEM;
	}

	/* Platform Setup */
	mutex_init(&chip->lock);
	mutex_init(&chip->oem_lock);
	mutex_init(&chip->power_lock);
	mutex_init(&chip->state_lock);
	mutex_init(&chip->data_lock);

	client->dev.platform_data = (void *)&tmf8806_pdata;
	chip->client = client;
	chip->pdata = &tmf8806_pdata;
	tmf8806_pdata.client = client;
	chip->power_status = TOF8806_POWER_OFF;
	i2c_set_clientdata(client, chip);

	//initialize kfifo for frame output
	INIT_KFIFO(chip->tof_output_fifo);
	chip->tof_output_frame.frame.frameId = 0;
	chip->tof_output_frame.frame.frameNumber = 0;
	chip->tof_output_frame.frame.payload_lsb = 0;
	chip->tof_output_frame.frame.payload_msb = 0;
	chip->tof_core.measureConfig.data.command = 0;
	/* Setup IRQ Handling */
	poll_prop_ptr = (void *)of_get_property(g_tof8806_sensor_chip->client->dev.of_node, TOF_PROP_NAME_POLLIO, NULL);
	g_tof8806_sensor_chip->poll_period = poll_prop_ptr ? be32_to_cpup(poll_prop_ptr) : 0;



	// Set ChipEnable HIGH
	error = tof_get_gpio_config(chip);
	if (error) {
		CAM_EXT_ERR(CAM_EXT_TOF, "Error gpio config.\n");
		goto gpio_err;
	}
	common_gpiod_enable = chip->pdata->gpiod_enable;

	if (writePin( chip->pdata->gpiod_enable, 1)) {
		CAM_EXT_ERR(CAM_EXT_TOF, "Chip enable failed.\n");
		goto gpio_err;
	}

	//set sysfs group
/*	cam_tof_kobj = kobject_create_and_add("tof8806_control", kernel_kobj);

	error = sysfs_create_groups(cam_tof_kobj, tmf8806_attr_groups);
	if (error) {
		CAM_EXT_ERR(CAM_EXT_TOF, "Error creating sysfs attribute group.\n");
		goto sysfs_err;
	}
	tof8806_registered_driver.sysfs_registered = TRUE;
	CAM_EXT_INFO(CAM_EXT_TOF, " register sysfs attribute group.\n");*/
	CAM_EXT_INFO(CAM_EXT_TOF, "Probe ok.\n");

	//tmf8806_stop();
	error = wait_for_tmf8806_ready();
	if(error == 0){
		CAM_EXT_INFO(CAM_EXT_TOF,"Start tof8806 ready ,ret = %d",error);
	}

	return 0;

	/* Probe error handling */
/*
sysfs_err:
	sysfs_remove_groups(cam_tof_kobj, tmf8806_attr_groups);
	tof8806_registered_driver.sysfs_registered = FALSE;
	*/
gpio_err:
	enablePinLow(chip);
	i2c_set_clientdata(client, NULL);
	CAM_EXT_ERR(CAM_EXT_TOF, "Probe failed.\n");

	return error;
}

static void tmf8806_remove(struct i2c_client *client)
{
	tmf8806_chip *chip = i2c_get_clientdata(client);

	tmf8806StopMeasurement(&chip->tof_core);

	if (chip->pdata->gpiod_interrupt != 0 && (PTR_ERR(chip->pdata->gpiod_interrupt) != -EBUSY)) {
		dev_info(&client->dev, "clear gpio irqdata %s\n", __func__);
		devm_free_irq(&client->dev, client->irq, chip);
		dev_info(&client->dev, "put %s\n", __func__);
		devm_gpiod_put(&client->dev, chip->pdata->gpiod_interrupt);
	}
	if (chip->poll_period && irq_thread_status) {
		(void)kthread_stop(chip->app0_poll_irq);
	}
	if (chip->pdata->gpiod_enable)
	{
		dev_info(&client->dev, "clear gpio enable %s\n", __func__);
		gpiod_direction_output(chip->pdata->gpiod_enable, 0);
		devm_gpiod_put(&client->dev, chip->pdata->gpiod_enable);
	}
	dev_info(&client->dev, "clear sys attr %s\n", __func__);

	if(tof8806_registered_driver.sysfs_registered){
		sysfs_remove_groups(&client->dev.kobj, (const struct attribute_group **)&tmf8806_attr_groups);
		tof8806_registered_driver.sysfs_registered = FALSE;
	}
	if(tof8806_registered_driver.dev_registered){
		input_unregister_device(chip->obj_input_dev);
		tof8806_registered_driver.dev_registered = FALSE;
	}
	CAM_EXT_INFO(CAM_EXT_TOF, "%s", __func__);
	do_tmf8806_power_down(chip);
	i2c_set_clientdata(client, NULL);
}

/**************************************************************************/
/* Linux Driver Specific Code                                             */
/**************************************************************************/
static struct i2c_device_id tmf8806_idtable[] = {
	{ "tof8806", 0 },
	{ }
};

static const struct of_device_id tmf8806_of_match[] = {
	{ .compatible = "ams,tof8806" },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tmf8806_idtable);
MODULE_DEVICE_TABLE(of, tmf8806_of_match);
static struct i2c_driver tmf8806_driver = {
	.driver = {
		.name = "ams-OSRAM tmf8806",
		.of_match_table = of_match_ptr(tmf8806_of_match),
	},
	.id_table = tmf8806_idtable,
	.probe = tmf8806_probe,
	.remove = tmf8806_remove,
};

int cam_tmf8806_driver_init(void)
{
	int rc;

	rc = platform_driver_register(&tof8806_pltf_driver);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_TOF, "platform_driver_register failed rc = %d", rc);
		return rc;
	}
	CAM_EXT_INFO(CAM_EXT_TOF, "platform_driver_registered");
	tof8806_registered_driver.platform_driver = 1;

	rc = i2c_add_driver(&tmf8806_driver);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_TOF, "i2c_add_driver failed rc = %d", rc);
		return rc;
	}
	CAM_EXT_INFO(CAM_EXT_TOF, "i2c_add_driver added");

	tof8806_registered_driver.i2c_driver = 1;
	return 0;
}

void cam_tmf8806_driver_exit(void)
{
	if (tof8806_registered_driver.i2c_driver) {
		i2c_del_driver(&tmf8806_driver);
		tof8806_registered_driver.i2c_driver = 0;
	}

	if (tof8806_registered_driver.platform_driver) {
		platform_driver_unregister(&tof8806_pltf_driver);
		tof8806_registered_driver.platform_driver = 0;
	}

}
//module_i2c_driver(tmf8806_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ams-OSRAM AG TMF8806 ToF sensor driver");
MODULE_VERSION("1.3");
