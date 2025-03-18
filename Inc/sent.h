#ifndef SENT_H
#define SENT_H
/**
 * @file sent.h
 * @brief Header file for SENT protocol implementation on STM32.
 *
 * This file contains function declarations and structures for encoding, decoding,
 * and managing SENT (Single Edge Nibble Transmission) messages.
 */

#include "main.h"
#include "timer_utils.h"

#include <math.h>
#include <inttypes.h>

#define SENT_MIN_TICK_UNIT_MS   0.003
#define SENT_MAX_TICK_UNIT_MS   0.09

#define SENT_MIN_NIBBLE_TICK_COUNT  12
#define SENT_MAX_NIBBLE_TICK_COUNT  27

#define SENT_SYNC_TICKS         56
#define SENT_PAUSE_TICKS        77

#define SENT_CRC4_SEED 0x05
#define SENT_CRC6_SEED 0x15

#define SENT_MAX_NIBBLES 8

#define SENT_NIBBLE_MASK(data)  ((data) & 0x0f)
#define SENT_STATUS_MASK(status)  ((status) & 0x03)
#define SENT_TICKS_TO_TIM(ticks, unit) ((uint32_t)roundf((float)(ticks) * (unit)))
#define SENT_TIM_TO_TICKS(ticks, unit) ((uint32_t)roundf((float)(ticks) / (unit)))

typedef enum {SENT_READY, SENT_TX, SENT_RX, SENT_NOT_READY} SENTHandleStatus_t;
typedef enum {SENT_SLOWTYPE_SHORT, SENT_SLOWTYPE_ENHANCED12, SENT_SLOWTYPE_ENHANCED16} SENTSlowMsgType_t;
typedef enum {SENT_SLOW_IDLE, SENT_SLOW_RX, SENT_SLOW_RX_SHORT, SENT_SLOW_RX_ENHANCED, SENT_SLOW_RX_ENHANCED_12, SENT_SLOW_RX_ENHANCED_16, SENT_SLOW_TX} SENTSlowStatus_t;

typedef struct {
    uint8_t status_nibble;
    uint8_t data_nibbles[SENT_MAX_NIBBLES];
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
            uint8_t crc;
            uint16_t data;
            uint8_t id;
        } slow_enhanced;
    } as;
} SENTSlowMsg_t;

typedef struct {
    uint32_t ticks[SENT_MAX_NIBBLES+4];
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

/**
 * @brief Initializes a SENT handle.
 *
 * @param handle Pointer to the SENT handle structure.
 * @param init Pointer to the initialization structure.
 * @return 1 if initialization is successful, 0 otherwise.
 */
uint8_t SENT_init(SENTHandle_t *const handle, SENTHandleInit_t *const init);

/**
 * @brief Initializes a SENT message structure.
 *
 * @param msg Pointer to the SENT message structure.
 * @param status_nibble The status nibble value.
 * @param data_nibbles Pointer to the data nibbles array.
 * @param data_length Number of data nibbles.
 */
void SENT_msg_init(SENTMsg_t *const msg, uint8_t status_nibble, uint8_t *const data_nibbles, uint8_t data_length);

/**
 * @brief Calculates the CRC for a SENT message.
 *
 * @param msg Pointer to the SENT message structure.
 * @return The calculated CRC value.
 */
uint8_t SENT_calc_crc(SENTMsg_t *const msg);

/**
 * @brief Calculates the CRC for a SENT slow message.
 *
 * @param msg Pointer to the SENT slow message structure.
 * @return The calculated CRC value.
 */
uint8_t SENT_calc_crc_slow(SENTSlowMsg_t *const msg);

/**
 * @brief Encodes a SENT message into a physical format.
 *
 * @param handle Pointer to the SENT handle.
 * @param dest Pointer to the physical message structure.
 * @param src Pointer to the SENT message structure.
 * @param tim_to_tick_ratio Conversion ratio from timer to tick.
 */
void SENT_encodePhysMsg(SENTHandle_t *const handle, SENTPhysMsg_t *const dest, SENTMsg_t *const src, uint32_t tim_to_tick_ratio);

/**
 * @brief Decodes a physical SENT message into a logical SENT message.
 *
 * @param handle Pointer to the SENT handle.
 * @param dest Pointer to the SENT message structure.
 * @param src Pointer to the physical message structure.
 * @param tim_to_tick_ratio Conversion ratio from timer to tick.
 */
void SENT_decodePhysMsg(SENTHandle_t *const handle, SENTMsg_t *const dest, SENTPhysMsg_t *const src, uint32_t tim_to_tick_ratio);


#endif // SENT_H
