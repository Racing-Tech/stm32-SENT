#ifndef SENT_TX_H
#define SENT_TX_H

#include "sent.h"

#define SENTTX_NIBBLE_LOW_TICKS   5
#define SENTTX_SYNC_TICKS         (SENT_SYNC_TICKS+SENTTX_NIBBLE_LOW_TICKS)
#define SENTTX_PAUSE_TICKS        (SENT_PAUSE_TICKS+SENTTX_NIBBLE_LOW_TICKS)

typedef struct SENTTxHandle SENTTxHandle_t;
typedef void (*SENTTxCallback_t)(SENTTxHandle_t *const handle);

typedef struct SENTTxHandle {
    SENTHandle_t base;
    SENTMsg_t *msg_source;
    SENTTxCallback_t msg_tx_callback;
    SENTSlowMsg_t *slow_msg_source;
    SENTTxCallback_t slow_msg_tx_callback;
    SENTPhysMsg_t msg_buffer[2];
    uint8_t msg_buffer_index;
} SENTTxHandle_t;

uint8_t SENTTx_init(SENTTxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel, SENTMsg_t *const msg_source, SENTTxCallback_t msg_tx_callback, SENTSlowMsg_t *const slow_msg_source, SENTTxCallback_t slow_msg_tx_callback, float tick_unit_time);
uint8_t SENTTx_start(SENTTxHandle_t *const handle);
void SENTTx_AutoReloadCallback(SENTTxHandle_t *const handle);
void SENTTx_CompareCallback(SENTTxHandle_t * const handle);

#endif // SENT_TX_H
