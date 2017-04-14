#include <string.h>
#include <kernel/uos.h>
#include <stream/stream.h>
#include <stream/streambuf.h>

#ifndef LogMsg
#define LogMsg(...)
#endif
#ifndef vStream
#define vStream 32
#endif

static 
int iobuf_putchar (stream_t *s, unsigned c)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return -1;

	mutex_lock(lock);
  if (unlikely(u->closed)){
    mutex_unlock (lock);
    return -1;
  }

	/* Wait for free space in FIFO. */
	for (;;) {
    if (!ring_uindex_full(fifo))
      break;
    LogMsg(vStream, vTrace, "s%x:putc:full\n", u);
    mutex_wait(lock);
    if (u->closed) {
      mutex_unlock(lock);
      return -1;
    }
	}

  if (ring_uindex_empty(fifo)){
    LogMsg(vStream, vTrace, "s%x:putc:!empty\n", u);
    mutex_signal(lock);
  }
  u->buf[fifo->write] = c;
  ring_uindex_put(fifo);

	mutex_unlock (lock);
  return 1;
}

static
int  iobuf_put(stream_t *s, const void* c, unsigned len)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return -1;

	mutex_lock(lock);
  if (unlikely(u->closed)){
    mutex_unlock (lock);
    return -1;
  }

  if ( unlikely(ring_uindex_full(fifo)) ){
    mutex_unlock (lock);
    return 0;
  }

  unsigned posted = ring_uindex_free_solid(fifo);
  if (len < posted)
    posted = len;
  if (ring_uindex_empty(fifo)){
    LogMsg(vStream, vTrace, "s%x:put:!empty\n", u);
    mutex_signal(lock);
  }
  memcpy( (u->buf+fifo->write), c, posted );
  ring_uindex_putn(fifo, posted);

	mutex_unlock (lock);
  return posted;
}

void iobuf_putn(stream_iobuf_t* u, unsigned len){
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return;

  mutex_lock(lock);
  if (unlikely(u->closed)){
    mutex_unlock (lock);
    return;
  }

  if (ring_uindex_empty(fifo)){
    LogMsg(vStream, vTrace, "s%x:putn:!empty\n", u);
    mutex_signal(lock);
  }
  ring_uindex_putn(&(u->fifo), len);

  mutex_unlock (lock);
};


static 
int iobuf_getchar (stream_t *s)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return -1;

	mutex_lock (lock);
  if (unlikely(u->closed))
    return -1;

	/* Wait for data in FIFO. */
	while (ring_uindex_empty(fifo)) {
    LogMsg(vStream, vTrace, "s%x:getc:empty\n", u);
    mutex_wait(lock);
    if (u->closed){
      mutex_unlock (lock);
      return -1;
    }
	}

  if (ring_uindex_full(fifo)){
    LogMsg(vStream, vTrace, "s%x:getc:!full\n", u);
    mutex_signal (lock);
  }

  int c;
	/* Get byte from FIFO. */
	c = u->buf[fifo->read];
  ring_uindex_get(fifo);

	mutex_unlock (lock);
	return c;
}

static 
int iobuf_peekchar (stream_t *s)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return -1;

	mutex_lock (lock);
  if (unlikely(u->closed))
    return -1;

  int c;
  // Is fifo empty?
  if (!ring_uindex_empty(fifo))
    c = u->buf[fifo->read];
  else
    c = -1;

	mutex_unlock (lock);
	return c;
}

static
int  iobuf_get(stream_t *s, void* dst, unsigned len)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return -1;

	mutex_lock(lock);
  if (unlikely(u->closed)){
    mutex_unlock (lock);
    return -1;
  }

  if ( unlikely(ring_uindex_empty(fifo)) ){
    mutex_unlock (lock);
    return 0;
  }

  unsigned posted = ring_uindex_avail_solid(fifo);
  if (ring_uindex_full(fifo)){
    LogMsg(vStream, vTrace, "s%x:get:!full\n", u);
    mutex_signal (lock);
  }
  if (len < posted)
    posted = len;
  memcpy( dst, (u->buf+fifo->read), posted );
  ring_uindex_getn(fifo, posted);

	mutex_unlock (lock);
  return posted;
}

void iobuf_getn(stream_iobuf_t* u, unsigned len){
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return ;

	mutex_lock(lock);
  if (unlikely(u->closed)){
    mutex_unlock (lock);
    return ;
  }

  if (ring_uindex_full(fifo)){
    LogMsg(vStream, vTrace, "s%x:getn:!full\n", u);
    mutex_signal (lock);
  }
  ring_uindex_getn(fifo, len);

	mutex_unlock (lock);
};

static 
void  iobuf_flush (stream_t *s)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return;

	mutex_lock (lock);

	/* Wait until output FIFO becomes empty. */
	while (!u->closed && !ring_uindex_empty(fifo)){
    LogMsg(vStream, vTrace, "s%x:flush:!empty\n", u);
    mutex_wait(lock);
  }

	mutex_unlock (lock);
}

static 
bool_t iobuf_eof (stream_t *s)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
	return u->closed;
}

static 
void iobuf_close (stream_t *s)
{
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  mutex_p     lock  = &u->lock;
  ring_uindex_t* fifo = &u->fifo;

  if (unlikely(u->closed))
    return;

	mutex_lock (lock);
  if (unlikely(u->closed))
    return;

  u->closed = 1;
  ring_uindex_reset(fifo);
  mutex_signal (lock);

	mutex_unlock (lock);
}

static
semaphore_p iobuf_notify(stream_t *s){
  stream_iobuf_t* u = (stream_iobuf_t*)s;
  return mutex_sem(&u->lock);
}

#ifdef __cplusplus
#define idx(i)
#define item(i)
#else
#define idx(i) [i] = 
#define item(i) .i =
#endif

static stream_interface_t iobuf_interface = {
	item(putc)    (int (*) (stream_t*, unsigned))     iobuf_putchar,
	item(getc)    (int (*) (stream_t*))               iobuf_getchar,
	item(peekc)   (int (*) (stream_t*))               iobuf_peekchar,
	item(put)     (int  (*)(stream_t *u, const void* c, unsigned len))  iobuf_put,
	item(get)     (int  (*)(stream_t *u, void* dst, unsigned len))      iobuf_get,
	item(flush)   (void (*) (stream_t*))            iobuf_flush,
	item(eof)     (bool_t (*) (stream_t*))          iobuf_eof,
	item(close)   (void (*) (stream_t*))            iobuf_close,
	item(receiver)  (semaphore_p (*) (stream_t *u)) iobuf_notify,
	item(transmiter)(semaphore_p (*) (stream_t *u)) iobuf_notify,
};

stream_iobuf_t* iobuf_init (stream_iobuf_t* u, char *buf, int bytes2)
{
  mutex_init(&u->lock);
  u->interface = &iobuf_interface;
  u->closed = 0;
  u->buf = buf;
  ring_uindex_init(&u->fifo, bytes2);
  return u;
}
