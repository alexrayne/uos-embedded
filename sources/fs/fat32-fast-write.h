#ifndef __FAT32_FAST_WRITE_H__
#define __FAT32_FAST_WRITE_H__

// !!!
// В настройках компилятора должны быть заданы две константы:
//      FLASH_PAGE_SIZE - размер страницы (единицы записи)
//      FLASH_ERASE_SEC_SIZE - размер сектора (единицы стирания)

#include <fs/fs-interface.h>
#include <flash/flash-interface.h>
#include <mem/mem.h>

// Степень двойки размера сектора
#ifndef FAT_SECTOR_SIZE_POW
#define FAT_SECTOR_SIZE_POW         9
#endif

// Степень двойки количества секторов на кластер
#ifndef FAT_SEC_PER_CLUSTER_POW
#define FAT_SEC_PER_CLUSTER_POW     6
#endif

#define FAT_SECTOR_SIZE         (1 << FAT_SECTOR_SIZE_POW)
#define FAT_SEC_PER_CLUSTER     (1 << FAT_SEC_PER_CLUSTER_POW)

#define O_READ      0x1
#define O_WRITE     0x2


typedef struct _fat32_fw_entry_t
{
    fs_entry_t  base;
    uint32_t    first_clus;
    uint32_t    cur_sector;
    int         mode;
} fat32_fw_entry_t;

//
// Структура драйвера файловой системы FAT
//
typedef struct _fat32_fw_t
{
    flashif_t *         flashif;
    int                 last_error;
    uint16_t            rsvd_sec_cnt;
    uint16_t            nb_entries;
    uint32_t            fat_sz32;
    uint32_t            tot_sec32;
    uint32_t            tot_clus;
    uint32_t            nxt_free;
    uint32_t            cur_parent_pos;
    fat32_fw_entry_t    cur_dir_entry;
    char                name_buf[16];
    uint32_t            cached_sector;
    uint32_t            cached_sector_size;
    uint32_t            nb_flash_sec_per_fat_sec;
    uint32_t            sector[FAT_SECTOR_SIZE / 4] __attribute__((aligned(8)));
    uint8_t             erase_sector[FLASH_ERASE_SEC_SIZE] __attribute__((aligned(8)));
} fat32_fw_t;


static inline unsigned __attribute__((always_inline))
fat32_fw_last_error(fat32_fw_t *fat)
{
    return fat->last_error;
}


void fat32_fw_init(fat32_fw_t *fat, flashif_t * flash);

//
// Форматирование файловой системы для быстрой записи.
// nb_sectors - количество секторов в разделе; размер каждого сектора определяется
//              макросом FAT_SECTOR_SIZE.
// volume_id  - строковый идентификатор раздела; должен иметь длину обязательно(!)
//              11 символов, при необходимости выровнять до этой длины пробелами.
//
void fat32_fw_format(fat32_fw_t *fat, unsigned nb_sectors, const char * volume_id);

//
// Подсчёт файлов в корневой директории. (Поскольку в FAT32 Fast Write 
// используется только корневая директория, без иерархии, то число файлов
// в файловой системе равно числу файлов в корневой директории). Необходимо
// помнить, что даже если пользовательских файлов в системе, то всегда 
// существует запись с атрибутом VOLUME_ID, содержащая название тома FAT32.
// Таким образом, данная функция при нормальном завершении возвращает
// число, большее или равное 1.
//
unsigned fat32_fw_nb_entries(fat32_fw_t *fat);

//
// Создание нового файла. Файл всегда создаётся в конце корневой директории.
// Информация об атрибутах файла передаётся в параметре entry.
//
void fat32_fw_create(fat32_fw_t *fat, fs_entry_t *entry);

//
// Открытие файла:
//     file_mode == O_READ   - на чтение
//     file_mode == O_WRITE  - на запись
//
void fat32_fw_open(fat32_fw_t *fat, int file_mode);

//
// Промотка на начало файла
//
void fat32_fw_seek_start(fat32_fw_t *fat);

//
// Промотка файла вперёд
//
void fat32_fw_advance(fat32_fw_t *fat, filsiz_t size);

//
// Запись очередной порции данных в последний файл.
//
void fat32_fw_write(fat32_fw_t *fat, void *data, filsiz_t size);

//
// Запись очередной порции данных в последний файл.
//
void fat32_fw_write(fat32_fw_t *fat, void *data, filsiz_t size);

//
// Чтение очередной порции данных из файла.
//
filsiz_t fat32_fw_read(fat32_fw_t *fat, void *data, filsiz_t size);

//
// Закрытие текущего файла.
//
void fat32_fw_close(fat32_fw_t *fat);

fs_entry_t *get_first_entry(fat32_fw_t *fat);
fs_entry_t *get_next_entry(fat32_fw_t *fat);


#endif
