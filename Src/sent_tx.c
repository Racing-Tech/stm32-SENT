#include "sent_tx.h"

uint8_t SENTTx_init(SENTTxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel) {
    if(handle == NULL || htim == NULL)
        return 0;

    handle->base.status = SENT_READY;
    handle->base.index = 0;
    handle->base.htim = htim;
    handle->base.channel = channel;

    return 1;
}

uint8_t SENTTx_send(SENTTxHandle_t *const handle, SENTMsg_t *const message) {
    if(handle == NULL || message == NULL)
        return 0;

    if(handle->base.status != SENT_READY)
        return 0;

    handle->base.status = SENT_TX;
    handle->base.index = 0;
    handle->message = message;

    __HAL_TIM_SET_COMPARE(handle->base.htim, handle->base.channel, SENTTX_TICKS_TO_TIM(handle->base.htim, SENTTX_NIBBLE_LOW_TICKS));
    HAL_TIM_PWM_Start_IT(handle->base.htim, handle->base.channel);

    return 1;
}

void SENTTx_AutoReloadCallback(SENTTxHandle_t *const handle) {
    if(handle == NULL)
        return;

    if(handle->base.index == handle->message->data_length + 4) {
        HAL_TIM_PWM_Stop_IT(handle->base.htim, handle->base.channel);
        handle->base.status = SENT_READY;
    }
}

void SENTTx_CompareCallback(SENTTxHandle_t * const handle) {
    if(handle == NULL)
        return;
    
    switch (handle->base.index)
    {
    case 0: //sync
        __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENTTX_SYNC_TICKS));
        break;
    case 1: //status
        __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENT_NIBBLE_MASK(handle->message->status_nibble)+12));
        break;
    default:
        if(handle->base.index < handle->message->data_length + 2) { //data nibble
            __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENT_NIBBLE_MASK(handle->message->data_nibbles[handle->base.index - 2])+12));
        } else if(handle->base.index > handle->message->data_length + 2) {
            __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENTTX_PAUSE_TICKS));
        } else {
            __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENT_NIBBLE_MASK(handle->message->crc)+12));
        }
        break;
    }

    ++handle->base.index;
}