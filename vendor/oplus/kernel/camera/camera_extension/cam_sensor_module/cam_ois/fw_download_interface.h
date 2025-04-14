#ifndef _DOWNLOAD_OIS_FW_H_
#define _DOWNLOAD_OIS_FW_H_

#include <linux/module.h>
#include <linux/firmware.h>
#include <cam_sensor_cmn_header.h>
#include "cam_ois_dev.h"
#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_sensor_util.h"
#include "cam_debug_util.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"

#include <linux/string.h>
#include <linux/time.h>
#include <linux/types.h>

//int RamWrite32A(uint32_t addr, uint32_t data);
//int RamRead32A(uint32_t addr, uint32_t* data);
int RamWrite32A_oneplus(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data);
int RamRead32A_oneplus(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t* data);
int DownloadFW(struct cam_ois_ctrl_t *o_ctrl);
bool IsOISReady(struct cam_ois_ctrl_t *o_ctrl);
void DeinitOIS(struct cam_ois_ctrl_t *o_ctrl);
void InitOISResource(struct cam_ois_ctrl_t *o_ctrl);
int WRITE_QTIMER_TO_OIS (struct cam_ois_ctrl_t *o_ctrl);
int StoreOisGyroGian(struct cam_ois_ctrl_t *o_ctrl);
int WriteOisGyroGianToRam(struct cam_ois_ctrl_t *o_ctrl,DUAL_OIS_CALI_RESULTS* current_gyro_gain);

int WriteOisGyroGian(struct cam_ois_ctrl_t *o_ctrl,OIS_GYROGAIN* current_gyro_gain);

int DoBU24721GyroOffset(struct cam_ois_ctrl_t *o_ctrl, uint32_t *gyro_data);
int DoSEM1217SGyroOffset(struct cam_ois_ctrl_t *o_ctrl, uint32_t *gyro_data);
int DoDW9786GyroOffset(struct cam_ois_ctrl_t *o_ctrl, uint32_t *gyro_data);

int QueryDualOisSmaWireStatus(struct cam_ois_ctrl_t *o_ctrl, uint32_t *sma_wire_broken);
int GetDualOisGyroGain(struct cam_ois_ctrl_t *o_ctrl,DUAL_OIS_GYROGAIN* initial_gyro_gain);
int WriteDualOisShiftRegister(struct cam_ois_ctrl_t *o_ctrl, uint32_t distance);

int32_t cam_ext_ois_construct_default_power_setting(struct cam_sensor_power_ctrl_t *power_info);
int32_t cam_ext_ois_construct_default_power_setting_dw9786(struct cam_sensor_power_ctrl_t *power_info);
int32_t cam_ext_ois_construct_default_power_setting_bu24721(struct cam_sensor_power_ctrl_t *power_info);
int32_t cam_ext_ois_construct_default_power_setting_1217s(struct cam_sensor_power_ctrl_t *power_info);


int cam_ois_download_start(struct cam_ois_ctrl_t *o_ctrl);
void cam_ois_do_power_down(struct cam_ois_ctrl_t *o_ctrl);
void cam_ext_set_ois_disable(struct cam_ois_ctrl_t *o_ctrl);

int RohmOisWrite(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data);
int RohmOisRead(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t* data);
int Rohm_ois_fw_download(struct cam_ois_ctrl_t *o_ctrl);
void spi_mode_switch(struct cam_ois_ctrl_t *o_ctrl);

uint8_t I2C_FW_8bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr);
uint32_t I2C_FM_32bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr);
uint32_t I2C_FM_16bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr);

uint8_t sem1217_8bit_read(uint32_t addr);
uint16_t sem1217_16bit_read(uint32_t addr);
int sem1217_8bit_write(uint32_t addr, uint8_t data);
int sem1217_block_write(void* register_data,int size);

int I2C_FM_8bit_write(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint8_t data);
int I2C_FM_16bit_write(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint16_t data);
void I2C_FM_block_write(struct cam_ois_ctrl_t *o_ctrl,void* register_data,int size);



uint8_t I2C_OIS_8bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr);
uint16_t I2C_OIS_16bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr);
int I2C_OIS_32bit__read(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint32_t* data);

int I2C_OIS_8bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint8_t data);
int I2C_OIS_16bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint16_t data);
int I2C_OIS_32bit_write(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint32_t data);

void I2C_OIS_block_write(struct cam_ois_ctrl_t *o_ctrl,void* register_data,int size);

int write_reg_16bit_value_8bit(struct cam_ois_ctrl_t *o_ctrl,unsigned short addr, uint16_t data);
int write_reg_16bit_value_16bit(struct cam_ois_ctrl_t *o_ctrl,unsigned short addr, uint16_t data);
int read_reg_16bit_value_16bit(struct cam_ois_ctrl_t *o_ctrl,unsigned short addr, uint32_t* data);
int i2c_block_write_reg(struct cam_ois_ctrl_t *o_ctrl,void* register_data, int size);

int update_ois_firmware_bu24721(void *arg);

void Wait(int us);
void Enable_gyro_gain(struct cam_ois_ctrl_t *o_ctrl);



#endif
/* _DOWNLOAD_OIS_FW_H_ */

