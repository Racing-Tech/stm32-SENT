#ifndef SENT_RX_H
#define SENT_RX_H

#include "sent.h"

#define SENTRX_STATUS_BIT3(status) ((status) & 0x8)
#define SENTRX_STATUS_BIT2(status) ((status) & 0x4)

#define SENTRX_TICK_TOLERANCE   0.2
#define SENTRX_MIN_TICK_UNIT_MS (SENT_MIN_TICK_UNIT_MS*(1-SENTRX_TICK_TOLERANCE))
#define SENTRX_MAX_TICK_UNIT_MS (SENT_MAX_TICK_UNIT_MS*(1+SENTRX_TICK_TOLERANCE))

typedef struct SENTRxHandle SENTRxHandle_t;
typedef void (*SENTRxCallback_t)(SENTRxHandle_t *const handle);

typedef enum {SENTRX_STATE_IDLE, SENTRX_STATE_SYNC, SENTRX_STATE_STATUS, SENTRX_STATE_DATA, SENTRX_STATE_PAUSE} SENTRxState_t;

typedef struct SENTRxHandle {
    SENTHandle_t base;
    uint32_t capture;
    SENTRxState_t state;
    SENTMsg_t msg_buffer;
    SENTSlowMsg_t slow_message_buffer;
    SENTRxCallback_t rx_callback;
    SENTRxCallback_t slow_rx_callback;
    uint32_t slow_channel_buffer_bit2;
    uint32_t slow_channel_buffer_bit3;
} SENTRxHandle_t;

uint8_t SENTRx_init(SENTRxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel, SENTRxCallback_t rx_callback, SENTRxCallback_t slow_rx_callback);
uint8_t SENTRx_getRxMessage(SENTRxHandle_t *const handle, SENTMsg_t *const msg);
uint8_t SENTRx_getRxSlowMessage(SENTRxHandle_t *const handle, SENTSlowMsg_t *const msg);
void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle);

#endif // SENT_RX_H
