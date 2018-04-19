#ifndef _UOS_LOADER_H_
#define _UOS_LOADER_H_

#include <loadable/loadable.h>
#include <loadable/loader-interface.h>

loaderif_t *loader_by_type(unsigned long type);

loadable_t *loader_first_loadable(unsigned long loadable_id);
loadable_t *loader_next_loadable(loadable_t *current, unsigned long loadable_id);
int loader_load(loadable_t *loadable);
int loader_unload(loadable_t *loadable);
int loader_init(loadable_t *loadable);
int loader_load_all();
int loader_unload_all();
//int loader_init_all();

#ifdef LOADABLE_CHECK_CRC
int loader_check_crc(loadable_t *loadable);
#endif

#endif //_UOS_LOADER_H_
