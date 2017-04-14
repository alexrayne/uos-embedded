#include <runtime/lib.h>
#include <stream/stream.h>

//* это враперы позволяют реализовать getc/putc функциями write/read.
int stream_read1(stream_t *u){
    unsigned tmp = 0;
    int res = 0;
    semaphore_p m = 0;
    while( (res = stream_getn(u, &tmp, 1), res) == 0){
        if (m == 0)
          m = stream_receiver(u);
        if (m)
          sem_wait(m);
        else
          mdelay(1);
    }
    return res;
}

//* это враперы позволяют реализовать write/read функциями getc/putc.
int stream_getcn(stream_t *u, void *dst, unsigned len){
    unsigned length;
    char*    str = (char*)dst;
    int (*getc)  (stream_t *u) = u->interface->getc;
    int (*peekc) (stream_t *u) = u->interface->peekc;
    for (length = 0; length < len; length++) {
        int c = peekc(u);
        if (c >= 0)
            *str++ = getc(u);
        else
          break;
    }
    return length;
}

int stream_getn(stream_t *u, void *dst, unsigned len){
  if (u->interface->get != NULL)
    return u->interface->get(u, dst, len);
  else
    return stream_getcn(u, dst, len);
}



void drain_input (stream_t *stream)
{
  while (stream_peekchar (stream) >= 0)
    stream_getchar (stream);
}



/*
 * Read a delimiter-terminated string from stream.
 */
int stream_getsx (stream_t *stream
        , char * __restrict buf, unsigned lenlimit
        , char delimiter )
{
	int c;
  int least = lenlimit;
  char *s;
  s = buf;
  // need preserve space for endZero
	const unsigned preserve = 1;

#if STREAM_HAVE_ACCEESS > 0
    if (stream->interface->access_rx != 0)
        (stream->interface->access_rx(stream, 1));
#endif

    while (--least > preserve) {
		if (stream_eof (stream)) {
			if (s == buf)
				buf = 0;
			break;
		}
		c = stream_getchar(stream);
//debug_printf ("c = `%c'\n", c);
		*s++ = c;
    if ( (c == delimiter) || (c == 0) )
			break;
	}
  if (c != '\0'){
		least--;
    *s = '\0';
	}
#if STREAM_HAVE_ACCEESS > 0
    if (stream->interface->access_rx != 0)
        (stream->interface->access_rx(stream, 0));
#endif
  return (lenlimit-least);
}



int stream_read (stream_t *u, void *dst, unsigned len){
    if (unlikely(len <= 0))
        return 0;
    if (unlikely(dst == 0))
        return 0;

    char *str = (char *)dst;
    int length;
    //unsigned char c;
    semaphore_p m = 0;

#if STREAM_HAVE_ACCEESS > 0
    if (stream->interface->access_tx != 0)
        (stream->interface->access_tx(u, 1));
#endif

    for (length = 0; length < len; length++) {
        int res = stream_getn(u, str, (len - length) );
        if (res >= 0){
          length  += res;
          str     += res;
          if (length == 0)
            break;

          if (m == 0)
            m = stream_receiver(u);
          if (m)
            sem_wait(m);
          else
            mdelay(1);
        }
        else {
          if (length <= 0)
            length = (unsigned)res;
          break;
        }
    }

#if STREAM_HAVE_ACCEESS > 0
    if (stream->interface->access_tx != 0)
        (stream->interface->access_tx(u, 0));
#endif
    return (int)length;
}


