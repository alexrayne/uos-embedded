#include <runtime/lib.h>
#include <stream/stream.h>


//* это враперы позволяют реализовать getc/putc функциями write/read.
int stream_write1(stream_t *u, unsigned c){
    unsigned tmp = c;
    int res = 0;
    semaphore_p m = 0;
    while( (res = stream_putn(u, &tmp, 1), res) == 0 ){
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
int stream_putcn(stream_t *u, const void *src, unsigned len){
    unsigned length;
    const char *str = (const char *)src;
    int (*putc) (stream_t *u, unsigned c) = u->interface->putc;
    for (length = 0; length < len; length++) {
        char c = FETCH_BYTE(str++);
        if ( putc(u, c) == 1)
          continue;
        else
          return length;
    }
    return len;
}

int stream_putn(stream_t *u, const void *str, unsigned len){
  if (u->interface->put != NULL)
    return u->interface->put(u, str,len);
  else
    return stream_putcn(u, str, len);
}



int stream_puts (stream_t *stream, const char *str)
{
	int length;
	unsigned char c;
	UOS_STRICT(STREAM_MEM, ) assert(uos_valid_memory_address(stream));
    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_memory_address(stream->interface));
	void (*putc) (stream_t *u, short c) = stream->interface->putc;
    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_code_address(putc));

#if STREAM_HAVE_ACCEESS > 0
	if (stream->interface->access_tx != 0)
	    (stream->interface->access_tx(stream, 1));
#endif

	for (length = 0; ; length++) {
		c = FETCH_BYTE (str);
		if (! c)
		    break;
		putc(to_stream(stream), c);
		//putchar (stream, c);
		++str;
	}

#if STREAM_HAVE_ACCEESS > 0
    if (stream->interface->access_tx != 0)
        (stream->interface->access_tx(stream, 0));
#endif
    return length;
}

int stream_write (stream_t *stream, const void * src, unsigned len)
{
    if (unlikely(len <= 0))
        return 0;
    if (unlikely(src == 0))
        return 0;

    const char *str = (const char *)src;
    unsigned length;
    semaphore_p m = 0;

#if STREAM_HAVE_ACCEESS > 0
    if (stream->interface->access_tx != 0)
        (stream->interface->access_tx(stream, 1));
#endif

    for (length = 0; length < len; length++) {
        int res = stream_putn(stream, str, (len - length) );
        if (res >= 0){
          length  += res;
          str     += res;
          if (length == 0)
            break;

          if (m == 0)
            m = stream_transmiter(stream);
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
        (stream->interface->access_tx(stream, 0));
#endif
    return (int)length;
}
