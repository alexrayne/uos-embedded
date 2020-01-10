#include <runtime/lib.h>
#include "eeprom.h"

void stm32l1_eeprom_write(void *pointer, unsigned value)
{
	// Unlock EEPROM
	FLASH->PEKEYR = FLASH_PEKEY1;
	FLASH->PEKEYR = FLASH_PEKEY2;
	// Erase location (4 bytes)
	unsigned *pu = pointer;
	*pu = 0;
	// Write location
	FLASH->PECR &= ~FLASH_FTDW;
	*pu = value;
	// Lock EEPROM
	FLASH->PECR |= FLASH_PELOCK;
}
