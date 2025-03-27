#ifndef _CAM_EEPROM_CUSTOM_H
#define _CAM_EEPROM_CUSTOM_H
#include "cam_eeprom_dev.h"

	extern uint64_t total_size;
	extern bool chip_version_old;
	int EEPROM_RamWrite32A(struct cam_eeprom_ctrl_t *e_ctrl,uint32_t addr, uint32_t data);
	int EEPROM_RamRead32A(struct cam_eeprom_ctrl_t *e_ctrl,uint32_t addr, uint32_t* data);
	void EEPROM_IORead32A(struct cam_eeprom_ctrl_t *e_ctrl, uint32_t IOadrs, uint32_t *IOdata );
	void EEPROM_IOWrite32A(struct cam_eeprom_ctrl_t *e_ctrl, uint32_t IOadrs, uint32_t IOdata );
	uint8_t EEPROM_FlashMultiRead(struct cam_eeprom_ctrl_t *e_ctrl,
		uint8_t SelMat, uint32_t UlAddress, uint32_t *PulData , uint8_t UcLength );
	int32_t EEPROM_CommonWrite(struct cam_eeprom_ctrl_t *e_ctrl,
		struct cam_write_eeprom_t *cam_write_eeprom);
	int32_t EEPROM_Fm24c256eWrite(struct cam_eeprom_ctrl_t *e_ctrl,
		struct cam_write_eeprom_t *cam_write_eeprom);
	int32_t cam_eeprom_driver_cmd_oem(struct cam_eeprom_ctrl_t *e_ctrl, void *arg);
	int oplus_cam_eeprom_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
		struct cam_eeprom_memory_map_t *emap, int j, uint8_t *memptr);

	void cam_eeprom_parse_dt_oem(struct cam_eeprom_ctrl_t *e_ctrl);
	void set_actuator_ois_eeprom_shared_mutex_init_flag(bool init_flag);
	bool get_actuator_ois_eeprom_shared_mutex_init_flag(void);
	struct mutex* get_actuator_ois_eeprom_shared_mutex(void);
	extern bool chip_version_old;
	void cam_eeprom_init(struct cam_eeprom_ctrl_t *e_ctrl);
	void cam_eeprom_register(void);

#endif /* _CAM_EEPROM_CUSTOM_H */
