#include <linux/module.h>
#include <linux/crc32.h>
#include "cam_sensor_io_custom.h"
#include "cam_debug.h"
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "cam_eeprom_custom.h"

#define         USER_MAT                0
#define         INF_MAT0                1
#define         INF_MAT1                2
#define         INF_MAT2                4
#define         CMD_IO_ADR_ACCESS       0xC000              // IO Write Access
#define         CMD_IO_DAT_ACCESS       0xD000              // IO Read Access
#define         CMD_IO_WR_ACCESS        0x5000              // IO Write Access
#define         CMD_IO_WRDAT_ACCESS     0x5001              // IO Read Access
#define         SEMCO_EEPROM_PACK_SIZE  128
#define         SEMCO_EEPROM_TOTAL_SIZE 1792
#define         WRITE_SEM1217_EEPROM_MAX_LENGTH  64
#define         WRITE_SEM1217_EEPROM_BASE_ADDRESS  0xC000
#define         EEPROM_FUNCTION_OV64B40_SEM1217S  1
#define         EEPROM_FUNCTION_IMX766_FM24C256E  2

uint64_t        total_size=0;
bool chip_version_old = FALSE;
struct mutex actuator_ois_eeprom_shared_mutex;
bool actuator_ois_eeprom_shared_mutex_init_flag = false;

extern struct v4l2_subdev_core_ops cam_eeprom_subdev_core_ops;

static struct v4l2_subdev_core_ops g_eeprom_core_ops;

struct cam_sensor_i2c_reg_array fm24pc256e_write_cc_enable_setting[] = {
    {.reg_addr = 0xFF,   .reg_data = 0x35, .delay = 0x00, .data_mask = 0x00}, \
};

struct cam_sensor_i2c_reg_array fm24pc256e_wp_disable_setting[] = {
    {.reg_addr = 0x06CA, .reg_data = 0x2D, .delay = 0x00, .data_mask = 0x00}, \
};

struct cam_sensor_i2c_reg_array fm24pc256e_wp_enable_setting[] = {
    {.reg_addr = 0x06CA, .reg_data = 0x2F, .delay = 0x00, .data_mask = 0x00}, \
};


//********************************************************************************
// Function Name 	: IOWrite32A
//********************************************************************************
int EEPROM_RamWrite32A(struct cam_eeprom_ctrl_t *e_ctrl,uint32_t addr, uint32_t data) {
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

	if (e_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_EEPROM,"Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		if(e_ctrl->change_cci && chip_version_old == FALSE) {
			rc = cam_ext_io_dev_write(e_ctrl->io_master_info_ois.cci_client, &i2c_write);
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"write 0x%04x failed, retry:%d", addr, i+1);
			} else {
				return rc;
			}
		} else {
			rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_write);
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"write 0x%04x failed, retry:%d", addr, i+1);
			} else {
				return rc;
			}
		}
	}
	return rc;
}

int EEPROM_RamRead32A(struct cam_eeprom_ctrl_t *e_ctrl,uint32_t addr, uint32_t* data) {
	int32_t rc = 0;
	int retry = 3;
	int i;

	if (e_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_EEPROM,"Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		if(e_ctrl->change_cci && chip_version_old == FALSE) {
			rc = cam_ext_io_dev_read(e_ctrl->io_master_info_ois.cci_client,
						(uint32_t)addr,
						(uint32_t *)data,
						CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD, false);

			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"read 0x%04x failed, retry:%d", addr, i+1);
			} else {
				return rc;
			}
		} else {
			rc = cam_ext_io_dev_read(e_ctrl->io_master_info.cci_client,
						(uint32_t)addr,
						(uint32_t *)data,
						CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD, false);

			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"read 0x%04x failed, retry:%d", addr, i+1);
			} else {
				return rc;
			}
		}
	}
	return rc;
}

void EEPROM_IORead32A(struct cam_eeprom_ctrl_t *e_ctrl, uint32_t IOadrs, uint32_t *IOdata ) {
	EEPROM_RamWrite32A(e_ctrl, CMD_IO_ADR_ACCESS, IOadrs ) ;
	EEPROM_RamRead32A (e_ctrl, CMD_IO_DAT_ACCESS, IOdata ) ;
}

//********************************************************************************
// Function Name 	: IOWrite32A
//********************************************************************************
void EEPROM_IOWrite32A(struct cam_eeprom_ctrl_t *e_ctrl, uint32_t IOadrs, uint32_t IOdata) {
	EEPROM_RamWrite32A(e_ctrl, CMD_IO_ADR_ACCESS, IOadrs ) ;
	EEPROM_RamWrite32A(e_ctrl, CMD_IO_DAT_ACCESS, IOdata ) ;
}

uint8_t	EEPROM_FlashMultiRead(struct cam_eeprom_ctrl_t *e_ctrl,
	uint8_t SelMat, uint32_t UlAddress, uint32_t *PulData, uint8_t UcLength) {
	uint8_t	i	 ;

	if (SelMat != USER_MAT && SelMat != INF_MAT0 && SelMat != INF_MAT1 && SelMat != INF_MAT2) {
		return 10;
	}

	if( UlAddress > 0x000003FFF ) {
		return 9;
	}

	EEPROM_IOWrite32A(e_ctrl, 0xE07008 , 0x00000000 | (uint32_t)(UcLength-1) );
	EEPROM_IOWrite32A(e_ctrl, 0xE0700C , ((uint32_t)SelMat << 16) | ( UlAddress & 0x00003FFF ) );

	EEPROM_IOWrite32A(e_ctrl, 0xE0701C , 0x00000000);
	EEPROM_IOWrite32A(e_ctrl, 0xE07010 , 0x00000001 );
	for( i=0 ; i < UcLength ; i++ ) {
		EEPROM_IORead32A(e_ctrl, 0xE07000 , &PulData[i] ) ;
	}

	EEPROM_IOWrite32A(e_ctrl, 0xE0701C , 0x00000002);
	return( 0 ) ;
}

int oplus_cam_eeprom_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_map_t    *emap, int j, uint8_t *memptr) {
	int                                rc = 0;
	int                                i,m,size,read_size;
	uint32_t                           data[32]={0};

	if(j>0) {
		size=emap[j-1].mem.valid_size;
	} else {
		size=0;
	}

	for(i=0;i<((emap[j].mem.valid_size/4)/32+1);i++) {
		if((i==(emap[j].mem.valid_size/4)/32)&&(((emap[j].mem.valid_size/4)%32)!=0)) {
			read_size=((emap[j].mem.valid_size/4)%32);
		} else if((i==(emap[j].mem.valid_size/4)/32)&&(((emap[j].mem.valid_size/4)%32)==0)) {
			break;
		} else {
			read_size = 32;
		}

		rc = EEPROM_FlashMultiRead(e_ctrl,USER_MAT,emap[j].mem.addr+i*32,data,read_size);
		if(rc!=0) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"read failed rc=%d ",rc);
			return rc;
		} else {
			for(m=0;m<read_size;m++) {
				memptr[size+i*4*32+m*4]=(data[m]&0xff);
				memptr[size+i*4*32+m*4+1]=((data[m]>>8)&0xff);
				memptr[size+i*4*32+m*4+2]=((data[m]>>16)&0xff);
				memptr[size+i*4*32+m*4+3]=(data[m]>>24);
				total_size=total_size+4;
			}
		}
	}

	return rc;
}

#define WRITE_EEPROM_MAX_LENGTH    64
int32_t EEPROM_Fm24c256eWrite(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_write_eeprom_t *cam_write_eeprom)
{
	uint32_t    i;
	int32_t     rc;
	uint8_t     j;
	uint32_t    star_addr = 0x0000;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	uint32_t    m_eeprom_size = 0;
	struct cam_sensor_i2c_reg_array    *i2c_reg_arrays
		= (struct cam_sensor_i2c_reg_array *)kmalloc((sizeof(struct cam_sensor_i2c_reg_array) * SEMCO_EEPROM_PACK_SIZE),
			GFP_KERNEL);
	if ( i2c_reg_arrays == NULL ) {
		CAM_EXT_ERR(CAM_EXT_EEPROM,"failed to allocate memory!!!!");
		rc = -ENOMEM;
		return rc;
	}

	if (e_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_EEPROM,"Invalid Args");
		kfree(i2c_reg_arrays);
		i2c_reg_arrays = NULL;
		return -EINVAL;
	}

	e_ctrl->io_master_info.cci_client->sid = 0xB2 >> 1;
	i2c_reg_settings.reg_setting = fm24pc256e_write_cc_enable_setting;
	i2c_reg_settings.size = 1;
	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_settings.delay = 0x00;
	rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client,
		&i2c_reg_settings, 1);
	msleep(50);

	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;

	if ( rc < 0 ) {
		CAM_EXT_ERR(CAM_EXT_EEPROM,"EEPROM fm24pc256e write cc enable failed, fallback to operate gt24p128c2csli");
	} else {
		i2c_reg_settings.reg_setting = fm24pc256e_wp_disable_setting;
		rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client,
			&i2c_reg_settings, 1);
		if ( rc < 0 ) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"EEPROM fm24pc256e write wp disable failed");
		}
		msleep(15);

		m_eeprom_size = cam_write_eeprom->calibDataSize;
		e_ctrl->io_master_info.cci_client->sid = 0xA2 >> 1;
		CAM_EXT_INFO(CAM_EXT_EEPROM,
			"EEPROM fm24pc256e write success, calibDataSize:%d, m_eeprom_size:%d, base_addr: 0x%x",
			cam_write_eeprom->calibDataSize,
			m_eeprom_size,
			cam_write_eeprom->baseAddr);

		for (i = 0; i < m_eeprom_size;) {
			i2c_reg_settings.size = 0;
			star_addr = (cam_write_eeprom->baseAddr + i);
			for (j = 0; j < WRITE_EEPROM_MAX_LENGTH && i < m_eeprom_size; j++) {
				i2c_reg_arrays[j].reg_addr = star_addr;
				i2c_reg_arrays[j].reg_data = cam_write_eeprom->calibData[i];
				i2c_reg_arrays[j].delay = 0;
				i2c_reg_settings.size++;
				i++;
			}
			i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
			i2c_reg_settings.reg_setting = i2c_reg_arrays;
			i2c_reg_settings.delay = 10;
			rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client, &i2c_reg_settings, 1);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"eeprom write failed rc %d", rc);
				return rc;
			}
		}

		e_ctrl->io_master_info.cci_client->sid = 0xB2 >> 1;
		i2c_reg_settings.reg_setting = fm24pc256e_write_cc_enable_setting;
		i2c_reg_settings.size = 1;
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client,
			&i2c_reg_settings, 1);
		if ( rc < 0 ) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"EEPROM fm24pc256e write cc enable failed");
		}
		msleep(50);

		i2c_reg_settings.reg_setting = fm24pc256e_wp_enable_setting;
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client,
			&i2c_reg_settings, 1);
		if ( rc < 0 ) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"EEPROM fm24pc256e write wp enable failed");
		}

		e_ctrl->io_master_info.cci_client->sid = 0xA2 >> 1;
	}

	CAM_EXT_INFO(CAM_EXT_EEPROM,"calibDataSize exit!!!, rc: %d", rc);
	if (i2c_reg_arrays != NULL ) {
		kfree(i2c_reg_arrays);
		i2c_reg_arrays = NULL;
	}

	return( 0 );
}

int32_t EEPROM_Sem1217sWrite(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_write_eeprom_t *cam_write_eeprom) {
	int i = 0;
	int j = 0;
	uint32_t readcalibData;
	int32_t  rc = 0;
	uint32_t  star_addr = 0x0000;
	int32_t  m_eeprom_size;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	struct cam_sensor_i2c_reg_array    i2c_reg_arrays[WRITE_SEM1217_EEPROM_MAX_LENGTH];
	struct cam_sensor_i2c_reg_array    i2c_reg_array;

	CAM_EXT_INFO(CAM_EXT_EEPROM,"entry write eeprom");

	//disable write protection
	if (cam_write_eeprom->isWRP == 0x01) {
		//ois off
		i2c_reg_settings.size = 1;
		i2c_reg_array.reg_addr = 0x0000;
		i2c_reg_array.reg_data = 0x0;
		i2c_reg_array.delay = 0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_reg_settings);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"write ois off failed rc %d",rc);
			return rc;
		}
		//af off
		i2c_reg_settings.size = 1;
		i2c_reg_array.reg_addr = 0x0200;
		i2c_reg_array.reg_data = 0x0;
		i2c_reg_array.delay = 0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_reg_settings);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"write af off failed rc %d",rc);
			return rc;
		}
		//disable write protection
		i2c_reg_settings.size = 1;
		i2c_reg_array.reg_addr = 0x4E04;
		i2c_reg_array.reg_data = 0x23016745;
		i2c_reg_array.delay = 0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;

		rc = cam_ext_io_dev_read(e_ctrl->io_master_info.cci_client,
			i2c_reg_array.reg_addr, &readcalibData,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_DWORD,
			false);
		CAM_EXT_INFO(CAM_EXT_EEPROM,"cam_write_eeprom->eepromName :%s  set reg_data:0x%x cam reg_addr:0x%x, WRPaddr: 0x%x",
			cam_write_eeprom->eepromName,i2c_reg_array.reg_data,i2c_reg_array.reg_addr,readcalibData);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"read WRPaddr failed rc %d",rc);
			return rc;
		}

		if (readcalibData != i2c_reg_array.reg_data) {
			rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_reg_settings);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"write WRPaddr failed rc %d",rc);
				return rc;
			}
			CAM_EXT_INFO(CAM_EXT_EEPROM,"write!cam: WRPaddr: 0x%x", readcalibData);
			msleep(30);
		}
	}
	CAM_EXT_ERR(CAM_EXT_EEPROM,"write start, cam: ID: 0x%x, reg_addr: 0x%x, val: %d",
		cam_write_eeprom->cam_id,
		cam_write_eeprom->baseAddr,
		cam_write_eeprom->calibData[0]);

	m_eeprom_size = (cam_write_eeprom->calibDataSize / WRITE_SEM1217_EEPROM_MAX_LENGTH + 1) * WRITE_SEM1217_EEPROM_MAX_LENGTH;
	for(i = cam_write_eeprom->calibDataSize; i < m_eeprom_size; i++) {
		cam_write_eeprom->calibData[i] = 0xFF;
	}

	for (i = 0; i < m_eeprom_size;) {
		i2c_reg_settings.size = 0;
		star_addr = (cam_write_eeprom->baseAddr + i);
		for (j = 0; j < WRITE_SEM1217_EEPROM_MAX_LENGTH && i < m_eeprom_size; j++) {
			i2c_reg_arrays[j].reg_addr = star_addr + WRITE_SEM1217_EEPROM_BASE_ADDRESS;
			i2c_reg_arrays[j].reg_data = cam_write_eeprom->calibData[i];
			i2c_reg_arrays[j].delay = 0;
			i2c_reg_settings.size++;
			i++;
		}
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_settings.reg_setting = i2c_reg_arrays;
		i2c_reg_settings.delay = 10;
		rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client, &i2c_reg_settings, 1);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"eeprom write failed rc %d", rc);
			return rc;
		}
	}

	if (cam_write_eeprom->isWRP == 0x01) {
		i2c_reg_settings.size = 1;
		i2c_reg_array.reg_addr = 0x4E04;
		i2c_reg_array.reg_data = 0x00000000;
		i2c_reg_array.delay = 0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;
		i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;

		rc = cam_ext_io_dev_read(e_ctrl->io_master_info.cci_client,
		i2c_reg_array.reg_addr, &readcalibData,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_DWORD,
			false);
		CAM_EXT_INFO(CAM_EXT_EEPROM,"cam_write_eeprom->eepromName :%s  set reg_data:0x%x cam reg_addr:0x%x, WRPaddr: 0x%x",
			cam_write_eeprom->eepromName,i2c_reg_array.reg_data,i2c_reg_array.reg_addr,readcalibData);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"read WRPaddr failed rc %d",rc);
			return rc;
		}
		if(readcalibData != i2c_reg_array.reg_data) {
		rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_reg_settings);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"write WRPaddr failed rc %d",rc);
			return rc;
		}
		CAM_EXT_INFO(CAM_EXT_EEPROM,"write!cam: WRPaddr: 0x%x", readcalibData);
		}
	}
	CAM_EXT_INFO(CAM_EXT_EEPROM,"exit write eeprom !!!");

	return rc;
}

#define WRITE_EEPROM_MAX_LENGTH 64
int32_t EEPROM_CommonWrite(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_write_eeprom_t *cam_write_eeprom) {
	int i = 0;
	int j = 0;
	uint32_t readcalibData;
	int32_t  rc = 0;
	uint32_t    star_addr = 0x0000;
	int32_t  m_eeprom_size;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	struct cam_sensor_i2c_reg_array    i2c_reg_arrays[WRITE_EEPROM_MAX_LENGTH];
	struct cam_sensor_i2c_reg_array    i2c_reg_array;

	i2c_reg_settings.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_settings.delay = WRITE_DATA_DELAY;

	CAM_EXT_INFO(CAM_EXT_EEPROM,"entry write eeprom");

	//disable write protection
	if (cam_write_eeprom->isWRP == 0x01) {
		i2c_reg_settings.size = 1;
	//new add WRPaddr and data in xml
		i2c_reg_array.reg_addr = cam_write_eeprom->WRPaddr;
		i2c_reg_array.reg_data = cam_write_eeprom->CloseWRP;

		i2c_reg_array.delay = 0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;

		rc = cam_ext_io_dev_read(e_ctrl->io_master_info.cci_client,
			 i2c_reg_array.reg_addr, &readcalibData,
			 CAMERA_SENSOR_I2C_TYPE_WORD,
			 CAMERA_SENSOR_I2C_TYPE_BYTE, false);
			 CAM_EXT_INFO(CAM_EXT_EEPROM,"cam_write_eeprom->eepromName :%s  set reg_data:0x%x cam reg_addr:0x%x, WRPaddr: 0x%x",  cam_write_eeprom->eepromName,i2c_reg_array.reg_data,i2c_reg_array.reg_addr,readcalibData);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"read WRPaddr failed rc %d",rc);
			return rc;
		}

		if (readcalibData != i2c_reg_array.reg_data) {
			rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_reg_settings);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"write WRPaddr failed rc %d",rc);
				return rc;
			}

			CAM_EXT_INFO(CAM_EXT_EEPROM,"write!cam: WRPaddr: 0x%x", readcalibData);
			msleep(30);
		}
	}
	CAM_EXT_INFO(CAM_EXT_EEPROM,"write start, cam: ID: 0x%x, reg_addr: 0x%x, val: %d",
				cam_write_eeprom->cam_id,
				cam_write_eeprom->baseAddr,
				cam_write_eeprom->calibData[0]);

	m_eeprom_size = cam_write_eeprom->calibDataSize;
	for (i = 0; i < m_eeprom_size;) {
		i2c_reg_settings.size = 0;
		star_addr = (cam_write_eeprom->baseAddr + i);
		for (j = 0; j < WRITE_EEPROM_MAX_LENGTH && i < m_eeprom_size; j++) {
			i2c_reg_arrays[j].reg_addr = star_addr;
			i2c_reg_arrays[j].reg_data = cam_write_eeprom->calibData[i];
			i2c_reg_arrays[j].delay = 0;
			i2c_reg_settings.size++;
			i++;
		}
		i2c_reg_settings.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_settings.reg_setting = i2c_reg_arrays;
		i2c_reg_settings.delay = 10;
		rc = cam_ext_cci_i2c_write_continuous_table(e_ctrl->io_master_info.cci_client, &i2c_reg_settings, 1);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"eeprom write failed rc %d, calibDataSize: %d", rc, m_eeprom_size);
			return rc;
		}
	}

	if (cam_write_eeprom->isWRP == 0x01) {
		i2c_reg_settings.size = 1;
	//new add WRPaddr and data in xml
		i2c_reg_array.reg_addr = cam_write_eeprom->WRPaddr;
		i2c_reg_array.reg_data = cam_write_eeprom->OpenWRP;

		i2c_reg_array.delay = 0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;

		rc = cam_ext_io_dev_read(e_ctrl->io_master_info.cci_client,
			 i2c_reg_array.reg_addr, &readcalibData,
			 CAMERA_SENSOR_I2C_TYPE_WORD,
			 CAMERA_SENSOR_I2C_TYPE_BYTE, false);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_EEPROM,"read WRPaddr failed rc %d",rc);
			return rc;
		}

		if(readcalibData != i2c_reg_array.reg_data) {
			rc = cam_ext_io_dev_write(e_ctrl->io_master_info.cci_client, &i2c_reg_settings);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"write WRPaddr failed rc %d",rc);
				return rc;
			}
			CAM_EXT_INFO(CAM_EXT_EEPROM,"write!cam: WRPaddr: 0x%x", readcalibData);
		}
	}
	CAM_EXT_INFO(CAM_EXT_EEPROM,"exit write eeprom !!!");

	return rc;
}

static int32_t cam_eeprom_write_data(struct cam_eeprom_ctrl_t *e_ctrl,
	void *arg) {
	int32_t  rc = -1;
	//bool is_fm24pc256e = false;
	//bool is_sem1217s = false;

	struct cam_control    *cmd = (struct cam_control *)arg;
	struct cam_write_eeprom_t cam_write_eeprom;

	if (!e_ctrl) {
		CAM_EXT_ERR(CAM_EXT_EEPROM,"failed: e_ctrl is NULL");
		return -EINVAL;
	}

	memset(&cam_write_eeprom, 0, sizeof(struct cam_write_eeprom_t));
	if (copy_from_user(&cam_write_eeprom,
		(void __user *) cmd->handle,
		sizeof(struct cam_write_eeprom_t))) {

		CAM_EXT_ERR(CAM_EXT_EEPROM,"Failed Copy from User");
		return -EFAULT;
	}
/*
	cam_write_eeprom.eepromName[EEPROM_NAME_LENGTH-1] = '\0';
	if (strcmp(cam_write_eeprom.eepromName, "imx766_fm24c256e_wide_second") == 0) {
		is_fm24pc256e = true;
	} else if (strcmp(cam_write_eeprom.eepromName, "ov64b40_sem1217s") == 0) {
		is_sem1217s = true;
	}

	//disable write protection
	if (cam_write_eeprom.calibDataSize > 0
		&& cam_write_eeprom.calibDataSize <= CALIB_DATA_LENGTH) {
		if (is_fm24pc256e) {
			rc = EEPROM_Fm24c256eWrite(e_ctrl, &cam_write_eeprom);
		} else if (is_sem1217s) {
			rc = EEPROM_Sem1217sWrite(e_ctrl, &cam_write_eeprom);
		} else {
			rc = EEPROM_CommonWrite(e_ctrl, &cam_write_eeprom);
		}
	}
*/
	if (cam_write_eeprom.calibDataSize > 0
		&& cam_write_eeprom.calibDataSize <= CALIB_DATA_LENGTH)
	{
		if (e_ctrl->eeprom_function == EEPROM_FUNCTION_IMX766_FM24C256E) {
			rc = EEPROM_Fm24c256eWrite(e_ctrl, &cam_write_eeprom);
		} else if (e_ctrl->eeprom_function == EEPROM_FUNCTION_OV64B40_SEM1217S)
		{
			rc = EEPROM_Sem1217sWrite(e_ctrl, &cam_write_eeprom);
		} else {
			rc = EEPROM_CommonWrite(e_ctrl, &cam_write_eeprom);
		}
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_EEPROM,"calibDataSize Error :%d",cam_write_eeprom.calibDataSize);
	}

	return rc;
}

static int32_t cam_eeprom_check_data(struct cam_eeprom_ctrl_t *e_ctrl,
	void *arg) {
	int i = 0;
	uint32_t readdata;
	int32_t  rc = 0;
	struct cam_control    *cmd = (struct cam_control *)arg;
	struct check_eeprom_data_t check_eeprom_data;
	memset(&check_eeprom_data, 0, sizeof(struct check_eeprom_data_t));
	if (copy_from_user(&check_eeprom_data,
		(void __user *) cmd->handle,
		sizeof(struct check_eeprom_data_t))) {

		CAM_EXT_ERR(CAM_EXT_EEPROM,"Failed Copy from User");
		return -EFAULT;
	}

	if (check_eeprom_data.checkDataSize > 0
			&& check_eeprom_data.checkDataSize <= CALIB_DATA_LENGTH) {
		check_eeprom_data.eepromData_checksum = 0;
		for (i = 0; i < check_eeprom_data.checkDataSize; i++) {
			rc = cam_ext_io_dev_read(e_ctrl->io_master_info.cci_client,
				(check_eeprom_data.startAddr + i*WRITE_DATA_MAX_LENGTH), &readdata,
				CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE, false);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_EEPROM,"eeprom read failed rc %d",rc);
				return rc;
			}
			check_eeprom_data.eepromData_checksum += readdata;
		}

		CAM_EXT_DBG(CAM_EXT_EEPROM,"eepromData_checksum: %d", check_eeprom_data.eepromData_checksum);

		if (copy_to_user((void __user *) cmd->handle,
			&check_eeprom_data,
			sizeof(struct check_eeprom_data_t))) {

			CAM_EXT_ERR(CAM_EXT_EEPROM,"Failed Copy to User");
			return -EFAULT;
		}
	}
	return rc;
}


void set_actuator_ois_eeprom_shared_mutex_init_flag(bool init_flag) {
	actuator_ois_eeprom_shared_mutex_init_flag = init_flag;
}

struct mutex *get_actuator_ois_eeprom_shared_mutex(void) {
	return &actuator_ois_eeprom_shared_mutex;
}

bool get_actuator_ois_eeprom_shared_mutex_init_flag(void) {
	return actuator_ois_eeprom_shared_mutex_init_flag;
}


void cam_eeprom_parse_dt_oem(struct cam_eeprom_ctrl_t *e_ctrl) {
	int id = 0;
	int ret = 0;
	struct cam_hw_soc_info         *soc_info = &e_ctrl->soc_info;
	struct device_node             *of_node = NULL;

	if (!soc_info->dev) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Dev is NULL");
		return;
	}

	of_node = soc_info->dev->of_node;

	ret = of_property_read_u32(of_node, "actuator_ois_eeprom_merge", &id);
	if (ret) {
		e_ctrl->actuator_ois_eeprom_merge_flag = 0;
		CAM_EXT_DBG(CAM_EXT_OIS, "get actuator_ois_eeprom_merge_flag failed rc:%d, default %d", ret, e_ctrl->actuator_ois_eeprom_merge_flag);
	} else {
		e_ctrl->actuator_ois_eeprom_merge_flag = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read actuator_ois_eeprom_merge_flag success, value:%d", e_ctrl->actuator_ois_eeprom_merge_flag);

		e_ctrl->actuator_ois_eeprom_merge_mutex = &actuator_ois_eeprom_shared_mutex;
		if (!actuator_ois_eeprom_shared_mutex_init_flag) {
			mutex_init(e_ctrl->actuator_ois_eeprom_merge_mutex);
			actuator_ois_eeprom_shared_mutex_init_flag = true;
		}
	}

	ret = of_property_read_u32(of_node, "eeprom_function", &id);
	if (ret) {
		e_ctrl->eeprom_function = 0;
		CAM_EXT_DBG(CAM_EXT_EEPROM, "get eeprom_function failed rc:%d, default %d", ret, e_ctrl->eeprom_function);
	} else {
		e_ctrl->eeprom_function = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_EEPROM, "read eeprom_function success, value:%d", e_ctrl->eeprom_function);
	}
}

static long cam_eeprom_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg) {
	int                            rc = 0;
	struct cam_eeprom_ctrl_t *e_ctrl =
		v4l2_get_subdevdata(sd);
	struct cam_control *cam_cmd = (struct cam_control *)arg;

	switch (cmd) {
		case VIDIOC_CAM_CONTROL:
			switch (cam_cmd->op_code) {
				case CAM_WRITE_CALIBRATION_DATA:
					CAM_EXT_DBG(CAM_EXT_EEPROM,"CAM_WRITE_CALIBRATION_DATA");
					rc = cam_eeprom_write_data(e_ctrl, arg);
					if (rc) {
						CAM_EXT_ERR(CAM_EXT_EEPROM,"Failed in write calibration data");
					}
					break;
				case CAM_CHECK_CALIBRATION_DATA:
					CAM_EXT_DBG(CAM_EXT_EEPROM,"CAM_CHECK_CALIBRATION_DATA");
					rc = cam_eeprom_check_data(e_ctrl, arg);
					if (rc) {
						CAM_EXT_ERR(CAM_EXT_EEPROM,"Failed in check eeprom data");
					}
					break;
				case CAM_WRITE_AE_SYNC_DATA:
					CAM_EXT_DBG(CAM_EXT_EEPROM,"CAM_WRITE_AE_SYNC_DATA");
					rc = cam_eeprom_write_data(e_ctrl, arg);
					if (rc) {
						CAM_EXT_ERR(CAM_EXT_EEPROM,"Failed in write AE sync data");
					}
					break;
				case CAM_QUERY_CAP: {
					cam_eeprom_init(e_ctrl);
					break;
				}
				default:
					CAM_EXT_DBG(CAM_EXT_EEPROM,"finish cam_eeprom_driver_cmd");
					break;
			}
			break;
		default:
			break;
	}

	rc = g_eeprom_core_ops.ioctl(sd, cmd, arg);

	return rc;
}


void cam_eeprom_register(void) {
	CAM_EXT_INFO(CAM_EXT_EEPROM, "cam_actuator_register E");

	g_eeprom_core_ops.ioctl = cam_eeprom_subdev_core_ops.ioctl;
	cam_eeprom_subdev_core_ops.ioctl = cam_eeprom_ioctl;

}

void cam_eeprom_init(struct cam_eeprom_ctrl_t *e_ctrl)
{
	CAM_EXT_INFO(CAM_EXT_EEPROM, "cam_actuator_init E");

	if (!e_ctrl) {
		CAM_EXT_INFO(CAM_EXT_EEPROM, "a_ctrl ptr is NULL");
		return;
	}

	cam_eeprom_parse_dt_oem(e_ctrl);


	CAM_EXT_INFO(CAM_EXT_EEPROM, "cam_actuator_init X");

}

