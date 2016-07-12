// ID: SPO-UOS-milandr-mil-std-1553_setup.c VER: 1.0.0
//
// История изменений:
//
// 1.0.0	Начальная версия
//
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <milandr/mil-std-1553_setup.h>
#include <milandr/mil-std-1553_bc.h>
#include <milandr/mil-std-1553_rt.h>

int read_idx, write_idx;
status_item_t status_array[STATUS_ITEMS_SIZE];
uint32_t nb_missing;

static bool_t timer_handler(void *arg)
{
    milandr_mil1553_t *mil = arg;

    if (mil->urgent_desc.raw != 0) {
    	//debug_printf("urgent_desc ");
    	//int i;
    	//int wc = (mil->urgent_desc.words_count==0?32:mil->urgent_desc.words_count);
    	//for(i=0;i<wc;i++) {
    	//	debug_printf("%04x ", mil->urgent_data[i]);
    	//}
    	//debug_printf("\n");
        mil->urgent_desc.reserve = 1;       // Признак того, что идёт передача вне очереди
        start_slot(mil, mil->urgent_desc, mil->urgent_data);
    } else if (mil->cur_slot == mil->cyclogram) {
    	if (mil->cur_slot != 0) {
    		start_slot(mil, mil->cur_slot->desc, mil->cur_slot->data);
    	}
    }

    mil->tim_reg->TIM_STATUS = ARM_TIM_CNT_ZERO_EVENT;

    arch_intr_allow(mil->tim_irq);

    return 1;
}

static int mil_lock(mil1553if_t *_mil)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
    mutex_lock(&_mil->mutex);
    mutex_lock(&mil->tim_lock);
    return MIL_ERR_OK;
}

static int mil_unlock(mil1553if_t *_mil)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
    mutex_unlock(&mil->tim_lock);
    mutex_unlock(&_mil->mutex);
    return MIL_ERR_OK;
}

static int mil_set_mode(mil1553if_t *_mil, unsigned mode)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
    // На ходу можно переключаться только между основным и резервным каналом
    // в режиме КШ.
    if (_mil->is_started(_mil)) {
        if ((mil->mode == MIL_MODE_BC_MAIN) || (mil->mode == MIL_MODE_BC_RSRV)) {
            mil_lock(_mil);
            if (mode == MIL_MODE_BC_MAIN) {
                // Отключить резервный, включить основной канал.
                mil->reg->CONTROL = (mil->reg->CONTROL & ~MIL_STD_CONTROL_TRB) | MIL_STD_CONTROL_TRA;
            } else if (mode == MIL_MODE_BC_RSRV) {
                // Отключить основной, включить резервный канал.
                mil->reg->CONTROL = (mil->reg->CONTROL & ~MIL_STD_CONTROL_TRA) | MIL_STD_CONTROL_TRB;
            } else {
            	mil_unlock(_mil);
                return MIL_ERR_BAD_ARG;
            }
            mil->mode = mode;
            mil_unlock(_mil);
            return MIL_ERR_OK;
        } else {
            return MIL_ERR_NOT_PERM;
        }
    } else {
        mil->mode = mode;
    }

    return MIL_ERR_OK;
}

static int mil_start(mil1553if_t *_mil)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    if (_mil->is_started(_mil))
        return MIL_ERR_OK;

    unsigned control = MIL_STD_CONTROL_DIV((KHZ/MIL_STD_CLOCK_DIV)/1000);// на блок подается MAIN_CLK/MIL_STD_CLOCK_DIV
        
    if ((mil->mode == MIL_MODE_BC_MAIN) || (mil->mode == MIL_MODE_BC_RSRV)) {
        control |= MIL_STD_CONTROL_MODE(MIL_STD_MODE_BC);
        if (mil->mode == MIL_MODE_BC_MAIN)
            control |= MIL_STD_CONTROL_TRA;
        else
            control |= MIL_STD_CONTROL_TRB;
            
        if (mil->tim_reg) {
        	if (mil->period_ms==0) {
        		mil->tim_reg->TIM_ARR = 0;
        	} else {
        		mil->tim_reg->TIM_ARR = mil->period_ms * KHZ-1;
        	}
            mil->tim_reg->TIM_IE = ARM_TIM_CNT_ARR_EVENT_IE;
            mil->tim_reg->TIM_CNTRL = ARM_TIM_CNT_EN;
        }
        mil->reg->INTEN = MIL_STD_INTEN_VALMESSIE | MIL_STD_INTEN_ERRIE;
        mil->reg->CONTROL = control;

        mil->cur_slot = mil->cyclogram;

        if (mil->urgent_desc.raw != 0) {
            mil->urgent_desc.reserve = 1;   // Признак того, что идёт передача вне очереди
            start_slot(mil, mil->urgent_desc, mil->urgent_data);
        } else {
        	if (mil->cur_slot != 0) {
        		start_slot(mil, mil->cur_slot->desc, mil->cur_slot->data);
        	}
        }
    } else if (mil->mode == MIL_MODE_RT) {
        control |= MIL_STD_CONTROL_MODE(MIL_STD_MODE_RT);
        control |= MIL_STD_CONTROL_ADDR(mil->addr_self) | MIL_STD_CONTROL_TRA | MIL_STD_CONTROL_TRB;
        //control |= 1<<21; // выключаем автоподстройку
        //control |= 1UL<<20; // включаем фильтрацию
        mil->reg->StatusWord1 = MIL_STD_STATUS_ADDR_OU(mil->addr_self);
        mil->reg->INTEN = MIL_STD_INTEN_RFLAGNIE | MIL_STD_INTEN_VALMESSIE | MIL_STD_INTEN_ERRIE;
        mil->reg->CONTROL = control;
    } else {
        return MIL_ERR_NOT_SUPP;
    }

    mil->is_running = 1;
    return MIL_ERR_OK;
}

static int mil_stop(mil1553if_t *_mil)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    mil->reg->INTEN = 0;
    mil->reg->CONTROL = MIL_STD_CONTROL_MR;
    if (mil->tim_reg) {
        mil->tim_reg->TIM_IE = 0;
        mil->tim_reg->TIM_CNTRL = 0;
    }
    
    while (!mem_queue_is_empty(&mil->cyclogram_rxq)) {
    	void *que_elem = 0;
    	mem_queue_get(&mil->cyclogram_rxq, &que_elem);
    	mem_free(que_elem);
    }

    while (!mem_queue_is_empty(&mil->urgent_rxq)) {
            void *que_elem = 0;
            mem_queue_get(&mil->urgent_rxq, &que_elem);
            mem_free(que_elem);
        }

    //while (!mem_queue_is_empty (&mil->rt_rxq)) {
    //    	void *que_elem = 0;
    //    	mem_queue_get(&mil->rt_rxq, &que_elem);
    //    	mem_free(que_elem);
    //}

    memset(status_array, 0, sizeof(status_array));
    read_idx = 0;
    write_idx = 0;

    mil->is_running = 0;
    return MIL_ERR_OK;
}

static int mil_is_started(mil1553if_t *_mil)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    return mil->is_running;
}

static int mil_read_subaddr(mil1553if_t *_mil, int subaddr, void *data, int nb_words)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    uint16_t *pdata = data;
    arm_reg_t *preg = &mil->reg->DATA[subaddr * MIL_SUBADDR_WORDS_COUNT];
    int i;
    
    mil_lock(_mil);
    for (i = 0; i < nb_words; ++i)
        *pdata++ = *preg++;
    mil_unlock(_mil);

    return MIL_ERR_OK;
}

static int mil_write_subaddr(mil1553if_t *_mil, int subaddr, void *data, int nb_words)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    uint16_t *pdata = data;
    arm_reg_t *preg = &mil->reg->DATA[subaddr * MIL_SUBADDR_WORDS_COUNT];
    int i;
    
    mil_lock(_mil);
    for (i = 0; i < nb_words; ++i)
        *preg++ = *pdata++;
    mil_unlock(_mil);

    return MIL_ERR_OK;
}

static int mil_bc_set_cyclogram(mil1553if_t *_mil, mil_slot_t *head, unsigned nb_slots)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    if (_mil->is_started(_mil))
        return MIL_ERR_NOT_PERM;

    mil->cyclogram = head;
    mil->nb_slots = nb_slots;
    return MIL_ERR_OK;
}

static int mil_bc_set_period(mil1553if_t *_mil, unsigned period_ms)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    if (_mil->is_started(_mil))
        return MIL_ERR_NOT_PERM;

    mil->period_ms = period_ms;

    return MIL_ERR_OK;
}

static bool_t status_handler(void *arg)
{
	milandr_mil1553_t *mil = (milandr_mil1553_t *)arg;
	MIL_STD_1553B_t   *reg = (MIL_STD_1553B_t *)mil->reg;
	int word;

	uint32_t flag = reg->STATUS;
	if (mil->mode == MIL_MODE_RT) {
		if (flag & MIL_STD_STATUS_ERR) {
			mil->nb_errors++;
			goto out;
		}
		const unsigned int comWrd1 = reg->CommandWord1;
		const unsigned int msg = reg->MSG;

		// Подадрес из командного слова 1
		const unsigned char subaddr = (comWrd1 & MIL_STD_COMWORD_SUBADDR_MODE_MASK) >> MIL_STD_COMWORD_SUBADDR_MODE_SHIFT;
		// Код команды
		const unsigned int cmdCode = (comWrd1 & MIL_STD_COMWORD_WORDSCNT_CODE_MASK) >> MIL_STD_COMWORD_WORDSCNT_CODE_SHIFT;

		// Количество слов данных в сообщении
		const unsigned char wordsCount = (cmdCode == 0 ? 32 : cmdCode);

		unsigned short answerWord = MIL_STD_STATUS_ADDR_OU(mil->addr_self);
		switch (msg)
		{
		// приём данных от КШ (КШ-ОУ), формат сообщения 7
		case MSG_DATARECV__BC_RT__GROUP:
			answerWord |= MIL_STD_STATUS_GROUP_COMMAND_RECEIVED;
		// приём данных от КШ (КШ-ОУ), формат сообщения 1
		case MSG_DATARECV__BC_RT__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0) {
				// Установить ответное слово
				mil->reg->StatusWord1 = answerWord;
			}
			if ((flag & MIL_STD_STATUS_VALMESS) != 0) {
				mil->nb_words += wordsCount;
				int index = MIL_STD_SUBADDR_WORD_INDEX(subaddr);
				if (!mil->rxbuf[subaddr].busy) {
                    for (word = 0; word < wordsCount; ++word) {
                        mil->rxbuf[subaddr].data[word] = mil->reg->DATA[index + word] & 0xFFFF;
                    }
					mil->rxbuf[subaddr].nb_words = wordsCount;
				}
			}
			break;
		// приём данных от ОУ (ОУ-ОУ), формат сообщения 8
		case MSG_DATARECV__RT_RT__GROUP:
			answerWord |= MIL_STD_STATUS_GROUP_COMMAND_RECEIVED;
		// приём данных от ОУ (ОУ-ОУ), формат сообщения 3
		case MSG_DATARECV__RT_RT__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0)
			{
				// Установить ответное слово
				mil->reg->StatusWord1 = answerWord;
			}
			if ((flag & MIL_STD_STATUS_VALMESS) != 0) {
				mil->nb_words += wordsCount;
				int index = MIL_STD_SUBADDR_WORD_INDEX(subaddr);
				if (!mil->rxbuf[subaddr].busy) {
					for (word = 0; word < wordsCount; ++word) {
						mil->rxbuf[subaddr].data[word] = mil->reg->DATA[index + word] & 0xFFFF;
					}
					mil->rxbuf[subaddr].nb_words = wordsCount;
				}
			}
			break;
		// передача данных в КШ (ОУ-КШ), формат сообщения 2
	 	case MSG_DATASEND__RT_BC__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0)
			{
				// Установить ответное слово
				mil->reg->StatusWord1 = answerWord;
				if (!mil->txbuf[subaddr].busy) {
					int index = MIL_STD_SUBADDR_WORD_INDEX(subaddr);
					for (word = 0; word < wordsCount; ++word) {
						mil->reg->DATA[index + word] = mil->txbuf[subaddr].data[word] & 0xFFFF;
					}
				}
			}
			if ((flag & MIL_STD_STATUS_VALMESS) != 0) {
				mil->nb_words += wordsCount;
			}
			break;
		// передача данных в ОУ (ОУ-ОУ), формат сообщения 8
		case MSG_DATASEND__RT_RT__GROUP:
			answerWord |= MIL_STD_STATUS_GROUP_COMMAND_RECEIVED;
		// передача данных в ОУ (ОУ-ОУ), формат сообщения 3
		case MSG_DATASEND__RT_RT__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0)
			{
				// Установить ответное слово
				mil->reg->StatusWord1 = answerWord;
				if (!mil->txbuf[subaddr].busy) {
					int index = MIL_STD_SUBADDR_WORD_INDEX(subaddr);
					for (word = 0; word < wordsCount; ++word)
						mil->reg->DATA[index + word] = mil->txbuf[subaddr].data[word] & 0xFFFF;
				}
			}
			if ((flag & MIL_STD_STATUS_VALMESS) != 0) {
				mil->nb_words += wordsCount;
			}
			break;
		// команда управления 0-15 от КШ без слов данных, формат сообщения 9
		case MSG_CMD_WITH_NODATA__0_0xf__GROUP:
			answerWord |= MIL_STD_STATUS_GROUP_COMMAND_RECEIVED;
		// команда управления 0-15 от КШ без слов данных, формат сообщения 4
		case MSG_CMD_WITH_NODATA__0_0xf__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0)
			{
				// неподдерживаемые команды (резерв)
				if (cmdCode >= 9 && cmdCode <= 0xf)
				{
					answerWord |= MIL_STD_STATUS_MSG_ERR;

					// Установить ответное слово
					mil->reg->StatusWord1 = answerWord;
				}
				else
				{
					// Установить ответное слово
					mil->reg->StatusWord1 = answerWord;
					// Команда "блокировать передатчик"
					if (cmdCode == CMD_LockSender)
					{
						if ((flag & MIL_STD_STATUS_RCVA) != 0)
						{
							mil->reg->CONTROL &= ~MIL_STD_CONTROL_TRB;
						}
						else if ((flag & MIL_STD_STATUS_RCVB) != 0)
							mil->reg->CONTROL &= ~MIL_STD_CONTROL_TRA;
					}
					// Команда "разблокировать передатчик"
					else if (cmdCode == CMD_UnlockSender)
					{
						if ((flag & MIL_STD_STATUS_RCVA) != 0) {
							mil->reg->CONTROL |= MIL_STD_CONTROL_TRB;
						}
						else if ((flag & MIL_STD_STATUS_RCVB) != 0) {
							mil->reg->CONTROL |= MIL_STD_CONTROL_TRA;
						}
					}
					// Команда "установить ОУ в исходное состояние"
					else if (cmdCode == CMD_SetRtInitialState)
					{
					    mil->reg->CONTROL |= MIL_STD_CONTROL_TRA;
					    mil->reg->CONTROL |= MIL_STD_CONTROL_TRB;
					}
				}
			}
			break;
		// команда управления 16-31 от КШ, ожидается слово данных (не групповая), формат сообщения 5
		case MSG_CMD_WITH_NODATA__0x10_0x1f__DATAWORD_EXPECTED__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0)
			{
				// неподдерживаемые команды (резерв)
				if (cmdCode >= 0x16)
				{
					answerWord |= MIL_STD_STATUS_MSG_ERR;

					// Установить ответное слово
					mil->reg->StatusWord1 = answerWord;
				}
				else
				{
					// Установить ответное слово
					mil->reg->StatusWord1 = answerWord;
					// (!) to be implemented
				}
			}
			break;
		// команда управления 16-31 от КШ со словом данных, формат сообщения 10
		case MSG_CMD_WITH_DATA__0x10_0x1f__GROUP:
			answerWord |= MIL_STD_STATUS_GROUP_COMMAND_RECEIVED;
		// команда управления 16-31 от КШ со словом данных, формат сообщения 6
		case MSG_CMD_WITH_DATA__0x10_0x1f__SINGLE:
			// Получено достоверное слово из канала
			if ((flag & MIL_STD_STATUS_RFLAGN) != 0)
			{
				// неподдерживаемые команды (резерв)
				if (cmdCode >= 0x16)
				{
					answerWord |= MIL_STD_STATUS_MSG_ERR;

					// Установить ответное слово
					mil->reg->StatusWord1 = answerWord;
				}
				else
				{
					// Установить ответное слово
					mil->reg->StatusWord1 = answerWord;
					// Команда "синхронизация (со словом данных)"
					if (cmdCode == CMD_SynchronizeWithDataWord)
					{
						// Заменить собственный адрес ОУ значением, которое получено в команде от КШ.
						const unsigned short newCtrl = (mil->reg->CONTROL & ~MIL_STD_CONTROL_ADDR_MASK) | ((mil->reg->ModeData & 0x1F)<<MIL_STD_CONTROL_ADDR_SHIFT);
						mil->reg->CONTROL = newCtrl;
					}
				}
			}
			break;
		} // switch (msg)
	} else {
		if (status_array[write_idx].done) {
			nb_missing++;
		} else {
			status_array[write_idx].status = flag;
			status_array[write_idx].error = reg->ERROR;
			status_array[write_idx].command_word_1 = reg->CommandWord1;
			status_array[write_idx].msg = reg->MSG;
			status_array[write_idx].time_stamp = ARM_TIMER2->TIM_CNT;
			status_array[write_idx].done = 1;
			write_idx = (write_idx+1>=STATUS_ITEMS_SIZE?0:write_idx+1);
		}
	}

out:
	if (reg == ARM_MIL_STD_1553B1) {
		arch_intr_allow(MIL_STD_1553B1_IRQn);
	} else {
		arch_intr_allow(MIL_STD_1553B2_IRQn);
	}

    return 1;
}

static int mil_bc_urgent_send(mil1553if_t *_mil, mil_slot_desc_t descr, void *data)
{
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    mil_lock(_mil);
    mil->urgent_desc = descr;
    mil->urgent_desc.reserve = 0;
    if (descr.command.req_pattern == 0 || descr.command.req_pattern == 0x1f) {
        // Команда управления
    } else {
        if (descr.transmit_mode == MIL_SLOT_BC_RT) {
            int wc = (descr.words_count == 0 ? 32 : descr.words_count);
            memcpy(mil->urgent_data, data, 2*wc);
        }
    }
    mil_unlock(_mil);
    return MIL_ERR_OK;
}

static int mil_bc_ordinary_send(mil1553if_t *_mil, int slot_index, void *data)
{
	milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    mil_lock(_mil);
	mil_slot_t *s = mil->cyclogram;
	if (s == 0) {
		mil_unlock(_mil);
		return MIL_ERR_NOT_PERM;
	}

	while (slot_index) {
		s = s->next;
		if (s == 0) {
			mil_unlock(_mil);
			return MIL_ERR_NOT_PERM;
		}
		slot_index--;
	}
	int wc = (s->desc.words_count == 0 ? 32 : s->desc.words_count);
	uint16_t *dst = s->data;
	uint16_t *slot_data = data;
	while(wc) {
		*dst++ = *slot_data++;
		wc--;
	}

	mil_unlock(_mil);

	return MIL_ERR_OK;
}

static mutex_t status_lock;     // формальный mutex, не должен запрещатся

void mil_rt_send(mil1553if_t *_mil, int subaddr, void *data, int nb_words)
{
	milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
	mil->txbuf[subaddr].busy = 1;
	memcpy(mil->txbuf[subaddr].data, data, nb_words * 8);
	mil->txbuf[subaddr].nb_words = nb_words;
	mil->txbuf[subaddr].busy = 0;
}

void mil_rt_receive(mil1553if_t *_mil, int subaddr, void *data, int *nb_words)
{
	milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
	mil->rxbuf[subaddr].busy = 1;
	*nb_words = mil->rxbuf[subaddr].nb_words;
	memcpy(data, mil->rxbuf[subaddr].data, *nb_words * 8);
	mil->rxbuf[subaddr].busy = 0;
}

void mil_rt_send_16(mil1553if_t *_mil, int subaddr, void *data, int nb_words)
{
	milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
	mil->txbuf[subaddr].busy = 1;
	uint16_t *src = data;
	volatile uint32_t *dst = mil->txbuf[subaddr].data;
	int i;
	for(i=0;i<nb_words;i++) {
		*dst++ = *src++;
	}
	mil->txbuf[subaddr].nb_words = nb_words;
	mil->txbuf[subaddr].busy = 0;
}

void mil_rt_receive_16(mil1553if_t *_mil, int subaddr, void *data, int *nb_words)
{
	milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;
	mil->rxbuf[subaddr].busy = 1;
	*nb_words = mil->rxbuf[subaddr].nb_words;
	int i;

	//debug_printf("nb_words = %d", mil->rxbuf[subaddr].nb_words);
    //
	//for (i=0;i<mil->rxbuf[subaddr].nb_words;i++) {
	//	debug_printf(" %04x", mil->txbuf[subaddr].data[i] & 0xffff);
	//}
	//debug_printf("\n");

	uint16_t *dst = data;
	uint32_t *src = mil->txbuf[subaddr].data;
	for(i=0;i<*nb_words;i++) {
		*dst++ = (*src++) & 0xffff;
	}
	mil->rxbuf[subaddr].busy = 0;
}

void milandr_mil1553_init(milandr_mil1553_t *_mil, int port, mem_pool_t *pool, unsigned nb_rxq_msg, TIMER_t *timer)
{
	extern uint32_t mask_intr_disabled;
    milandr_mil1553_t *mil = (milandr_mil1553_t *)_mil;

    if (port == 0) {
        mil->reg = ARM_MIL_STD_1553B1;
        mil->irq = MIL_STD_1553B1_IRQn;
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_MIL_STD_1553B1;
    } else {
        mil->reg = ARM_MIL_STD_1553B2;
        mil->irq = MIL_STD_1553B2_IRQn;
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_MIL_STD_1553B2;
    }
    // Разрешение тактовой частоты на контроллер ГОСТ Р52070-2003
#if MIL_STD_CLOCK_DIV==1
    ARM_RSTCLK->ETH_CLOCK |= ARM_ETH_CLOCK_MAN_BRG(0) | ARM_ETH_CLOCK_MAN_EN;// частота делится на 1
#elif MIL_STD_CLOCK_DIV==2
    ARM_RSTCLK->ETH_CLOCK |= ARM_ETH_CLOCK_MAN_BRG(1) | ARM_ETH_CLOCK_MAN_EN;// частота делится на 2
#elif MIL_STD_CLOCK_DIV==4
    ARM_RSTCLK->ETH_CLOCK |= ARM_ETH_CLOCK_MAN_BRG(2) | ARM_ETH_CLOCK_MAN_EN;// частота делится на 4
#elif MIL_STD_CLOCK_DIV==8
    ARM_RSTCLK->ETH_CLOCK |= ARM_ETH_CLOCK_MAN_BRG(3) | ARM_ETH_CLOCK_MAN_EN;// частота делится на 8
#else
#error  MIL_STD_CLOCK_DIV not valid
#endif

    mil->pool = pool;
    if (nb_rxq_msg) {
        mem_queue_init(&mil->cyclogram_rxq, pool, nb_rxq_msg);
        mem_queue_init(&mil->urgent_rxq, pool, nb_rxq_msg);
//        mem_queue_init(&mil->rt_rxq, pool, nb_rxq_msg);
    }
    
    mil->tim_reg = timer;
    if (timer == ARM_TIMER1) {
        mil->tim_irq = TIMER1_IRQn;
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_TIMER1;
        ARM_RSTCLK->TIM_CLOCK |= ARM_TIM_CLOCK_BRG1(0) | ARM_TIM_CLOCK_EN1;
    } else if (timer == ARM_TIMER2) {
        mil->tim_irq = TIMER2_IRQn;
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_TIMER2;
        ARM_RSTCLK->TIM_CLOCK |= ARM_TIM_CLOCK_BRG2(0) | ARM_TIM_CLOCK_EN2;
    } else if (timer == ARM_TIMER3) {
        mil->tim_irq = TIMER3_IRQn;
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_TIMER3;
        ARM_RSTCLK->TIM_CLOCK |= ARM_TIM_CLOCK_BRG3(0) | ARM_TIM_CLOCK_EN3;
    } else if (timer == ARM_TIMER4) {
        mil->tim_irq = TIMER4_IRQn;
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_TIMER4;
        ARM_RSTCLK->UART_CLOCK |= ARM_UART_CLOCK_TIM4_BRG(0) | ARM_UART_CLOCK_TIM4_EN;
    }
    
    mil->cyclogram = 0;
    mil->cur_slot = 0;
    mil->is_running = 0;
    mil->mode = MIL_MODE_UNDEFINED;
    mil->period_ms = 0;
    
    mil->milif.set_mode = mil_set_mode;
    mil->milif.start = mil_start;
    mil->milif.stop = mil_stop;
    mil->milif.is_started = mil_is_started;
    mil->milif.read_subaddr = mil_read_subaddr;
    mil->milif.write_subaddr = mil_write_subaddr;
    mil->milif.lock = mil_lock;
    mil->milif.unlock = mil_unlock;
    mil->milif.bc_set_cyclogram = mil_bc_set_cyclogram;
    mil->milif.bc_set_period = mil_bc_set_period;
    mil->milif.bc_urgent_send = mil_bc_urgent_send;
    mil->milif.bc_ordinary_send = mil_bc_ordinary_send;


    // IRQ 1 (MIL_STD_1553B1_IRQn) никогда не запрещается
	mask_intr_disabled = ~(1<<MIL_STD_1553B1_IRQn);
    ARM_NVIC_IPR(MIL_STD_1553B1_IRQn/4) = 0x40400040;
    mutex_attach_irq(&status_lock, MIL_STD_1553B1_IRQn, status_handler, mil);

    if (timer) {
        timer->TIM_CNTRL = 0;
        timer->TIM_CNT = 0;
        timer->TIM_PSG = 0;

        mutex_attach_irq(&mil->tim_lock, mil->tim_irq, timer_handler, mil);
    }
}

void milandr_mil1553_init_pins(int port)
{
    if ((port & 1) == 0)
    {
        milandr_init_pin(ARM_GPIOC, PORT_C, 13, FUNC_MAIN);  // PRMA+
        milandr_init_pin(ARM_GPIOC, PORT_C, 14, FUNC_MAIN);  // PRMA-
        milandr_init_pin(ARM_GPIOD, PORT_D,  1, FUNC_MAIN);  // PRDA+
        milandr_init_pin(ARM_GPIOD, PORT_D,  2, FUNC_MAIN);  // PRDA-
        milandr_init_pin(ARM_GPIOD, PORT_D,  5, FUNC_MAIN);  // PRD_PRM_A

        milandr_init_pin(ARM_GPIOC, PORT_C, 15, FUNC_MAIN);  // PRMB+
        milandr_init_pin(ARM_GPIOD, PORT_D,  0, FUNC_MAIN);  // PRMB-
        milandr_init_pin(ARM_GPIOD, PORT_D,  3, FUNC_MAIN);  // PRDB+
        milandr_init_pin(ARM_GPIOD, PORT_D,  4, FUNC_MAIN);  // PRDB-
        milandr_init_pin(ARM_GPIOD, PORT_D,  6, FUNC_MAIN);  // PRD_PRM_B
    }
    else
    {
        milandr_init_pin(ARM_GPIOF, PORT_F,  3, FUNC_MAIN);  // PRMC+
        milandr_init_pin(ARM_GPIOF, PORT_F,  4, FUNC_MAIN);  // PRMC-
        milandr_init_pin(ARM_GPIOF, PORT_F,  7, FUNC_MAIN);  // PRDC+
        milandr_init_pin(ARM_GPIOF, PORT_F,  8, FUNC_MAIN);  // PRDC-
        milandr_init_pin(ARM_GPIOF, PORT_F, 11, FUNC_MAIN);  // PRD_PRM_C

        milandr_init_pin(ARM_GPIOF, PORT_F,  5, FUNC_MAIN);  // PRMD+
        milandr_init_pin(ARM_GPIOF, PORT_F,  6, FUNC_MAIN);  // PRMD-
        milandr_init_pin(ARM_GPIOF, PORT_F,  9, FUNC_MAIN);  // PRDD+
        milandr_init_pin(ARM_GPIOF, PORT_F, 10, FUNC_MAIN);  // PRDD-
        milandr_init_pin(ARM_GPIOF, PORT_F, 12, FUNC_MAIN);  // PRD_PRM_D
    }
}
