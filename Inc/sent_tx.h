#ifndef SENT_TX_H
#define SENT_TX_H

#include "sent.h"

#define SENTTX_TICK_UNIT_MS       0.003
#define SENTTX_NIBBLE_LOW_TICKS   5
#define SENTTX_SYNC_TICKS         (SENT_SYNC_TICKS+SENTTX_NIBBLE_LOW_TICKS)
#define SENTTX_PAUSE_TICKS        (SENT_PAUSE_TICKS+SENTTX_NIBBLE_LOW_TICKS)

#define SENTTX_TICKS_TO_TIM(tim, ticks) ((ticks) * TIM_MS_TO_TICKS((tim), SENTTX_TICK_UNIT_MS))

typedef struct {
    SENTHandle_t base;
    SENTMsg_t *message;
} SENTTxHandle_t;

uint8_t SENTTx_init(SENTTxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel);
uint8_t SENTTx_send(SENTTxHandle_t *const handle, SENTMsg_t *const message);
void SENTTx_AutoReloadCallback(SENTTxHandle_t *const handle);
void SENTTx_CompareCallback(SENTTxHandle_t * const handle);

#endif // SENT_TX_H
