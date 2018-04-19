#include <runtime/lib.h>
#include "loader-interface.h"
#include "uos-interface.h"
#include "loadable.h"

extern loadable_t __loadable_start, __loadable_end;
extern loadable_t __loadable_header_uos_loadable_t;

static loadable_t *il_first_loadable()
{
    return &__loadable_start;
}

static loadable_t *il_next_loadable(loadable_t *current)
{
    assert((current >= &__loadable_start) && (current < &__loadable_end));
    ++current;
    
    unsigned long *raw = (unsigned long *)current;
    while ((*raw != LOADABLE_MAGIC) && (raw < (unsigned long *)&__loadable_end))
        raw++;
    
    if (raw >= (unsigned long *)&__loadable_end)
        return 0;
        
    current = (loadable_t *) raw;
        
#ifdef LOADABLE_CHECK_HEADER_CRC
        // Здесь должна быть проверка контрольной суммы заголовка
#endif

    return current;
}

static int il_load(loadable_t *loadable)
{
#ifdef LOADABLE_CHECK_CRC
    // Здесь должна быть проверка контрольной суммы
#endif

    return loadable->load((loadable_t *)&__loadable_header_uos_loadable_t);
}

static int il_unload(loadable_t *loadable)
{
    return loadable->unload();
}

/*
static void il_init_loadable(loadable_t *loadable)
{
    loadable->init();
}
*/

const loaderif_t __uos_internal_loader = 
{
    .first_loadable = il_first_loadable,
    .next_loadable = il_next_loadable,
    .load = il_load,
    .unload = il_unload,
//    .init_loadable = il_init_loadable
};
