#ifndef _OPLUS_CAM_EEPROM_H
#define _OPLUS_CAM_EEPROM_H
#include "cam_eeprom_dev.h"

int32_t oplus_cam_eeprom_write(struct cam_eeprom_ctrl_t *e_ctrl);
void oplus_cam_eeprom(struct cam_eeprom_ctrl_t *e_ctrl);
#endif /* _OPLUS_CAM_EEPROM_H */