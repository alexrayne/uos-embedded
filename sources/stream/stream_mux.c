#include <kernel/uos.h>
#include <stream/stream.h>
#include <stream/stream_mux.h>
#include <assert.h>



static 
int smux_putchar (stream_t *u, unsigned c)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->out);
  return stream_putchar(self->out, c);
}

static
int  smux_put(stream_t *u, const void* c, unsigned len)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->out);
  return stream_putn(self->out, c, len);
}

static 
int smux_getchar (stream_t *u)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->in);
  return stream_getchar(self->in);
}

static 
int smux_peekchar (stream_t *u)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->in);
  return stream_peekchar(self->in);
}

static
int  smux_get(stream_t *u, void* dst, unsigned len)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->in);
  return stream_getn(self->in, dst, len);
}

static 
void  smux_flush (stream_t *u)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->out);
  stream_flush(self->out);
}

static 
bool_t smux_eof (stream_t *u)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->in);
  return stream_eof(self->in);
}

static 
void smux_close (stream_t *u)
{
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->in);
  assert(self->out);
  stream_close(self->in);
  stream_close(self->out);
}

static
semaphore_p smux_recv(stream_t *u){
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->in);
  return stream_receiver(self->in);
}

static
semaphore_p smux_sender(stream_t *u){
  stream_mux_t* self = (stream_mux_t*)u;
  assert(self->out);
  return stream_transmiter(self->out);
}

#ifdef __cplusplus
#define idx(i)
#define item(i)
#else
#define idx(i) [i] = 
#define item(i) .i =
#endif

static stream_interface_t smux_interface = {
	item(putc)    (int (*) (stream_t*, unsigned))   smux_putchar,
	item(getc)    (int (*) (stream_t*))             smux_getchar,
	item(peekc)   (int (*) (stream_t*) )            smux_peekchar,
	item(put)     (int  (*)(stream_t *u, const void* c, unsigned len))  smux_put,
	item(get)     (int  (*)(stream_t *u, void* dst, unsigned len))      smux_get,
	item(flush)   (void (*) (stream_t*))            smux_flush,
	item(eof)     (bool_t (*) (stream_t*))          smux_eof,
	item(close)   (void (*) (stream_t*))            smux_close,
	item(receiver)  (semaphore_p (*) (stream_t *u)) smux_recv,
	item(transmiter)(semaphore_p (*) (stream_t *u)) smux_sender,
};

//* \param bytes2 - MUST be power2
stream_mux_t* stream_mux_init (stream_mux_t* u
                    , stream_t* in, stream_t* out
                    )
{
  u->in   = in;
  u->out  = out;
  u->interface = &smux_interface;
  return u;
}



