#ifndef _RUNTIME_LOG_BUF_H_
#define _RUNTIME_LOG_BUF_H_
/*
 * runtime cyclic log buffer
 *
 * utf8 ru_RU
 *
 *  Created on: 21.10.2015
 *      Author: a_lityagin
 * \~russian
 *  задаю опреции работы с журналом реального времени. это циклический буфер
 *  записей вида rtlog_node - включает штамп события, его сообщение и аргументы.
 *
 *  эмулятор vprintf позволяет сохранять в журнал сообщения без затратной печати,
 *  а распечатать сообщение по чтении из журнала.
 * */


#include <runtime/sys/uosc.h>
#include <stdint.h>
#include <stdarg.h>
#include <buffers/ring_index.h>
#include <stream/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RTLOG_ARGS_LIMIT
#define RTLOG_ARGS_LIMIT 6
#endif

typedef struct _rtlog_node_t {
    unsigned stamp;
    const char* msg;
    unsigned long args[RTLOG_ARGS_LIMIT];
} rtlog_node;

typedef struct _rtlog_t {
    unsigned        stamp;
    ring_uindex_t   idx;
    rtlog_node*     store;
} rtlog;


//*\arg size - размер store в байтах. буфера выделяется как массив rtlog_node
//*            размером в степень2
void rtlog_init    ( rtlog* u, void* store, size_t size);

INLINE 
void rtlog_clear   ( rtlog* u) {ring_uindex_reset(&u->idx);};

INLINE
int rtlog_avail   ( rtlog* u ) {return ring_uindex_avail(&u->idx);};

//* это эмуляторы аналогичных функций печати. количество сохраняемых аргументов 
//*     не более RTLOG_ARGS_LIMIT
//* !!! передача аргументов float/double не гарантируется!
int rtlog_vprintf ( rtlog* u, unsigned argsn, const char *fmt, va_list args)  __noexcept __NOTHROW;
//* 1й параметр va_args трактуется как const char *fmt
int rtlog_printf  ( rtlog* u, unsigned argsn, ...)  __noexcept __NOTHROW;
int rtlog_puts    ( rtlog* u, const char *str) __noexcept __NOTHROW;

//* печатает records_count последних записей журнала в dst
void rtlog_dump_last( rtlog* u, stream_t *dst, unsigned records_count);

#ifdef __cplusplus
}
#endif

#endif
