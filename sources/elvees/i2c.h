#ifndef __ELVEES_I2C_H_
#define __ELVEES_I2C_H_

#include <i2c/i2c-interface.h>

typedef struct _elvees_i2c_t {
	i2cif_t             i2cif;
	
	unsigned            cur_mode;
    i2c_transaction_t   *cur_trans;
	uint8_t             *trx_bytes;
	int                 trx_size;
} elvees_i2c_t;

void elvees_i2c_init (elvees_i2c_t *i2c);

#endif /* __ELVEES_I2C_H_ */
