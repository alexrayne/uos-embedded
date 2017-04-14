#ifndef _RING_IDX_H_
#define _RING_IDX_H_
/*
 * ringbuf index operator
 *
 * utf8 ru_RU
 *
 *  Created on: 21.10.2015
 *      Author: a_lityagin
 * \~russian
 *  этот тип может задавать простейший заголовок кольцевого буфера (ФИФО).
 *  !!! размер буфера должен быть равен степени 2 
 * */


#include <runtime/sys/uosc.h>

#ifndef INLINE
#define INLINE static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif


struct _ring_uindex_t {
    unsigned    mask;
    unsigned    read;
    unsigned    write;
};
typedef struct _ring_uindex_t ring_uindex_t;

INLINE 
void ring_uindex_init (ring_uindex_t *cb, unsigned size2){
    cb->read = cb->write = 0;//cb->data
    cb->mask = size2 -1;
}

INLINE
void ring_uindex_reset (ring_uindex_t *cb){
    cb->read = cb->write = 0;//cb->data
}

INLINE
unsigned ring_uindex_size (ring_uindex_t *cb)
{
    return cb->mask+1;
}

INLINE
unsigned ring_uindex_cur_read (ring_uindex_t *cb)
{
    return cb->read;
}

INLINE
unsigned ring_uindex_cur_write (ring_uindex_t *cb)
{
    return cb->write;
}

INLINE
unsigned ring_uindex_avail(ring_uindex_t *cb){
    return  (cb->write - cb->read) & cb->mask;
}

INLINE
unsigned ring_uindex_free(ring_uindex_t *cb){
    return (cb->read - cb->write -1) & cb->mask;
}

// \return solid fragment from read
INLINE
unsigned ring_uindex_avail_solid(ring_uindex_t *cb){
    if (cb->read <= cb->write)
        return cb->write - cb->read;
    else
        return cb->mask+1 - cb->read;
}

// \return solid freespace from write
INLINE
unsigned ring_uindex_free_solid(ring_uindex_t *cb){
    if (cb->read > cb->write)
        return cb->read - cb->write;
    else
        return cb->mask+1 - cb->write;
}

INLINE
char ring_uindex_full(ring_uindex_t *cb){
    return ring_uindex_avail(cb) == cb->mask;
}

INLINE
char ring_uindex_empty(ring_uindex_t *cb){
    return cb->write == cb->read;
}

//* !!! MUST check avail data, before get
INLINE
void ring_uindex_get (ring_uindex_t *cb)
{
    cb->read = ((cb->read+1) & cb->mask) ; // +cb->data
}

INLINE
void ring_uindex_getn(ring_uindex_t *cb, unsigned len)
{
    cb->read = ((cb->read+len) & cb->mask) ; // +cb->data
}

//* !!! MUST check free data, before get
INLINE
void ring_uindex_put (ring_uindex_t *cb)
{
    cb->write = ((cb->write+1) & cb->mask) ; // +cb->data
}

INLINE
void ring_uindex_putn (ring_uindex_t *cb, unsigned len)
{
    cb->write = ((cb->write+len) & cb->mask) ; // +cb->data
}



#ifdef __cplusplus
}
#endif

#endif
