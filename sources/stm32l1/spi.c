#include <loadable/uos-interface.h>
#include "spi.h"

extern uos_loadable_t *uos;

//#define SSP1_TX_DMA     4
//#define SSP1_RX_DMA     5
//#define SSP2_TX_DMA     6
//#define SSP2_RX_DMA     7
//
//#ifdef ARM_1986BE1
//#   define SSP3_TX_DMA     8
//#   define SSP3_RX_DMA     9
//#endif
//
//#define FIFO_SIZE   (8 - 1)
//#define MAX_DMA_TRANSFERS   1024

#define PCLKDIV     1

static inline int
init_hw(stm32l1_spim_t *spi, unsigned freq, unsigned bits_per_word, unsigned mode)
{
    SPI_t *reg = spi->reg;

    if (freq != spi->last_freq) {
        reg->CR1 &= ~SPI_SPE;

        // select baud rate - first possible lower or equal to requested freq
        unsigned fPCLK = 1000 * _UC(uos) khz() / PCLKDIV;
        unsigned br; // 000: fPCLK/2, 001: fPCLK/4, 010: fPCLK/8, 011: fPCLK/16, 100: fPCLK/32, 101: fPCLK/64, 110: fPCLK/128, 111: fPCLK/256
        br = 0;
        fPCLK /= 2;
        while (fPCLK > freq){
            br++;
            fPCLK /= 2;

            if (br > 7){
                _UC(uos) debug_printf ("SPI Master %d: too low frequency!\n", spi->port);
                spi->last_freq = 0;
                return SPI_ERR_BAD_FREQ;
            }
        }
        reg->CR1 = ( reg->CR1 & ~SPI_BR(7) ) | SPI_BR(br); // set baud rate
        spi->last_freq = freq;
    }
    if ((mode ^ spi->last_mode) & (SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_MODE_LSB_FIRST)) {
        reg->CR1 &= ~SPI_SPE;

        if (mode & SPI_MODE_CPOL)
            reg->CR1 |= SPI_CPOL;
        else reg->CR1 &= ~SPI_CPOL;

        if (mode & SPI_MODE_CPHA)
            reg->CR1 |= SPI_CPHA;
        else reg->CR1 &= ~SPI_CPHA;

        if (mode & SPI_MODE_LSB_FIRST)
            reg->CR1 |= SPI_LSBFIRST;
        else
            reg->CR1 &= ~SPI_LSBFIRST;

        spi->last_mode = mode;
    }
    if (bits_per_word != spi->last_bits) {
        reg->CR1 &= ~SPI_SPE;

        if (bits_per_word == 8) {
            reg->CR1 &= ~SPI_DFF;
        } else if (bits_per_word == 16) {
            reg->CR1 |= SPI_DFF;
        } else {
            _UC(uos) debug_printf ("SPI Master %d: unsupported number of bits per word: %d\n",
                spi->port, bits_per_word);
            return SPI_ERR_BAD_BITS;
        }

        spi->last_bits = bits_per_word;
    }

    reg->CR1 |= SPI_SPE;        // enable SPI

    return SPI_ERR_OK;
}

static inline int trx_no_dma(spimif_t *spimif, spi_message_t *msg, unsigned bits_per_word, unsigned cs_num, unsigned mode)
{
    stm32l1_spim_t      *spi = (stm32l1_spim_t *) spimif;
    SPI_t               *reg = spi->reg;
    uint8_t             *rxp_8bit;
    uint8_t             *txp_8bit;
    uint16_t            *rxp_16bit;
    uint16_t            *txp_16bit;
    unsigned            i;

    //    Активируем CS
    //    Если функция cs_control не установлена, то считаем, что для выборки
    //    устройства используется вывод NSS
    if (spi->cs_control)
        spi->cs_control(spi->port, cs_num, mode & SPI_MODE_CS_HIGH);
    else
        reg->CR1 |= SPI_MSTR; // reg->CR1 |= SPI_SPE;

    if (bits_per_word <= 8) {
        rxp_8bit = (uint8_t *) msg->rx_data;
        txp_8bit = (uint8_t *) msg->tx_data;

        i = 0;
        while (i < msg->word_count) {
            while (!(reg->SR & SPI_TXE));           // wait for tx buffer empty
            if (txp_8bit)
                reg->DR = *txp_8bit++;
            else
                reg->DR = 0;

            while (!(reg->SR & SPI_RXNE));
            if (rxp_8bit) {
                *rxp_8bit = reg->DR;
                rxp_8bit++;
            }
            else
                reg->DR;

            if (++i >= msg->word_count)
                break;
        }

     //   while (reg->SR & SPI_BSY);
    } else if (bits_per_word > 8 && bits_per_word <= 16) {
        rxp_16bit = (uint16_t *) msg->rx_data;
        txp_16bit = (uint16_t *) msg->tx_data;

        i = 0;
        while (i < msg->word_count) {
            while (!(reg->SR & SPI_TXE));
            if (txp_16bit) {
                reg->DR = *txp_16bit++;
            } else {
                reg->DR = 0;
            }

            while (!(reg->SR & SPI_RXNE));
            if (rxp_16bit) {
                *rxp_16bit = reg->DR;
                rxp_16bit++;
            }
            else
                reg->DR;

            if (++i >= msg->word_count)
                break;
        }
    } else {
        // Деактивируем CS
        if (!(mode & SPI_MODE_CS_HOLD)) {
            if (spi->cs_control)
                spi->cs_control(spi->port, cs_num, !(mode & SPI_MODE_CS_HIGH));
            else
                reg->CR1 &= ~SPI_MSTR; // reg->CR1 &= ~SPI_SPE;
        }

        return SPI_ERR_BAD_BITS;
    }

	// Деактивируем CS
	// Если функция cs_control не установлена, то считаем, что для выборки
	// устройства используется вывод NSS
    if (!(mode & SPI_MODE_CS_HOLD)) {
        if (spi->cs_control)
            spi->cs_control(spi->port, cs_num, !(mode & SPI_MODE_CS_HIGH));
        else
            reg->CR1 &= ~SPI_MSTR; // reg->CR1 &= ~SPI_SPE;
    }

    return SPI_ERR_OK;
}

static int trx(spimif_t *spimif, spi_message_t *msg)
{
    unsigned            bits_per_word = SPI_MODE_GET_NB_BITS(msg->mode);
    unsigned            cs_num = SPI_MODE_GET_CS_NUM(msg->mode);
    unsigned            mode = SPI_MODE_GET_MODE(msg->mode);
    int res;

    _UC(uos) mutex_lock(&spimif->lock);

    res = init_hw((stm32l1_spim_t *)spimif, msg->freq, bits_per_word, mode);
    if (res != SPI_ERR_OK) {
        _UC(uos) mutex_unlock(&spimif->lock);
        return res;
    }

    res = trx_no_dma(spimif, msg, bits_per_word, cs_num, mode);

    _UC(uos) mutex_unlock(&spimif->lock);

    return res;
}

int stm32l1_spi_init(stm32l1_spim_t *spi, unsigned port, spi_cs_control_func csc)
{
    _UC(uos) memset(spi, 0, sizeof(stm32l1_spim_t));

    spi->port = port;
    spi->spimif.trx = trx;
    spi->cs_control = csc;
    spi->spimif.data_align = 1;

    if (port == 1) {
        RCC->APB2ENR |= RCC_SPI1EN;
        RCC->APB2LPENR |= RCC_SPI1LPEN;
        spi->reg = SPI1;

    } else if (port == 2) {
        RCC->APB1ENR |= RCC_SPI2EN;
        RCC->APB1LPENR |= RCC_SPI2LPEN;
        spi->reg = SPI2;

#if defined(STM32L_CAT3) || defined(STM32L_CAT4) || defined(STM32L_CAT5) || defined(STM32L_CAT6)
    } else if (port == 3) {
        RCC->APB1ENR |= RCC_SPI3EN;
        RCC->APB1LPENR |= RCC_SPI3LPEN;
        spi->reg = SPI3;

#endif // defined(STM32L_CAT3) || defined(STM32L_CAT4) || defined(STM32L_CAT5) || defined(STM32L_CAT6)
    } else {
        return SPI_ERR_BAD_PORT;
    }

    spi->reg->CR1 = 0;
    spi->reg->CR2 = 0;

    if (csc)
        spi->reg->CR1 |= SPI_SSI | SPI_SSM | SPI_MSTR;
    else
        spi->reg->CR2 |= SPI_SSOE;

    spi->reg->CR1 |= SPI_SPE;

    return SPI_ERR_OK;
}

