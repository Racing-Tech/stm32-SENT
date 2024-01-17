#ifndef SENT_RX_H
#define SENT_RX_H

#include "sent.h"

#define SENTRX_TICK_TOLERANCE   0.2
#define SENTRX_MIN_TICK_UNIT_MS (SENT_MIN_TICK_UNIT_MS*(1-SENTRX_TICK_TOLERANCE))
#define SENTRX_MAX_TICK_UNIT_MS (SENT_MAX_TICK_UNIT_MS*(1+SENTRX_TICK_TOLERANCE))

typedef struct SENTRxHandle SENTRxHandle_t;
typedef void (*SENTRxCallback_t)(SENTRxHandle_t *const handle);

typedef enum {SENTRX_STATE_IDLE, SENTRX_STATE_SYNC, SENTRX_STATE_STATUS, SENTRX_STATE_DATA, SENTRX_STATE_PAUSE} SENTRxState_t;

typedef struct SENTRxHandle {
    SENTHandle_t base;
    uint32_t captures[2];
    SENTRxState_t state;
    double tick_unit_time;
    SENTMsg_t message_buffer;
    SENTRxCallback_t rx_callback;
    GPIO_TypeDef *output_port;
    uint32_t output_pin;
} SENTRxHandle_t;

uint8_t SENTRx_init(SENTRxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel, SENTRxCallback_t rx_callback, GPIO_TypeDef *output_port, uint32_t output_pin);
uint8_t SENTRx_getRxMessage(SENTRxHandle_t *const handle, SENTMsg_t *const message);
void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle);

#endif // SENT_RX_H
