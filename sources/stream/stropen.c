#include <runtime/lib.h>
#include <string.h>
#include <stdarg.h>
#include <stream/stream.h>

//*******************************************************
//            stream_buf
//*******************************************************
static 
int buf_putchar (stream_t *s, unsigned c)
{
  stream_str_t* u = (stream_str_t*)s;
	if (u->size > 1) {
		/* TODO: unicode to utf8 conversion. */
		*u->buf++ = c;
		--u->size;
    return 1;
	}
  else
    return -1;
}

static int
buf_getchar (stream_t *s)
{
  stream_str_t* u = (stream_str_t*)s;
	/* TODO: utf8 to unicode conversion. */
	unsigned char c = *u->buf;

	if (! c)
		return 0;
	++u->buf;
	return c;
}

static int
buf_peekchar (stream_t *s)
{
  stream_str_t* u = (stream_str_t*)s;
	/* TODO: utf8 to unicode conversion. */
	unsigned char c = *u->buf;

	if (! c)
		return -1;
	return c;
}

static
int buf_putn (stream_t *s, void* c, unsigned len) 
{
  stream_str_t* u = (stream_str_t*)s;
  if (unlikely(c == 0))
    return u->size;
  if (len > u->size)
    len = u->size;
  memcpy(u->buf, c, len);
  u->buf += len;
  return len;
}

static
int buf_getn (stream_t *s, void* c, unsigned len) 
{
  stream_str_t* u = (stream_str_t*)s;
  size_t sz = strlen(u->buf);
  if (unlikely(c == 0))
    return sz;
  if (len > sz)
    len = sz;
  if (len > 0){
    memcpy(u->buf, c, sz);
    u->buf += len;
    return len;
  }
  else
    return -1;
}


static bool_t
buf_feof (stream_t *s)
{
  stream_str_t* u = (stream_str_t*)s;
	return ! *u->buf;
}

#ifdef __cplusplus
#define idx(i)
#define item(i)
#else
#define idx(i) [i] = 
#define item(i) .i =
#endif


static stream_interface_t buf_interface = {
	item(putc)    (int (*) (stream_t*, unsigned))               buf_putchar,
	item(getc) (int (*) (stream_t*))	buf_getchar,
	item(peekc) (int (*) (stream_t*))   buf_peekchar,
  item(put)    (int  (*)(stream_t *u, const void* c, unsigned len)) buf_putn,
  item(get)    (int  (*)(stream_t *u, void* c, unsigned len)) buf_getn,
	item(flush) (void (*) (stream_t*))  0,
	item(eof) (bool_t (*) (stream_t*))		buf_feof,
};

stream_t* stropen (stream_str_t *u, char *buf, int size)
{
	u->interface = &buf_interface;
	u->buf = buf;
	u->size = size;
	return (stream_t*) u;
}

void
strclose (stream_str_t *u)
{
	if (u->size > 0) {
		*u->buf++ = 0;
		--u->size;
	}
}


