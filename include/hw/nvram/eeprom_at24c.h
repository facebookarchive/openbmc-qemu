#ifndef HW_NVRAM_EEPROM_AT24C
#define HW_NVRAM_EEPROM_AT24C

#include "hw/i2c/i2c.h"

void at24c_eeprom_init(I2CBus *bus, uint8_t address, const uint8_t *rom, uint32_t rom_size, bool writable);

#endif
