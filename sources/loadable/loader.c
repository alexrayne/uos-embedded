#include <runtime/lib.h>
#include <loadable/loader.h>
#include <crc/crc32-ieee.h>

enum
{
#ifdef CONFIG_INTERNAL_LOADER
    INTERNAL_LOADER,
#endif
#ifdef CONFIG_EXT_FLASH_LOADER
    EXT_FLASH_LOADER,
#endif
    NB_OF_LOADERS
};

extern loaderif_t __uos_internal_loader;
extern loaderif_t __uos_ext_flash_loader;

static loaderif_t *loaders[NB_OF_LOADERS] =
{
#ifdef CONFIG_INTERNAL_LOADER
    [INTERNAL_LOADER]  = &__uos_internal_loader,
#endif
#ifdef CONFIG_EXT_FLASH_LOADER
    [EXT_FLASH_LOADER] = &__uos_ext_flash_loader,
#endif
};

static int loader_index(unsigned long type)
{
    if (type == LOADABLE_INTERNAL_LOADER) {
#ifdef CONFIG_INTERNAL_LOADER
        return INTERNAL_LOADER;
#else
        return -1;
#endif
    } else if (type == LOADABLE_EXT_FLASH_LOADER) {
#ifdef CONFIG_EXT_FLASH_LOADER
        return EXT_FLASH_LOADER;
#else
        return -1;
#endif
    } else {
        return -1;
    }
}

loaderif_t *loader_by_type(unsigned long type)
{
    int idx = loader_index(type);
    if (idx < 0)
        return 0;
    return loaders[idx];
}

static loadable_t *next_loadable_of_loader(loaderif_t *loader,
        loadable_t *current, unsigned long lid)
{
    while (1) {
        current = loader->next_loadable(current);
        if (!current)
            break;
        if ((lid == LOADABLE_ID_ANY) || (loadable_id(current) == lid))
            break;
    }
    return current;
}

loadable_t *loader_next_loadable(loadable_t *current, unsigned long lid)
{
    assert(current->magic == LOADABLE_MAGIC);

    int i = loader_index(loadable_loader_type(current));
    if (i < 0)
        return 0;
        
    loaderif_t *loader = loaders[i];
    current = next_loadable_of_loader(loader, current, lid);
    if (current)
        return current;

    for (++i; i < NB_OF_LOADERS; ++i) {
        loader = loaders[i];
        current = loader->first_loadable();
        if (!current)
            continue;
        if ((lid == LOADABLE_ID_ANY) || (loadable_id(current) == lid))
            return current;
        current = next_loadable_of_loader(loader, current, lid);
        if (current)
            return current;
    }

    return current;
}

loadable_t *loader_first_loadable(unsigned long lid)
{
    loadable_t *loadable = loaders[0]->first_loadable();
    if ((lid == LOADABLE_ID_ANY) || (loadable_id(loadable) == lid))
        return loadable;
    else
        return loader_next_loadable(loadable, lid);
}

int loader_load(loadable_t *loadable)
{
    loaderif_t *loader = loader_by_type(loadable_loader_type(loadable));
    if (!loader)
        return LOADABLE_ERR_NO_LOADER;

    return loader->load(loadable);
}

int loader_unload(loadable_t *loadable)
{
    loaderif_t *loader = loader_by_type(loadable_loader_type(loadable));
    if (!loader)
        return LOADABLE_ERR_NO_LOADER;

    return loader->unload(loadable);
}

/*
int loader_init(loadable_t *loadable)
{
    loaderif_t *loader = loader_by_type(loadable_loader_type(loadable));
    if (!loader)
        return LOADABLE_ERR_NO_LOADER;

    loader->init_loadable(loadable);
    return LOADABLE_ERR_OK;
}
*/

static int iterate_loadables(int (* func)(loadable_t *))
{
    int res;
    int res_final = 0;

    loadable_t *loadable = loader_first_loadable(LOADABLE_ID_ANY);
    while (loadable) {
        if ((loadable_id(loadable) != LOADABLE_ID_UOS) && loadable_is_active(loadable)) {
			res = func(loadable);
			if (res != 0)
				res_final = res;
		}
        loadable = loader_next_loadable(loadable, LOADABLE_ID_ANY);
    }
    
    return res_final;
}

int loader_load_all()
{
    return iterate_loadables(loader_load);
}

int loader_unload_all()
{
    return iterate_loadables(loader_unload);
}

/*
int loader_init_all()
{
    return iterate_loadables(loader_init);
}
*/

#ifdef LOADABLE_CHECK_CRC
int loader_check_crc(loadable_t *loadable)
{
    if (loadable->size < 32)
        return LOADABLE_ERR_SIZE;
    
    uint8_t  null_buf[8];
    memset(null_buf, 0, 8);
    
    crc32_ieee_reset();
    crc32_ieee(loadable, 16);
    crc32_ieee(null_buf, 8);
    uint32_t crc = crc32_ieee(&loadable->load, loadable->size - 24);
    
    if (crc != loadable->crc)
        return LOADABLE_ERR_CRC;
        
    return LOADABLE_ERR_OK;
}
#endif

