#ifndef UOS_STREAM_MUX_H
#define UOS_STREAM_MUX_H 1

#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif


//* provides read from buffer <in> stream, and write to <out> stream
//* 
typedef struct {
	const stream_interface_t *interface;
  stream_t*  in;
  stream_t*  out;
} stream_mux_t;

//* \param bytes2 - MUST be power2
stream_mux_t* stream_mux_init (stream_mux_t* u
                    , stream_t* in, stream_t* out
                    );

#ifdef __cplusplus
}
#endif

#endif //UOS_STREAM_IOBUF_H 1

