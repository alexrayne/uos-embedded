#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <stm32l1/i2c.h>

#define INTERRUPT_MASK  (I2C_ITEVTEN | I2C_ITERREN)

static inline int calc_ccr_sm(unsigned i2c_khz)
{
    return KHZ_PCLK1 / i2c_khz / 3;
}

static inline int calc_ccr_fm(unsigned i2c_khz)
{
    return KHZ_PCLK1 / i2c_khz / 25;
}

static inline int set_timings(I2C_t *reg, unsigned mode, unsigned khz)
{
	switch (mode) {
	case I2C_MODE_SM:
	case I2C_MODE_SMBUS:
	    if (khz > 100)
    	    return I2C_ERR_BAD_FREQ;
    	reg->CCR = I2C_CCR(calc_ccr_sm(khz));
    	reg->TRISE = KHZ_PCLK1 / 1000 + 1;
	    break;
	case I2C_MODE_FM:
	    if (khz > 100)
	        reg->CCR = I2C_CCR(calc_ccr_sm(khz)) | I2C_DUTY | I2C_FS;
	    else
	        reg->CCR = I2C_CCR(calc_ccr_sm(khz)) | I2C_FS;
    	reg->TRISE = 300 * KHZ_PCLK1 / 1000000 + 1;
	    break;
	case I2C_MODE_FM_PLUS:
        return I2C_ERR_MODE_NOT_SUPP;
	default:
	    return I2C_ERR_MODE_NOT_SUPP;
	}
	
	return I2C_ERR_OK;
}

static inline int i2c_ev_irq(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        return IRQ_I2C1_EV;
    else
        return IRQ_I2C2_EV;
}

static inline int i2c_er_irq(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        return IRQ_I2C1_ER;
    else
        return IRQ_I2C2_ER;
}

static inline int i2c_rx_dma_irq(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        return IRQ_DMA1_CHANNEL7;
    else
        return IRQ_DMA1_CHANNEL5;
}

static inline int i2c_tx_dma_irq(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        return IRQ_DMA1_CHANNEL6;
    else
        return IRQ_DMA1_CHANNEL4;
}

static inline DMA_CHANNEL_t *i2c_rx_dma(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        return DMA1_CH(7);
    else
        return DMA1_CH(5);
}

static inline DMA_CHANNEL_t *i2c_tx_dma(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        return DMA1_CH(6);
    else
        return DMA1_CH(4);
}

static inline void clear_rx_dma_intr_flags(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        DMA1->IFCR = DMA_CTCIF(7);
    else
        DMA1->IFCR = DMA_CTCIF(5);
}

static inline void clear_tx_dma_intr_flags(stm32l1_i2c_t *stm32l1_i2c)
{
    if (stm32l1_i2c->port == 1)
        DMA1->IFCR = DMA_CTCIF(6);
    else
        DMA1->IFCR = DMA_CTCIF(4);
}

static int start_trx(stm32l1_i2c_t *stm32l1_i2c)
{
    while (stm32l1_i2c->cur_trans->size == 0) {
        stm32l1_i2c->cur_trans = stm32l1_i2c->cur_trans->next;
        if (! stm32l1_i2c->cur_trans)
            return I2C_ERR_BAD_PARAM;
    }
    
    stm32l1_i2c->reg->CR1 |= I2C_ACK | I2C_START;

    return I2C_ERR_OK;
}

static int stm32l1_i2c_trx(i2cif_t *i2c, i2c_message_t *msg)
{
    stm32l1_i2c_t *stm32l1_i2c = (stm32l1_i2c_t *) i2c;
    I2C_t *ireg = stm32l1_i2c->reg;
    int res = I2C_ERR_OK;
    
    if (! msg->first)
        return I2C_ERR_BAD_PARAM;
    
    mutex_lock(&i2c->lock);
	
	res = set_timings(ireg, msg->mode & I2C_MODE_MASK, I2C_MODE_GET_FREQ_KHZ(msg->mode));
	if (res != I2C_ERR_OK)
	    goto trx_exit;

	stm32l1_i2c->cur_mode = msg->mode;
	stm32l1_i2c->cur_trans = msg->first;
	ireg->CR1 = I2C_PE;
	res = start_trx(stm32l1_i2c);
	if (res != I2C_ERR_OK)
	    goto trx_exit;
	    
	res = (int)mutex_wait(&i2c->lock);
	
trx_exit:
	ireg->CR1 = 0;
  	mutex_unlock(&i2c->lock);
    return res;
}

static void process_last_byte(stm32l1_i2c_t *stm32l1_i2c)
{
    I2C_t *ireg = stm32l1_i2c->reg;
    const int is_last_transaction = (stm32l1_i2c->cur_trans->next == 0);
    
    if (is_last_transaction) {
        stm32l1_i2c->reg->CR2 &= ~I2C_DMAEN;
        ireg->CR1 |= I2C_STOP;
        mutex_activate(&stm32l1_i2c->i2cif.lock, (void *) I2C_ERR_OK);
    } else {
        stm32l1_i2c->cur_trans = stm32l1_i2c->cur_trans->next;
        int res = start_trx(stm32l1_i2c);
        if (res != I2C_ERR_OK) {
            stm32l1_i2c->reg->CR2 &= ~I2C_DMAEN;
            ireg->CR1 |= I2C_STOP;
            mutex_activate(&stm32l1_i2c->i2cif.lock, (void *) res);
        }
    }
}

static bool_t tx_dma_transfer_complete_handler(void *arg)
{
    stm32l1_i2c_t *stm32l1_i2c = arg;
    stm32l1_i2c->reg->CR2 &= ~I2C_DMAEN;
//debug_printf("tx_tc\n");

    clear_tx_dma_intr_flags(stm32l1_i2c);
    arch_intr_allow(i2c_tx_dma_irq(stm32l1_i2c));
    return 0;
}

static bool_t rx_dma_transfer_complete_handler(void *arg)
{
    stm32l1_i2c_t *stm32l1_i2c = arg;
    stm32l1_i2c->reg->CR2 &= ~I2C_DMAEN;
//debug_printf("rx_tc\n");

    process_last_byte(stm32l1_i2c);
    
    clear_rx_dma_intr_flags(stm32l1_i2c);
    arch_intr_allow(i2c_rx_dma_irq(stm32l1_i2c));
    return 0;
}

static void start_dma(stm32l1_i2c_t *stm32l1_i2c)
{
    const int rx_mode = (stm32l1_i2c->cur_trans->size < 0);
    
    i2c_tx_dma(stm32l1_i2c)->CCR &= ~DMA_EN;
    i2c_rx_dma(stm32l1_i2c)->CCR &= ~DMA_EN;
    if (rx_mode) {
        i2c_rx_dma(stm32l1_i2c)->CMAR  = (unsigned) stm32l1_i2c->cur_trans->data;
        i2c_rx_dma(stm32l1_i2c)->CNDTR = -stm32l1_i2c->cur_trans->size;
        i2c_rx_dma(stm32l1_i2c)->CCR  |= DMA_EN;
        stm32l1_i2c->reg->CR2 |= I2C_LAST | I2C_DMAEN;
    } else {
        i2c_tx_dma(stm32l1_i2c)->CMAR  = (unsigned) stm32l1_i2c->cur_trans->data;
        i2c_tx_dma(stm32l1_i2c)->CNDTR = stm32l1_i2c->cur_trans->size;
        i2c_tx_dma(stm32l1_i2c)->CCR  |= DMA_EN;
        stm32l1_i2c->reg->CR2 |= I2C_DMAEN;
    }
}

static bool_t trx_handler(void *arg)
{
    stm32l1_i2c_t *stm32l1_i2c = arg;
    I2C_t *ireg = stm32l1_i2c->reg;
    const uint32_t sr1 = ireg->SR1;
    
//debug_printf("trx_handler, SR1 = %08X\n", sr1);

    if (sr1 & I2C_SB) {
        const int rx_mode = (stm32l1_i2c->cur_trans->size < 0);
//debug_printf("SB, rx_mode %d\n", rx_mode);
        if (stm32l1_i2c->cur_mode & I2C_MODE_10BIT_ADDR) {
            ireg->DR = 0xF | ((I2C_MODE_GET_SLAVE_ADDR(stm32l1_i2c->cur_mode) >> 7) & 0x6) | rx_mode;
        } else {
            ireg->DR = I2C_MODE_GET_SLAVE_ADDR(stm32l1_i2c->cur_mode) | rx_mode;
            start_dma(stm32l1_i2c);
        }
    } else if (sr1 & I2C_ADD10) {
//debug_printf("ADD10\n");
        ireg->DR = I2C_MODE_GET_SLAVE_ADDR(stm32l1_i2c->cur_mode);
        start_dma(stm32l1_i2c);
    } else if (sr1 & I2C_ADDR) {
//debug_printf("ADDR, size = %d\n", stm32l1_i2c->cur_trans->size);
        if (stm32l1_i2c->cur_trans->size == -1)
            stm32l1_i2c->reg->CR1 &= ~I2C_ACK;
        (void) ireg->SR2;
    } else if (sr1 & I2C_BTF) {
//debug_printf("BTF\n");
        process_last_byte(stm32l1_i2c);
    }
    
    arch_intr_allow(i2c_ev_irq(stm32l1_i2c));
    return 0;
}

static bool_t error_handler(void *arg)
{
    stm32l1_i2c_t *stm32l1_i2c = arg;
    I2C_t *ireg = stm32l1_i2c->reg;
    const uint32_t sr1 = ireg->SR1;
    int res = I2C_ERR_OK;
    
    if (sr1 & I2C_ARLO)
        res = I2C_ERR_ARBITR_LOST;
	else if (sr1 & I2C_BERR)
	    res = I2C_ERR_BUS;
	else if (sr1 & I2C_AF)
	    res = I2C_ERR_NOT_ACKED;

//debug_printf("error_handler %d\n", res);

	ireg->SR1 = 0;
	stm32l1_i2c->reg->CR2 &= ~I2C_DMAEN;
	mutex_activate(&stm32l1_i2c->i2cif.lock, (void *) res);
	    
    arch_intr_allow(i2c_er_irq(stm32l1_i2c));
    return 0;
}

int stm32l1_i2c_init(stm32l1_i2c_t *i2c, int port)
{
    i2cif_t *i2cif = to_i2cif(i2c);
    i2cif->trx = stm32l1_i2c_trx;
    i2c->port = port;
    
    if (port == 1) {
        i2c->reg = I2C1;
        RCC->APB1ENR |= RCC_I2C1EN;
        RCC->APB1LPENR |= RCC_I2C1LPEN;
        RCC->AHBENR |= RCC_DMA1EN;
        RCC->AHBLPENR |= RCC_DMA1LPEN;
    } else if (port == 2) {
        i2c->reg = I2C2;
        RCC->APB1ENR |= RCC_I2C2EN;
        RCC->APB1LPENR |= RCC_I2C2LPEN;
        RCC->AHBENR |= RCC_DMA1EN;
        RCC->AHBLPENR |= RCC_DMA1LPEN;
    } else {
        return I2C_ERR_BAD_PORT;
    }
    
    i2c_tx_dma(i2c)->CPAR = (unsigned)&i2c->reg->DR;
    i2c_tx_dma(i2c)->CCR = DMA_MSIZE_8 | DMA_PSIZE_8 | DMA_MINC |
        DMA_DIR_FROM_MEM | DMA_TCIE;

    i2c_rx_dma(i2c)->CPAR = (unsigned)&i2c->reg->DR;
    i2c_rx_dma(i2c)->CCR = DMA_MSIZE_8 | DMA_PSIZE_8 | DMA_MINC | DMA_TCIE;

    mutex_attach_irq(&i2c->trx_mutex,    i2c_ev_irq(i2c), trx_handler, i2c);
    mutex_attach_irq(&i2c->error_mutex,  i2c_er_irq(i2c), error_handler, i2c);
    mutex_attach_irq(&i2c->tx_dma_mutex, i2c_tx_dma_irq(i2c), tx_dma_transfer_complete_handler, i2c);
    mutex_attach_irq(&i2c->rx_dma_mutex, i2c_rx_dma_irq(i2c), rx_dma_transfer_complete_handler, i2c);

    i2c->reg->CR1 = 0;
    i2c->reg->CR2 = I2C_FREQ(KHZ_PCLK1 / 1000) | INTERRUPT_MASK;
        
    return I2C_ERR_OK;
}

