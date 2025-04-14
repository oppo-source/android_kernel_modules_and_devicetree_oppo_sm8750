// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include "cam_sensor_util.h"
#include "oplus_cam_ois.h"

#define OIS_HALL_DATA_SIZE   52
#define OIS_HALL_DATA_SET_SIZE 4
#define OIS_HALL_DATA_SET_COUNT 11
#define OIS_DW9786_HALL_DATA_SIZE   58

void OISCountinueRead(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, void *data, uint16_t size)
{
	int i = 0;
	int32_t rc = 0;
	int retry = 3;

	for(i = 0; i < retry; i++)
	{
		rc = camera_io_dev_read_seq(&(o_ctrl->io_master_info), addr, (uint8_t *)data,
		                            CAMERA_SENSOR_I2C_TYPE_WORD,
		                            CAMERA_SENSOR_I2C_TYPE_WORD,
		                            size);
		if (rc < 0)
		{
			CAM_ERR(CAM_OIS, "ois type=%d,Continue read failed, rc:%d, retry:%d",o_ctrl->ois_type, rc, i+1);
		}
		else
		{
			break;
		}
	}
}

int OIS_READ_HALL_DATA_TO_UMD_Bu24721 (struct cam_ois_ctrl_t *o_ctrl,struct i2c_settings_array *i2c_settings) {

	int32_t 						rc = 0,i=0;
	struct i2c_settings_list	   *i2c_list;
	uint8_t 					   *read_buff = NULL;
	uint32_t						buff_length = 0;
	uint32_t						read_length = 0;
	uint64_t						fifo_count = 0;
	uint64_t						preqtime_ms=0,newqtime=0;
	uint32_t						hall_data_x = 0,hall_data_y = 0;
	int 							j=0;
	uint8_t 					   *temp_buff = NULL;
	uint32_t						delayCount = 0, delayTime = 0;
	uint64_t						qtimer_ns = 0;



	list_for_each_entry(i2c_list,
		&(i2c_settings->list_head), list) {
		read_buff = i2c_list->i2c_settings.read_buff;
		buff_length = i2c_list->i2c_settings.read_buff_len;
		if ((read_buff == NULL) || (buff_length == 0)) {
				CAM_DBG(CAM_OIS,
						"Invalid input buffer, buffer: %pK, length: %d",
						read_buff, buff_length);
				return -EINVAL;
		}
		read_length = OIS_HALL_DATA_SIZE;
		temp_buff = kzalloc(read_length, GFP_KERNEL);
		memset(temp_buff, 0, read_length);
		CAM_DBG(CAM_OIS,"buffer: %pK, fbufer_length: %d  read_length=%d",read_buff, buff_length,read_length);
		CAM_DBG(CAM_OIS,"start read");
		OISCountinueRead(o_ctrl, 0xF200, (void *)temp_buff, read_length);
		cam_sensor_util_get_current_qtimer_ns(&qtimer_ns);

		CAM_DBG(CAM_OIS,"read done");

		fifo_count = temp_buff[0] & 0xF;
		read_buff[56] = fifo_count;

		if(fifo_count > 12) {
				CAM_DBG(CAM_OIS,"ois have drop data fifo_count=%d",fifo_count);
				fifo_count = 12;
				read_buff[56] = fifo_count;
		}

		for(j=0; j<fifo_count * 4; j++)
		{
			read_buff[j]=temp_buff[j + 1];
		}

        delayCount = ((uint32_t)(temp_buff[j + 1] << 8) | temp_buff[j + 2]);
        if( fifo_count == 0 || delayCount > 1000 )
        {
            CAM_ERR(CAM_OIS," ois data error , force to clear delayCount: %d ",delayCount);
            delayCount = 0;
        }
		delayTime = delayCount * 17800; //nano sec
		qtimer_ns -= delayTime;

		//set qtimer
		read_buff[48] = (qtimer_ns >> 56) & 0xFF;
		read_buff[49] = (qtimer_ns >> 48) & 0xFF;
		read_buff[50] = (qtimer_ns >> 40) & 0xFF;
		read_buff[51] = (qtimer_ns >> 32)& 0xFF;
		read_buff[52] = (qtimer_ns >> 24) & 0xFF;
		read_buff[53] = (qtimer_ns >> 16) & 0xFF;
		read_buff[54] = (qtimer_ns >> 8) & 0xFF;
		read_buff[55] = qtimer_ns & 0xFF;

		if((newqtime-(fifo_count-1)*2000000)<preqtime_ms)
				CAM_DBG(CAM_OIS,"time error");
		preqtime_ms=newqtime;
		CAM_DBG(CAM_OIS,"READ fifo_count=%d, i: %d, j: %d",fifo_count, i, j);
		CAM_DBG(CAM_OIS,"Rqtimer value: 0x%lx", qtimer_ns);

		if(fifo_count > 0) {
			for(i=0 ; i<fifo_count; i++){
				hall_data_x=((read_buff[i*4]<<8)+read_buff[i*4+1]);
				hall_data_y=((read_buff[i*4+2]<<8)+read_buff[i*4+3]);
				CAM_DBG(CAM_OIS,"hall_data_x=0x%x hall_data_y=0x%x ",
					hall_data_x,
					hall_data_y);
			}
		}
	}
	kfree(temp_buff);
	return rc;
}

int OIS_READ_HALL_DATA_TO_UMD_SEM1217S (struct cam_ois_ctrl_t *o_ctrl,struct i2c_settings_array *i2c_settings)
{
	int32_t 						rc = 0,i=0;
	struct i2c_settings_list	   *i2c_list;
	uint8_t 					   *read_buff = NULL;
	uint32_t						buff_length = 0;
	uint32_t						read_length = 0;
	uint64_t						fifo_count = 0;
	uint32_t						hall_data_x = 0,hall_data_y = 0;
	int 							j=0;
	uint8_t 						*temp_buff = NULL;
	uint8_t 						*temp_data = NULL;
	uint32_t						delayCount = 0, delayTime = 0;
	uint64_t						qtimer_ns = 0;
	uint64_t						qtimer_ns_1 = 0;
	uint64_t						time_diff = 0;

	list_for_each_entry(i2c_list,
		&(i2c_settings->list_head), list) {
		read_buff = i2c_list->i2c_settings.read_buff;
		buff_length = i2c_list->i2c_settings.read_buff_len;
		if ((read_buff == NULL) || (buff_length == 0)) {
				CAM_DBG(CAM_OIS,
						"Invalid input buffer, buffer: %pK, length: %d",
						read_buff, buff_length);
				return -EINVAL;
		}
		read_length = OIS_HALL_DATA_SET_SIZE*OIS_HALL_DATA_SET_COUNT;
		temp_buff = kzalloc(read_length, GFP_KERNEL);
		memset(temp_buff, 0, read_length);
		CAM_DBG(CAM_OIS,"buffer: %pK, fbufer_length: %d  read_length=%d",read_buff, buff_length,read_length);
		CAM_DBG(CAM_OIS,"start read");

		cam_sensor_util_get_current_qtimer_ns(&qtimer_ns_1);

		OISCountinueRead(o_ctrl, 0x1100, (void *)temp_buff, read_length);
		cam_sensor_util_get_current_qtimer_ns(&qtimer_ns);

		fifo_count = temp_buff[0] & 0xF;
		read_buff[56] = fifo_count;
		time_diff = qtimer_ns-qtimer_ns_1;
		CAM_DBG(CAM_OIS,"read done fifo_count %x, start_qtimer: %lld ns, end_qtimer: %lld ns laps %lld ns",
			fifo_count, qtimer_ns_1, qtimer_ns, time_diff);

		if(fifo_count > OIS_HALL_DATA_SET_COUNT - 1) {
			CAM_DBG(CAM_OIS,"ois have drop data fifo_count=%d",fifo_count);
			fifo_count = OIS_HALL_DATA_SET_COUNT - 1;
			read_buff[56] = fifo_count;
		}

		// 0        1..3            4.5.6.7       40.41.42.43
		// fifo   reservred        HALL XY0        HALL XY9
		// big-little ending/new-old data order convert
		temp_data = &temp_buff[4];
		if (fifo_count > 0)
		{
			for(i=((fifo_count * OIS_HALL_DATA_SET_SIZE/4)-1), j=0; i>=0; i--,j++)
			{
				read_buff[i*4+1] = temp_data[j*4+0]; // HALL X
				read_buff[i*4+0] = temp_data[j*4+1]; // HALL X
				read_buff[i*4+3] = temp_data[j*4+2]; // HALL Y
				read_buff[i*4+2] = temp_data[j*4+3]; // HALL Y
			}
		}

		delayCount =((uint32_t)(temp_buff[3] << 8) | temp_buff[2]);
		CAM_DBG(CAM_OIS,"%x,%x => %d",
			temp_buff[2],temp_buff[3],delayCount);
		if( fifo_count == 0 || delayCount > 60000 )
		{
			CAM_ERR(CAM_OIS," ois data error , force to clear delayTime: %d ",delayTime);
			delayCount = 0;
		}

		delayTime = delayCount * 100/3; //nano sec
		qtimer_ns -= delayTime;
		CAM_DBG(CAM_OIS,"after correct time stick fifo_count %x, start_qtimer: %lld ns, end_qtimer: %lld ns laps %lld ns",
			fifo_count, qtimer_ns_1, qtimer_ns, time_diff);

		//set qtimer
		read_buff[48] = (qtimer_ns >> 56) & 0xFF;
		read_buff[49] = (qtimer_ns >> 48) & 0xFF;
		read_buff[50] = (qtimer_ns >> 40) & 0xFF;
		read_buff[51] = (qtimer_ns >> 32)& 0xFF;
		read_buff[52] = (qtimer_ns >> 24) & 0xFF;
		read_buff[53] = (qtimer_ns >> 16) & 0xFF;
		read_buff[54] = (qtimer_ns >> 8) & 0xFF;
		read_buff[55] = qtimer_ns & 0xFF;

		CAM_DBG(CAM_OIS,"READ fifo_count=%d, i: %d, j: %d",fifo_count, i, j);
		CAM_DBG(CAM_OIS,"Rqtimer value: 0x%lx", qtimer_ns);

		if(fifo_count > 0) {
			for(i=0 ; i<fifo_count; i++){
				hall_data_x=((read_buff[i*4]<<8)+read_buff[i*4+1]);
				hall_data_y=((read_buff[i*4+2]<<8)+read_buff[i*4+3]);
				CAM_DBG(CAM_OIS,"hall_data_x=0x%d hall_data_y=0x%d ",
					hall_data_x,
					hall_data_y);
			}
		}
	}
	kfree(temp_buff);
	return rc;
}

int OIS_READ_HALL_DATA_TO_UMD_DW9786 (struct cam_ois_ctrl_t *o_ctrl,struct i2c_settings_array *i2c_settings) {
	uint8_t 					   *read_buff = NULL;
	uint8_t 					   *temp_buff = NULL;
	int32_t 						rc = 0;
	int32_t 						i=0, j=0;
	uint32_t						copy_length = 0;
	uint32_t						buff_length = 0;
	uint32_t						read_length = 0;
	uint32_t						hall_data_x = 0,hall_data_y = 0;
	uint32_t						qtime_H = 0, qtime_L = 0;
	uint64_t						qtimer_ns = 0;
	uint64_t						fifo_count = 0;
	int32_t 						data_offset = 10;
	struct i2c_settings_list	   *i2c_list;
	// uint32_t ReadVal = 0;
	//char trace[64] = {0};

	list_for_each_entry(i2c_list,
		&(i2c_settings->list_head), list) {
		read_buff = i2c_list->i2c_settings.read_buff;
		buff_length = i2c_list->i2c_settings.read_buff_len;
		if ((read_buff == NULL) || (buff_length == 0)) {
				CAM_ERR(CAM_OIS,
						"Invalid input buffer, buffer: %pK, length: %d",
						read_buff, buff_length);
				return -EINVAL;
		}
		read_length = OIS_DW9786_HALL_DATA_SIZE; //58
		temp_buff = kzalloc(read_length, GFP_KERNEL);
		memset(temp_buff, 0, read_length);
		CAM_DBG(CAM_OIS,"buffer: %pK, fbufer_length: %d  read_length=%d",read_buff, buff_length,read_length);
		//snprintf(trace, sizeof(trace), "KMD %d_%d_0x%x OIS READ", o_ctrl->io_master_info.cci_client->cci_device, o_ctrl->io_master_info.cci_client->cci_i2c_master, o_ctrl->io_master_info.cci_client->sid*2);
		msleep(1);
		//trace_int(trace, read_length);
		OISCountinueRead(o_ctrl, 0xB900, (void *)temp_buff, read_length);
		//trace_int(trace, 0);
		CAM_DBG(CAM_OIS,"buffer:%x %x X:%x-%x Y:%x-%x",temp_buff[0],temp_buff[1],temp_buff[10],temp_buff[11],temp_buff[12],temp_buff[13]);
		// read_reg_16bit_value_16bit(o_ctrl, 0xB96E, &ReadVal);
		// CAM_ERR(CAM_OIS, "DW9786 Read 0xB96E = 0x%x", ReadVal);

		fifo_count = temp_buff[1] & 0xFF;
		if(fifo_count > 12)
		{
			CAM_ERR(CAM_OIS,"ois have drop data fifo_count=%d",fifo_count);
			if(fifo_count != 0xff)
			{
				fifo_count = 12;
			}
			else
			{
				fifo_count = 0;
				CAM_ERR(CAM_OIS,"ois data is invalied skip!");
			}
		}
		copy_length = data_offset + fifo_count*4;
		for(j=0; j < copy_length; j++)
		{
			read_buff[j] = temp_buff[j];
		}
		read_buff[1] = fifo_count;
		CAM_DBG(CAM_OIS,"DW9786 READ fifo_count=%d",fifo_count);

		cam_sensor_util_get_current_qtimer_ns(&qtimer_ns);
		qtime_H = (((uint32_t)read_buff[2]<<24)+((uint32_t)read_buff[3]<<16)+((uint32_t)read_buff[4]<<8)+(uint32_t)read_buff[5]);
		qtime_L = (((uint32_t)read_buff[6]<<24)+((uint32_t)read_buff[7]<<16)+((uint32_t)read_buff[8]<<8)+(uint32_t)read_buff[9]);
		CAM_DBG(CAM_OIS,"Qtimer Now H=0x%x L=0x%x",(qtimer_ns>>32)&0xffffffff,qtimer_ns&0xffffffff);
		CAM_DBG(CAM_OIS,"Qtimer Red H=0x%x L=0x%x",qtime_H,qtime_L);

		if(fifo_count > 0) {
			for(i=0 ; i<fifo_count; i++){
				hall_data_x=((read_buff[data_offset+i*4]<<8)+read_buff[data_offset+i*4+1]);
				hall_data_y=((read_buff[data_offset+i*4+2]<<8)+read_buff[data_offset+i*4+3]);
				CAM_DBG(CAM_OIS,"hall_data_x=0x%x hall_data_y=0x%x ",
					hall_data_x,
					hall_data_y);
			}
		}
		//rc = WRITE_QTIMER_TO_OIS(o_ctrl);
		// if (rc < 0) {
		// 	CAM_ERR(CAM_OIS,"Failed to write qtimer value: %d",rc);
		// }
	}
	kfree(temp_buff);
	return rc;
}