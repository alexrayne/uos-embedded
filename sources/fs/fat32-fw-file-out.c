#include <runtime/lib.h>
#include <kernel/uos.h>
#include "fat32-fast-write.h"
#include "fat32-fw-file-out.h"

static fat32_fw_t *filesys;
static int filesys_ready;
static mutex_t filesys_mutex;

void ffw_out_init(fat32_fw_t *ffw, fs_entry_t *file)
{
    mutex_lock(&filesys_mutex);
    
    filesys = ffw;
    fat32_fw_create(filesys, file);
    if (fat32_fw_last_error(filesys) == FS_ERR_OK) {
        fat32_fw_open(filesys, O_WRITE);
        if (fat32_fw_last_error(filesys) == FS_ERR_OK) {
            filesys_ready = 1;
        }
    }
    
    mutex_unlock(&filesys_mutex);
}

void ffw_out_putchar(void *arg, short c)
{
    mutex_lock(&filesys_mutex);

    if (filesys_ready) {
        fat32_fw_write(filesys, &c, 1);
        if (fat32_fw_last_error(filesys) != FS_ERR_OK)
            filesys_ready = 0;
    }
    
    mutex_unlock(&filesys_mutex);
}

void ffw_out_close(void *arg)
{
    mutex_lock(&filesys_mutex);
    
    if (filesys_ready) {
        fat32_fw_close(filesys);
        filesys_ready = 0;
    }
    
    mutex_unlock(&filesys_mutex);
}

static stream_interface_t fat32_fw_file_out_interface = {
    .putc = (void (*) (stream_t*, short)) ffw_out_putchar,
    .close = (void (*) (stream_t *u)) ffw_out_close
};

stream_t ffw_out = { &fat32_fw_file_out_interface };



