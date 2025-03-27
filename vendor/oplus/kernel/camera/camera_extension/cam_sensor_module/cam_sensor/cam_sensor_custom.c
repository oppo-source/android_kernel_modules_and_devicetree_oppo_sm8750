#include "cam_sensor_custom.h"
#include "cam_debug.h"

#include "tmf8806_driver.h"
#include "tof8801_driver.h"
#include "cam_trace.h"
#include "cam_kevent_fb_custom.h"
#include <linux/iio/consumer.h>

bool is_ftm_current_test = false;
struct cam_sensor_i2c_reg_setting sensor_setting;
struct sony_dfct_tbl_t sony_dfct_tbl;

static struct v4l2_subdev_core_ops g_sensor_ops;
extern struct v4l2_subdev_core_ops cam_sensor_subdev_core_ops;

extern int32_t delete_request(struct i2c_settings_array *i2c_array);

extern int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	int64_t req_id, enum cam_sensor_packet_opcodes opcode);
extern int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl);
extern int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl);
extern int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg);
extern int32_t cam_handle_mem_ptr(uint64_t handle, uint32_t cmd,
	struct cam_sensor_ctrl_t *s_ctrl);
extern int cam_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl);
extern int cam_sensor_read_qsc(struct cam_sensor_ctrl_t *s_ctrl);
extern int cam_sensor_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl);
extern int cam_mem_get_cpu_buf(int32_t buf_handle, uintptr_t *vaddr_ptr, size_t *len);
extern void cam_mem_put_cpu_buf(int32_t buf_handle);
extern int cam_packet_util_validate_cmd_desc(struct cam_cmd_buf_desc *cmd_desc);

extern int cam_olc_raise_exception(int excep_tpye, unsigned char *pay_load);
extern const unsigned char *acquire_event_field(int excepId);

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

#define BURST_MAX_DATA_NUM (512)



/*breakdown  setitngs for burst mode and normal mode */
int cam_ext_sensor_write_continuous(struct cam_sensor_ctrl_t *s_ctrl)
{
	int initSettingsIndex = 0;
	int current_opreation = CCI_WRITE;
	int current_addrtype = DataTypeWord;//2
	int current_datatype = DataTypeByte;//1
	struct cam_sensor_i2c_reg_array *i2c_write_setting_gl = NULL;
	struct cam_sensor_i2c_reg_setting i2c_write;
	int opreation_type = CCI_WRITE;
	int addrtype = DataTypeWord;//2
	int datatype = DataTypeByte;//1
	int i = 0;
	int rc = 0;

	if (i2c_write_setting_gl == NULL)
	{
		i2c_write_setting_gl = (struct cam_sensor_i2c_reg_array *)kzalloc(
									sizeof(struct cam_sensor_i2c_reg_array)*BURST_MAX_DATA_NUM, GFP_KERNEL);
		if(!i2c_write_setting_gl)
		{
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Alloc i2c_write_setting_gl failed");
			return -1;
		}
	}

	memset(i2c_write_setting_gl, 0, sizeof(struct cam_sensor_i2c_reg_array)*BURST_MAX_DATA_NUM);

	for (i = 0; i < s_ctrl->sensor_init_setting.size; i++)
	{
		CAM_EXT_DBG(CAM_EXT_SENSOR, "0x%0x=0x%0x  addrtype  %u   datatype  %u "
		, s_ctrl->sensor_init_setting.reg_setting[i].reg_addr, s_ctrl->sensor_init_setting.reg_setting[i].reg_data
		, s_ctrl->sensor_init_setting.reg_setting[i].addr_type, s_ctrl->sensor_init_setting.reg_setting[i].data_type);
		{
			i2c_write_setting_gl[initSettingsIndex].reg_addr = s_ctrl->sensor_init_setting.reg_setting[i].reg_addr;
			i2c_write_setting_gl[initSettingsIndex].reg_data = s_ctrl->sensor_init_setting.reg_setting[i].reg_data;
			i2c_write_setting_gl[initSettingsIndex].delay = s_ctrl->sensor_init_setting.reg_setting[i].delay;
			i2c_write_setting_gl[initSettingsIndex].data_mask = 0x00;
			initSettingsIndex++;
		}
		if (i < s_ctrl->sensor_init_setting.size - 1)
		{
			opreation_type = s_ctrl->sensor_init_setting.reg_setting[i + 1].operation;
			addrtype = s_ctrl->sensor_init_setting.reg_setting[i + 1].addr_type;
			datatype = s_ctrl->sensor_init_setting.reg_setting[i + 1].data_type;
		}
		else
		{
			opreation_type = s_ctrl->sensor_init_setting.reg_setting[i].operation;
			addrtype = s_ctrl->sensor_init_setting.reg_setting[i].addr_type;
			datatype = s_ctrl->sensor_init_setting.reg_setting[i].data_type;
		}
		if (i == 0)
		{
			current_opreation = s_ctrl->sensor_init_setting.reg_setting[i].operation;
			current_addrtype = s_ctrl->sensor_init_setting.reg_setting[i].addr_type;
			current_datatype = s_ctrl->sensor_init_setting.reg_setting[i].data_type;
		}
		CAM_EXT_DBG(CAM_EXT_SENSOR, "current_opreation  %d  opreation_type  %d  current_addrtype %d addrtype %d current_datatype %d datatype %d initSettingsIndex %d i %d"
		,current_opreation,opreation_type,current_addrtype,addrtype,current_datatype,datatype,initSettingsIndex,i);
		if ((current_opreation != opreation_type
				|| current_addrtype != addrtype
				|| current_datatype != datatype
				|| initSettingsIndex == BURST_MAX_DATA_NUM
				|| i == s_ctrl->sensor_init_setting.size - 1)
			&& (initSettingsIndex > 0))
		{
			i2c_write.reg_setting = i2c_write_setting_gl;
			i2c_write.size = initSettingsIndex;
			i2c_write.addr_type = current_addrtype;
			i2c_write.data_type = current_datatype;
			i2c_write.delay = 0x00;
			if (current_opreation == CCI_WRITE)
			{
				CAM_EXT_DBG(CAM_EXT_SENSOR, "camera_io_dev_write  0x%0x=0x%0x  size=%u", i2c_write_setting_gl[0].reg_addr, i2c_write_setting_gl[0].reg_data, i2c_write.size);
				trace_begin("camera_io_dev_write: 0x%0x=0x%0x   size %u", i2c_write_setting_gl[0].reg_addr, i2c_write_setting_gl[0].reg_data, i2c_write.size);
				rc = camera_io_dev_write(&(s_ctrl->io_master_info), &i2c_write);
				trace_end();
			}
			else if (current_opreation == CCI_WRITE_BURST || current_opreation == CCI_WRITE_SEQUENTIAL)
			{
				CAM_EXT_DBG(CAM_EXT_SENSOR, "camera_io_dev_write_continuous	0x%0x=0x%0x  size=%u", i2c_write_setting_gl[0].reg_addr, i2c_write_setting_gl[0].reg_data, i2c_write.size);
				trace_begin("camera_io_dev_write_continuous:0x%0x=0x%0x   size %u", i2c_write_setting_gl[0].reg_addr, i2c_write_setting_gl[0].reg_data, i2c_write.size);
				rc = camera_io_dev_write_continuous(&(s_ctrl->io_master_info),
					&i2c_write, current_opreation);
				trace_end();
			}
			current_opreation = opreation_type;
			current_addrtype = addrtype;
			current_datatype = datatype;
			initSettingsIndex = 0;
		}
	}
	if (i2c_write_setting_gl != NULL)
	{
		kfree(i2c_write_setting_gl);
		i2c_write_setting_gl = NULL;
	}
	return rc;
}

int cam_ext_sensor_start_thread(void *arg)
{
	struct cam_sensor_ctrl_t *s_ctrl = (struct cam_sensor_ctrl_t *)arg;
	int rc = 0;
	CAM_EXT_ERR(CAM_EXT_SENSOR, "Enter sensor_start_thread!");
	if (!s_ctrl)
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "s_ctrl is NULL");
		return -1;
	}
	trace_begin("%s sensor_start_AdPower_thread", s_ctrl->sensor_name);


	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	//power up for sensor
	rc = cam_sensor_power_up(s_ctrl);

	if (!rc) {
		oplus_cam_monitor_state(s_ctrl,
					s_ctrl->v4l2_dev_str.ent_function,
					CAM_SENSOR_ADVANCE_POWER_UP_TYPE,
					true);
	}

	//write initsetting for sensor
	if (rc == 0)
	{
		mutex_lock(&(s_ctrl->sensor_initsetting_mutex));
		if(s_ctrl->sensor_initsetting_state == CAM_SENSOR_SETTING_WRITE_INVALID)
		{
			trace_begin("initsettings size:%u", s_ctrl->sensor_init_setting.size);
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Enter CAM_SENSOR_SETTING_WRITE_INVALID!");
			if (s_ctrl->is_surpport_wr_continuous == TRUE)
			{
				rc = cam_ext_sensor_write_continuous(s_ctrl);
			}
			else
			{
				rc = camera_io_dev_write(&(s_ctrl->io_master_info),
					&(s_ctrl->sensor_init_setting));
			}
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Enter CAM_SENSOR_SETTING_WRITE_INVALID Done!");
			if(rc < 0)
			{
				CAM_EXT_ERR(CAM_EXT_SENSOR, "write setting failed!");
			}
			else
			{
				CAM_EXT_INFO(CAM_EXT_SENSOR, "write setting success!");
				s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_SUCCESS;
			}
			trace_end();
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor setting have write!");
		}
		mutex_unlock(&(s_ctrl->sensor_initsetting_mutex));
	}
	// write QSC init
	if (rc == 0)
	{
		mutex_lock(&(s_ctrl->sensor_initsetting_mutex));
		if(s_ctrl->sensor_qsc_setting.enable_qsc_write_in_advance
			&& s_ctrl->sensor_qsc_setting.read_qsc_success)
		{
			trace_begin("qsc_setting size:%u", s_ctrl->sensor_qsc_setting.qsc_setting.size);
			rc = camera_io_dev_write_continuous(&(s_ctrl->io_master_info),
				&(s_ctrl->sensor_qsc_setting.qsc_setting),
				CAM_SENSOR_I2C_WRITE_SEQ);
			if(rc < 0)
			{
				CAM_EXT_ERR(CAM_EXT_SENSOR, "write qsc failed!");
			}
			else
			{
				CAM_EXT_INFO(CAM_EXT_SENSOR, "write qsc success!");
				s_ctrl->sensor_qsc_setting.qscsetting_state = CAM_SENSOR_SETTING_WRITE_SUCCESS;
			}
			trace_end();
		}
		mutex_unlock(&(s_ctrl->sensor_initsetting_mutex));
	}

	if (s_ctrl->sensor_init_setting.reg_setting != NULL)
	{
		kfree(s_ctrl->sensor_init_setting.reg_setting);
		s_ctrl->sensor_init_setting.reg_setting = NULL;
	}
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	trace_end();

	return rc;
}

int cam_ext_sensor_start(struct cam_sensor_ctrl_t *s_ctrl, void *arg)
{
	int rc = 0;
	int i = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_i2c_reg_array *reg_setting = NULL;
	struct cam_oem_initsettings *initsettings = vzalloc(sizeof(struct cam_oem_initsettings));
	bool m_support_continuous = FALSE;
	if (initsettings == NULL) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "failed to allocate memory!!!!");
		return rc;
	}
	if (!s_ctrl || !cmd)
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "cam_sensor_start s_ctrl or arg is null ");
		return -1;
	}
	memset(initsettings, 0, sizeof(struct cam_oem_initsettings));

	if (copy_from_user(initsettings, u64_to_user_ptr(cmd->handle), cmd->size))
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "initsettings copy_to_user failed ");
	}

	reg_setting = (struct cam_sensor_i2c_reg_array *)kmalloc(
		(sizeof(struct cam_sensor_i2c_reg_array) * initsettings->size), GFP_KERNEL);
	if (reg_setting == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "failed to allocate initsettings memory!!!!");
		if (initsettings != NULL)
		{
			vfree(initsettings);
			initsettings = NULL;
		}
		return rc;
	}

	for (i = 0; i < initsettings->size; i++)
	{
		reg_setting[i].reg_addr = initsettings->reg_setting[i].reg_addr;
		reg_setting[i].addr_type = initsettings->reg_setting[i].addr_type;
		reg_setting[i].reg_data = initsettings->reg_setting[i].reg_data;
		reg_setting[i].data_type = initsettings->reg_setting[i].data_type;
		reg_setting[i].delay = initsettings->reg_setting[i].delay;
		reg_setting[i].data_mask = 0x00;
		reg_setting[i].operation = initsettings->reg_setting[i].operation;

		if ((reg_setting[i].operation == CCI_WRITE_BURST || reg_setting[i].operation == CCI_WRITE_SEQUENTIAL)
			&& m_support_continuous == FALSE)
		{
			m_support_continuous = TRUE;
		}
	}
	s_ctrl->sensor_init_setting.addr_type = initsettings->addr_type;
	s_ctrl->sensor_init_setting.data_type = initsettings->data_type;
	s_ctrl->sensor_init_setting.size = initsettings->size;
	s_ctrl->is_surpport_wr_continuous = m_support_continuous;

	if (initsettings != NULL)
	{
		vfree(initsettings);
		initsettings = NULL;
	}
	s_ctrl->sensor_init_setting.reg_setting = reg_setting;

	mutex_lock(&(s_ctrl->sensor_power_state_mutex));
	if(s_ctrl->sensor_power_state == CAM_SENSOR_POWER_OFF)
	{
		s_ctrl->sensor_open_thread = kthread_run(
			cam_ext_sensor_start_thread, s_ctrl, s_ctrl->device_name);
		if (!s_ctrl->sensor_open_thread)
		{
			CAM_EXT_ERR(CAM_EXT_SENSOR, "create sensor start thread failed");
			if (reg_setting != NULL)
			{
				kfree(reg_setting);
				reg_setting = NULL;
			}
			rc = -1;
		}
		else
		{
			CAM_EXT_INFO(CAM_EXT_SENSOR, "create sensor start thread success");
		}
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor have power up");
	}

	mutex_unlock(&(s_ctrl->sensor_power_state_mutex));

	return rc;
}

int cam_ext_sensor_stop(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor do stop,sensor_name %s",s_ctrl->sensor_name);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	mutex_lock(&(s_ctrl->sensor_power_state_mutex));
	if (s_ctrl->sensor_power_state == CAM_SENSOR_POWER_ON) {
	} else {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor had power down, return");
		mutex_unlock(&(s_ctrl->sensor_power_state_mutex));
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
	}
	mutex_unlock(&(s_ctrl->sensor_power_state_mutex));

	if (s_ctrl->sensor_state == CAM_SENSOR_INIT) {
		//power off for sensor, if sensor had power up and had not acquire device.
                CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor power down in advance %s ",s_ctrl->sensor_name);
		oplus_cam_monitor_state(s_ctrl,
				s_ctrl->v4l2_dev_str.ent_function,
				CAM_SENSOR_NORMAL_POWER_UP_TYPE,
				false);
		rc = cam_sensor_power_down(s_ctrl);
	}

	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_ext_ftm_power_down(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	CAM_EXT_ERR(CAM_EXT_SENSOR, "FTM stream off");

	rc = cam_ext_ftm_apply_setting(s_ctrl, true);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR,
			"FTM Failed to stream off setting,rc=%d.",rc);
	}
	rc = cam_sensor_power_down(s_ctrl);
	CAM_EXT_ERR(CAM_EXT_SENSOR, "FTM power down rc=%d",rc);
	return rc;
}

int cam_ext_ftm_apply_setting(
	struct cam_sensor_ctrl_t *s_ctrl,
	bool apply_stream_off)
{
	const int DT_UNIT	= 5;
	const int IIC_RETRY 	= 3;
	int rc = 0, compatible_size = 0, reg_size = 0, len = 0;
	uint32_t reg_read = 0, setting_size = 0, temp, i, j;
	uint32_t *dt_setting		= NULL;
	uint32_t  dt_data[40]		= {0xFFFF};
	char   node_name[128]		= {0};
	char   compatible_str[128]	= {0};
	struct device_node *of_node	= s_ctrl->of_node;
	struct device_node *of_node_setting = NULL;
	struct cam_camera_slave_info       *slave_info;
	struct cam_sensor_i2c_reg_array    *reg_setting;
	struct cam_sensor_i2c_reg_array     reg_setting_one;
	struct cam_sensor_i2c_reg_setting   sensor_setting;

	if (apply_stream_off) {
		scnprintf(node_name, sizeof(node_name), "stream_off_common");
	} else {
		if (s_ctrl->sensordata == NULL) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"FTM apply setting fail. sensordata is null!");
			return -EINVAL;

		}
		slave_info = &(s_ctrl->sensordata->slave_info);

		compatible_size = of_property_count_u32_elems(of_node,
				"ftm_setting_compatible");
		if (compatible_size <= 0 || compatible_size % DT_UNIT != 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"Check ftm_setting_compatible size  %d", compatible_size);
			return -EINVAL;
		}

		rc = of_property_read_u32_array(of_node,
				"ftm_setting_compatible",
				dt_data,
				compatible_size);
		reg_size = compatible_size / DT_UNIT;

		for (i = 0; i < reg_size; i++) {
			if (dt_data[i*DT_UNIT] == 'R') {
				for (j = 0; j < IIC_RETRY; j++) {
					rc = camera_io_dev_read(
						&(s_ctrl->io_master_info),
						dt_data[i*DT_UNIT+1],
						&reg_read,
						(enum camera_sensor_i2c_type)(dt_data[i*DT_UNIT+2]),
						(enum camera_sensor_i2c_type)(dt_data[i*DT_UNIT+3]),
						true);
					if (rc) {
						CAM_EXT_ERR(CAM_EXT_SENSOR,
							"read dt_data fail %d, try again", rc);
					} else {
						break;
					}
				}

				if (j >= IIC_RETRY) {
					CAM_EXT_ERR(CAM_EXT_SENSOR,
						"read dt_data fail %d", rc);
					return -EINVAL;
				} else {
					len = scnprintf(compatible_str + len,
						sizeof(compatible_str) - len,
						"_%04X_%04X",
						dt_data[i*DT_UNIT+1], reg_read);
				}

			} else if (dt_data[i*DT_UNIT] == 'W') {
				sensor_setting.addr_type =
					(enum camera_sensor_i2c_type)(dt_data[i*DT_UNIT+2]);
				sensor_setting.data_type =
					(enum camera_sensor_i2c_type)(dt_data[i*DT_UNIT+3]);
				sensor_setting.delay = 3;
				sensor_setting.size  = 1;
				reg_setting_one.reg_addr	= dt_data[i*DT_UNIT+1];
				reg_setting_one.reg_data	= dt_data[i*DT_UNIT+4];
				reg_setting_one.delay		= 3;
				reg_setting_one.data_mask	= 0XFFFF;
				sensor_setting.reg_setting	= &reg_setting_one;

				for (j = 0; j < IIC_RETRY; j++) {
					rc = camera_io_dev_write(
						&(s_ctrl->io_master_info),
						&sensor_setting);
					if (rc) {
						CAM_EXT_ERR(CAM_EXT_SENSOR,
							"%s write 0x%x 0x%x failed, try again.",
							s_ctrl->sensor_name,
							reg_setting_one.reg_addr,
							reg_setting_one.reg_data);
					} else {
						break;
					}
				}

				if (j >= IIC_RETRY) {
					CAM_EXT_ERR(CAM_EXT_SENSOR, "Write Failed.");
					return -EINVAL;
				}
			}
		}

		scnprintf(node_name, sizeof(node_name), "camera%s", compatible_str);
	}

	of_node = of_find_node_by_name(NULL, "ftm_camera_settings");
	if (!of_node) {
		CAM_EXT_ERR(CAM_EXT_SENSOR,
			"Check ftm_camera_settings");
		return -EINVAL;

	}

	of_node_setting = of_find_compatible_node(of_node, NULL, node_name);
	if (!of_node_setting) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Check %s", node_name);
		return -EINVAL;
	}

	rc = of_property_read_u32(of_node_setting, "addr_type",
			&temp);
	sensor_setting.addr_type = (enum camera_sensor_i2c_type)temp;
	rc |= of_property_read_u32(of_node_setting, "data_type",
			&temp);
	sensor_setting.data_type = (enum camera_sensor_i2c_type)temp;
	rc |= of_property_read_u32(of_node_setting, "setting_delay",
			&temp);
	sensor_setting.delay = temp;

	setting_size = of_property_count_u32_elems(of_node_setting,
			"setting");
	sensor_setting.size = setting_size / (DT_UNIT-1);

	if (rc || setting_size <= 0 || setting_size % (DT_UNIT-1) != 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Check %s", node_name);
		return -EINVAL;

	}
	dt_setting = vmalloc(sizeof(uint32_t) * setting_size);
	if (dt_setting == NULL) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "vmalloc fail for dt_setting.");
		return -ENOMEM;

	}
	rc  = of_property_read_u32_array(of_node_setting, "setting",
			dt_setting, setting_size);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "get dt_setting fail.");
		rc = -EINVAL;
		goto free_dt_setting;
	}

	reg_setting = vmalloc(sizeof(struct cam_sensor_i2c_reg_array)
		* setting_size / (DT_UNIT-1));
	if (reg_setting == NULL) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "vmalloc fail for reg_setting.");
		rc = -ENOMEM;
		goto free_dt_setting;
	}

	for (i = 0; i < setting_size / (DT_UNIT-1); i ++) {
		reg_setting[i].reg_addr	= dt_setting[i*(DT_UNIT-1)+0];
		reg_setting[i].reg_data	= dt_setting[i*(DT_UNIT-1)+1];
		reg_setting[i].delay	= dt_setting[i*(DT_UNIT-1)+2];
		reg_setting[i].data_mask= dt_setting[i*(DT_UNIT-1)+3];
	}

	sensor_setting.reg_setting = reg_setting;
	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	vfree(reg_setting);

free_dt_setting:
	vfree(dt_setting);

	return rc;
}

int cam_ext_ftm_power_up(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	rc = cam_sensor_power_up(s_ctrl);
	if(rc < 0)
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "FTM power up faild!");
		return rc;
	}
	is_ftm_current_test =true;

	rc = cam_ext_ftm_apply_setting(s_ctrl, false);
	if (rc < 0)
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "FTM Failed to write sensor setting");
		goto power_down;
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "FTM successfully to write sensor setting");
	}
	return rc;
power_down:
	CAM_EXT_ERR(CAM_EXT_SENSOR, "FTM wirte setting failed,do power down");
	cam_sensor_power_down(s_ctrl);
	return rc;
}

bool cam_ext_ftm_if_do(void)
{
	CAM_EXT_INFO(CAM_EXT_SENSOR, "ftm state :%d",is_ftm_current_test);
	return is_ftm_current_test;
}

int32_t oplus_cam_handle_mem_ptr_for_compatibleinfo(
	uint64_t handle,
	uint32_t cmd,
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_cmd_probe_v2 *compatibleinfo)
{
	int rc = 0, i;
	uint32_t *cmd_buf;
	void *ptr;
	size_t len;
	struct cam_packet *pkt = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cmd_buf1 = 0;
	uintptr_t packet = 0;
	size_t    remain_len = 0;
	uint32_t probe_ver = 0;
	size_t required_size = 0;

	rc = cam_mem_get_cpu_buf(handle,
		&packet, &len);
	if (rc < 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Failed to get the command Buffer");
		return -EINVAL;
	}

	pkt = (struct cam_packet *)packet;
	if (pkt == NULL) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "packet pos is invalid");
		rc = -EINVAL;
		goto end;
	}

	if ((len < sizeof(struct cam_packet)) ||
		(pkt->cmd_buf_offset >= (len - sizeof(struct cam_packet)))) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Not enough buf provided");
		rc = -EINVAL;
		goto end;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "command descriptor pos is invalid");
		rc = -EINVAL;
		goto end;
	}

	probe_ver = pkt->header.op_code & 0xFFFFFF;
	CAM_EXT_INFO(CAM_EXT_SENSOR, "Received Header opcode: %u", probe_ver);

	i = 0; // for probe cmd buf.

	// for (i = 0; i < pkt->num_cmd_buf; i++) {
		rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
		if (rc)
			return rc;

		if (!(cmd_desc[i].length)) {
			rc = -EINVAL;
			goto end;
		}

		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cmd_buf1, &len);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"Failed to parse the command Buffer Header");
			goto end;
		}
		if (cmd_desc[i].offset >= len) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"offset past length of buffer");
			cam_mem_put_cpu_buf(cmd_desc[i].mem_handle);
			rc = -EINVAL;
			goto end;
		}
		remain_len = len - cmd_desc[i].offset;
		if (cmd_desc[i].length > remain_len) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"Not enough buffer provided for cmd");
			cam_mem_put_cpu_buf(cmd_desc[i].mem_handle);
			rc = -EINVAL;
			goto end;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

		if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE)
			required_size = sizeof(struct cam_cmd_i2c_info) +
				sizeof(struct cam_cmd_probe);
		else if(probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2)
			required_size = sizeof(struct cam_cmd_i2c_info) +
				sizeof(struct cam_cmd_probe_v2) ;

		required_size += sizeof(struct cam_cmd_probe_v2);
		if (remain_len < required_size) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"not enough buffer for compatibleinfo");
			cam_mem_put_cpu_buf(cmd_desc[i].mem_handle);
			rc = -EINVAL;
			goto end;
		}

		memcpy(compatibleinfo,
			ptr + sizeof(struct cam_cmd_i2c_info) + sizeof(struct cam_cmd_probe_v2),
			sizeof(struct cam_cmd_probe_v2));

		cam_mem_put_cpu_buf(cmd_desc[i].mem_handle);
	// }

end:
	cam_mem_put_cpu_buf(handle);
	return rc;
}

void oplus_cam_sensor_match_id_pre(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	uint32_t probe_intent = 0;

	if (!of_property_read_u32(s_ctrl->of_node,
		"probe_intent", &probe_intent)) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "xml_sensor_id %d will be modified.",
			s_ctrl->sensordata->slave_info.sensor_id);
		s_ctrl->sensordata->slave_info.sensor_id |= probe_intent;
		CAM_EXT_INFO(CAM_EXT_SENSOR, "xml_sensor_id|probe_intent: %d",
			s_ctrl->sensordata->slave_info.sensor_id);
	}
	if (s_ctrl->need_write_probe_register)
	{
		camera_io_dev_write(&s_ctrl->io_master_info,&(s_ctrl->probe_reg_setting));
		CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor: %s write probe register", s_ctrl->sensor_name);
	}
}

void oplus_cam_sensor_match_id_post(
	uint64_t handle,
	uint32_t cmd,
	struct cam_sensor_ctrl_t *s_ctrl,
	int32_t *qcom_rc)
{
	int rc = 0;
	struct cam_cmd_probe_v2 compatibleinfo;
	uint32_t addr;
	uint32_t data = 0;
	enum camera_sensor_i2c_type data_type;
	enum camera_sensor_i2c_type addr_type;
	uint16_t sensor_id_reg_addr = 0;

	rc = oplus_cam_handle_mem_ptr_for_compatibleinfo(
		handle, cmd, s_ctrl, &compatibleinfo);

	if (!rc && compatibleinfo.reg_addr) {
		if (s_ctrl->io_master_info.master_type == I2C_MASTER) {
			sensor_id_reg_addr = s_ctrl->io_master_info.qup_client->i2c_client->addr;
			s_ctrl->io_master_info.qup_client->i2c_client->addr = ((compatibleinfo.reg_addr >> 16) >> 1);
		} else if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
			sensor_id_reg_addr = s_ctrl->io_master_info.cci_client->sid;
			s_ctrl->io_master_info.cci_client->sid = ((compatibleinfo.reg_addr >> 16) >> 1);
		}
		addr = compatibleinfo.reg_addr & 0xFFFF;
		data_type = compatibleinfo.data_type;
		addr_type = compatibleinfo.addr_type;
		CAM_EXT_INFO(CAM_EXT_SENSOR, "compatibility verification: sid 0x%02x addr 0x%04x type %d %d",
			((compatibleinfo.reg_addr >> 16) >> 1), addr, addr_type, data_type);
		rc = camera_io_dev_read(
			&(s_ctrl->io_master_info), addr, &data, addr_type, data_type, TRUE);
		if (!rc) {
			if (data == compatibleinfo.expected_data) {
				CAM_EXT_INFO(CAM_EXT_SENSOR, "compatibility verification succ.");
			} else {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "read compatible id: 0x%x expected id 0x%x:",
					data, compatibleinfo.expected_data);
				*qcom_rc = -EINVAL;
			}
		} else {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "read compatibleid fail rc %d", rc);
		}
		if (s_ctrl->io_master_info.master_type == I2C_MASTER) {
			s_ctrl->io_master_info.qup_client->i2c_client->addr = sensor_id_reg_addr;
		} else if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
			s_ctrl->io_master_info.cci_client->sid = sensor_id_reg_addr;
		}
	}
}

int32_t cam_ext_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	bool power_up_process = false;
	struct iio_channel* therm_channel;
	int therm_val = -EINVAL;

	if (!s_ctrl || !arg) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "s_ctrl is NULL");
		return -EINVAL;
	}

	if (cmd->op_code != CAM_SENSOR_PROBE_CMD) {
		if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Invalid handle type: %d",
				cmd->handle_type);
			return -EINVAL;
		}
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	switch (cmd->op_code) {
		case CAM_OEM_GET_ID : {
			if (copy_to_user((void __user *)cmd->handle,&s_ctrl->soc_info.index, sizeof(uint32_t))) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "copy camera id to user fail ");
			}
			break;
		}
		case CAM_OEM_IO_CMD : {
			cam_ext_sensor_start(s_ctrl, arg);
			break;
		}
		case CAM_WRITE_VSYNC_DATA : {
			bool is_vsync_enable = FALSE;
			bool m_support_continuous = FALSE;
			struct cam_oem_initsettings *vsyncsettings = vzalloc(sizeof(struct cam_oem_initsettings));
			memset(vsyncsettings, 0, sizeof(struct cam_oem_initsettings));
			if (copy_from_user(vsyncsettings, u64_to_user_ptr(cmd->handle), cmd->size))
			{
				CAM_EXT_ERR(CAM_EXT_SENSOR, "vsyncsettings copy_to_user failed");
			}
			struct cam_sensor_i2c_reg_array *reg_setting;
			reg_setting = (struct cam_sensor_i2c_reg_array *)kmalloc((sizeof(struct cam_sensor_i2c_reg_array) * vsyncsettings->size), GFP_KERNEL);
			if (reg_setting == NULL)
		  	{
		  		CAM_EXT_ERR(CAM_EXT_SENSOR, "failed to allocate vsyncsettings memory!!!!");
		  		if (vsyncsettings != NULL)
  				{
  					vfree(vsyncsettings);
  					vsyncsettings = NULL;
  				}
  				return rc;
  			}
			for(int i = 0; i < vsyncsettings->size; i++){
				if(vsyncsettings->reg_setting[i].reg_data == s_ctrl->vsync_info.vsync_enable){
					is_vsync_enable = TRUE;
				}
				reg_setting[i].reg_addr  = vsyncsettings->reg_setting[i].reg_addr;
				reg_setting[i].reg_data  = vsyncsettings->reg_setting[i].reg_data;
				reg_setting[i].delay     = vsyncsettings->reg_setting[i].delay;
				reg_setting[i].data_mask = 0x00;
				reg_setting[i].operation = vsyncsettings->reg_setting[i].operation;
				CAM_EXT_INFO(CAM_EXT_SENSOR, "sensor is %s, reg_addr is %d, reg_data is %d, delay is %d, operation is %d",s_ctrl->io_master_info.sensor_name,reg_setting[i].reg_addr,reg_setting[i].reg_data,reg_setting[i].delay,reg_setting[i].operation);
				if ((reg_setting[i].operation == CCI_WRITE_BURST || reg_setting[i].operation == CCI_WRITE_SEQUENTIAL) && m_support_continuous == FALSE){
					m_support_continuous = TRUE;
				}
			}
			struct cam_sensor_i2c_reg_setting write_setting;
			write_setting.addr_type = vsyncsettings->addr_type;
	  		write_setting.data_type = vsyncsettings->data_type;
		  	write_setting.size = vsyncsettings->size;
			s_ctrl->is_surpport_wr_continuous = m_support_continuous;

			if (vsyncsettings != NULL)
		  	{
		  		vfree(vsyncsettings);
		  		vsyncsettings = NULL;
		  	}
		  	write_setting.reg_setting = reg_setting;
			CAM_EXT_INFO(CAM_EXT_SENSOR,"is_in_high_level is %d,is_vsync_enable is %d",s_ctrl->is_in_high_level,is_vsync_enable);
			if(s_ctrl->is_in_high_level == TRUE && is_vsync_enable == TRUE)
			{
				break;
			}
			camera_io_dev_write(&(s_ctrl->io_master_info), &write_setting);
			break;
		}
		case CAM_SET_VSYNC_SIGNAL: {
			if (copy_from_user(&s_ctrl->is_in_high_level, u64_to_user_ptr(cmd->handle), cmd->size))
			{
				CAM_EXT_ERR(CAM_EXT_SENSOR, "is_in_high_level copy_to_user failed");
			}
			CAM_EXT_INFO(CAM_EXT_SENSOR,"is_in_high_level is %d",s_ctrl->is_in_high_level);
			break;
		}
		case CAM_SENSOR_PROBE_CMD: {
		// mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		// rc = cam_sensor_driver_cmd(s_ctrl, arg);

		if (s_ctrl->is_probe_succeed == 1) {
			CAM_EXT_WARN(CAM_EXT_SENSOR,
				"Sensor %s already Probed in the slot",
				s_ctrl->sensor_name);
			break;
		}

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, cmd->op_code,
				s_ctrl);
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "Get Buffer Handle Failed");
				mutex_unlock(&(s_ctrl->cam_sensor_mutex));
				return rc;
			}
		} else {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Invalid Command Type: %d",
				cmd->handle_type);
			rc = -EINVAL;
			mutex_unlock(&(s_ctrl->cam_sensor_mutex));
			return rc;
		}

		/* Parse and fill vreg params for powerup settings */
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_setting,
			s_ctrl->sensordata->power_info.power_setting_size);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"Fail in filling vreg params for %s PUP rc %d",
				s_ctrl->sensor_name, rc);
			goto free_power_settings;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"Fail in filling vreg params for %s PDOWN rc %d",
				s_ctrl->sensor_name, rc);
			goto free_power_settings;
		}

		/* Power up and probe sensor */
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR,
				"Power up failed for %s sensor_id: 0x%x, slave_addr: 0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr
				);
			goto free_power_settings;
		}

		if (s_ctrl->i2c_data.reg_bank_unlock_settings.is_settings_valid) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_REG_BANK_UNLOCK);
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "REG_bank unlock failed");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
			rc = delete_request(&(s_ctrl->i2c_data.reg_bank_unlock_settings));
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_SENSOR,
					"failed while deleting REG_bank unlock settings");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
		}

		oplus_cam_sensor_match_id_pre(s_ctrl);

		/* Match sensor ID */
		rc = cam_sensor_match_id(s_ctrl);

		memset(s_ctrl->cam_sensor_reg_otp, 0, sizeof(s_ctrl->cam_sensor_reg_otp));
		if (!rc){
			cam_get_sensor_reg_otp(s_ctrl);
		}

		if (!rc) {
			oplus_cam_sensor_match_id_post(
				cmd->handle, cmd->op_code, s_ctrl, &rc);
		}

		if (rc < 0) {
			CAM_EXT_INFO(CAM_EXT_SENSOR,
				"Probe failed for %s slot:%d, slave_addr:0x%x, sensor_id:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			cam_sensor_power_down(s_ctrl);
			goto free_power_settings;
		}

		cam_sensor_read_qsc(s_ctrl);
		cam_sensor_get_dpc_data(s_ctrl);

		if (s_ctrl->i2c_data.reg_bank_lock_settings.is_settings_valid) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_REG_BANK_LOCK);
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "REG_bank lock failed");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
			rc = delete_request(&(s_ctrl->i2c_data.reg_bank_lock_settings));
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_SENSOR,
					"failed while deleting REG_bank lock settings");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
		}
		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Fail in %s sensor Power Down",
				s_ctrl->sensor_name);
			goto free_power_settings;
		}

		tmf8806_clean();
		tof8801_clean();
		/*
		* Set probe succeeded flag to 1 so that no other camera shall
		* probed on this slot
		*/
		s_ctrl->is_probe_succeed = 1;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;

		CAM_EXT_INFO(CAM_EXT_SENSOR,
				"Probe success for %s slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));

		return rc;

free_power_settings:
		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_down_setting_size = 0;
		power_info->power_setting_size = 0;
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
		}
		case CAM_RELEASE_DEV: {
			if (s_ctrl->i2c_data.awbotp_settings.is_settings_valid)
				delete_request(&s_ctrl->i2c_data.awbotp_settings);

			if (s_ctrl->i2c_data.lsc_settings.is_settings_valid)
				delete_request(&s_ctrl->i2c_data.lsc_settings);

			if (s_ctrl->i2c_data.qsc_settings.is_settings_valid)
			{
				CAM_EXT_INFO(CAM_EXT_SENSOR, "release: delete qsc_settings");
				delete_request(&s_ctrl->i2c_data.qsc_settings);
			}

			if (s_ctrl->i2c_data.spc_settings.is_settings_valid)
			{
				CAM_EXT_INFO(CAM_EXT_SENSOR, "release: delete spc_settings");
				delete_request(&s_ctrl->i2c_data.spc_settings);
			}

			if (s_ctrl->i2c_data.resolution_settings.is_settings_valid) {
				delete_request(&s_ctrl->i2c_data.resolution_settings);
			}
			mutex_lock(&(s_ctrl->sensor_power_state_mutex));
			if ((s_ctrl->sensor_state != CAM_SENSOR_INIT) &&
				(s_ctrl->sensor_state != CAM_SENSOR_START) &&
				s_ctrl->bridge_intf.link_hdl == -1 &&
				s_ctrl->sensor_power_state == CAM_SENSOR_POWER_ON) {
				oplus_cam_monitor_state(s_ctrl,
					s_ctrl->v4l2_dev_str.ent_function,
					CAM_SENSOR_NORMAL_POWER_UP_TYPE,
					false);
			}
			mutex_unlock(&(s_ctrl->sensor_power_state_mutex));
			goto qcom_process;
		}
		case CAM_STOP_DEV: {
			if (s_ctrl->i2c_data.awbotp_settings.is_settings_valid)
				delete_request(&s_ctrl->i2c_data.awbotp_settings);

			if (s_ctrl->i2c_data.lsc_settings.is_settings_valid)
				delete_request(&s_ctrl->i2c_data.lsc_settings);

			if (s_ctrl->i2c_data.qsc_settings.is_settings_valid)
			{
				CAM_EXT_INFO(CAM_EXT_SENSOR, "release: delete qsc_settings");
				delete_request(&s_ctrl->i2c_data.qsc_settings);
			}

			if (s_ctrl->i2c_data.spc_settings.is_settings_valid)
			{
				CAM_EXT_INFO(CAM_EXT_SENSOR, "release: delete spc_settings");
				delete_request(&s_ctrl->i2c_data.spc_settings);
			}

			if (s_ctrl->i2c_data.resolution_settings.is_settings_valid)
				delete_request(&s_ctrl->i2c_data.resolution_settings);

			goto qcom_process;
		}
		case CAM_ACQUIRE_DEV: {
			mutex_lock(&(s_ctrl->sensor_power_state_mutex));
			if (s_ctrl->sensor_state == CAM_SENSOR_INIT	&&
				s_ctrl->sensor_power_state == CAM_SENSOR_POWER_OFF &&
				s_ctrl->sensordata) {
				power_up_process = true;
			}
			mutex_unlock(&(s_ctrl->sensor_power_state_mutex));
			mutex_unlock(&(s_ctrl->cam_sensor_mutex));
			rc = cam_sensor_driver_cmd(s_ctrl, arg);
			mutex_lock(&(s_ctrl->cam_sensor_mutex));
			if (s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE	&&
				power_up_process == true) {
				oplus_cam_monitor_state(s_ctrl,
					s_ctrl->v4l2_dev_str.ent_function,
					CAM_SENSOR_NORMAL_POWER_UP_TYPE,
					true);
			}

		}
			break;
		case CAM_DUMP_SENSOR_OTP:{
			cam_get_sensor_reg_otp(s_ctrl);
			if (copy_to_user((void __user *)cmd->handle, &s_ctrl->cam_sensor_reg_otp, sizeof(s_ctrl->cam_sensor_reg_otp)))
			{
				CAM_EXT_WARN(CAM_EXT_SENSOR,"send data to user fail ");
			}
        }
		    break;
		case CAM_GET_DPC_DATA:{
			CAM_INFO(CAM_SENSOR, "sony_dfct_tbl: fd_dfct_num=%d, sg_dfct_num=%d",
				sony_dfct_tbl.fd_dfct_num, sony_dfct_tbl.sg_dfct_num);
			if (copy_to_user((void __user *) cmd->handle, &sony_dfct_tbl, sizeof(struct  sony_dfct_tbl_t)))
			{
				CAM_ERR(CAM_SENSOR, "Failed Copy to User");
				rc = -EFAULT;
				mutex_unlock(&(s_ctrl->cam_sensor_mutex));
				return rc;
			}
		}
			break;
		case CAM_GET_TEMPERATURE:{
			if (s_ctrl->sensor_power_state == CAM_SENSOR_POWER_ON)
			{
				therm_channel = iio_channel_get(&(s_ctrl->pdev->dev), "pmk8550_gpio03_therm_channel");
				if (NULL == therm_channel || IS_ERR(therm_channel))
				{
					rc = -EINVAL;
					CAM_EXT_ERR(CAM_EXT_SENSOR, "%s_therm_channel is ERROR", s_ctrl->sensor_name);
				}
				else
				{
					rc = iio_read_channel_processed(therm_channel, &therm_val);
					if (rc < 0)
					{
						CAM_EXT_ERR(CAM_EXT_SENSOR, "%s get therm ERROR, res %d", s_ctrl->sensor_name, rc);
					}
					else
					{
						CAM_EXT_INFO(CAM_EXT_SENSOR, "%s_therm_val %d", s_ctrl->sensor_name, therm_val);
						if (copy_to_user((void __user *)cmd->handle,&therm_val, sizeof(int))) {
							CAM_EXT_ERR(CAM_EXT_SENSOR, "copy camera id to user fail ");
						}
					}

					iio_channel_release(therm_channel);
				}
			}
		}
			break;
		default:
			goto qcom_process;
	}

	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;

qcom_process:
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_driver_cmd(s_ctrl, arg);
	return rc;
}

void cam_ext_sensor_driver_get_dt_data(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *of_node = s_ctrl->of_node;
	const uint32_t     default_setting_size =6;
	uint32_t           write_setting_size;
	uint32_t           reg_count = 0;

	rc = of_property_read_u32(of_node, "is-support-laser",
			&s_ctrl->is_support_laser);
	if ( rc < 0) {
		CAM_EXT_WARN(CAM_EXT_SENSOR, "Invalid sensor params");
		s_ctrl->is_support_laser = 0;
	}

	rc = of_property_read_u32(of_node, "enable_qsc_write_in_advance",
			&s_ctrl->sensor_qsc_setting.enable_qsc_write_in_advance);
	if ( rc < 0)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR, "enable_qsc_write_in_advance  Invalid sensor params");
		s_ctrl->sensor_qsc_setting.enable_qsc_write_in_advance = 0;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "enable_qsc_write_in_advance = %d",
			s_ctrl->sensor_qsc_setting.enable_qsc_write_in_advance);
	}

	rc = of_property_read_u32(of_node, "qsc_reg_addr",
			&s_ctrl->sensor_qsc_setting.qsc_reg_addr);
	if ( rc < 0)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR, "Invalid sensor params");
		s_ctrl->sensor_qsc_setting.qsc_reg_addr = 0;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "qsc_reg_addr = 0x%x",
			s_ctrl->sensor_qsc_setting.qsc_reg_addr);
	}

	rc = of_property_read_u32(of_node, "eeprom_slave_addr",
			&s_ctrl->sensor_qsc_setting.eeprom_slave_addr);
	if ( rc < 0)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR, "Invalid sensor params");
		s_ctrl->sensor_qsc_setting.eeprom_slave_addr = 0;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "eeprom_slave_addr = 0x%x",
			s_ctrl->sensor_qsc_setting.eeprom_slave_addr);
	}

	rc = of_property_read_u32(of_node, "qsc_data_size",
			&s_ctrl->sensor_qsc_setting.qsc_data_size);
	if ( rc < 0)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR, "Invalid sensor params");
		s_ctrl->sensor_qsc_setting.qsc_data_size = 0;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "qsc_data_size = %d",
			s_ctrl->sensor_qsc_setting.qsc_data_size);
	}

	rc = of_property_read_u32(of_node, "write_qsc_addr",
			&s_ctrl->sensor_qsc_setting.write_qsc_addr);
	if ( rc < 0)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR, "Invalid sensor params");
		s_ctrl->sensor_qsc_setting.write_qsc_addr = 0;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "write_qsc_addr = 0x%x",
			s_ctrl->sensor_qsc_setting.write_qsc_addr);
	}

	rc = of_property_read_u32(of_node, "sensor_setting_id",
			&s_ctrl->sensor_setting_id);
	if (rc)
	{
		s_ctrl->sensor_setting_id = 0;
		CAM_EXT_WARN(CAM_EXT_SENSOR, "get sensor_setting_id failed rc:%d, default %d",
			rc, s_ctrl->sensor_setting_id);
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "read sensor_setting_id success, value:%d",
			s_ctrl->sensor_setting_id);
	}

	rc = of_property_read_u32(of_node, "swremosaic_sensor_id",
			&s_ctrl->swremosaic_sensor_id);
	if ( rc < 0)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR, "swremosaic_sensor_id Invalid sensor params");
		s_ctrl->swremosaic_sensor_id = 0;
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "swremosaic_sensor_id = 0x%x",
			s_ctrl->swremosaic_sensor_id);
	}

	rc = of_property_read_u32(of_node, "is_need_framedrop",
			&s_ctrl->is_need_framedrop);
	if (rc)
	{
		s_ctrl->is_need_framedrop = 0;
		CAM_EXT_WARN(CAM_EXT_SENSOR, "get is_need_dropframe failed rc:%d, default %d",
			rc, s_ctrl->is_need_framedrop);
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_SENSOR, "read sensor_setting_id success, value:%d",
			s_ctrl->is_need_framedrop);
	}

	rc = of_property_read_bool(of_node, "need-write-probe-register");
	if (rc) {
		//length
		write_setting_size = of_property_count_u32_elems(of_node, "probe-reg-setting");
		//count
		while(write_setting_size >= default_setting_size)
		{
			write_setting_size -= default_setting_size;
			reg_count++;
		}

		if (reg_count <= 0)
		{
			s_ctrl->need_write_probe_register = false;
			CAM_EXT_WARN(CAM_EXT_SENSOR, "get probe register num %d, failed", reg_count);
			return;
		}

		s_ctrl->probe_reg_setting.reg_setting = kzalloc(reg_count * sizeof(*s_ctrl->probe_reg_setting.reg_setting), GFP_KERNEL);
		if(!s_ctrl->probe_reg_setting.reg_setting)
		{
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s kzalloc memory fail", __func__);
			return;
		}
		s_ctrl->probe_reg_setting.size = reg_count;


		for (int32_t i = 0; i < reg_count; i++)
		{
			of_property_read_u32_index(of_node, "probe-reg-setting",
				(i*default_setting_size+0), &(s_ctrl->probe_reg_setting.reg_setting[i].reg_addr));
			of_property_read_u32_index(of_node, "probe-reg-setting",
				(i*default_setting_size+2), &(s_ctrl->probe_reg_setting.reg_setting[i].reg_data));
			of_property_read_u32_index(of_node, "probe-reg-setting",
				(i*default_setting_size+4), &(s_ctrl->probe_reg_setting.reg_setting[i].delay));
			of_property_read_u32_index(of_node, "probe-reg-setting",
				(i*default_setting_size+5), &(s_ctrl->probe_reg_setting.reg_setting[i].data_mask));
		}

		s_ctrl->probe_reg_setting.delay = 0;
		of_property_read_u32_index(of_node, "probe-reg-setting",
				1, &(s_ctrl->probe_reg_setting.addr_type));
		of_property_read_u32_index(of_node, "probe-reg-setting",
				3, &(s_ctrl->probe_reg_setting.data_type));

		s_ctrl->need_write_probe_register = true;
		CAM_EXT_INFO(CAM_EXT_SENSOR, "need write probe_register count %d, reg [0x%x %d 0x%x %d %d 0x%x]",
				s_ctrl->probe_reg_setting.size,
				s_ctrl->probe_reg_setting.reg_setting[0].reg_addr,
				s_ctrl->probe_reg_setting.addr_type,
				s_ctrl->probe_reg_setting.reg_setting[0].reg_data,
				s_ctrl->probe_reg_setting.data_type,
				s_ctrl->probe_reg_setting.reg_setting[0].delay,
				s_ctrl->probe_reg_setting.reg_setting[0].data_mask);
	} else {
		s_ctrl->need_write_probe_register = false;
		CAM_EXT_WARN(CAM_EXT_SENSOR, "get need_write_probe_register failed rc:%d, default %d", rc, s_ctrl->need_write_probe_register);
	}

}

static long cam_ext_sensor_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);
	if (!s_ctrl) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "s_ctrl ptr is NULL");
			return -EINVAL;
	}
	if (s_ctrl->cam_sensor_init_completed == false) {
		cam_ext_sensor_driver_get_dt_data(s_ctrl);
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.resolution_settings.list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.lsc_settings.list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.qsc_settings.list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.awbotp_settings.list_head));
		//INIT_LIST_HEAD(&(s_ctrl->i2c_data.pdc_settings.list_head));
		mutex_init(&(s_ctrl->sensor_power_state_mutex));
		mutex_init(&(s_ctrl->sensor_initsetting_mutex));
		s_ctrl->sensor_power_state = CAM_SENSOR_POWER_OFF;
		s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
		s_ctrl->sensor_qsc_setting.qscsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
		s_ctrl->cam_sensor_init_completed = true;
	}

	switch (cmd)
	{
		/* Add for AT camera test */
		case VIDIOC_CAM_FTM_POWNER_DOWN:
		{
			rc = cam_ext_ftm_power_down(s_ctrl);
			break;
		}
		case VIDIOC_CAM_FTM_POWNER_UP:
		{
			rc = cam_ext_ftm_power_up(s_ctrl);
			break;
		}
		case VIDIOC_CAM_SENSOR_STATR:
		{
			rc = cam_ext_sensor_start(s_ctrl, arg);
			break;
		}
		case VIDIOC_CAM_SENSOR_STOP:
		{
			rc = cam_ext_sensor_stop(s_ctrl);
			break;
		}
		case VIDIOC_CAM_CONTROL:
			rc = cam_ext_sensor_driver_cmd(s_ctrl, arg);
			if (rc) {
				if (rc == -EBADR)
				{
					CAM_EXT_INFO(CAM_EXT_SENSOR,
						"Failed in Driver cmd: %d, it has been flushed", rc);
				}
				else if (rc != -ENODEV)
				{
					CAM_EXT_ERR(CAM_EXT_SENSOR,
					"Failed in Driver cmd: %d", rc);
				}
			}
			break;
		case CAM_SD_SHUTDOWN:
			if (cam_req_mgr_is_shutdown()	&&
				s_ctrl	&&
				s_ctrl->sensor_state != CAM_SENSOR_INIT	&&
				s_ctrl->is_probe_succeed != 0) {
				oplus_cam_monitor_state(s_ctrl,
					s_ctrl->v4l2_dev_str.ent_function,
					CAM_SENSOR_NORMAL_POWER_UP_TYPE,
					false);
			}
			rc = g_sensor_ops.ioctl(sd, cmd, arg);
			break;
		default:
		{
			rc = g_sensor_ops.ioctl(sd, cmd, arg);
			break;
		}
	}

	return rc;
}

void cam_sensor_register(void)
{
	g_sensor_ops.ioctl = cam_sensor_subdev_core_ops.ioctl;
	cam_sensor_subdev_core_ops.ioctl = cam_ext_sensor_subdev_ioctl;
}

int cam_ext_write_reg(
	struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t addr, enum camera_sensor_i2c_type addr_type,
	uint32_t data, enum camera_sensor_i2c_type data_type)
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
		.addr_type = addr_type,
		.data_type = data_type,
		.delay = 0x00,
	};

	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_write(&(s_ctrl->io_master_info),&i2c_write);
		if (rc < 0)
		{
			CAM_EXT_WARN(CAM_EXT_SENSOR, "Sensor[%s] write 0x%x failed, retry:%d",
				s_ctrl->sensor_name,
				addr,
				i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
}

int cam_ext_read_reg(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing)
{
	int retry = 3;
	int rc = 0;
	int i;
	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_read(
			&(s_ctrl->io_master_info),
			addr,
			data,
			addr_type,
			data_type, is_probing);
		if (rc < 0)
		{
			CAM_EXT_WARN(CAM_EXT_SENSOR, "Sensor[%s] read 0x%x failed, retry:%d",
				s_ctrl->sensor_name,
				addr,
				i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
}


int cam_get_sensor_reg_otp(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = -EINVAL;
	int i = 0, j = 0, compatible_size = 0, reg_size = 0;
	uint32_t data = 0;
	uint8_t data_size = 0;
	const int unit_size = 5;
	uint32_t  dt_data[10] = {0xFFFF};
	uint8_t temp_arr[CAM_OEM_OTP_DATA_MAX_LENGTH] = {0};
	compatible_size = of_property_count_u32_elems(s_ctrl->of_node, "reg_otp_compatible");
	if (compatible_size <= 0 ||
		compatible_size > sizeof(dt_data)/sizeof(dt_data[0]) ||
		compatible_size % unit_size != 0)
	{
		 CAM_EXT_WARN(CAM_EXT_SENSOR,"Check reg_otp_compatible size  %d", compatible_size);
		 return -EINVAL;
	}
	rc = of_property_read_u32_array(s_ctrl->of_node,
  				"reg_otp_compatible",
  				dt_data,
  				compatible_size);
  	reg_size = compatible_size / unit_size;
  	CAM_EXT_INFO(CAM_EXT_SENSOR,"compatible_size: %d, reg_size :%d", compatible_size, reg_size);
	if(s_ctrl->sensor_power_state == CAM_SENSOR_POWER_OFF)
	{
		CAM_EXT_WARN(CAM_EXT_SENSOR,"sensor power off, can not run i2c");
		return -EINVAL;
	}
  	for (i=0; i<reg_size; ++i)
  	{
  		if (dt_data[i*unit_size] == 'R')
  		{
  			data_size = (uint8_t)(dt_data[i*unit_size+4] & 0xFF);
  			for(j=0; j<data_size; ++j)
  			{
  				rc = cam_ext_read_reg(
					s_ctrl,
					dt_data[i*unit_size+1]+j,
					&data,
					dt_data[i*unit_size+2],
					dt_data[i*unit_size+3], true);
				if (rc < 0)
				{
					CAM_EXT_ERR(CAM_EXT_SENSOR,"i2c read sensor reg fail");
					return rc;
				}
				else
				{
					temp_arr[j] = (uint8_t)(data & 0xFF);
				}
  			}
  		}
  		if (dt_data[i*unit_size] == 'W')
  		{
  			cam_ext_write_reg(s_ctrl,
  				dt_data[i*unit_size+1],
  				dt_data[i*unit_size+2],
  				dt_data[i*unit_size+4],
  				dt_data[i*unit_size+3]);
  		}
  	}
  	s_ctrl->cam_sensor_reg_otp[0] = data_size;
  	memcpy(s_ctrl->cam_sensor_reg_otp+sizeof(data_size), temp_arr, data_size);

	return rc;
}

