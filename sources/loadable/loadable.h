#ifndef _LOADABLE_H_
#define _LOADABLE_H_

#define LOADABLE_MAGIC              0x44000555

// Поле id
#define LOADABLE_PASSIVE_FLAG       (1 << 31)
#define LOADABLE_LOADER_TYPE_MASK   0x0F000000
#define LOADABLE_LOADER_TYPE_SHIFT  24
#define LOADABLE_ID_MASK            0x00FFFFFF

#define LOADABLE_ID_UOS             0
#define LOADABLE_ID_ANY             LOADABLE_ID_MASK

// Поле version
#define LOADABLE_MAJVER_MASK        0xFF000000
#define LOADABLE_MAJVER_SHIFT       24
#define LOADABLE_MINVER_MASK        0x00FF0000
#define LOADABLE_MINVER_SHIFT       16
#define LOADABLE_FT_MAJVER_MASK     0x0000FF00
#define LOADABLE_FT_MAJVER_SHIFT    8
#define LOADABLE_FT_MINVER_MASK     0x000000FF
#define LOADABLE_FT_MINVER_SHIFT    0


#define LOADABLE_NO_LOADER              0
#define LOADABLE_INTERNAL_LOADER        1
#define LOADABLE_EXT_FLASH_LOADER       2

#ifdef COMPILE_UOS_LOADABLE
#define LOADABLE_DEFAULT_LOADER      LOADABLE_INTERNAL_LOADER
#else
#define LOADABLE_DEFAULT_LOADER      LOADABLE_EXT_FLASH_LOADER
#endif


typedef struct _loadable_t loadable_t;

struct _loadable_t
{
    unsigned long   magic;
    unsigned long   id;
    unsigned long   version;
    unsigned long   addr;
    unsigned long   size;
    unsigned long   crc;
    int             (* load)(loadable_t *uos_loadable);
    int             (* unload)();
    //void          (* init)();
};

#define DECLARE_LOADABLE(type, loadable_id, active, ldr, maj_ver, min_ver, \
        maj_ft_ver, min_ft_ver, load_func, unload_func/*, init_func*/) \
    extern unsigned long _btext;\
    type __loadable_header_##type\
     __attribute__((section(".loadable_header"))) = \
    {{\
        .magic = LOADABLE_MAGIC,\
        .id = (loadable_id) | ((active) ? 0 : LOADABLE_PASSIVE_FLAG) | \
                ((ldr) << LOADABLE_LOADER_TYPE_SHIFT),\
        .version = ((maj_ver) << LOADABLE_MAJVER_SHIFT) | \
                ((min_ver) << LOADABLE_MINVER_SHIFT) | \
                ((maj_ft_ver) << LOADABLE_FT_MAJVER_SHIFT) | \
                ((min_ft_ver) << LOADABLE_FT_MINVER_SHIFT),\
        .addr = (unsigned long)&_btext,\
        .size = 0,\
        .crc = 0,\
        .load = (load_func),\
        .unload = (unload_func),\
        /*.init = (init_func),*/\
    },
#define END_DECLARE_LOADABLE };

#ifdef COMPILE_UOS_LOADABLE
#   define _UC(x)
#else
#   define _UC(x) (x)->
#endif

#define _MC(x)  if (x) (x)->

#ifdef COMPILE_UOS_LOADABLE
#   define lassert(uos,expr)  assert(expr)
#else
#   ifdef NDEBUG
#       define lassert(uos,expr) do {} while (0)
#   else
#       define lassert(uos,expr) do {               \
            if (__builtin_expect (! (expr), 0))     \
                uos->assert_fail (#expr, __FILE__,  \
                    __LINE__, __PRETTY_FUNCTION__); \
            } while (0)
#   endif
#endif


static inline unsigned long loadable_id(loadable_t *l)
{
    return l->id & LOADABLE_ID_MASK;
}

static inline int loadable_is_passive(loadable_t *l)
{
    return l->id & LOADABLE_PASSIVE_FLAG;
}

static inline int loadable_is_active(loadable_t *l)
{
    return !(l->id & LOADABLE_PASSIVE_FLAG);
}

static inline int loadable_loader_type(loadable_t *l)
{
    return (l->id & LOADABLE_LOADER_TYPE_MASK) >> LOADABLE_LOADER_TYPE_SHIFT;
}

static inline unsigned long loadable_major_ver(loadable_t *l)
{
    return (l->version & LOADABLE_MAJVER_MASK) >> LOADABLE_MAJVER_SHIFT;
}

static inline unsigned long loadable_minor_ver(loadable_t *l)
{
    return (l->version & LOADABLE_MINVER_MASK) >> LOADABLE_MINVER_SHIFT;
}

static inline unsigned long loadable_major_ft_ver(loadable_t *l)
{
    return (l->version & LOADABLE_FT_MAJVER_MASK) >> LOADABLE_FT_MAJVER_SHIFT;
}

static inline unsigned long loadable_minor_ft_ver(loadable_t *l)
{
    return (l->version & LOADABLE_FT_MINVER_MASK) >> LOADABLE_FT_MINVER_SHIFT;
}


#endif // _LOADABLE_H_
