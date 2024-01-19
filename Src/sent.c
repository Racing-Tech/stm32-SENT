#include "sent.h"
#include <string.h>

const uint8_t crc4_table[] = {0, 13, 7, 10, 14, 3, 9, 4, 1, 12, 6, 11, 15, 2, 8, 5};
const uint8_t crc6_table[] = {0, 25, 50, 43, 61, 36, 15, 22, 35, 58, 17, 8, 30, 7, 44, 53,
                      31, 6, 45, 52, 34, 59, 16, 9, 60, 37, 14, 23, 1, 24, 51, 42,
                      62, 39, 12, 21, 3, 26, 49, 40, 29, 4, 47, 54, 32, 57, 18, 11,
                      33, 56, 19, 10, 28, 5, 46, 55, 2, 27, 48, 41, 63, 38, 13, 20};

uint8_t SENT_calc_crc4_raw(uint8_t *const msg, uint8_t len) {
    uint8_t crc = SENT_CRC4_SEED;

    for(uint8_t i=0; i<len; ++i) {
        crc = msg[i] ^ crc4_table[crc]; /* XOR-in the next input byte */
    }

    crc = 0x00 ^ crc4_table[crc];

    return crc & 0x0F;
}

uint8_t SENT_calc_crc6_raw(uint8_t *const msg, uint8_t len) {
    uint8_t crc = SENT_CRC6_SEED;

    for(uint8_t i=0; i<len; ++i) {
        crc = msg[i] ^ crc6_table[crc]; /* XOR-in the next input byte */
    }

    crc = 0x00 ^ crc6_table[crc];

    return crc & 0x3F;
}

uint8_t SENT_calc_crc(SENTMsg_t *const msg) {
    return SENT_calc_crc4_raw(msg->data_nibbles, msg->data_length);
}
uint8_t SENT_calc_crc_slow(SENTSlowMsg_t *const msg) {
    uint8_t tmp[4] = {0};

    switch (msg->type)
    {
    case SENT_SLOWTYPE_SHORT:
        tmp[0] = msg->as.slow_short.id;
        tmp[1] = msg->as.slow_short.data >> 4;
        tmp[2] = msg->as.slow_short.data & 0xF;
        return SENT_calc_crc4_raw(tmp, 3);
    case SENT_SLOWTYPE_ENHANCED12:
    case SENT_SLOWTYPE_ENHANCED16:
        uint8_t bit;
        for(uint8_t i=0; i<24; ++i) {
            if(i==1 || i==13 || i==23)
                bit = 0;
            else if(i==3)
                bit = msg->type == SENT_SLOWTYPE_ENHANCED12 ? 0 : 1;
            else if(i%2 == 0)
                bit = (msg->as.slow_enhanced.data >> (11-(i/2))) & 1;
            else {
                if(msg->type == SENT_SLOWTYPE_ENHANCED12) {
                    if(i<15)
                        bit = (msg->as.slow_enhanced.id >> (7-((i-5)/2))) & 1;
                    else
                        bit = (msg->as.slow_enhanced.id >> (7-((i-15)/2))) & 1;
                } else {
                    if(i<15)
                        bit = (msg->as.slow_enhanced.id >> (3-((i-5)/2))) & 1;
                    else
                        bit = (msg->as.slow_enhanced.data >> (15-((i-15)/2))) & 1;
                }
            }

            tmp[i/6] |= bit << (5-(i%6));
        }
        return SENT_calc_crc6_raw(tmp, 4);    
    default:
        return -1;
    }
}

void SENT_msg_init(SENTMsg_t *const msg, uint8_t status_nibble, uint8_t *const data_nibbles, uint8_t data_length) {
    if(msg == NULL)
        return;

    if(data_length > 6)
        return;
    
    msg->status_nibble = status_nibble;
    memcpy(msg->data_nibbles, data_nibbles, data_length);
    msg->data_length = data_length;
    msg->crc = SENT_calc_crc(msg);
}


void SENT_encodePhysMsg(SENTHandle_t *const handle, SENTPhysMsg_t *const dest, SENTMsg_t *const src, uint32_t pause_ticks) {
    dest->ticks[0] = SENT_TICKS_TO_TIM(SENT_SYNC_TICKS, handle->tim_to_tick_ratio);
    dest->ticks[1] = SENT_TICKS_TO_TIM(SENT_STATUS_MASK(src->status_nibble)+SENT_MIN_NIBBLE_TICK_COUNT, handle->tim_to_tick_ratio);
    for(uint8_t i=0; i<src->data_length; ++i) {
        dest->ticks[i+2] = SENT_TICKS_TO_TIM(SENT_NIBBLE_MASK(src->data_nibbles[i])+SENT_MIN_NIBBLE_TICK_COUNT, handle->tim_to_tick_ratio);
    }
    dest->ticks[src->data_length+2] = SENT_TICKS_TO_TIM(SENT_NIBBLE_MASK(src->crc)+SENT_MIN_NIBBLE_TICK_COUNT, handle->tim_to_tick_ratio);
    dest->ticks[src->data_length+3] = SENT_TICKS_TO_TIM(pause_ticks, handle->tim_to_tick_ratio);
    dest->length = src->data_length+4;
}

void SENT_decodePhysMsg(SENTHandle_t *const handle, SENTMsg_t *const dest, SENTPhysMsg_t *const src) {
    
}