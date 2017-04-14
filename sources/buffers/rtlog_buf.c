#include <runtime/lib.h>
#include <kernel/internal.h>
#include <stdarg.h>
#include <assert.h>
#include <mem/mem.h>
#include "rtlog_buf.h"


#ifdef DEBUG_RTLOG
#define RTLOG_printf(...) debug_printf(__VA_ARGS__)
#else
#define RTLOG_printf(...)
#endif

#ifndef RTLOG_STRICT_ARGS
#define RTLOG_STRICT_BOUNDS    1
#define RTLOG_STRICT_MEM       2
#define RTLOG_STRICT_ARGS      8
#define RTLOG_STRICT_STACK     16
#endif

#ifndef RTLOG_STRICTNESS
#define RTLOG_STRICTNESS (RTLOG_STRICT_ARGS)
#endif

#define STRICT( level, body ) if ((RTLOG_STRICTNESS & RTLOG_STRICT_##level) != 0) body

void rtlog_validate(rtlog* u){
    assert(uos_valid_memory_address(u->store));
    mem_validate_block(u->store);
    assert(mem_size(u->store) >= (u->idx.mask * sizeof(rtlog_node)) );
}

//*\arg size - размер store в байтах. буфера выделяется как массив rtlog_node
//*            размером в степень2
void rtlog_init    ( rtlog* u, void* store, size_t size)
{
    assert(size > 0);
    size_t limit = size / sizeof(rtlog_node);
    //rount to nearest power2
    size_t count = (1u<<31) >> __builtin_clz(limit) ;
    STRICT(BOUNDS, assert((count <= limit) && ((count*2) > limit) ) );
    ring_uindex_init(&u->idx, count);
    u->store = (rtlog_node*)store;
    u->stamp = 0;
}

//* это эмуляторы аналогичных функций печати. количество сохраняемых аргументов 
//*     не более RTLOG_ARGS_LIMIT
int rtlog_printf  ( rtlog* u, unsigned nargs, ...){
    va_list args;
    int err;
    va_start (args, nargs);
    const char *fmt = va_arg(args, const char *);
    err = rtlog_vprintf(u, nargs, fmt, args);
    va_end (args);
    return err;

    //if (ring_uindex_free(&u->idx) <= 0)
    //    return -1;
}

rtlog_node* rtlog_aqure_slot(rtlog* u, const char *fmt){
    if (ring_uindex_full(&u->idx))
        // drop first message if full
        ring_uindex_get(&u->idx);
    unsigned slot = u->idx.write;
    STRICT(BOUNDS, assert(slot <= u->idx.mask));
    rtlog_node* n = u->store + slot;
    n->stamp = u->stamp;
    ++(u->stamp);
    n->msg = fmt;
    return n;
}

int rtlog_vprintf ( rtlog* u, unsigned nargs, const char *fmt, va_list args) {
    STRICT(MEM, rtlog_validate(u));
    STRICT(ARGS, )
        assert2( (nargs <= RTLOG_ARGS_LIMIT*2)
                , "rtlog_vprintf passed %d args but supports masx %d\n"
                , nargs, RTLOG_ARGS_LIMIT*2
                );
    if (nargs > RTLOG_ARGS_LIMIT*2)
        nargs = RTLOG_ARGS_LIMIT*2;
    arch_state_t x = arch_intr_off();
    rtlog_node* n = rtlog_aqure_slot(u, fmt);
    unsigned i;
    if ((nargs > 0) /*&& ( (void*)(args) != 0 )*/ )
    for (i = 0; i < RTLOG_ARGS_LIMIT/*nargs*/; i++)
        n->args[i] = va_arg(args, unsigned long);
    else
    for (i = nargs; i < RTLOG_ARGS_LIMIT; i++)
        n->args[i] = 0;
    ring_uindex_put(&u->idx);
    if (nargs > RTLOG_ARGS_LIMIT) {
      //for long args ocupy 1 more slot
      n->msg = 0;
      rtlog_node* n = rtlog_aqure_slot(u, fmt);
      for (i = 0; i < RTLOG_ARGS_LIMIT/*nargs*/; i++)
        n->args[i] = va_arg(args, unsigned long);
      ring_uindex_put(&u->idx);
    }
    STRICT(BOUNDS, assert(u->idx.write <= u->idx.mask) );
    arch_intr_restore (x);
    RTLOG_printf("rtlog: printf %s to %d[stamp%x] %d args : %x, %x, %x, %x, %x, %x\n"
                , fmt
                , u->idx.write, n->stamp
                , nargs
                , n->args[0], n->args[1], n->args[2], n->args[3]
                , n->args[4], n->args[5]
                );
    return 0;
}

int rtlog_puts( rtlog* u, const char *str){
    va_list dummy;
    return rtlog_vprintf(u, 0, str, dummy);
}

//* печатает records_count последних записей журнала в dst
void rtlog_dump_last( rtlog* u, stream_t *dst, unsigned records_count)
{
    unsigned slot = 0;
    rtlog_node n;
    rtlog_node n1;
    unsigned last_stamp = u->stamp;
    n1.stamp = ~last_stamp;

    {
        arch_state_t x = arch_intr_off();

        unsigned avail = ring_uindex_avail(&u->idx);
        if (records_count > avail)
            records_count = avail;

        slot = u->idx.write;
        slot = (slot - records_count) & u->idx.mask;
        arch_intr_restore (x);

        if (records_count <= 0)
            return;
    }

    while (slot != u->idx.write){

        {
            arch_state_t x = arch_intr_off();
            memcpy(&n, u->store+slot, sizeof(n));
            arch_intr_restore (x);
        }

        RTLOG_printf("rtdump: at %d[stamp%x] %s"
                    , slot, n.stamp
                    , n.msg
                    );

        if ( (last_stamp+1) != n.stamp)
            stream_printf(dst, ".... droped %d messages ....\n"
                          , (n.stamp - 1 - last_stamp) 
                          );
        last_stamp = n.stamp;
        if (n.msg != 0) {
        void* sp = get_stack_pointer();
        if (n.stamp != (n1.stamp+1))
        stream_printf(dst, n.msg
                    , n.args[0], n.args[1], n.args[2], n.args[3]
                    , n.args[4], n.args[5]
                    );
        else
        stream_printf(dst, n.msg
                    , n1.args[0], n1.args[1], n1.args[2], n1.args[3]
                    , n1.args[4], n1.args[5]
                    , n.args[0], n.args[1], n.args[2], n.args[3]
                    , n.args[4], n.args[5]
                    );
        STRICT(STACK, assert(sp == get_stack_pointer()) );
        } //if (n.msg == 0)
        else {
          memcpy(&n1, &n, sizeof(n));
        }

        slot = (slot+1) & u->idx.mask;
        --records_count;
        if (records_count <= 0)
            break;
    }
}
