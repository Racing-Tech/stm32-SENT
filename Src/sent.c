#include "sent.h"
#include <string.h>

uint8_t SENT_calc_crc(uint8_t *const data, uint8_t length) {
    uint8_t crc = SENT_CRC_SEED;

    for(uint8_t i=0; i<length; ++i) {
        crc ^= data[i]; /* XOR-in the next input byte */

        for (int i = 0; i < 4; i++) {
            if ((crc & 0x08) != 0) {
                crc = (crc << 1) ^ SENT_CRC_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc & 0x0F;
}

void SENT_message_init(SENTMsg_t *const message, uint8_t status_nibble, uint8_t *const data_nibbles, uint8_t data_length) {
    if(message == NULL)
        return;

    if(data_length > 6)
        return;
    
    message->status_nibble = status_nibble;
    memcpy(message->data_nibbles, data_nibbles, data_length);
    message->data_length = data_length;
    message->crc = SENT_calc_crc(data_nibbles, data_length);
}