#ifndef _CRC32_IEEE_H_
#define _CRC32_IEEE_H_

void crc32_ieee_reset();
uint32_t crc32_ieee(const void *_data, unsigned _len);

#endif
