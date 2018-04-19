#ifndef __FAT32_FW_FILE_OUT_H__
#define __FAT32_FW_FILE_OUT_H__

#include <stream/stream.h>

void ffw_out_init(fat32_fw_t *ffw, fs_entry_t *file);
void ffw_out_putchar(void *arg, short c);
void ffw_out_close(void *arg);

extern stream_t ffw_out;

#endif // __FAT32_FW_FILE_OUT_H__



