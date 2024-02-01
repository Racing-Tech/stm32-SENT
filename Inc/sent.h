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

#define SENT_CRC4_SEED 0x05
#define SENT_CRC6_SEED 0x15

#define SENT_NIBBLE_MASK(data)  ((data) & 0x0f)
#define SENT_STATUS_MASK(status)  ((status) & 0x03)
#define SENT_TICKS_TO_TIM(ticks, unit) ((ticks) * (unit))
#define SENT_TIM_TO_TICKS(ticks, unit) ((ticks) / (unit))

typedef enum {SENT_READY, SENT_TX, SENT_RX, SENT_NOT_READY} SENTHandleStatus_t;
typedef enum {SENT_SLOWTYPE_SHORT, SENT_SLOWTYPE_ENHANCED12, SENT_SLOWTYPE_ENHANCED16} SENTSlowMsgType_t;
typedef enum {SENT_SLOW_IDLE, SENT_SLOW_RX, SENT_SLOW_RX_SHORT, SENT_SLOW_RX_ENHANCED, SENT_SLOW_RX_ENHANCED_12, SENT_SLOW_RX_ENHANCED_16, SENT_SLOW_TX} SENTSlowStatus_t;

typedef struct {
    uint8_t status_nibble;
    uint8_t data_nibbles[6];
    uint8_t crc;
    uint8_t data_length;
} SENTMsg_t;

typedef struct {
    SENTSlowMsgType_t type;
    union {
        struct {
            uint16_t crc :4;
            uint16_t data :8;
            uint16_t id   :4;
        } slow_short;
        struct {
            uint32_t crc;
            uint32_t data;
            uint8_t id;
        } slow_enhanced;
    } as;
} SENTSlowMsg_t;

typedef struct {
    uint32_t ticks[10];
    uint8_t length;
} SENTPhysMsg_t;

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t has_pause;
    uint16_t message_tick_len;
    uint8_t data_length;
    double tick_unit_time;
} SENTHandleInit_t;

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t has_pause;
    uint16_t message_tick_len;
    uint8_t nibble_count;
    SENTHandleStatus_t status;
    uint8_t index;
    SENTSlowStatus_t slow_channel_status;
    uint8_t slow_channel_index;
    uint32_t tim_to_tick_ratio;
} SENTHandle_t;

uint8_t SENT_init(SENTHandle_t *const handle, SENTHandleInit_t *const init);
void SENT_msg_init(SENTMsg_t *const msg, uint8_t status_nibble, uint8_t *const data_nibbles, uint8_t data_length);
uint8_t SENT_calc_crc(SENTMsg_t *const msg);
uint8_t SENT_calc_crc_slow(SENTSlowMsg_t *const msg);
void SENT_encodePhysMsg(SENTHandle_t *const handle, SENTPhysMsg_t *const dest, SENTMsg_t *const src, uint32_t tim_to_tick_ratio);
void SENT_decodePhysMsg(SENTHandle_t *const handle, SENTMsg_t *const dest, SENTPhysMsg_t *const src, uint32_t tim_to_tick_ratio);

#endif // SENT_H
