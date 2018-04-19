#ifndef __STM32L1_PROG_FLASH_H__
#define __STM32L1_PROG_FLASH_H__

#include <flash/flash-interface.h>

struct _stm32l1_prog_flash_t
{
    flashif_t       flashif;
    
    uint32_t		start_addr;
    uint32_t		size;
};
typedef struct _stm32l1_prog_flash_t stm32l1_prog_flash_t;

void stm32l1_prog_flash_init(stm32l1_prog_flash_t *m, uint32_t start_addr, uint32_t size);

#endif // __STM32L1_PROG_FLASH_H__
