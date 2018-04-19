#include <runtime/lib.h>
#include <crc/crc32-ieee.h>
#include "loader-interface.h"
#include "uos-interface.h"
#include "loadable.h"

extern loadable_t __loadable_header_uos_loadable_t;

#ifndef FIRST_LOADABLE_ADDR       
#define FIRST_LOADABLE_ADDR     0x08008000
#endif

#ifndef LAST_LOADABLE_ADDR
#define LAST_LOADABLE_ADDR      0x0801f000
#endif

#ifndef LOADABLE_ADDR_GAP
#define LOADABLE_ADDR_GAP       0x1000
#endif

static loadable_t *find_loadable(loadable_t *loadable)
{
    if ((unsigned)loadable >= LAST_LOADABLE_ADDR)
        return 0;
        
    do {
        if (loadable->magic == LOADABLE_MAGIC && 
            ((unsigned)loadable + loadable->size <= LAST_LOADABLE_ADDR)) {
#ifdef LOADABLE_CHECK_CRC
            int res;
            res = loader_check_crc(loadable);
            if (res == LOADABLE_ERR_OK)
                return loadable;
#endif
        }
        loadable = (loadable_t *)((uint8_t *)loadable + LOADABLE_ADDR_GAP);
    } while ((unsigned)loadable < LAST_LOADABLE_ADDR);
    
    return 0;
}

static loadable_t *efl_first_loadable()
{
    loadable_t *loadable = (loadable_t *) FIRST_LOADABLE_ADDR;
    return find_loadable(loadable);
}

static loadable_t *efl_next_loadable(loadable_t *current)
{
    loadable_t *loadable;
    
    if (current->size)
        loadable = (loadable_t *)
            (((unsigned)current + current->size + LOADABLE_ADDR_GAP - 1) &
            ~(LOADABLE_ADDR_GAP - 1));
    else
        loadable = (loadable_t *)((uint8_t *)current + LOADABLE_ADDR_GAP);
        
    return find_loadable(loadable);
}

static int efl_load(loadable_t *loadable)
{
    return loadable->load((loadable_t *) &__loadable_header_uos_loadable_t);
}

static int efl_unload(loadable_t *loadable)
{
    return loadable->unload();
}

/*
static void efl_init_loadable(loadable_t *loadable)
{
    loadable->init();
}
*/

const loaderif_t __uos_ext_flash_loader = 
{
    .first_loadable = efl_first_loadable,
    .next_loadable = efl_next_loadable,
    .load = efl_load,
    .unload = efl_unload,
    //.init_loadable = efl_init_loadable
};

