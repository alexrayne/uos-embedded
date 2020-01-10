#ifndef _BYTE_QUEUE_H_
#define _BYTE_QUEUE_H_

typedef struct _bq_t
{
    uint8_t *       array;
    unsigned long   mask;
    unsigned long   puti;
    unsigned long   geti;
} bq_t;

static inline int ones_count (unsigned long value)
{
    int count = 0;
    do {
        if (value & 1)
            count++;
        value >>= 1;
    } while (value != 0);
    
    return count;
}

static inline void bq_init (bq_t *bq, void *buffer, unsigned long size)
{
    assert ((ones_count (size) == 1) && (size > 1));
    
    bq->array = buffer;
    bq->mask = size - 1;
    bq->puti = 0;
    bq->geti = 0;    
}

static inline int bq_is_empty (bq_t *bq)
{
    return bq->puti == bq->geti;
}

static inline int bq_is_full (bq_t *bq)
{
    return ((bq->puti + 1) & bq->mask) == bq->geti;
}

static inline void bq_put (bq_t *bq, uint8_t byte)
{
    bq->array[bq->puti] = byte;
    bq->puti = (bq->puti + 1) & bq->mask;
}

static inline uint8_t bq_get (bq_t *bq)
{
    uint8_t byte;
    byte = bq->array[bq->geti];
    bq->geti = (bq->geti + 1) & bq->mask;
    return byte;
}

static inline void bq_clear (bq_t *bq)
{
    bq->puti = 0;
    bq->geti = 0;
}


// 
// Функции для возможности записи и чтения массива
//

static inline unsigned long bq_avail_put (bq_t *bq)
{
    return (bq->geti - bq->puti + bq->mask) & bq->mask;
}

static inline unsigned long bq_avail_get (bq_t *bq)
{
    return (bq->puti - bq->geti + bq->mask + 1) & bq->mask;
}

static inline unsigned long bq_avail_put_to_end (bq_t *bq)
{
    if ((bq->geti > bq->puti) && (bq->geti != 0))
        return bq->geti - bq->puti - 1;
    else
        return bq->mask + 1 - bq->puti;
}

static inline unsigned long bq_avail_get_to_end (bq_t *bq)
{
    if (bq->puti > bq->geti)
        return bq->puti - bq->geti;
    else
        return bq->mask + 1 - bq->geti;
}

static inline void bq_put_array (bq_t *bq, const void *buffer, unsigned long size)
{
    unsigned long avail_to_end = bq_avail_put_to_end (bq);
    if (size <= avail_to_end) {
        memcpy (bq->array + bq->puti, buffer, size);
        bq->puti += size;
        bq->puti &= bq->mask;
    } else {
        memcpy (bq->array + bq->puti, buffer, avail_to_end);
        memcpy (bq->array, (uint8_t *)buffer + avail_to_end, size - avail_to_end);
        bq->puti = size - avail_to_end;
    }
}

static inline void bq_get_array (bq_t *bq, void *buffer, unsigned long size)
{
    unsigned long avail_to_end = bq_avail_get_to_end (bq);
    if (size <= avail_to_end) {
        memcpy (buffer, bq->array + bq->geti, size);
        bq->geti += size;
        bq->geti &= bq->mask;
    } else {
        memcpy (buffer, bq->array + bq->geti, avail_to_end);
        memcpy ((uint8_t *)buffer + avail_to_end, bq->array, size - avail_to_end);
        bq->geti = size - avail_to_end;
    }
}

#endif // _BYTE_QUEUE_H_
