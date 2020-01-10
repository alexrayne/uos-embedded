#include <runtime/lib.h>
#include <kernel/uos.h>
#include <flash/m25pxx.h>
#include <stdint.h>

#define M25PXX_CMD_WREN         0x06
#define M25PXX_CMD_WRDI         0x04
#define M25PXX_CMD_RDID         0x9F
#define M25PXX_CMD_RDSR         0x05
#define M25PXX_CMD_WRSR         0x01
#define M25PXX_CMD_READ         0x03
#define M25PXX_CMD_FAST_READ    0x0B
#define M25PXX_CMD_PP           0x02
//block 64k erase
#define M25PXX_CMD_BE           0xD8
#define M25PXX_CMD_CE           0xC7
#define M25PXX_CMD_BULKE        M25PXX_CMD_CE
#define M25PXX_CMD_DP           0xB9
#define M25PXX_CMD_RES          0xAB

#define M25PXX_STATUS_WIP       (1 << 0)
#define M25PXX_STATUS_WEL       (1 << 1)
#define M25PXX_STATUS_BP        (7 << 2)
#define M25PXX_STATUS_BP0       (1 << 2)
#define M25PXX_STATUS_BP1       (1 << 3)
#define M25PXX_STATUS_BP2       (1 << 4)
#define M25PXX_STATUS_SRWD      (1 << 7)

int m25pxx_post_1byte(m25pxx_t *m)
{
    m->msg.tx_data = m->databuf;
    m->msg.rx_data = 0;
    m->msg.word_count = 1;
    return spim_trx(m->spi, &m->msg);
}

int m25pxx_write_msg_1byte(m25pxx_t *m){
    m->msg.mode &= ~SPI_MODE_CS_HOLD;
    return m25pxx_post_1byte(m);
}

int m25pxx_post_msg_1byte(m25pxx_t *m){
    m->msg.mode |= SPI_MODE_CS_HOLD;
    return m25pxx_post_1byte(m);
}


int m25pxx_post_msg_1byte_read_msg(m25pxx_t *m, unsigned read_size){
    int res = m25pxx_post_msg_1byte(m);
    if (res != SPI_ERR_OK)
        return res;

    m->msg.tx_data = 0;
    m->msg.rx_data = m->databuf;
    m->msg.word_count = 3;
    m->msg.mode &= ~SPI_MODE_CS_HOLD;
    return spim_trx(m->spi, &m->msg);
}

int m25pxx_send_addr3(m25pxx_t *m, uint32_t adress)
{
#ifdef MIPS32
    m->databuf[3] = adress;
    adress = adress >> 8;
    m->databuf[2] = adress;
    adress = adress >> 8;
    m->databuf[1] = adress;
#else
    uint8_t *p = (uint8_t *) &address;
    m->databuf[1] = p[2];
    m->databuf[2] = p[1];
    m->databuf[3] = p[0];
#endif
    m->msg.tx_data = m->databuf;
    m->msg.rx_data = 0;
    m->msg.word_count = 4;
    return spim_trx(m->spi, &m->msg);
}

int m25pxx_write_addr3(m25pxx_t *m, uint32_t adress){
    m->msg.mode &= ~SPI_MODE_CS_HOLD;
    return m25pxx_send_addr3(m, adress);
}

int m25pxx_post_addr3(m25pxx_t *m, uint32_t adress){
    m->msg.mode |= SPI_MODE_CS_HOLD;
    return m25pxx_send_addr3(m, adress);
}

#ifdef FLASH_M25_VALIDATE
// here is a list of pairs [JEDEC ManID, Memory type ID] that can be validated by m25pxx_connect
//
typedef unsigned m25_jedec_code;
const m25_jedec_code jedec_id_list[] = {
          0x2020    //micron m25p
        , 0x40ef    //winbond m25
        , 0
    };
#endif

static int m25pxx_connect(flashif_t *flash)
{
    m25pxx_t *m = (m25pxx_t *) flash;
    mutex_lock(&flash->lock);

    m->databuf[0] = M25PXX_CMD_RDID;
    bool_t ok = (m25pxx_post_msg_1byte_read_msg(m, 3) == SPI_ERR_OK);
    if (ok && FLASH_M25_VALIDATE)
    {
        const m25_jedec_code flash_code = (m->databuf[1] << 8) | m->databuf[0];
        const m25_jedec_code* valid = jedec_id_list;
        ok = 0;
        while (*valid != 0) {
            ok = (*valid++ == flash_code);
            if (ok)
                break;
        }
    }

    if (!ok) {
        mutex_unlock(&flash->lock);
        return FLASH_ERR_NOT_CONN;
    }

    flash->page_size = 256;
    //HARDCODE use 64KB block as sector.
    // micron m25 use 64K sector
    // winbond provides 64K blocks
    flash->nb_pages_in_sector = 256;

    // TODO HARDCODE
    unsigned pages = 1ul << (m->databuf[2]);
    flash->nb_sectors = pages>>16;

    flash->min_address = 0;

    mutex_unlock(&flash->lock);
    return FLASH_ERR_OK;
}

static
int read_status(m25pxx_t *m, uint8_t *status)
{
    m->databuf[0] = M25PXX_CMD_RDSR;
    if (m25pxx_post_msg_1byte_read_msg(m, 1) != SPI_ERR_OK)
        return FLASH_ERR_IO;

    *status = m->databuf[0];

    return FLASH_ERR_OK;
}

static
int enable_write(m25pxx_t *m)
{
    m->databuf[0] = M25PXX_CMD_WREN;
    if (m25pxx_write_msg_1byte(m) != SPI_ERR_OK)
        return FLASH_ERR_IO;

    uint8_t st;
    if (read_status(m, &st) != SPI_ERR_OK)
        return FLASH_ERR_IO;
    if ((st & M25PXX_STATUS_WEL) != 0)
        return FLASH_ERR_OK;
    else {
        //debug_printf("flash: WE failed with status $%x\n", st);
        return FLASH_ERR_BAD_ANSWER;
    }
}

static int m25pxx_erase_all_nb(flashif_t *flash)
{
    int res;
    uint8_t status;
    m25pxx_t *m = (m25pxx_t *) flash;

    res = read_status(m, &status);
    if (res != FLASH_ERR_OK) {
        return res;
    }

    if ((status & M25PXX_STATUS_BP) != 0){
        //debug_printf("flash protected status $%x\n", status);
        return FLASH_ERR_BAD_ANSWER;
    }

    res = enable_write(m);
    if (res != FLASH_ERR_OK) {
        return res;
    }

    m->databuf[0] = M25PXX_CMD_BULKE;
    if (m25pxx_write_msg_1byte(m) != SPI_ERR_OK) {
        return FLASH_ERR_IO;
    }

    while (1) {
        res = read_status(m, &status);
        if (res != FLASH_ERR_OK) {
            return res;
        }

        if (! (status & M25PXX_STATUS_WIP)) break;
    }

    return FLASH_ERR_OK;
}

static int m25pxx_erase_all(flashif_t *flash)
{
    int res;
    mutex_lock(&flash->lock);
    res = m25pxx_erase_all_nb(flash);
    mutex_unlock(&flash->lock);
    return res;
}

static int erase_sector(flashif_t *flash, unsigned sector_num)
{
    int res;
    uint8_t status;
    m25pxx_t *m = (m25pxx_t *) flash;

    res = enable_write(m);
    if (res != FLASH_ERR_OK) {
        mutex_unlock(&flash->lock);
        return res;
    }

    m->databuf[0] = M25PXX_CMD_BE;
    uint32_t address = sector_num * flash_sector_size(flash);
    if (m25pxx_write_addr3(m, address) != SPI_ERR_OK) {
        mutex_unlock(&flash->lock);
        return FLASH_ERR_IO;
    }

    while (1) {
        res = read_status(m, &status);
        if (res != FLASH_ERR_OK) {
            mutex_unlock(&flash->lock);
            return res;
        }

        if (! (status & M25PXX_STATUS_WIP)) break;
    }

    return FLASH_ERR_OK;
}

static int m25pxx_erase_sectors(flashif_t *flash, unsigned sector_num,
    unsigned nb_sectors)
{
    int res;
    unsigned i;
    mutex_lock(&flash->lock);
    for (i = 0; i < nb_sectors; ++i) {
        res = erase_sector(flash, sector_num + i);
        if (res != FLASH_ERR_OK) return res;
    }
    mutex_unlock(&flash->lock);
    return FLASH_ERR_OK;
}

static int write_one_page(flashif_t *flash, unsigned address, 
                            void *data, unsigned size)
{
    int res;
    uint8_t status;
    m25pxx_t *m = (m25pxx_t *) flash;

    res = enable_write(m);
    if (res != FLASH_ERR_OK) return res;

    m->databuf[0] = M25PXX_CMD_PP;
    if (m25pxx_post_addr3(m, address) != SPI_ERR_OK)
        return FLASH_ERR_IO;

    m->msg.tx_data = data;
    m->msg.rx_data = 0;
    m->msg.word_count = size;
    m->msg.mode &= ~SPI_MODE_CS_HOLD;
    if (spim_trx(m->spi, &m->msg) != SPI_ERR_OK)
        return FLASH_ERR_IO;

    while (1) {
        res = read_status(m, &status);
        if (res != FLASH_ERR_OK) return res;
        if (! (status & M25PXX_STATUS_WIP)) break;
    }
    
    return FLASH_ERR_OK;
}

static int read_one_page(flashif_t *flash, unsigned address, 
                            void *data, unsigned size)
{
    m25pxx_t *m = (m25pxx_t *) flash;

    m->databuf[0] = M25PXX_CMD_READ;
    if (m25pxx_post_addr3(m, address) != SPI_ERR_OK)
        return FLASH_ERR_IO;

    m->msg.tx_data = 0;
    m->msg.rx_data = data;
    m->msg.word_count = size;
    m->msg.mode &= ~SPI_MODE_CS_HOLD;
    if (spim_trx(m->spi, &m->msg) != SPI_ERR_OK)
        return FLASH_ERR_IO;

    return FLASH_ERR_OK;
}

typedef int (* io_func)(flashif_t *flash, unsigned address, 
                        void *data, unsigned size);
                
static int cyclic_func(flashif_t *flash, unsigned address, 
                        void *data, unsigned size, io_func func)
{
    int res;
    unsigned cur_size = size;
    uint8_t *cur_data = (uint8_t *)data;
    
    mutex_lock(&flash->lock);
    
    while (size > 0) {
        cur_size = (size < flash_page_size(flash)) ?
            size : flash_page_size(flash);
        res = func(flash, address, cur_data, cur_size);
        if (res != FLASH_ERR_OK) {
            mutex_unlock(&flash->lock);
            return res;
        }
        size -= cur_size;
        address += cur_size;
        cur_data += cur_size;
    }
    
    mutex_unlock(&flash->lock);
    return FLASH_ERR_OK;
}

static int m25pxx_write(flashif_t *flash, unsigned page_num, unsigned offset, 
                        void *data, unsigned size)
{
    return cyclic_func(flash, page_num * flash_page_size(flash) + offset,
        data, size, write_one_page);
}

static int m25pxx_read(flashif_t *flash, unsigned page_num, unsigned offset,
                        void *data, unsigned size)
{
    return cyclic_func(flash, page_num * flash_page_size(flash) + offset,
        data, size, read_one_page);
}

void m25pxx_init(m25pxx_t *m, spimif_t *s, unsigned freq, unsigned mode)
{
    m->spi = s;
    flashif_t *f = &m->flashif;

    f->connect = m25pxx_connect;
    f->erase_all = m25pxx_erase_all;
    f->erase_sectors = m25pxx_erase_sectors;
    f->write = m25pxx_write;
    f->read = m25pxx_read;

    m->msg.freq = freq;
    m->msg.mode = (mode & 0xFF0F) | SPI_MODE_NB_BITS(8);
}
