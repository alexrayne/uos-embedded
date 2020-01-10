#ifndef _UOS_LOADER_INTERFACE_H_
#define _UOS_LOADER_INTERFACE_H_

#include <loadable/loadable.h>

#define LOADABLE_ERR_OK             0
#define LOADABLE_ERR_NO_LOADER      -1
#define LOADABLE_ERR_FAILED         -2
#define LOADABLE_ERR_SIZE           -3
#define LOADABLE_ERR_CRC            -4
#define LOADABLE_ERR_MODULE_MISS    -5

typedef struct _loaderif_t loaderif_t;

struct _loaderif_t
{
    loadable_t * (* first_loadable)();
    loadable_t * (* next_loadable)(loadable_t *loadable);
    int (* load)(loadable_t *loadable);
    int (* unload)(loadable_t *loadable);
    void (* init_loadable)(loadable_t *loadable);
};

#endif //_UOS_LOADER_INTERFACE_H_
