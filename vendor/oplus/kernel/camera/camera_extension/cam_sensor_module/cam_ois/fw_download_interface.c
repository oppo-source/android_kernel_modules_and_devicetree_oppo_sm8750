#include <linux/kfifo.h>
#include <asm/arch_timer.h>
#include "fw_download_interface.h"
#include "linux/proc_fs.h"
#include "BU24721/bu24721_fw.h"
#include "SEM1217S/sem1217_fw.h"
#include "DW9786/dw9786_fw.h"
#include "cam_debug.h"
#include "cam_ois_custom.h"
#include "cam_sensor_io_custom.h"
#include "cam_sensor_util_custom.h"
#include "cam_monitor.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#ifndef _STRUCT_TIMESPEC
struct timeval {
	long tv_sec;
	long tv_usec;
};
#endif
#endif

extern uint8_t WrGyroGainData( uint8_t UcMode );
extern uint8_t WrGyroGainData_LS( uint8_t UcMode );
extern uint8_t WrGyAcOffsetData( uint8_t UcMode );
extern uint32_t MeasGyAcOffset(  void  );
extern uint32_t MeasGyAcOffset(  void  );
extern void Gyro_gain_set(struct cam_ois_ctrl_t *o_ctrl, OIS_UWORD X_gain, OIS_UWORD Y_gain);

extern void Update_Gyro_offset_gain_cal_from_flash(struct cam_ois_ctrl_t *o_ctrl);

extern int32_t camera_io_dev_read_seq(struct camera_io_master *io_master_info,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type, int32_t num_bytes);
extern int32_t camera_io_dev_read(struct camera_io_master *io_master_info,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing);
extern int32_t camera_io_dev_write(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting);
extern int32_t camera_io_dev_write_continuous(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag);

#define MAX_DATA_NUM 64
#define MAX_SEM1217S_DATA_NUM 128

struct mutex ois_mutex;
struct cam_ois_ctrl_t *sem1217s_ois_ctrl = NULL;
struct cam_ois_ctrl_t *ois_ctrls[CAM_OIS_TYPE_MAX] = {NULL};
struct proc_dir_entry *face_common_dir = NULL;
struct proc_dir_entry *proc_file_entry = NULL;
struct proc_dir_entry *proc_file_entry_tele = NULL;

#define OIS_REGISTER_SIZE 100
#define OIS_READ_REGISTER_DELAY 10
#define COMMAND_SIZE 255
static struct kobject *cam_ois_kobj;
bool dump_ois_registers = false;

/*
* ois bu24721 info
* {0x000,     0x0000,      0x0000     0x00/0x01}
* reg addr,   reg value,   reg type   read/write
*/

uint32_t ois_registers_bu24721[OIS_REGISTER_SIZE][4] = {

	{0xF000, 0x0000, 0x0001, 0x00},  // OIS IC Info
	{0xF004, 0x0000, 0x0001, 0x00},  // OIS IC Version
	{0xF01C, 0x0000, 0x0004, 0x00},  // OIS Program ID
	{0xF0E2, 0x0000, 0x0002, 0x00},  // OIS Gyro X
	{0xF0E4, 0x0000, 0x0002, 0x00},  // OIS Gyro Y
	{0xF09A, 0x0000, 0x0001, 0x00},  // OIS Sync Enable
	{0xF200, 0x0000, 0x0001, 0x00},  // Read Hall Data
	{0xF0AA, 0x0000, 0x0001, 0x00},  // OIS Centering Amount
	{0xF020, 0x0000, 0x0001, 0x00},  // OIS Control
	{0xF021, 0x0000, 0x0001, 0x00},  // OIS Mode
	{0xF023, 0x0000, 0x0001, 0x00},  // OIS Gyro Mode
	{0xF024, 0x0000, 0x0001, 0x00},  // OIS Status
	{0xF025, 0x0000, 0x0001, 0x00},  // OIS Angle Limit
	{0xF02A, 0x0000, 0x0001, 0x00},  // OIS SPI Mode Select
	{0xF02C, 0x0000, 0x0001, 0x00},  // OIS Gyro Control Address
	{0xF02D, 0x0000, 0x0001, 0x00},  // OIS Gyro Control Data
	{0xF050, 0x0000, 0x0001, 0x00},  // Exit Standy
	{0xF054, 0x0000, 0x0001, 0x00},  // Enter Standy
	{0xF058, 0x0000, 0x0001, 0x00},  // SW RESET
	{0xF07A, 0x0000, 0x0002, 0x00},  // OIS Gyro gain X
	{0xF07C, 0x0000, 0x0002, 0x00},  // OIS Gyro gain Y
	{0xF088, 0x0002, 0x0001, 0x01},  // OIS Gyro Offset Req
	{0xF08A, 0x0000, 0x0002, 0x00},  // OIS Gyro Offset
	{0xF088, 0x0003, 0x0001, 0x01},  // OIS Gyro Offset Req
	{0xF08A, 0x0000, 0x0002, 0x00},  // OIS Gyro Offset
	{0xF097, 0x0000, 0x0001, 0x00},  // Pre SW RESET
	{0xF09A, 0x0000, 0x0001, 0x00},  // OIS Sync Centering Enable
	{0xF09C, 0x0000, 0x0001, 0x00},  // OIS Gyro Offset Update/Diff Req
	{0xF09D, 0x0000, 0x0002, 0x00},  // OIS Gyro Offset Update/Diff
	{0xF0AA, 0x0000, 0x0001, 0x00},  // OIS Sync Centering Ratio
	{0xF18E, 0x0000, 0x0001, 0x00},  // OIS Pan/Tilt Setting
	{0xF1E2, 0x0000, 0x0002, 0x00},  // OIS Gyro Calib Flash Addr
	{0xF1E4, 0x0000, 0x0001, 0x00},  // OIS Gyro Calib Update

	{0xF0E4, 0x0000, 0x0002, 0x00},  // OIS Gyro X L
	{0xF0E5, 0x0000, 0x0002, 0x00},  // OIS Gyro X H
	{0xF0E2, 0x0000, 0x0002, 0x00},  // OIS Gyro Y L
	{0xF0E3, 0x0000, 0x0002, 0x00},  // OIS Gyro Y H

	{0xF060, 0x00,   0x0001, 0x01},  // OIS HALL
	{0xF062, 0x0000, 0x0002, 0x00},  // OIS HALL X
	{0xF060, 0x01,   0x0001, 0x01},  // OIS HAL
	{0xF062, 0x0000, 0x0002, 0x00},  // OIS HALL Y
	{0xF15E, 0x00,   0x0001, 0x01},  // OIS gyro target
	{0xF15C, 0x0000, 0x0002, 0x00},  // OIS gyro target X
	{0xF15E, 0x01,   0x0001, 0x01},  // OIS gyro target
	{0xF15C, 0x0000, 0x0002, 0x00},  // OIS gyro target Y

};

static ssize_t ois_read(struct file *p_file,
	char __user *puser_buf, size_t count, loff_t *p_offset)
{
    return 1;
}

static ssize_t ois_write(struct file *p_file,
	const char __user *puser_buf,
	size_t count, loff_t *p_offset)
{
	char data[COMMAND_SIZE] = {0};
	char* const delim = " ";
	int iIndex = 0;
	char *token = NULL, *cur = NULL;
	uint32_t addr =0, value = 0;
	int result = 0;

	if(puser_buf)
	{
		if (count >= COMMAND_SIZE || copy_from_user(&data, puser_buf, count))
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "copy from user buffer error");
			return -EFAULT;
		}
	}

	cur = data;
	while ((token = strsep(&cur, delim)))
	{
		//CAM_EXT_ERR(CAM_EXT_OIS, "string = %s iIndex = %d, count = %d", token, iIndex, count);
                int ret=0;
		if (iIndex  == 0)
		{
		    ret = kstrtouint(token, 16, &addr);
		}
		else if (iIndex == 1)
		{
		    ret = kstrtouint(token, 16, &value);
		}
		if(ret < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS,"String conversion to unsigned int failed");
		}
		iIndex++;
	}
	if (ois_ctrls[CAM_OIS_MASTER] && addr != 0)
	{
		result = RamWrite32A_oneplus(ois_ctrls[CAM_OIS_MASTER], addr, value);
		if (result < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "write addr = 0x%x, value = 0x%x fail", addr, value);
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "write addr = 0x%x, value = 0x%x success", addr, value);
		}
	}
	return count;
}

static ssize_t ois_read_tele(struct file *p_file,
	char __user *puser_buf, size_t count, loff_t *p_offset)
{
    return 1;
}

static ssize_t ois_write_tele(struct file *p_file,
	const char __user *puser_buf,
	size_t count, loff_t *p_offset)
{
	char data[COMMAND_SIZE] = {0};
	char* const delim = " ";
	int iIndex = 0;
	char *token = NULL, *cur = NULL;
	uint32_t addr =0, value = 0;
	int result = 0;

	if(puser_buf)
	{
		if (count >= COMMAND_SIZE || copy_from_user(&data, puser_buf, count))
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "copy from user buffer error");
			return -EFAULT;
		}
	}

	cur = data;
	while ((token = strsep(&cur, delim)))
	{
		//CAM_EXT_ERR(CAM_EXT_OIS, "string = %s iIndex = %d, count = %d", token, iIndex, count);
		int ret=0;
		if (iIndex  == 0)
		{
			ret = kstrtouint(token, 16, &addr);
		}
		else if (iIndex == 1)
		{
			ret = kstrtouint(token, 16, &value);
		}

		if(ret < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS,"String conversion to unsigned int failed");
		}
		iIndex++;
	}

	if (ois_ctrls[CAM_OIS_SLAVE] && addr != 0)
	{
		result = RamWrite32A_oneplus(ois_ctrls[CAM_OIS_SLAVE], addr, value);
		if (result < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "write addr = 0x%x, value = 0x%x fail", addr, value);
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "write addr = 0x%x, value = 0x%x success", addr, value);
		}
	}
	return count;
}

static const struct proc_ops proc_file_fops = {
	.proc_read  = ois_read,
	.proc_write = ois_write,
};
static const struct proc_ops proc_file_fops_tele = {
	.proc_read  = ois_read_tele,
	.proc_write = ois_write_tele,
};

int ois_start_read(void *arg, bool start)
{
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;

	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "failed: o_ctrl %pK", o_ctrl);
		return -EINVAL;
	}

	mutex_lock(&(o_ctrl->ois_read_mutex));
	o_ctrl->ois_read_thread_start_to_read = start;
	mutex_unlock(&(o_ctrl->ois_read_mutex));

	msleep(OIS_READ_REGISTER_DELAY);

	return 0;
}

int ois_read_thread(void *arg)
{
	int rc = 0;
	int i;
	char buf[OIS_REGISTER_SIZE*2*4 + 1] = {0};  // Add one more bytes to prevent out-of-bounds access
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;
	int ois_reg_type = 0;

	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "failed: o_ctrl %pK", o_ctrl);
		return -EINVAL;
	}

	CAM_EXT_INFO(CAM_EXT_OIS, "ois_read_thread created");

	msleep(500);

	while (!kthread_should_stop())
	{
		memset(buf, 0, sizeof(buf));
		mutex_lock(&(o_ctrl->ois_read_mutex));
		if (o_ctrl->ois_read_thread_start_to_read)
		{
			if (strstr(o_ctrl->ois_name, "bu24721_tele"))
			{
				for (i = 0; i < OIS_REGISTER_SIZE; i++) {
					if (ois_registers_bu24721[i][0]) {
						if(ois_registers_bu24721[i][2] != 0){
							ois_reg_type = ois_registers_bu24721[i][2];
						}
						if(0x00 == ois_registers_bu24721[i][3])
						{
							ois_registers_bu24721[i][1] = 0;
							rc = camera_io_dev_read(&(o_ctrl->io_master_info), (uint32_t)ois_registers_bu24721[i][0], (uint32_t *)&ois_registers_bu24721[i][1],
							                   CAMERA_SENSOR_I2C_TYPE_WORD, ois_reg_type, false);
						}
						else if(0x01 == ois_registers_bu24721[i][3])  //write
						{
							rc = I2C_OIS_8bit_write(o_ctrl, ois_registers_bu24721[i][0],ois_registers_bu24721[i][1]);
							IsOISReady(o_ctrl);
						}
						if(rc < 0) {
							CAM_EXT_ERR(CAM_EXT_OIS, " ois name:%s rc: %s", o_ctrl->ois_name, rc);
						}
					}
				}

				for (i = 0; i < OIS_REGISTER_SIZE; i++) {
					if (ois_registers_bu24721[i][0]) {
						snprintf(buf+strlen(buf), sizeof(buf), "0x%x,0x%x,", ois_registers_bu24721[i][0], ois_registers_bu24721[i][1]);
					}
				}
			}
			CAM_EXT_INFO(CAM_EXT_OIS, "%s OIS register data: %s", o_ctrl->ois_name, buf);
		}
		mutex_unlock(&(o_ctrl->ois_read_mutex));

		msleep(OIS_READ_REGISTER_DELAY);
	}

	CAM_EXT_INFO(CAM_EXT_OIS, "ois_read_thread exist");

	return rc;
}

int ois_start_read_thread(void *arg, bool start)
{
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;

	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl is NULL");
		return -1;
	}

	if (start)
	{
		if (o_ctrl->ois_read_thread)
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "ois_read_thread is already created, no need to create again.");
		}
		else
		{
			o_ctrl->ois_read_thread = kthread_run(ois_read_thread, o_ctrl, o_ctrl->ois_name);
			if (!o_ctrl->ois_read_thread)
			{
				CAM_EXT_ERR(CAM_EXT_OIS, "create ois read thread failed");
				mutex_unlock(&(o_ctrl->ois_read_mutex));
				return -2;
			}
		}
	}
	else
	{
		if (o_ctrl->ois_read_thread)
		{
			mutex_lock(&(o_ctrl->ois_read_mutex));
			o_ctrl->ois_read_thread_start_to_read = 0;
			mutex_unlock(&(o_ctrl->ois_read_mutex));
			kthread_stop(o_ctrl->ois_read_thread);
			o_ctrl->ois_read_thread = NULL;
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "ois_read_thread is already stopped, no need to stop again.");
		}
	}

	return 0;
}

#define OIS_ATTR(_name, _mode, _show, _store) \
    struct kobj_attribute ois_attr_##_name = __ATTR(_name, _mode, _show, _store)

static ssize_t dump_registers_store(struct kobject *kobj,
                             struct kobj_attribute * attr,
                             const char * buf,
                             size_t count)
{
        unsigned int if_start_thread;
        CAM_EXT_WARN(CAM_EXT_OIS, "%s", buf);
        if (sscanf(buf, "%u", &if_start_thread) != 1)
		{
                return -1;
        }

        if(if_start_thread)
		{
                dump_ois_registers = true;
        }
		else
		{
                dump_ois_registers = false;
        }

        return count;
}

static OIS_ATTR(dump_registers, 0644, NULL, dump_registers_store);
static struct attribute *ois_node_attrs[] = {
        &ois_attr_dump_registers.attr,
        NULL,
};
static const struct attribute_group ois_common_group = {
        .attrs = ois_node_attrs,
};
static const struct attribute_group *ois_groups[] = {
        &ois_common_group,
        NULL,
};

void WitTim( uint16_t time)
{
	msleep(time);
}
void setI2cSlvAddr( uint8_t a , uint8_t b)
{
	CAM_EXT_ERR(CAM_EXT_OIS, "setI2cSlvAddr a:%d b:%d",a,b);
}

int RamWrite32A_oneplus(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD,
		.delay = 0x00,
	};

	if (o_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
}

int RamRead32A_oneplus(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t* data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	if (o_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}
	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_read(&(o_ctrl->io_master_info), (uint32_t)addr, (uint32_t *)data,
		                        CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD, false);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,read 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
}

int RohmOisWrite(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x01,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	CAM_EXT_INFO(CAM_EXT_OIS, "write cci device=%d master=%d",o_ctrl->io_master_info.cci_client->cci_device,o_ctrl->io_master_info.cci_client->cci_i2c_master);
	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_INFO(CAM_EXT_OIS, "write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			return rc;
		}
	}
	return rc;
}

int RohmOisRead(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t* data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

//    o_ctrl->io_master_info.cci_client->sid = 0x7C;
	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_read(&(o_ctrl->io_master_info), (uint32_t)addr, (uint32_t *)data,
		                        CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, false);
		CAM_EXT_INFO(CAM_EXT_OIS, "cci device=%d master=%d sid=0x%x",o_ctrl->io_master_info.cci_client->cci_device,o_ctrl->io_master_info.cci_client->cci_i2c_master, o_ctrl->io_master_info.cci_client->sid);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,read 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_INFO(CAM_EXT_OIS, "read success ois type=%d,read 0x%04x data=%d",o_ctrl->ois_type, addr,*data);
			return rc;
		}
	}
//	o_ctrl->io_master_info.cci_client->sid = 0x3e;
	return rc;
}

int StoreOisGyroGian(struct cam_ois_ctrl_t *o_ctrl)
{
	unsigned char rc = 0;
	if(strstr(o_ctrl->ois_name, "bu24721"))
	{
		WriteGyroGainToFlash(o_ctrl);
	}
  	else if (strstr(o_ctrl->ois_name, "sem1217s"))
  	{
  		SEM1217S_WriteGyroGainToFlash();
  	}else if (strstr(o_ctrl->ois_name, "dw9786")){
  		DW9786_StoreGyroGainToFlash(o_ctrl);
  	}
	return rc;
}

int DoBU24721GyroOffset(struct cam_ois_ctrl_t *o_ctrl, uint32_t *gyro_data)
{
	int rc = 0;
	uint32_t ReadGyroOffset_X = 0, ReadGyroOffset_Y = 0;
	struct _FACT_ADJ FADJCAL = { 0x0000, 0x0000};

	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	FADJCAL= Gyro_offset_cal(o_ctrl);

	ReadGyroOffset_X = (( FADJCAL.gl_GX_OFS) & 0xFFFF );
	ReadGyroOffset_Y = (( FADJCAL.gl_GY_OFS) & 0xFFFF );
	*gyro_data = ReadGyroOffset_Y << 16 | ReadGyroOffset_X;

	return rc;

}

int DoDW9786GyroOffset(struct cam_ois_ctrl_t *o_ctrl, uint32_t *gyro_data)
{
	int rc = 0;
	uint32_t ReadGyroOffset_X = 0, ReadGyroOffset_Y = 0;
	struct _FACT_ADJ_DW FADJCAL = { 0x0000, 0x0000};

	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}


	FADJCAL= dw9786_gyro_ofs_calibration(o_ctrl);

	ReadGyroOffset_X = (( FADJCAL.gl_GX_OFS) & 0xFFFF );
	ReadGyroOffset_Y = (( FADJCAL.gl_GY_OFS) & 0xFFFF );
	*gyro_data = ReadGyroOffset_Y << 16 | ReadGyroOffset_X;

	return rc;

}
int DoSEM1217SGyroOffset(struct cam_ois_ctrl_t *o_ctrl, uint32_t *gyro_data)
{
	int rc = 0;
	uint32_t ReadGyroOffset_X = 0, ReadGyroOffset_Y = 0;
	struct SEM1217S_FACT_ADJ FADJCAL = { 0x0000, 0x0000};

	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	sem1217s_ois_ctrl = o_ctrl;

	FADJCAL = SEM1217S_Gyro_offset_cal();

	ReadGyroOffset_X = FADJCAL.gl_GX_OFS ;
	ReadGyroOffset_Y = FADJCAL.gl_GY_OFS ;
	*gyro_data = ReadGyroOffset_Y << 16 | ReadGyroOffset_X;

	return rc;
}

int WriteOisGyroGian(struct cam_ois_ctrl_t *o_ctrl,OIS_GYROGAIN* current_gyro_gain)
{
	int rc = 0;
	if (!o_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}
    if(strstr(o_ctrl->ois_name,"bu24721")){
	    Gyro_gain_set(o_ctrl,current_gyro_gain->GyroGain_X , current_gyro_gain->GyroGain_Y);
    }else if(strstr(o_ctrl->ois_name,"sem1217s")){
        SEM1217S_Gyro_gain_set(current_gyro_gain->GyroGain_X , current_gyro_gain->GyroGain_Y);
    }else if(strstr(o_ctrl->ois_name,"dw9786")){
        DW9786_WriteGyroGainToFlash(o_ctrl,current_gyro_gain->GyroGain_X , current_gyro_gain->GyroGain_Y);
    }

	return rc;
}

int DownloadFW(struct cam_ois_ctrl_t *o_ctrl)
{
	uint8_t rc = 0;
	if (o_ctrl)
	{
		mutex_lock(&ois_mutex);
		if (strstr(o_ctrl->ois_name, "sem1217s"))
		{
			sem1217s_ois_ctrl = o_ctrl;
			rc = sem1217s_fw_download(o_ctrl);
			mutex_unlock(&ois_mutex);
			return rc;
		}

		if(strstr(o_ctrl->ois_name, "bu24721"))
		{
			rc = Rohm_ois_fw_download(o_ctrl);
			Update_Gyro_offset_gain_cal_from_flash(o_ctrl);
			Gyro_select(o_ctrl, Q_ICM42631, o_ctrl->isTeleOisUseMonitor);
			if (rc)
			{
				CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,Download %s FW failed",
					o_ctrl->ois_type,
					o_ctrl->ois_name);
			}
			else
			{
				if (dump_ois_registers&&!ois_start_read_thread(o_ctrl, 1))
				{
					ois_start_read(o_ctrl, 1);
				}
				/*just for test tele start*/
				//if (dump_ois_registers&&!ois_start_read_thread(ois_ctrls[CAM_OIS_MASTER], 1)) {
				//ois_start_read(ois_ctrls[CAM_OIS_MASTER], 1);
				//}
				/*just for test tele end*/
			}
		}
		mutex_unlock(&ois_mutex);
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl is NULL");
	}

	return rc;
}

struct hall_info
{
    uint32_t timeStampSec;  //us
    uint32_t timeStampUsec;
    uint32_t mHalldata;
};

void timeval_add(struct timeval *tv,int32_t usec)
{
	if (usec > 0) {
		if (tv->tv_usec + usec >= 1000*1000) {
			tv->tv_sec += 1;
			tv->tv_usec = tv->tv_usec - 1000 * 1000 + usec;
		} else {
			tv->tv_usec += usec;
		}
	} else {
		if (tv->tv_usec < abs(usec)) {
			tv->tv_sec -= 1;
			tv->tv_usec = tv->tv_usec + 1000 * 1000 + usec;
		} else {
			tv->tv_usec += usec;
		}
	}
}

void do_gettimeofday(struct timeval *tv)
{
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_usec = now.tv_nsec/1000;
}

int WRITE_QTIMER_TO_OIS (struct cam_ois_ctrl_t *o_ctrl)
{
	uint64_t qtime_ns = 0,qtime_ns_after=0,value=351000;
	int32_t  rc = 0,i=0,j=0;
	static struct cam_sensor_i2c_reg_array *i2c_write_setting_gl = NULL;
	struct cam_sensor_i2c_reg_setting i2c_write;
	uint32_t reg_addr_qtimer = 0x0;
	if(strstr(o_ctrl->ois_name, "dw9786")){
		reg_addr_qtimer = 0xB970;
	}

	CAM_EXT_DBG(CAM_EXT_OIS, "[%s]reg_adr_qtimer = 0x%x",o_ctrl->ois_name,reg_addr_qtimer);

	if (i2c_write_setting_gl == NULL)
	{
		i2c_write_setting_gl = (struct cam_sensor_i2c_reg_array *)kzalloc(
		                           sizeof(struct cam_sensor_i2c_reg_array)*MAX_DATA_NUM*2, GFP_KERNEL);
		if(!i2c_write_setting_gl)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Alloc i2c_write_setting_gl failed");
			return -1;
		}
	}
	/*if(is_wirte==TRUE){
		CAM_EXT_ERR(CAM_EXT_OIS, "have write Qtimer");
		return 0;
	}*/
	memset(i2c_write_setting_gl, 0, sizeof(struct cam_sensor_i2c_reg_array)*MAX_DATA_NUM*2);
        while(value>350000  && j<5 ){
	        rc = cam_sensor_util_get_current_qtimer_ns(&qtime_ns);
	        if (rc < 0) {
		        CAM_EXT_ERR(CAM_EXT_OIS,
			        "Failed to get current qtimer value: %d",
			        rc);
		        return rc;
	        }
	        CAM_EXT_DBG(CAM_EXT_OIS,"qtime_ns: H=0x%x L=0x%x",(uint32_t)((qtime_ns>>32)&0xffffffff),(uint32_t)(qtime_ns&0xffffffff));

	        for(i = 0; i< 2; i++) {
		        if (i == 0) {
			        i2c_write_setting_gl[i].reg_addr = reg_addr_qtimer;
			        i2c_write_setting_gl[i].reg_data = (uint32_t)((qtime_ns>>32)&0xffffffff);
			        i2c_write_setting_gl[i].delay = 0X00;
			        i2c_write_setting_gl[i].data_mask = 0x00;
		        } else {
			        i2c_write_setting_gl[i].reg_data = (uint32_t)(qtime_ns&0xffffffff);
			        i2c_write_setting_gl[i].delay = 0x00;
			        i2c_write_setting_gl[i].data_mask = 0x00;
		        }
	        }
	        i2c_write.reg_setting = i2c_write_setting_gl;
	        i2c_write.size = 2;
	        i2c_write.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	        i2c_write.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;
	        i2c_write.delay = 0x00;
                rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),&i2c_write, 1);
	        if (rc < 0) {
		        CAM_EXT_ERR(CAM_EXT_OIS,
			        "Failed to write qtimer value: %d",
			        rc);
		        return rc;
	        }
	        rc = cam_sensor_util_get_current_qtimer_ns(&qtime_ns_after);
	        if (rc < 0) {
		        CAM_EXT_ERR(CAM_EXT_OIS,
			        "Failed to get current qtime_ns_after value: %d",
			        rc);
		        return rc;
	        }
                CAM_EXT_DBG(CAM_EXT_OIS,"qtime_ns_after: ms=0x%x us=0x%x",(uint32_t)((qtime_ns_after>>32)&0xffffffff),(uint32_t)(qtime_ns_after&0xffffffff));
                value = qtime_ns_after-qtime_ns;
                //is_wirte=TRUE;
                if(j>0)
                        CAM_EXT_DBG(CAM_EXT_OIS,"write time=%d",value);
                j++;
        }
        return rc;
}

bool IsOISReady(struct cam_ois_ctrl_t *o_ctrl)
{
	uint32_t temp, retry_cnt;
	int rc = 0;
	uint32_t ReadVal = 0;
	retry_cnt = 10;

	if (o_ctrl)
	{
		do
		{
			if (strstr(o_ctrl->ois_name, "bu24721"))
			{
				RohmOisRead(o_ctrl,0xF024, &temp);
				if ((temp&0x01) == 1)
				{
					CAM_EXT_INFO(CAM_EXT_OIS, "OIS %d IsOISReady", o_ctrl->ois_type);
					return true;
				}
				msleep(5);
				CAM_EXT_INFO(CAM_EXT_OIS, "OIS %d 0xF024 = 0x%x", o_ctrl->ois_type, temp);
			}
			else if(strstr(o_ctrl->ois_name, "sem1217s"))
			{
				rc = camera_io_dev_read(&(o_ctrl->io_master_info), 0x0001, (uint32_t *)&ReadVal,
					CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, false);
				if (rc < 0) {
					CAM_EXT_ERR(CAM_EXT_OIS, "[SEM1217S] Read Addr_OIS_STS(0x0001) failed, retry");
				}
				else
				{
					CAM_EXT_INFO(CAM_EXT_OIS, "[SEM1217S] Read Addr_OIS_STS(0x0001) = 0x%x", ReadVal);
					if (ReadVal == 0x01 || ReadVal == 0x02)/* STATE_READY or STATE_RUN*/
					{
						return true;
					}
				}
			}
			else if(strstr(o_ctrl->ois_name, "dw9786"))
			{
				rc = camera_io_dev_read(&(o_ctrl->io_master_info), 0xE000, (uint32_t *)&ReadVal,
					CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, false);
				if (rc < 0) {
					CAM_EXT_ERR(CAM_EXT_OIS, "[DW9786] Read Addr_OIS_STS(0xE000) failed, retry");
				}
				else
				{
					CAM_EXT_INFO(CAM_EXT_OIS, "[DW9786] Read Addr_OIS_STS(0xE000) = 0x%x", ReadVal);
					if (ReadVal == 0)/* STATE_READY or STATE_RUN*/
					{
					    CAM_EXT_INFO(CAM_EXT_OIS, "OIS %d IsOISReady", o_ctrl->ois_type);
						return true;
					}
				}
			}
			retry_cnt--;
			msleep(5);
		}
		while(retry_cnt);

		CAM_EXT_ERR(CAM_EXT_OIS, "IsOISReady not ready, return false");
		return false;
	} else {
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl is NULL");
		return false;
	}
}

void DeinitOIS(struct cam_ois_ctrl_t *o_ctrl)
{
	if (o_ctrl)
	{
		if(o_ctrl->ois_read_thread_start_to_read)
		{
			ois_start_read_thread(o_ctrl, 0);
		}
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl is NULL");
	}
}
int update_ois_firmware_1217s(void *arg)
{
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;
	int rc = 0;
	CAM_EXT_INFO(CAM_EXT_OIS, "update_ois_firmware_1217s");
	o_ctrl->io_master_info.master_type = CCI_MASTER;
	o_ctrl->io_master_info.cci_client->sid = 0x61;
	o_ctrl->io_master_info.cci_client->i2c_freq_mode = I2C_FAST_PLUS_MODE;
	o_ctrl->io_master_info.cci_client->retries = 3;
	o_ctrl->io_master_info.cci_client->id_map = 0;
	ois_ctrls[CAM_OIS_MASTER] = o_ctrl;
	sem1217s_ois_ctrl = o_ctrl;

	cam_ext_ois_power_up(o_ctrl);
	oplus_cam_monitor_state(o_ctrl,
			o_ctrl->v4l2_dev_str.ent_function,
			CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
			true);
	msleep(10);

	rc = sem1217s_fw_download(o_ctrl);
	if(rc == 0)
	{
		CAM_EXT_INFO(CAM_EXT_OIS, "sm1217s download successed!");
	}

	cam_ext_ois_power_down(o_ctrl);
	oplus_cam_monitor_state(o_ctrl,
			o_ctrl->v4l2_dev_str.ent_function,
			CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
			false);
	CAM_EXT_INFO(CAM_EXT_OIS, "exit update_ois_firmware_1217s rc:%d", rc);

	return rc;
}
int update_ois_firmware_bu24721(void *arg)
{
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;
	int rc = 0;
	CAM_EXT_INFO(CAM_EXT_OIS, "update_ois_firmware_bu24721");
	o_ctrl->io_master_info.master_type = CCI_MASTER;
	o_ctrl->io_master_info.cci_client->sid = 0x3E;
	o_ctrl->io_master_info.cci_client->i2c_freq_mode = I2C_FAST_PLUS_MODE;
	o_ctrl->io_master_info.cci_client->retries = 3;
	o_ctrl->io_master_info.cci_client->id_map = 0;

	ois_ctrls[CAM_OIS_MASTER] = o_ctrl;

	cam_ext_ois_power_up(o_ctrl);
	oplus_cam_monitor_state(o_ctrl,
			o_ctrl->v4l2_dev_str.ent_function,
			CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
			true);
	msleep(10);

	rc = Rohm_ois_fw_download(o_ctrl);

	cam_ext_ois_power_down(o_ctrl);
	oplus_cam_monitor_state(o_ctrl,
			o_ctrl->v4l2_dev_str.ent_function,
			CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
			false);
	CAM_EXT_INFO(CAM_EXT_OIS, "exit update_ois_firmware_bu24721 rc:%d", rc);
	return rc;
}

int update_ois_firmware_dw9786(void *arg)
{
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;
	int rc = 0;
	CAM_EXT_INFO(CAM_EXT_OIS, "update_ois_firmware_dw9786");
	o_ctrl->io_master_info.master_type = CCI_MASTER;
	o_ctrl->io_master_info.cci_client->sid = 0x19;
	o_ctrl->io_master_info.cci_client->i2c_freq_mode = I2C_FAST_PLUS_MODE;
	o_ctrl->io_master_info.cci_client->retries = 3;
	o_ctrl->io_master_info.cci_client->id_map = 0;

	ois_ctrls[CAM_OIS_SLAVE] = o_ctrl;

	cam_ext_ois_power_up(o_ctrl);
	oplus_cam_monitor_state(o_ctrl,
			o_ctrl->v4l2_dev_str.ent_function,
			CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
			true);
	msleep(10);

	rc = dw9786_download_fw(o_ctrl);

	cam_ext_ois_power_down(o_ctrl);
	oplus_cam_monitor_state(o_ctrl,
			o_ctrl->v4l2_dev_str.ent_function,
			CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
			false);
	CAM_EXT_INFO(CAM_EXT_OIS, "exit update_ois_firmware_dw9786 rc:%d", rc);
	return rc;
}

void InitOISResource(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc;
	mutex_init(&ois_mutex);
	if (o_ctrl)
	{
		if (strstr(o_ctrl->ois_name, "bu24721"))
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "create bu24721 ois download fw thread");
			o_ctrl->ois_downloadfw_thread = kthread_run(update_ois_firmware_bu24721, o_ctrl, o_ctrl->ois_name);
		}

		if (strstr(o_ctrl->ois_name, "sem1217s"))
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "create sem1217s ois download fw thread for sem1217");
			o_ctrl->ois_downloadfw_thread = kthread_run(update_ois_firmware_1217s, o_ctrl, o_ctrl->ois_name);
		}

		if (strstr(o_ctrl->ois_name, "dw9786"))
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "create dw9786 ois download fw thread for dw9786");
			o_ctrl->ois_downloadfw_thread = kthread_run(update_ois_firmware_dw9786, o_ctrl, o_ctrl->ois_name);
		}

		if(!cam_ois_kobj)
		{
			cam_ois_kobj = kobject_create_and_add("ois_control", kernel_kobj);
			rc = sysfs_create_groups(cam_ois_kobj, ois_groups);
			if (rc != 0)
			{
				CAM_EXT_ERR(CAM_EXT_OIS,"Error creating sysfs ois group");
				sysfs_remove_groups(cam_ois_kobj, ois_groups);
			}
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "ois_control node exist");
		}
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl is NULL");
	}

	//Create OIS control node
	if(face_common_dir == NULL)
	{
		face_common_dir =  proc_mkdir("OIS", NULL);
		if(!face_common_dir)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "create dir fail CAM_EXT_ERROR API");
			//return FACE_ERROR_GENERAL;
		}
	}

	if(proc_file_entry == NULL)
	{
		proc_file_entry = proc_create("OISControl",
			0777,
			face_common_dir,
			&proc_file_fops);
		if(proc_file_entry == NULL)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Create fail");
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "Create successs");
		}
	}

	if(proc_file_entry_tele == NULL)
	{
		proc_file_entry_tele = proc_create("OISControl_tele",
			0777,
			face_common_dir,
			&proc_file_fops_tele);
		if(proc_file_entry_tele == NULL)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Create fail");
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_OIS, "Create successs");
		}
	}
}

int32_t cam_ext_ois_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 5;
	power_info->power_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting)*5,
			GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VAF;
	power_info->power_setting[0].seq_val = CAM_VAF;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 0;

	power_info->power_setting[1].seq_type = SENSOR_VIO;
	power_info->power_setting[1].seq_val = CAM_VIO;
	power_info->power_setting[1].config_val = 1;
	power_info->power_setting[1].delay = 0;

	power_info->power_setting[2].seq_type = SENSOR_VANA;
	power_info->power_setting[2].seq_val = CAM_VANA;
	power_info->power_setting[2].config_val = 1;
	power_info->power_setting[2].delay = 0;

	power_info->power_setting[3].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_setting[3].seq_val = CAM_V_CUSTOM1;
	power_info->power_setting[3].config_val = 1;
	power_info->power_setting[3].delay = 0;

	power_info->power_setting[4].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_setting[4].seq_val = CAM_V_CUSTOM2;
	power_info->power_setting[4].config_val = 1;
	power_info->power_setting[4].delay = 10;

	power_info->power_down_setting_size = 5;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting)*5,
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	power_info->power_down_setting[1].seq_type = SENSOR_VIO;
	power_info->power_down_setting[1].seq_val = CAM_VIO;
	power_info->power_down_setting[1].config_val = 0;

	power_info->power_down_setting[2].seq_type = SENSOR_VANA;
	power_info->power_down_setting[2].seq_val = CAM_VANA;
	power_info->power_down_setting[2].config_val = 0;

	power_info->power_down_setting[3].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_down_setting[3].seq_val = CAM_V_CUSTOM2;
	power_info->power_down_setting[3].config_val = 0;

	power_info->power_down_setting[4].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_down_setting[4].seq_val = CAM_V_CUSTOM1;
	power_info->power_down_setting[4].config_val = 0;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

int32_t cam_ext_ois_construct_default_power_setting_bu24721(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 5;
	power_info->power_setting = kzalloc(sizeof(struct cam_sensor_power_setting)*5, GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VIO;
	power_info->power_setting[0].seq_val = CAM_VIO;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 0;

	power_info->power_setting[1].seq_type = SENSOR_VDIG;
	power_info->power_setting[1].seq_val = CAM_VDIG;
	power_info->power_setting[1].config_val = 1;
	power_info->power_setting[1].delay = 0;

	power_info->power_setting[2].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_setting[2].seq_val = CAM_V_CUSTOM1;
	power_info->power_setting[2].config_val = 1;
	power_info->power_setting[2].delay = 0;

	power_info->power_setting[3].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_setting[3].seq_val = CAM_V_CUSTOM2;
	power_info->power_setting[3].config_val = 1;
	power_info->power_setting[3].delay = 10;

	power_info->power_setting[4].seq_type = SENSOR_VAF;
	power_info->power_setting[4].seq_val = CAM_VAF;
	power_info->power_setting[4].config_val = 1;
	power_info->power_setting[4].delay = 10;

	power_info->power_down_setting_size = 5;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting)*5,
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	power_info->power_down_setting[1].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_down_setting[1].seq_val = CAM_V_CUSTOM2;
	power_info->power_down_setting[1].config_val = 0;

	power_info->power_down_setting[2].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_down_setting[2].seq_val = CAM_V_CUSTOM1;
	power_info->power_down_setting[2].config_val = 0;

	power_info->power_down_setting[3].seq_type = SENSOR_VDIG;
	power_info->power_down_setting[3].seq_val = CAM_VDIG;
	power_info->power_down_setting[3].config_val = 0;

	power_info->power_down_setting[4].seq_type = SENSOR_VIO;
	power_info->power_down_setting[4].seq_val = CAM_VIO;
	power_info->power_down_setting[4].config_val = 0;


	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

int32_t cam_ext_ois_construct_default_power_setting_dw9786(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 5;
	power_info->power_setting = kzalloc(sizeof(struct cam_sensor_power_setting)*5, GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VIO;
	power_info->power_setting[0].seq_val = CAM_VIO;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 1;

	power_info->power_setting[1].seq_type = SENSOR_VDIG;
	power_info->power_setting[1].seq_val = CAM_VDIG;
	power_info->power_setting[1].config_val = 1;
	power_info->power_setting[1].delay = 0;

	power_info->power_setting[2].seq_type = SENSOR_VAF;
	power_info->power_setting[2].seq_val = CAM_VAF;
	power_info->power_setting[2].config_val = 1;
	power_info->power_setting[2].delay = 5;

	power_info->power_setting[3].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_setting[3].seq_val = CAM_V_CUSTOM2;
	power_info->power_setting[3].config_val = 1;
	power_info->power_setting[3].delay = 0;

	power_info->power_setting[4].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_setting[4].seq_val = CAM_V_CUSTOM1;
	power_info->power_setting[4].config_val = 1;
	power_info->power_setting[4].delay = 2;

	power_info->power_down_setting_size = 5;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting)*5,
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;
	power_info->power_down_setting[0].delay = 100;

	power_info->power_down_setting[1].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_down_setting[1].seq_val = CAM_V_CUSTOM1;
	power_info->power_down_setting[1].config_val = 0;
	power_info->power_down_setting[1].delay = 2;

	power_info->power_down_setting[2].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_down_setting[2].seq_val = CAM_V_CUSTOM2;
	power_info->power_down_setting[2].config_val = 0;

	power_info->power_down_setting[3].seq_type = SENSOR_VDIG;
	power_info->power_down_setting[3].seq_val = CAM_VDIG;
	power_info->power_down_setting[3].config_val = 0;

	power_info->power_down_setting[4].seq_type = SENSOR_VIO;
	power_info->power_down_setting[4].seq_val = CAM_VIO;
	power_info->power_down_setting[4].config_val = 0;
	power_info->power_down_setting[4].delay = 1;


	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

int32_t cam_ext_ois_construct_default_power_setting_1217s(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 6;
	power_info->power_setting = kzalloc(sizeof(struct cam_sensor_power_setting)*6, GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VIO;
	power_info->power_setting[0].seq_val = CAM_VIO;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 0;

	power_info->power_setting[1].seq_type = SENSOR_VDIG;
	power_info->power_setting[1].seq_val = CAM_VDIG;
	power_info->power_setting[1].config_val = 1;
	power_info->power_setting[1].delay = 0;

	power_info->power_setting[2].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_setting[2].seq_val = CAM_V_CUSTOM1;
	power_info->power_setting[2].config_val = 1;
	power_info->power_setting[2].delay = 0;

	power_info->power_setting[3].seq_type = SENSOR_VAF;
	power_info->power_setting[3].seq_val = CAM_VAF;
	power_info->power_setting[3].config_val = 1;
	power_info->power_setting[3].delay = 10;

	power_info->power_setting[4].seq_type = SENSOR_VANA;
	power_info->power_setting[4].seq_val = CAM_VANA;
	power_info->power_setting[4].config_val = 1;
	power_info->power_setting[4].delay = 0;

	power_info->power_setting[5].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_setting[5].seq_val = CAM_V_CUSTOM2;
	power_info->power_setting[5].config_val = 1;
	power_info->power_setting[5].delay = 10;

	power_info->power_down_setting_size = 6;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting)*6,
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	power_info->power_down_setting[1].seq_type = SENSOR_VIO;
	power_info->power_down_setting[1].seq_val = CAM_VIO;
	power_info->power_down_setting[1].config_val = 0;

	power_info->power_down_setting[2].seq_type = SENSOR_VANA;
	power_info->power_down_setting[2].seq_val = CAM_VANA;
	power_info->power_down_setting[2].config_val = 0;

	power_info->power_down_setting[3].seq_type = SENSOR_CUSTOM_REG2;
	power_info->power_down_setting[3].seq_val = CAM_V_CUSTOM2;
	power_info->power_down_setting[3].config_val = 0;

	power_info->power_down_setting[4].seq_type = SENSOR_CUSTOM_REG1;
	power_info->power_down_setting[4].seq_val = CAM_V_CUSTOM1;
	power_info->power_down_setting[4].config_val = 0;

	power_info->power_down_setting[5].seq_type = SENSOR_VDIG;
	power_info->power_down_setting[5].seq_val = CAM_VDIG;
	power_info->power_down_setting[5].config_val = 0;


	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

int cam_ois_slaveInfo_pkt_parser_oem(struct cam_ois_ctrl_t *o_ctrl)
{
	o_ctrl->io_master_info.cci_client->i2c_freq_mode = I2C_FAST_PLUS_MODE;
	o_ctrl->io_master_info.cci_client->sid = (0x7c >> 1);
	o_ctrl->io_master_info.cci_client->retries = 3;
	o_ctrl->io_master_info.cci_client->id_map = 0;

	return 0;
}

int ois_download_fw_thread(void *arg)
{
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;
	int rc = -1;
	mutex_lock(&(o_ctrl->ois_mutex));
	cam_ois_slaveInfo_pkt_parser_oem(o_ctrl);

	if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
		mutex_lock(&(o_ctrl->ois_power_down_mutex));
		o_ctrl->ois_power_down_thread_exit = true;
		if (o_ctrl->ois_power_state == CAM_OIS_POWER_OFF){
			rc = cam_ext_ois_power_up(o_ctrl);
			if(rc != 0) {
				CAM_EXT_ERR(CAM_EXT_OIS, "ois power up failed");
					mutex_unlock(&(o_ctrl->ois_power_down_mutex));
					mutex_unlock(&(o_ctrl->ois_mutex));
					return rc;
			}
			oplus_cam_monitor_state(o_ctrl,
				o_ctrl->v4l2_dev_str.ent_function,
				CAM_OIS_UPDATE_FW_POWER_UP_TYPE,
				true);
		} else {
			CAM_EXT_INFO(CAM_EXT_OIS, "ois type=%d,OIS already power on, no need to power on again",o_ctrl->ois_type);
		}
		CAM_EXT_INFO(CAM_EXT_OIS, "ois[%s] type:%d  power up successful",o_ctrl->ois_type,o_ctrl->ois_name);
		o_ctrl->ois_power_state = CAM_OIS_POWER_ON;
		mutex_unlock(&(o_ctrl->ois_power_down_mutex));
	}

	o_ctrl->cam_ois_state = CAM_OIS_CONFIG;

	mutex_unlock(&(o_ctrl->ois_mutex));

	mutex_lock(&(o_ctrl->do_ioctl_ois));
	if(o_ctrl->ois_download_fw_done == CAM_OIS_FW_NOT_DOWNLOAD)
	{
		rc = DownloadFW(o_ctrl);
		if(rc != 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois download fw failed");
			o_ctrl->ois_download_fw_done = CAM_OIS_FW_NOT_DOWNLOAD;
			mutex_unlock(&(o_ctrl->do_ioctl_ois));
			return rc;
		}
		else
		{
			o_ctrl->ois_download_fw_done = CAM_OIS_FW_DOWNLOAD_DONE;
		}
	}
	RamWrite32A_oneplus(o_ctrl,0xf012,0x0);
	RamWrite32A_oneplus(o_ctrl,0xf010,0x0);
	mutex_unlock(&(o_ctrl->do_ioctl_ois));
	return rc;
}

int cam_ois_download_start(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	mutex_lock(&(o_ctrl->ois_mutex));
	mutex_lock(&(o_ctrl->ois_power_down_mutex));
	if (o_ctrl->ois_power_state == CAM_OIS_POWER_ON)
	{
		CAM_EXT_INFO(CAM_EXT_OIS, "do not need to create ois download fw thread");
		o_ctrl->ois_power_down_thread_exit = true;
		mutex_unlock(&(o_ctrl->ois_power_down_mutex));
		o_ctrl->cam_ois_state = CAM_OIS_CONFIG;
		mutex_unlock(&(o_ctrl->ois_mutex));
		return rc;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_OIS, "create ois download fw thread");
		o_ctrl->ois_downloadfw_thread = kthread_run(ois_download_fw_thread, o_ctrl, o_ctrl->ois_name);
		if (!o_ctrl->ois_downloadfw_thread)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "create ois download fw thread failed");
			mutex_unlock(&(o_ctrl->ois_power_down_mutex));
			mutex_unlock(&(o_ctrl->ois_mutex));
			return -1;
		}
	}
	mutex_unlock(&(o_ctrl->ois_power_down_mutex));
	mutex_lock(&(o_ctrl->do_ioctl_ois));
	o_ctrl->ois_fd_have_close_state = CAM_OIS_IS_OPEN;
	mutex_unlock(&(o_ctrl->do_ioctl_ois));
	mutex_unlock(&(o_ctrl->ois_mutex));
	return rc;
}

void cam_ois_do_power_down(struct cam_ois_ctrl_t *o_ctrl)
{
	mutex_lock(&(o_ctrl->ois_mutex));

	//when close ois,should be disable ois
	mutex_lock(&(o_ctrl->ois_power_down_mutex));
	if (o_ctrl->ois_power_state == CAM_OIS_POWER_ON)
	{
		RamWrite32A_oneplus(o_ctrl,0xf012,0x0);
	}
	mutex_unlock(&(o_ctrl->ois_power_down_mutex));
	mutex_lock(&(o_ctrl->do_ioctl_ois));
	o_ctrl->ois_fd_have_close_state = CAM_OIS_IS_DOING_CLOSE;
	mutex_unlock(&(o_ctrl->do_ioctl_ois));

	cam_ext_ois_shutdown(o_ctrl);

	mutex_unlock(&(o_ctrl->ois_mutex));
}


/****************************   Rohm FW  function    ********************************/

int Rohm_ois_fw_download(struct cam_ois_ctrl_t *o_ctrl)
{
	int32_t     rc = 0;
	struct timespec64 mStartTime, mEndTime, diff;
	uint64_t mSpendTime = 0;
	ktime_get_real_ts64(&mStartTime);

	if (!o_ctrl) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}
	if(strstr(o_ctrl->ois_name, "bu24721"))
	{
		//1.download Fw
		CAM_EXT_INFO(CAM_EXT_OIS, "Tele OIS download FW ...");
		rc = Rohm_bu24721_fw_download(o_ctrl);
		if(!rc)
		{
			//2.checksum

			//3.config para
		}
		else
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Tele OIS download FW Failed rc = %d",rc);
		}
	}

	ktime_get_real_ts64(&mEndTime);
	diff = timespec64_sub(mEndTime, mStartTime);
	mSpendTime = (timespec64_to_ns(&diff))/1000000;

	CAM_EXT_INFO(CAM_EXT_OIS, "tele ois FW download rc=%d, (Spend: %llu ms)", rc, mSpendTime);
	return rc;
}

uint8_t I2C_OIS_8bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr)
{
	uint32_t data = 0xff;
	int32_t rc = 0;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}
	rc = camera_io_dev_read(&(o_ctrl->io_master_info), addr,&data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, false);
	return (uint8_t)data;
}

uint16_t I2C_OIS_16bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr)
{
	uint32_t data = 0xff;
	int32_t rc = 0;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	rc = camera_io_dev_read(&(o_ctrl->io_master_info), addr,&data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
	return (uint16_t)data;
}

uint16_t sem1217_16bit_read(uint32_t addr)
{
	uint32_t data = 0xff;
	int32_t rc = 0;

	if (sem1217s_ois_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	rc = camera_io_dev_read(&(sem1217s_ois_ctrl->io_master_info), addr,&data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
	return (uint16_t)data;
}

int  I2C_OIS_32bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint32_t* data)
{
	//uint32_t data = 0xff;
	int32_t rc = 0;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	rc = camera_io_dev_read(&(o_ctrl->io_master_info), addr, data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD, false);
	return rc;
}

int I2C_OIS_8bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint8_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			return rc;
		}
	}
	return rc;
}

int I2C_OIS_16bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint16_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			return rc;
		}
	}
	return rc;
}

int I2C_OIS_32bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint32_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_32bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
            CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			return rc;
		}
	}
	return rc;
}

void I2C_OIS_block_write(struct cam_ois_ctrl_t *o_ctrl,void* register_data,int size)
{
	uint8_t *data =  (uint8_t *)register_data;
	int32_t rc = 0;
	int i = 0;
	int reg_data_cnt = size - 2;
	int continue_cnt = 0;
	int retry = 3;
	static struct cam_sensor_i2c_reg_array *i2c_write_setting_gl = NULL;

	struct cam_sensor_i2c_reg_setting i2c_write;

	if (o_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return;
	}

	if (i2c_write_setting_gl == NULL)
	{
		i2c_write_setting_gl = (struct cam_sensor_i2c_reg_array *)kzalloc(
		                           sizeof(struct cam_sensor_i2c_reg_array)*MAX_DATA_NUM*2, GFP_KERNEL);
		if(!i2c_write_setting_gl)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Alloc i2c_write_setting_gl failed");
			return;
		}
	}

	memset(i2c_write_setting_gl, 0, sizeof(struct cam_sensor_i2c_reg_array)*MAX_DATA_NUM*2);

	for(i = 0; i< reg_data_cnt; i++)
	{
		if (i == 0)
		{
			i2c_write_setting_gl[continue_cnt].reg_addr = ((data[0]&0xff)<<8)|(data[1]&0xff);
			i2c_write_setting_gl[continue_cnt].reg_data = data[2];
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
			CAM_EXT_INFO(CAM_EXT_OIS, "i2c_write_setting_gl[%d].reg_addr = %04x data:0x%02x %02x %02x %02x",continue_cnt,i2c_write_setting_gl[continue_cnt].reg_addr,data[2],data[3],data[4],data[5]);
		}
		else
		{
			i2c_write_setting_gl[continue_cnt].reg_data = data[i+2];
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
			//CAM_EXT_ERR(CAM_EXT_OIS, "i2c_write_setting_gl[%d].reg_addr = %x data:%x",continue_cnt,i2c_write_setting_gl[continue_cnt].reg_addr,i2c_write_setting_gl[continue_cnt].reg_data);
		}
		continue_cnt++;
	}
	i2c_write.reg_setting = i2c_write_setting_gl;
	i2c_write.size = continue_cnt;
	i2c_write.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_write.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_write.delay = 0x02;

	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		                                    &i2c_write, 1);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,Continue write failed, rc:%d, retry:%d",o_ctrl->ois_type, rc, i+1);
		}
		else
		{
			break;
		}
	}
}

uint8_t sem1217_8bit_read(uint32_t addr)
{
	uint32_t data = 0xff;
	int32_t rc = 0;

	if (sem1217s_ois_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	rc = camera_io_dev_read(&(sem1217s_ois_ctrl->io_master_info), addr,&data,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, false);

	return (uint8_t)data;
}

uint32_t I2C_FM_32bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr)
{
	uint32_t data = 0xff;
	int32_t rc = 0;
	uint32_t sid_defult = 0;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	sid_defult = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = FLASH_SLVADR; //flash addr
	rc = camera_io_dev_read(&(o_ctrl->io_master_info), addr,&data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD, false);
	o_ctrl->io_master_info.cci_client->sid = sid_defult;
	return data;
}

uint32_t I2C_FM_16bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr)
{
	uint32_t data = 0xff;
	int32_t rc = 0;
	uint32_t sid_defult = 0;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	sid_defult = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = FLASH_SLVADR; //flash addr
	rc = camera_io_dev_read(&(o_ctrl->io_master_info), addr,&data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
	o_ctrl->io_master_info.cci_client->sid = sid_defult;
	return data;
}

int I2C_FM_8bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint8_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;
	uint32_t sid_defult = 0;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	sid_defult = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = FLASH_SLVADR; //flash addr

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		}
		else
		{
			CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%x",o_ctrl->ois_type, addr,data);
			o_ctrl->io_master_info.cci_client->sid = sid_defult;
			return rc;
		}
	}
	o_ctrl->io_master_info.cci_client->sid = sid_defult;
	return rc;
}

int sem1217_8bit_write(uint32_t addr, uint8_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.delay = 0x00,
	};

	if (sem1217s_ois_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(sem1217s_ois_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d", sem1217s_ois_ctrl->ois_type, addr, i+1);
		}
		else
		{
			return rc;
		}
	}

	return rc;
}

int I2C_FM_16bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint16_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;
	uint32_t sid_defult = 0;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	sid_defult = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = FLASH_SLVADR; //flash addr

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			o_ctrl->io_master_info.cci_client->sid = sid_defult;
			return rc;
		}
	}
	o_ctrl->io_master_info.cci_client->sid = sid_defult;
	return rc;
}

void I2C_FM_block_write(struct cam_ois_ctrl_t *o_ctrl,void* register_data,int size)
{
	//uint8_t *register_data_addr = (uint8_t *)register_data;
	uint8_t *data =  (uint8_t *)register_data;
	int32_t rc = 0;
	int i = 0;
	int reg_data_cnt = size - 2;
	int continue_cnt = 0;
	int retry = 3;
	uint32_t sid_defult = 0;
	static struct cam_sensor_i2c_reg_array *i2c_write_setting_gl = NULL;

	struct cam_sensor_i2c_reg_setting i2c_write;

	if (o_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return;
	}

	if (i2c_write_setting_gl == NULL)
	{
		i2c_write_setting_gl = (struct cam_sensor_i2c_reg_array *)kzalloc(
		                           sizeof(struct cam_sensor_i2c_reg_array)*MAX_DATA_NUM*2, GFP_KERNEL);
		if(!i2c_write_setting_gl)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Alloc i2c_write_setting_gl failed");
			return;
		}
	}

	memset(i2c_write_setting_gl, 0, sizeof(struct cam_sensor_i2c_reg_array)*MAX_DATA_NUM*2);

	for(i = 0; i< reg_data_cnt; i++)
	{
		if (i == 0)
		{
			i2c_write_setting_gl[continue_cnt].reg_addr = ((data[0]&0xff)<<8)|(data[1]&0xff);
			i2c_write_setting_gl[continue_cnt].reg_data = data[2];
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
			//CAM_EXT_ERR(CAM_EXT_OIS, "i2c_write_setting_gl[%d].reg_addr = %04x data:0x%02x %02x %02x %02x",continue_cnt,i2c_write_setting_gl[continue_cnt].reg_addr,data[2],data[3],data[4],data[5]);
		}
		else
		{
			i2c_write_setting_gl[continue_cnt].reg_data = data[i+2];
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
			//CAM_EXT_ERR(CAM_EXT_OIS, "i2c_write_setting_gl[%d].reg_addr = %x data:%x",continue_cnt,i2c_write_setting_gl[continue_cnt].reg_addr,i2c_write_setting_gl[continue_cnt].reg_data);
		}
		continue_cnt++;
	}
	i2c_write.reg_setting = i2c_write_setting_gl;
	i2c_write.size = continue_cnt;
	i2c_write.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_write.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_write.delay = 0x01;

	sid_defult = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = FLASH_SLVADR; //flash addr

	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		                                    &i2c_write, 1);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,Continue write failed, rc:%d, retry:%d",o_ctrl->ois_type, rc, i+1);
		}
		else
		{
			break;
		}
	}
	o_ctrl->io_master_info.cci_client->sid = sid_defult;
}

int sem1217_block_write(void* register_data,int size)
{
	uint8_t *data =  (uint8_t *)register_data;
	int32_t rc = 0;
	int i = 0;
	int reg_data_cnt = size - 2;
	int continue_cnt = 0;
	int retry = 3;
	static struct cam_sensor_i2c_reg_array *i2c_write_setting_gl = NULL;

	struct cam_sensor_i2c_reg_setting i2c_write;

	if (sem1217s_ois_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -1;
	}

	if (i2c_write_setting_gl == NULL)
	{
		i2c_write_setting_gl = (struct cam_sensor_i2c_reg_array *)kzalloc(
		                           sizeof(struct cam_sensor_i2c_reg_array)*MAX_SEM1217S_DATA_NUM*2, GFP_KERNEL);
		if(!i2c_write_setting_gl)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Alloc i2c_write_setting_gl failed");
			return -1;
		}
	}

	memset(i2c_write_setting_gl, 0, sizeof(struct cam_sensor_i2c_reg_array)*MAX_SEM1217S_DATA_NUM*2);

	for(i = 0; i< reg_data_cnt; i++)
	{
		if (i == 0)
		{
			i2c_write_setting_gl[continue_cnt].reg_addr = ((data[0]&0xff)<<8)|(data[1]&0xff);
			i2c_write_setting_gl[continue_cnt].reg_data = data[2];
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
		}
		else
		{
			i2c_write_setting_gl[continue_cnt].reg_data = data[i+2];
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
		}
		continue_cnt++;
	}
	i2c_write.reg_setting = i2c_write_setting_gl;
	i2c_write.size = continue_cnt;
	i2c_write.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_write.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_write.delay = 0x01;

	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_write_continuous(&(sem1217s_ois_ctrl->io_master_info),
		                                    &i2c_write, 1);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,Continue write failed, rc:%d, retry:%d",sem1217s_ois_ctrl->ois_type, rc, i+1);
		}
		else
		{
			break;
		}
	}

	return rc;
}

int write_reg_16bit_value_8bit(struct cam_ois_ctrl_t *o_ctrl,unsigned short addr, uint16_t data){
	int32_t rc = 0;
	uint8_t sid_defult = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	sid_defult = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = 0x3A;      //0x97E6 & 0x3FFF use different slave id
	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		o_ctrl->io_master_info.cci_client->sid = sid_defult;
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			return rc;
		}
	}
	return rc;
}

int write_reg_16bit_value_16bit(struct cam_ois_ctrl_t *o_ctrl,unsigned short addr, uint16_t data){
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0x00,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.delay = 0x00,
	};

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = camera_io_dev_write(&(o_ctrl->io_master_info), &i2c_write);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,I2C_OIS_8bit_write 0x%04x failed, retry:%d",o_ctrl->ois_type, addr, i+1);
		} else {
			CAM_EXT_DBG(CAM_EXT_OIS, "I2C_OIS_8bit_write success ois type=%d,write 0x%04x ,data=%d",o_ctrl->ois_type, addr,data);
			return rc;
		}
	}
	return rc;
}

int read_reg_16bit_value_16bit(struct cam_ois_ctrl_t *o_ctrl,unsigned short addr, uint32_t* data)
{
	int32_t rc = 0;

	if (o_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -EINVAL;
	}
	//CAM_EXT_ERR(CAM_EXT_OIS, "Read cci%d_Master%d",o_ctrl->io_master_info.cci_client->cci_device,o_ctrl->io_master_info.cci_client->cci_i2c_master);

	rc = camera_io_dev_read(&(o_ctrl->io_master_info), addr,data,
	               CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
	return rc;
}

int i2c_block_write_reg(struct cam_ois_ctrl_t *o_ctrl, void* register_data, int size){
	uint8_t *data =  (uint8_t *)register_data;
	int32_t rc = 0;
	int i = 0;
	int reg_data_cnt = size - 2;
	int continue_cnt = 0;
	int retry = 3;
	static struct cam_sensor_i2c_reg_array *i2c_write_setting_gl = NULL;

	struct cam_sensor_i2c_reg_setting i2c_write;

	if (o_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid Args");
		return -1;
	}

	if (i2c_write_setting_gl == NULL)
	{
		i2c_write_setting_gl = (struct cam_sensor_i2c_reg_array *)kzalloc(
		                           sizeof(struct cam_sensor_i2c_reg_array)*MAX_SEM1217S_DATA_NUM*2, GFP_KERNEL);
		if(!i2c_write_setting_gl)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "Alloc i2c_write_setting_gl failed");
			return -1;
		}
	}

	memset(i2c_write_setting_gl, 0, sizeof(struct cam_sensor_i2c_reg_array)*MAX_SEM1217S_DATA_NUM*2);

	for(i = 0; i< reg_data_cnt/2; i++)
	{
		if (i == 0)
		{
			i2c_write_setting_gl[continue_cnt].reg_addr = ((data[0]&0xff)<<8)|(data[1]&0xff);
			i2c_write_setting_gl[continue_cnt].reg_data = ((data[2]&0xff)<<8)|(data[3]&0xff);
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
		}
		else if( i >= 1)
		{
			i2c_write_setting_gl[continue_cnt].reg_data = ((data[2*i+2]&0xff)<<8)|(data[2*i+3]&0xff);
			i2c_write_setting_gl[continue_cnt].delay = 0x00;
			i2c_write_setting_gl[continue_cnt].data_mask = 0x00;
		}
		continue_cnt++;
	}
	i2c_write.reg_setting = i2c_write_setting_gl;
	i2c_write.size = continue_cnt;
	i2c_write.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_write.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_write.delay = 0x01;
	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		                                    &i2c_write, 1);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,Continue write failed, rc:%d, retry:%d",o_ctrl->ois_type, rc, i+1);
		}
		else
		{
			break;
		}
	}

	return rc;
}

void Wait(int us)
{
	msleep((us+999)/1000);
}

void Enable_gyro_gain(struct cam_ois_ctrl_t *o_ctrl){

    CAM_EXT_ERR(CAM_EXT_OIS, "Enable_gyro_gain when start ois");
    I2C_OIS_8bit_write(o_ctrl,0xf020, 0x01);
    I2C_OIS_16bit_write(o_ctrl,0xF1CA, 0x370A);
    I2C_OIS_16bit_write(o_ctrl,0xF1CC, 0x3B85);
    Update_Gyro_offset_gain_cal_from_flash(o_ctrl);

}
