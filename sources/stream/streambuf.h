#ifndef UOS_STREAM_IOBUF_H
#define UOS_STREAM_IOBUF_H 1

#include <stream/stream.h>
#include <buffers/ring_index.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  stream io buffer - fifo buffer, locked and closable, implements its own stream interface
 */
typedef struct {
	const stream_interface_t *interface;
	char* buf;
  ring_uindex_t fifo;
  mutex_t       lock;
  volatile char closed;
} stream_iobuf_t;

//* \param bytes2 - MUST be power2
stream_iobuf_t* iobuf_init (stream_iobuf_t* u, char *buf, int bytes2);

INLINE
unsigned iobuf_free_solid(stream_iobuf_t* u){
  return ring_uindex_free_solid(&(u->fifo));
};

INLINE
unsigned iobuf_avail_solid(stream_iobuf_t* u){
  return ring_uindex_avail_solid(&(u->fifo));
};

INLINE
unsigned iobuf_free(stream_iobuf_t* u){
  return ring_uindex_free(&(u->fifo));
};

INLINE
unsigned iobuf_avail(stream_iobuf_t* u){
  return ring_uindex_avail(&(u->fifo));
};

INLINE
char* iobuf_read_at(stream_iobuf_t* u){
  return u->buf+u->fifo.read;
};

INLINE
char* iobuf_write_at(stream_iobuf_t* u){
  return u->buf+u->fifo.write;
};

void iobuf_putn(stream_iobuf_t* u, unsigned len);
void iobuf_getn(stream_iobuf_t* u, unsigned len);



#ifdef __cplusplus
}
#endif

#endif //UOS_STREAM_IOBUF_H 1

