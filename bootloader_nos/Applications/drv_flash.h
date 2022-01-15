#ifndef __DRV_FLASH_H
#define __DRV_FLASH_H

#include <stdint.h>

int drv_flash_erase(uint32_t addr, uint32_t size);
int drv_flash_write(uint32_t addr, const uint8_t *buf, uint32_t size);

#endif
