#include <runtime/lib.h>
#include <kernel/uos.h>
#include "mcp2515.h"
#include "mcp2515-private.h"


static int do_reset(mcp2515_t *mcp2515)
{
    mcp2515->msg.word_count = 1;
    mcp2515->dataout[0] = CMD_RESET;
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK)
        return CAN_ERR_IO;
    else
        return CAN_ERR_OK;
}

int mcp2515_reset(canif_t *can)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    
    mutex_lock(&can->lock);
    int res = do_reset(mcp2515);
    mutex_unlock(&can->lock);        
    
    return res;
}

int mcp2515_set_timing(canif_t *can, unsigned baud_rate, unsigned timing)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    
    mutex_lock(&can->lock);
    
    mcp2515->baud_rate = baud_rate;
    if (timing == 0) {
        mcp2515->timing = CAN_TIMING_SET_PRS(2) | CAN_TIMING_SET_PS1(2) | 
            CAN_TIMING_SET_PS2(3) | CAN_TIMING_SET_SJW(1);
    } else {
        mcp2515->timing = timing;
    }
    
    mutex_unlock(&can->lock);
    return CAN_ERR_OK;
}

static void translate_id_to_mcp_format(uint32_t id, mcp2515_frame_t *mcp_frame)
{
    if (id & CAN_ID_EXT) {
        mcp_frame->eid0 = id;
        mcp_frame->eid8 = id >> 8;
        mcp_frame->sidl = ((id >> 16) & 0x3) | EXIDE | ((id >> 13) & 0xE0);
        mcp_frame->sidh = id >> 21;
    } else {
        mcp_frame->eid0 = 0;
        mcp_frame->eid8 = 0;
        mcp_frame->sidl = id << 5;
        mcp_frame->sidh = id >> 3;
    }
}

static void translate_to_mcp_frame(const can_frame_t *frame, mcp2515_frame_t *mcp_frame)
{
    translate_id_to_mcp_format(frame->id, mcp_frame);
    mcp_frame->dlc = frame->dlc;
    if (frame->id & CAN_ID_RTR)
        mcp_frame->dlc |= RTR;
    memcpy(mcp_frame->data, frame->data, frame->dlc);
}

static void translate_to_can_frame(const mcp2515_frame_t *mcp_frame, can_frame_t *frame)
{
    if (mcp_frame->sidl & EXIDE) {
        frame->id = mcp_frame->eid0 | (mcp_frame->eid8 << 8) | ((mcp_frame->sidl & 0x3) << 16) |
            ((mcp_frame->sidl & 0xE0) << 13) | (mcp_frame->sidh << 21) | CAN_ID_EXT;
        if (mcp_frame->dlc & RTR)
            frame->id |= CAN_ID_RTR;
    } else {
        frame->id = (mcp_frame->sidh << 3) | (mcp_frame->sidl >> 5);
        if (mcp_frame->sidl & SRR)
            frame->id |= CAN_ID_RTR;
    }
    frame->dlc = mcp_frame->dlc & LEN_MASK;
    memcpy(frame->data, mcp_frame->data, frame->dlc);
}

static int update_flags(mcp2515_t *mcp2515)
{
    mcp2515->msg.word_count = 3;
    mcp2515->dataout[0] = CMD_READ;
    mcp2515->dataout[1] = CANINTF;
    mcp2515->dataout[2] = 0;
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK)
        return CAN_ERR_IO;
        
    mcp2515->int_flags |= mcp2515->datain[2];

    mcp2515->msg.word_count = 4;
    mcp2515->dataout[0] = CMD_BIT_MODIFY;
    mcp2515->dataout[1] = CANINTF;
    mcp2515->dataout[2] = mcp2515->int_flags & (TXnIF(0) | TXnIF(1) | TXnIF(2) | RXnIF(0) | RXnIF(1));
    mcp2515->dataout[3] = ~(mcp2515->int_flags & (TXnIF(0) | TXnIF(1) | TXnIF(2) | RXnIF(0) | RXnIF(1)));
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK)
        return CAN_ERR_IO;

    return CAN_ERR_OK;
}
    
int mcp2515_output(canif_t *can, const can_frame_t *buffer, int nb_of_frames)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    int nb_free;
    int busy_buf;
    int i;
    int res;
    int cur_buf = 0;
    
    mutex_lock(&can->lock);
    
    // Алгоритм должен выдавать кадры в том же порядке, в каком они были
    // переданы в драйвер, при этом обеспечить максимальную загрузку линии.
    
    while (cur_buf < nb_of_frames) {
        // 1. Обновляем статусы (свободен/занят) буферов выдачи.
        // Ждём, когда хотя бы 2 буфера будут свободны.
        do {
            res = update_flags(mcp2515);
            if (res != CAN_ERR_OK) {
                mutex_unlock(&can->lock);
                return res;
            }
            
            nb_free = 0;
            busy_buf = NB_TX_BUFFERS;
            for (i = 0; i < NB_TX_BUFFERS; ++i) {
                if ((mcp2515->int_flags & TXnIF(i)))
                    ++nb_free;
                else busy_buf = i;
            }
        } while (nb_free < 2);
        
        // 2. Повышаем приоритет занятого буфера до максимального
        mcp2515->msg.word_count = 4;
        mcp2515->dataout[0] = CMD_BIT_MODIFY;
        mcp2515->dataout[1] = TXBnCTRL(busy_buf);
        mcp2515->dataout[2] = TXP_MASK;
        mcp2515->dataout[3] = TXP_MAX;
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
        
        // 3. Загружаем свободные буферы. Их приоритеты присваиваются
        // в соответствии с их номером.
        for (i = 0; i < NB_TX_BUFFERS; ++i) {
            if (mcp2515->int_flags & TXnIF(i)) {
                mcp2515_frame_t mcp_frame;
                translate_to_mcp_frame(&buffer[cur_buf], &mcp_frame);
                
                mcp2515->msg.word_count = 1 + sizeof(mcp_frame);
                mcp2515->dataout[0] = CMD_LOAD_TXBnSIDH(i);
                memcpy(mcp2515->dataout + 1, &mcp_frame, sizeof(mcp_frame));
                if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
                    mutex_unlock(&can->lock);
                    return CAN_ERR_IO;
                }
                
                mcp2515->msg.word_count = 3;
                mcp2515->dataout[0] = CMD_WRITE;
                mcp2515->dataout[1] = TXBnCTRL(i);
                mcp2515->dataout[2] = TXREQ | TXP(TXP_MAX - i);
                if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
                    mutex_unlock(&can->lock);
                    return CAN_ERR_IO;
                }
                
                mcp2515->int_flags &= ~TXnIF(i);
                
                ++cur_buf;
                if (cur_buf == nb_of_frames)
                    break;
            }
        }
    }    
    
    mutex_unlock(&can->lock);
    return CAN_ERR_OK;
}

static int read_buffer(mcp2515_t *mcp2515, int buf_n, mcp2515_frame_t *mcp_frame)
{
    mcp2515->msg.word_count = 1 + sizeof(mcp2515_frame_t);
    mcp2515->dataout[0] = CMD_READ_RXBnSIDH(buf_n);
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK)
        return CAN_ERR_IO;

    memcpy(mcp_frame, mcp2515->datain + 1, sizeof(mcp2515_frame_t));

    mcp2515->int_flags &= ~RXnIF(buf_n);
    
    return CAN_ERR_OK;
}

int mcp2515_input(canif_t *can, can_frame_t *buffer, int nb_of_frames)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    static mcp2515_frame_t saved_frame;
    static int there_is_saved_frame = 0;
    int cur_buf = 0;
    int res;
    int i;
    
    if (nb_of_frames <= 0)
        return CAN_ERR_BAD_PARAM;
    
    mutex_lock(&can->lock);
    
    // Алгоритм должен выдавать на верхний уровень кадры в том же порядке, 
    // в каком они были приняты из линии. Это означает, что нужно считать 
    // сначала RXB1 как более приоритетный, сбросить флаг занятости RXB1,
    // считать повторно флаги, и если появился флаг занятости RXB0 считать
    // его. Считывать RXB0, если он готов, нужно даже если верхнему уровню
    // не нужен сейчас кадр (считываем во временный буфер и отдаём верхнему
    // уровню при следующем запросе в первую очередь).
    // В спецификации написано, что RXB0 имеет приоритет на RXB1, но 
    // практика показывает обратное...
    
    if (there_is_saved_frame) {
        translate_to_can_frame(&saved_frame, &buffer[0]);
        ++cur_buf;
        there_is_saved_frame = 0;
    }

    while (cur_buf < nb_of_frames) {
        for (i = NB_RX_BUFFERS-1; i >= 0; --i) {
            res = update_flags(mcp2515);
            if (res != CAN_ERR_OK) {
                mutex_unlock(&can->lock);
                return res;
            }
            
            if (mcp2515->int_flags & RXnIF(i)) {
                res = read_buffer(mcp2515, i, &saved_frame);
                if (res != CAN_ERR_OK) {
                    mutex_unlock(&can->lock);
                    return res;
                }

                if (cur_buf == nb_of_frames)
                    there_is_saved_frame = 1;
                else
                    translate_to_can_frame(&saved_frame, &buffer[cur_buf++]);
            }
        }
    }
    
    mutex_unlock(&can->lock);
    return CAN_ERR_OK;
}

static int switch_to_configuration_mode(mcp2515_t *mcp2515)
{
    // Переход в режим конфигурирования
    mcp2515->msg.word_count = 4;
    mcp2515->dataout[0] = CMD_BIT_MODIFY;
    mcp2515->dataout[1] = CANCTRL;
    mcp2515->dataout[2] = REQOP_MASK;
    mcp2515->dataout[3] = REQOP(OPMOD_CONFIG);
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK)
        return CAN_ERR_IO;
    
    return CAN_ERR_OK;
}

static int back_to_current_mode(mcp2515_t *mcp2515)
{
    // Возврат в текущий режим
    mcp2515->msg.word_count = 4;
    mcp2515->dataout[0] = CMD_BIT_MODIFY;
    mcp2515->dataout[1] = CANCTRL;
    mcp2515->dataout[2] = REQOP_MASK;
    if (mcp2515->cur_mode == IFACE_MODE_OPERATION)
        mcp2515->dataout[3] = REQOP(OPMOD_NORMAL);
    else if (mcp2515->cur_mode == IFACE_MODE_SLEEP || mcp2515->cur_mode == IFACE_MODE_SHUTDOWN)
        mcp2515->dataout[3] = REQOP(OPMOD_SLEEP);
        
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK)
        return CAN_ERR_IO;

    return CAN_ERR_OK;
}

int mcp2515_add_filter(canif_t *can, uint32_t mask, uint32_t pattern)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    mcp2515_frame_t mask_frame;
    mcp2515_frame_t pattern_frame;
    int res;
    
    mutex_lock(&can->lock);
    
    translate_id_to_mcp_format(mask, &mask_frame);
    translate_id_to_mcp_format(pattern, &pattern_frame);
    
    res = switch_to_configuration_mode(mcp2515);
    if (res != CAN_ERR_OK) {
        mutex_unlock(&can->lock);
        return res;
    }
    
    // Устанавливаем одинаковые шаблоны (другие режиме не поддерживаем)
    mcp2515->msg.word_count = 6;
    mcp2515->dataout[0] = CMD_WRITE;
    memcpy(&mcp2515->dataout[2], &pattern_frame, 4);
    int i;
    for (i = 0; i < NB_FILTERS; ++i) {
        mcp2515->dataout[1] = RXFnSIDH(i);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
    }
    
    // Устанавливаем одинаковые маски
    memcpy(&mcp2515->dataout[2], &mask_frame, 4);
    for (i = 0; i < NB_MASKS; ++i) {
        mcp2515->dataout[1] = RXMnSIDH(i);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
    }

    // Включение режима фильтрования    
    mcp2515->msg.word_count = 4;
    mcp2515->dataout[0] = CMD_BIT_MODIFY;
    mcp2515->dataout[2] = RXM_MASK;
    mcp2515->dataout[3] = RXM_SID_AND_EID;
    for (i = 0; i < NB_RX_BUFFERS; ++i) {
        mcp2515->dataout[1] = RXBnCTRL(i);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
    }
    
    res = back_to_current_mode(mcp2515);
    if (res != CAN_ERR_OK) {
        mutex_unlock(&can->lock);
        return res;
    }
   
    mutex_unlock(&can->lock);
    return CAN_ERR_OK;
}

int mcp2515_clear_filters(canif_t *can)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    int res;
    
    mutex_lock(&can->lock);
    
    res = switch_to_configuration_mode(mcp2515);
    if (res != CAN_ERR_OK) {
        mutex_unlock(&can->lock);
        return res;
    }
    
    // Выключение режима фильтрования    
    mcp2515->msg.word_count = 4;
    mcp2515->dataout[0] = CMD_BIT_MODIFY;
    mcp2515->dataout[2] = RXM_MASK;
    mcp2515->dataout[3] = RXM_OFF;
    
    int i;
    for (i = 0; i < NB_RX_BUFFERS; ++i) {
        mcp2515->dataout[1] = RXBnCTRL(i);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
    }
    
    res = back_to_current_mode(mcp2515);
    if (res != CAN_ERR_OK) {
        mutex_unlock(&can->lock);
        return res;
    }
    
    mutex_unlock(&can->lock);
    return CAN_ERR_OK;
}

int mcp2515_get_state(canif_t *can)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    
    mutex_lock(&can->lock);
    
    mcp2515->msg.word_count = 3;
    mcp2515->dataout[0] = CMD_READ;
    mcp2515->dataout[1] = EFLG;
    mcp2515->dataout[2] = 0;
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
        mutex_unlock(&can->lock);
        return CAN_ERR_IO;
    }
    
    mutex_unlock(&can->lock);
        
    unsigned flags = mcp2515->datain[2];
    if (flags & TXBO)
        return CAN_BUS_OFF;
    else if ((flags & TXEP) || (flags & RXEP))
        return CAN_ERROR_PASSIVE;
    else
        return CAN_ERROR_ACTIVE;
}

int mcp2515_switch_mode(canif_t *can, int mode)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    
    mutex_lock(&can->lock);
    
    switch (mode) {
    case IFACE_MODE_SHUTDOWN:
    case IFACE_MODE_SLEEP:
    {
        // Перевод в режим сна
        mcp2515->msg.word_count = 4;
        mcp2515->dataout[0] = CMD_BIT_MODIFY;
        mcp2515->dataout[1] = CANCTRL;
        mcp2515->dataout[2] = OPMOD_MASK;
        mcp2515->dataout[3] = REQOP(OPMOD_SLEEP);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
            
        break;
    }

    case IFACE_MODE_OPERATION:
    {
        unsigned prs = CAN_TIMING_GET_PRS(mcp2515->timing);
        unsigned ps1 = CAN_TIMING_GET_PS1(mcp2515->timing);
        unsigned ps2 = CAN_TIMING_GET_PS2(mcp2515->timing);
        unsigned sjw = CAN_TIMING_GET_SJW(mcp2515->timing);
        unsigned brp;
        int res;
        
        // Инициализация контроллера
        res = do_reset(mcp2515);
        if (res != CAN_ERR_OK) {
            mutex_unlock(&can->lock);
            return res;
        }
        
        mcp2515->int_flags = TXnIF(0) | TXnIF(1) | TXnIF(2);
        
        mcp2515->msg.word_count = 3;
        mcp2515->dataout[0] = CMD_WRITE;
        
        brp = mcp2515->osc_hz / (2 * mcp2515->baud_rate * (1 + prs + ps1 + ps2));
        mcp2515->dataout[1] = CNF1;
        mcp2515->dataout[2] = BRP(brp-1) | SJW(sjw-1);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
        
        mcp2515->dataout[1] = CNF2;
        mcp2515->dataout[2] = BTLMODE | PHSEG1(ps1-1) | PRSEG(prs-1);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }

        mcp2515->dataout[1] = CNF3;
        mcp2515->dataout[2] = PHSEG2(ps2-1);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }
        
        mcp2515->msg.word_count = 4;
        mcp2515->dataout[0] = CMD_BIT_MODIFY;
        mcp2515->dataout[1] = CANCTRL;
        mcp2515->dataout[2] = OPMOD_MASK;
        mcp2515->dataout[3] = REQOP(OPMOD_NORMAL);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&can->lock);
            return CAN_ERR_IO;
        }

        break;
    }
    default:
        mutex_unlock(&can->lock);
        return CAN_ERR_BAD_PARAM;
    }
    
    mcp2515->cur_mode = mode;
    mutex_unlock(&can->lock);
    return CAN_ERR_OK;
}

int mcp2515_get_mode(canif_t *can)
{
    mcp2515_t *mcp2515 = (mcp2515_t *)can;
    return mcp2515->cur_mode;
}

int mcp2515_init(mcp2515_t *mcp2515, spimif_t *spi, int spi_cs_num, unsigned spi_freq_hz, unsigned osc_hz)
{
    mcp2515->cur_mode = IFACE_MODE_SHUTDOWN;
    mcp2515->osc_hz = osc_hz;
    mcp2515->spi = spi;
    mcp2515->msg.freq = spi_freq_hz;
    mcp2515->msg.mode = SPI_MODE_CS_NUM(spi_cs_num) | SPI_MODE_NB_BITS(8);
    mcp2515->msg.tx_data = mcp2515->dataout;
    mcp2515->msg.rx_data = mcp2515->datain;
    
    canif_t *can = to_canif(mcp2515);
    can->set_timing = mcp2515_set_timing;
    can->output = mcp2515_output;
    can->input = mcp2515_input;
    can->add_filter = mcp2515_add_filter;
    can->clear_filters = mcp2515_clear_filters;
    can->get_state = mcp2515_get_state;
    can->switch_mode = mcp2515_switch_mode;
    can->get_mode = mcp2515_get_mode;
    can->reset = mcp2515_reset;
    
    return CAN_ERR_OK;
}

int mcp2515_enable_gpio(mcp2515_t *mcp2515, mcp2515_pin_t pin, int set_value)
{
    if (pin >= MCP2515_NB_OF_PINS || pin < 0)
        return CAN_ERR_BAD_PARAM;
        
    if (pin == MCP2515_PIN_RX0BF || pin == MCP2515_PIN_RX1BF) {
    
        mutex_lock(&mcp2515->canif.lock);
        
        int pin_index = pin - MCP2515_PIN_RX0BF;
        mcp2515->msg.word_count = 4;
        mcp2515->dataout[0] = CMD_BIT_MODIFY;
        mcp2515->dataout[1] = BFPCTRL;
        mcp2515->dataout[2] = BnBFS(pin_index) | BnBFE(pin_index) | BnBFM(pin_index);
        mcp2515->dataout[3] = BnBFE(pin_index);
        if (set_value)
            mcp2515->dataout[3] |= BnBFS(pin_index);
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&mcp2515->canif.lock);
            return CAN_ERR_IO;
        }
        
        mutex_unlock(&mcp2515->canif.lock);
    }
    
    return CAN_ERR_OK;
}

int mcp2515_disable_gpio(mcp2515_t *mcp2515, mcp2515_pin_t pin)
{
    if (pin >= MCP2515_NB_OF_PINS || pin < 0)
        return CAN_ERR_BAD_PARAM;
    
    if (pin == MCP2515_PIN_RX0BF || pin == MCP2515_PIN_RX1BF) {
        
        mutex_lock(&mcp2515->canif.lock);
        
        int pin_index = pin - MCP2515_PIN_RX0BF;
        mcp2515->msg.word_count = 4;
        mcp2515->dataout[0] = CMD_BIT_MODIFY;
        mcp2515->dataout[1] = BFPCTRL;
        mcp2515->dataout[2] = BnBFE(pin_index);
        mcp2515->dataout[3] = 0;
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&mcp2515->canif.lock);
            return CAN_ERR_IO;
        }
        
        mutex_unlock(&mcp2515->canif.lock);
    }
    
    return CAN_ERR_OK;
}

int mcp2515_gpio_set_value(mcp2515_t *mcp2515, mcp2515_pin_t pin, int val)
{
    if (pin != MCP2515_PIN_RX0BF && pin != MCP2515_PIN_RX1BF)
        return CAN_ERR_BAD_PARAM;
        
    mutex_lock(&mcp2515->canif.lock);

    int pin_index = pin - MCP2515_PIN_RX0BF;
    mcp2515->msg.word_count = 4;
    mcp2515->dataout[0] = CMD_BIT_MODIFY;
    mcp2515->dataout[1] = BFPCTRL;
    mcp2515->dataout[2] = BnBFS(pin_index);
    if (val)
        mcp2515->dataout[3] = BnBFS(pin_index);
    else
        mcp2515->dataout[3] = 0;
        
    if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
        mutex_unlock(&mcp2515->canif.lock);
        return CAN_ERR_IO;
    }
    
    mutex_unlock(&mcp2515->canif.lock);
    return CAN_ERR_OK;
}

int mcp2515_gpio_get_value(mcp2515_t *mcp2515, mcp2515_pin_t pin)
{
    if (pin >= MCP2515_NB_OF_PINS || pin < 0)
        return CAN_ERR_BAD_PARAM;
        
    int result = 0;
    
    mutex_lock(&mcp2515->canif.lock);
        
    if (pin == MCP2515_PIN_RX0BF || pin == MCP2515_PIN_RX1BF) {
        mcp2515->msg.word_count = 3;
        mcp2515->dataout[0] = CMD_READ;
        mcp2515->dataout[1] = BFPCTRL;
        mcp2515->dataout[2] = 0;
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&mcp2515->canif.lock);
            return CAN_ERR_IO;
        }

        int pin_index = pin - MCP2515_PIN_RX0BF;        
        if (mcp2515->datain[2] & BnBFS(pin_index))
            result = 1;
    } else {
        mcp2515->msg.word_count = 3;
        mcp2515->dataout[0] = CMD_READ;
        mcp2515->dataout[1] = TXRTSCTRL;
        mcp2515->dataout[2] = 0;
        if (spim_trx(mcp2515->spi, &mcp2515->msg) != SPI_ERR_OK) {
            mutex_unlock(&mcp2515->canif.lock);
            return CAN_ERR_IO;
        }

        int pin_index = pin - MCP2515_PIN_TX0RTS;        
        if (mcp2515->datain[2] & BnRTS(pin_index))
            result = 1;
    }
    
    mutex_unlock(&mcp2515->canif.lock);
    
    return result;
}

