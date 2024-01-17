#ifndef SENT_H
#define SENT_H

#include "main.h"
#include "timer_utils.h"

#include <inttypes.h>

#define SENT_MIN_TICK_UNIT_MS   0.003
#define SENT_MAX_TICK_UNIT_MS   0.09

#define SENT_MIN_NIBBLE_TICK_COUNT  12
#define SENT_MAX_NIBBLE_TICK_COUNT  27

#define SENT_SYNC_TICKS         56
#define SENT_PAUSE_TICKS        77

#define SENT_CRC_POLY 0x1D
#define SENT_CRC_SEED 0x05

#define SENT_NIBBLE_MASK(data)  ((data) & 0x0f)
#define SENT_TICKS_TO_TIM(tim, ticks, unit) ((ticks) * TIM_MS_TO_TICKS((tim), (unit)))
#define SENT_TIM_TO_TICKS(tim, ticks, unit) (TIM_TICKS_TO_MS((tim), (ticks))/(unit))

typedef enum {SENT_READY, SENT_TX, SENT_RX, SENT_NOT_READY} SENTHandleStatus_t;

typedef struct {
    uint8_t status_nibble;
    uint8_t data_nibbles[6];
    uint8_t crc;
    uint8_t data_length;
} SENTMsg_t;

typedef struct {
    SENTHandleStatus_t status;
    uint8_t index;
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} SENTHandle_t;

void SENT_message_init(SENTMsg_t *const message, uint8_t status_nibble, uint8_t *const data_nibbles, uint8_t data_length);
uint8_t SENT_calc_crc(SENTMsg_t *const message);

#endif // SENT_H
