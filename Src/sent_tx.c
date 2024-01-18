#include "sent_tx.h"

uint8_t SENTTx_init(SENTTxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel) {
    if(handle == NULL || htim == NULL)
        return 0;

    handle->base.status = SENT_READY;
    handle->base.index = 0;
    handle->base.slow_channel_status = SENT_SLOW_IDLE;
    handle->base.slow_channel_index = 0;
    handle->base.htim = htim;
    handle->base.channel = channel;

    return 1;
}

uint8_t SENTTx_add_slow_msg(SENTTxHandle_t *const handle, SENTSlowMsg_t *const message) {
    if(handle == NULL || message == NULL)
        return 0;

    if(handle->base.slow_channel_status != SENT_SLOW_IDLE)
        return 0;

    handle->base.slow_channel_index = 0;
    handle->base.slow_channel_status = SENT_SLOW_TX;
    handle->slow_message = message;
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

void SENTTx_SlowChannelFSM(SENTTxHandle_t *const handle, SENTMsg_t *const msg, SENTSlowMsg_t *const slow_msg) {
    switch (handle->base.slow_channel_status)
    {
    default:
    case SENT_SLOW_IDLE:
        handle->base.slow_channel_index = 0;
        break;
    case SENT_SLOW_TX:
        switch (slow_msg->type)
        {
        case SENT_SLOWTYPE_SHORT:
            if(handle->base.slow_channel_index >= 16)
                handle->base.slow_channel_status = SENT_SLOW_IDLE;

            msg->status_nibble &= ~(0b1100);

            if(handle->base.slow_channel_index == 0)
                msg->status_nibble |= 1 << 3;
            
            msg->status_nibble |= ((*((uint16_t*)(&slow_msg->as.slow_short)) >> (15-handle->base.slow_channel_index)) << 2) & 0x4;
            break;
        case SENT_SLOWTYPE_ENHANCED12:
            if(handle->base.slow_channel_index >= 18)
                handle->base.slow_channel_status = SENT_SLOW_IDLE;

            msg->status_nibble &= ~(0b1100);

            if(handle->base.slow_channel_index < 6)
                msg->status_nibble |= 1 << 3;
            else if(handle->base.slow_channel_index >= 8 && handle->base.slow_channel_index < 12)
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.id >> (7+7-handle->base.slow_channel_index)) << 2) & 0x8;
            else if(handle->base.slow_channel_index >= 13 && handle->base.slow_channel_index < 17)
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.id >> (3+12-handle->base.slow_channel_index)) << 2) & 0x8;
            
            if(handle->base.slow_channel_index < 6)
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.crc >> (5-handle->base.slow_channel_index)) << 2) & 0x4;
            else
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.data >> (11+6-handle->base.slow_channel_index)) << 2) & 0x4;
            break;
        case SENT_SLOWTYPE_ENHANCED16:
            if(handle->base.slow_channel_index >= 22)
                handle->base.slow_channel_status = SENT_SLOW_IDLE;

            msg->status_nibble &= ~(0b1100);

            if(handle->base.slow_channel_index < 6)
                msg->status_nibble |= 1 << 3;
            else if(handle->base.slow_channel_index >= 8 && handle->base.slow_channel_index < 12)
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.id >> (7+7-handle->base.slow_channel_index)) << 2) & 0x8;
            else if(handle->base.slow_channel_index >= 13 && handle->base.slow_channel_index < 17)
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.id >> (3+12-handle->base.slow_channel_index)) << 2) & 0x8;
            
            if(handle->base.slow_channel_index < 6)
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.crc >> (5-handle->base.slow_channel_index)) << 2) & 0x4;
            else
                msg->status_nibble |= ((slow_msg->as.slow_enhanced.data >> (15+6-handle->base.slow_channel_index)) << 2) & 0x4;
            break;
            break;
        }
        break;
    }
    ++handle->base.slow_channel_index;
}

void SENTTx_CompareCallback(SENTTxHandle_t *const handle) {
    if(handle == NULL)
        return;
    
    switch (handle->base.index)
    {
    case 0: //sync
        __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENTTX_SYNC_TICKS));
        break;
    case 1: //status
        SENTTx_SlowChannelFSM(handle, handle->message, handle->slow_message);
        __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENT_NIBBLE_MASK(handle->message->status_nibble)+SENT_MIN_NIBBLE_TICK_COUNT));
        break;
    default:
        if(handle->base.index < handle->message->data_length + 2) { //data nibble
            __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENT_NIBBLE_MASK(handle->message->data_nibbles[handle->base.index - 2])+SENT_MIN_NIBBLE_TICK_COUNT));
        } else if(handle->base.index > handle->message->data_length + 2) {
            __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENTTX_PAUSE_TICKS));
        } else {
            __HAL_TIM_SET_AUTORELOAD(handle->base.htim, SENTTX_TICKS_TO_TIM(handle->base.htim, SENT_NIBBLE_MASK(handle->message->crc)+SENT_MIN_NIBBLE_TICK_COUNT));
        }
        break;
    }

    ++handle->base.index;
}