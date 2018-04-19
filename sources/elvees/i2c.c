#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <elvees/i2c.h>

#define I2C_IRQ     23

static int elvees_start_trx(elvees_i2c_t *i2c)
{
    while (i2c->cur_trans->size == 0) {
        i2c->cur_trans = i2c->cur_trans->next;
        if (! i2c->cur_trans)
            return I2C_ERR_BAD_PARAM;
    }

	MC_I2C_CTR = MC_I2C_EN | MC_I2C_IEN;
	MC_I2C_PRER = KHZ / (5 * I2C_MODE_GET_FREQ_KHZ(i2c->cur_mode)) - 1;

    i2c->trx_bytes = i2c->cur_trans->data;

    if (i2c->cur_trans->size > 0) {
        i2c->trx_size = i2c->cur_trans->size;
        MC_I2C_TXR = I2C_MODE_GET_SLAVE_ADDR(i2c->cur_mode);
    } else if (i2c->cur_trans->size < 0) {
        i2c->trx_size = -i2c->cur_trans->size;
        MC_I2C_TXR = I2C_MODE_GET_SLAVE_ADDR(i2c->cur_mode) | MC_I2C_READ;
    }
    
    MC_I2C_CR = MC_I2C_SND | MC_I2C_STA;
    
    return I2C_ERR_OK;
}

static int wait_byte_transmitted(mutex_t *lock)
{
    mutex_wait(lock);
    MC_I2C_CR = MC_I2C_IACK;
    
    if (MC_I2C_SR & MC_I2C_AL)
        return I2C_ERR_ARBITR_LOST;
        
    if (MC_I2C_SR & MC_I2C_RXNACK)
        return I2C_ERR_NOT_ACKED;

    return I2C_ERR_OK;
}

static int elvees_i2c_trx(i2cif_t *i2cif, i2c_message_t *msg)
{
    elvees_i2c_t *i2c = (elvees_i2c_t *) i2cif;
    int res = I2C_ERR_OK;
    
    if (! msg->first)
        return I2C_ERR_BAD_PARAM;
    
    mutex_lock(&i2cif->lock);
	
	i2c->cur_mode = msg->mode;
	i2c->cur_trans = msg->first;

    while (i2c->cur_trans) {
        const int is_last_transaction = (i2c->cur_trans->next == 0);
        const int is_tx_transaction = (i2c->cur_trans->size > 0);
        const int is_rx_transaction = (i2c->cur_trans->size < 0);
        
      	res = elvees_start_trx(i2c);
	    if (res != I2C_ERR_OK)
	        goto trx_exit;
	        
        res = wait_byte_transmitted(&i2cif->lock);
        if (res != I2C_ERR_OK)
            goto trx_exit;
	        
        while (i2c->trx_size) {
            unsigned cr;
            if (is_tx_transaction) {
                cr = MC_I2C_SND;
                MC_I2C_TXR = *i2c->trx_bytes++;
            } else {
                cr = MC_I2C_RCV;
            }

            const int is_last_byte = (--i2c->trx_size == 0);
            if (is_last_transaction && is_last_byte)
                cr |= MC_I2C_NACK | MC_I2C_STO;
            MC_I2C_CR = cr;
            
            res = wait_byte_transmitted(&i2cif->lock);
            if (is_last_byte) {
	            if ((is_tx_transaction && (res != I2C_ERR_OK)) ||
	                (is_rx_transaction && (res != I2C_ERR_NOT_ACKED))) {
	                    res = I2C_ERR_NOT_ACKED;
    	                goto trx_exit;
    	            }
            } else {
                if (res != I2C_ERR_OK)
                    goto trx_exit;
            }
            
            if (is_rx_transaction)
                *i2c->trx_bytes++ = MC_I2C_RXR;
        }
        
        i2c->cur_trans = i2c->cur_trans->next;
    }
    res = I2C_ERR_OK;

trx_exit:
	MC_I2C_CTR = MC_I2C_PRST;
  	mutex_unlock(&i2cif->lock);
    return res;
}

void elvees_i2c_init(elvees_i2c_t *i2c)
{
    MC_I2C_CTR = MC_I2C_PRST;
    i2c->i2cif.trx = elvees_i2c_trx;
    mutex_attach_irq(&i2c->i2cif.lock, I2C_IRQ, 0, 0);
}

