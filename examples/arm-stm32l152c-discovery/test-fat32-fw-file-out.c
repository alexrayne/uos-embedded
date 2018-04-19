#include <runtime/lib.h>
#include <kernel/uos.h>
#include <stm32l1/spi.h>
#include <stm32l1/gpio.h>
#include <timer/timer.h>
#include <fs/fat32-fast-write.h>
#include <fs/fat32-fw-file-out.h>

#include <flash/sdhc-spi.h>
const char *flash_name = "SD";
sdhc_spi_t flash;

#define SPI_NUM           2
#define ERASE_SECTOR      5

ARRAY (task, 1000);
stm32l1_spim_t spi;
fat32_fw_t ffw;
timer_t timer;

stm32l1_gpio_t spi_fl_cs;
stm32l1_gpio_t spi_fl_miso;
stm32l1_gpio_t spi_fl_mosi;
stm32l1_gpio_t spi_fl_sck;
stm32l1_gpio_t button;

int sd_inserted = 1;
int file_counter = 0;

void print_fat32fw_info()
{
    debug_printf("Reserved sectors:  %d\n", ffw.rsvd_sec_cnt);
    debug_printf("FAT table size:    %d\n", ffw.fat_sz32);
    debug_printf("Total clusters:    %d\n", ffw.tot_clus);
    debug_printf("Next free cluster: %d\n", ffw.nxt_free);
}

void connect_sd(flashif_t *f)
{
    if (flash_connect(f) == FLASH_ERR_OK) {
        debug_printf("Found %s, size: %u Kb, nb pages: %u, page size: %d b\n\
            nb sectors: %u, sector size: %d b\n",
            flash_name, (unsigned)(flash_size(f) >> 10),
            flash_nb_pages(f), flash_page_size(f),
            flash_nb_sectors(f), flash_sector_size(f));
            
        // Уменьшаем размер, чтобы не ждать долго окончания форматирования
        //f->nb_sectors = 128;

        fat32_fw_init(&ffw, (flashif_t *)&flash);
        if (fat32_fw_last_error(&ffw) == FS_ERR_OK) {
            debug_printf("Found FAT32 Fast Write flash file system:\n");
            print_fat32fw_info();
        } else {
            if (fat32_fw_last_error(&ffw) == FS_ERR_BAD_FORMAT)
                debug_printf("Bad formatted flash!\n");
            else debug_printf("Error %d\n", fat32_fw_last_error(&ffw));
        }
    } else {
        debug_printf("%s not found\n", flash_name);
        for (;;);
    }
}

void write_to_stream()
{
    static unsigned char file_name[16];
    static fs_entry_t file_entry;
    
    snprintf(file_name, 12, "test_%d.txt", file_counter);
    
    file_entry.name = (char *)file_name;
    file_entry.year = 2015;
    file_entry.month = 6;
    file_entry.day = 1;
    file_entry.hour = 18;
    file_entry.minute = 15;
    file_entry.second = 7;
   
    ffw_out_init(&ffw, &file_entry);
    
    printf(&ffw_out, "Hello, world #%d!\n", file_counter++);
    
    fclose(&ffw_out);

    debug_printf("\nFiles in the filesystem:\n");
    fs_entry_t *entry = get_first_entry(&ffw);
    while (entry) {
        debug_printf("%12s: %d byte(s)\n", entry->name, entry->size);
        entry = get_next_entry(&ffw);
    }
}

void hello (void *arg)
{
    flashif_t *f = (flashif_t *)arg;

    timer_delay(&timer, 100);

    connect_sd(f);
    
    debug_printf("Format in progress...\n");
    fat32_fw_format(&ffw, flash_nb_pages(f), "TST_FAT32FW");
    if (fat32_fw_last_error(&ffw) == FS_ERR_OK) {
        debug_printf("Flash formatted successfully!\n");
        print_fat32fw_info();
    } else  {
        debug_printf("Error while formatting flash!\n");
        for (;;);
    }

    debug_printf("Press button to disconnect SD-card\n");
    for (;;) {
        if (gpio_val(to_gpioif(&button)) == 1) {
            if (sd_inserted) {
                sd_inserted = 0;
                debug_printf("Please disconnect SD-card\n");
                mdelay(1000);
                debug_printf("Insert SD-card again and press the button\n");
            } else {
                sd_inserted = 1;
                connect_sd(f);
                write_to_stream();
            }
        }
    }
}

void spi_flash_cs(unsigned port, unsigned cs_num, int level)
{
    gpio_set_val( to_gpioif(&spi_fl_cs), level );
}

void uos_init (void)
{
    debug_printf("\nTesting FAT32 Fast Write on %s\n", flash_name);

    timer_init(&timer, KHZ, 1);
    
    stm32l1_gpio_init( &button, GPIO_PORT_A, 0, GPIO_FLAGS_INPUT );

    stm32l1_gpio_init( &spi_fl_cs,   GPIO_PORT_B, 12, GPIO_FLAGS_OUTPUT | GPIO_FLAGS_50MHZ );
    stm32l1_gpio_init( &spi_fl_sck,  GPIO_PORT_B, 13, GPIO_FLAGS_ALT_SPI2 | GPIO_FLAGS_50MHZ );
    stm32l1_gpio_init( &spi_fl_miso, GPIO_PORT_B, 14, GPIO_FLAGS_ALT_SPI2 | GPIO_FLAGS_50MHZ );
    stm32l1_gpio_init( &spi_fl_mosi, GPIO_PORT_B, 15, GPIO_FLAGS_ALT_SPI2 | GPIO_FLAGS_50MHZ );
    gpio_set_val( to_gpioif(&spi_fl_cs), 1 );

    stm32l1_spi_init(&spi, SPI_NUM, spi_flash_cs);

    sd_spi_init(&flash, (spimif_t *)&spi, 0);

    task_create (hello, &flash, "hello", 1, task, sizeof (task));
}
