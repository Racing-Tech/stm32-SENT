#include "sent_rx.h"
#include <string.h>

HAL_TIM_ActiveChannel SENTRx_getActiveChannel(uint32_t channel) {
    HAL_TIM_ActiveChannel ch;

    switch(channel) {
        default:
        case TIM_CHANNEL_1:
            ch = HAL_TIM_ACTIVE_CHANNEL_1;
            break;
        case TIM_CHANNEL_2:
            ch = HAL_TIM_ACTIVE_CHANNEL_2;
            break;
        case TIM_CHANNEL_3:
            ch = HAL_TIM_ACTIVE_CHANNEL_3;
            break;
        case TIM_CHANNEL_4:
            ch = HAL_TIM_ACTIVE_CHANNEL_4;
            break;
    }

    return ch;
}

uint8_t SENTRx_init(SENTRxHandle_t *const handle, SENTHandleInit_t *const init, SENTRxCallback_t rx_callback,  SENTRxSlowCallback_t slow_rx_callback) {
    if(handle == NULL || init == NULL || init->htim == NULL)
        return 0;

    SENT_init(&handle->base, init);

    handle->rx_callback = rx_callback;
    handle->slow_rx_callback = slow_rx_callback;
    handle->slow_channel_buffer_bit2 = 0;
    handle->slow_channel_buffer_bit3 = 0;
    handle->capture = 0;
    handle->state = SENTRX_STATE_SYNC;
    handle->last_tick_to_tim_ratio = handle->base.tim_to_tick_ratio;
    
    return 1;
}

uint8_t SENTRx_start(SENTRxHandle_t *const handle) {
    if(handle == NULL)
        return 0;

    if(handle->base.status != SENT_READY)
        return 0;

    __HAL_TIM_SET_AUTORELOAD(handle->base.htim, TIM_GET_MAX_AUTORELOAD(handle->base.htim));
    HAL_TIM_IC_Start_IT(handle->base.htim, handle->base.channel);

    return 1;
}

uint8_t SENTRx_getRxMessage(SENTRxHandle_t *const handle, SENTMsg_t *const msg) {
    if(handle == NULL || msg == NULL)
        return 0;

    memcpy(msg, &handle->msg_buffer, sizeof(SENTMsg_t));
    return 1;
}

uint8_t SENTRx_getRxSlowMessage(SENTRxHandle_t *const handle, SENTSlowMsg_t *const msg) {
    if(handle == NULL || msg == NULL)
        return 0;

    memcpy(msg, &handle->slow_msg_buffer, sizeof(SENTSlowMsg_t));
    return 1;
}

void SENTRx_ElaborateSlowShort(SENTSlowMsg_t *const msg, uint32_t bit2_data) {
    msg->type = SENT_SLOWTYPE_SHORT;
    msg->as.slow_short.id = (bit2_data >> 12) & 0x0F;
    msg->as.slow_short.data = (bit2_data >> 4) & 0xFF;
    msg->as.slow_short.crc = bit2_data & 0x0F;
}

void SENTRx_ElaborateSlowEnhanced(SENTSlowMsg_t *const msg, uint32_t bit2_data, uint32_t bit3_data) {
    if(bit3_data & 0x400)
        msg->type = SENT_SLOWTYPE_ENHANCED16;
    else
        msg->type = SENT_SLOWTYPE_ENHANCED12;

    msg->as.slow_enhanced.crc = (bit2_data >> 12) & 0x3F;
    msg->as.slow_enhanced.data = bit2_data & 0x0FFF;

    if(msg->type == SENT_SLOWTYPE_ENHANCED12) {
        msg->as.slow_enhanced.id = (bit3_data >> 1) & 0x0F;
        msg->as.slow_enhanced.id |= (bit3_data >> 2) & 0xF0;
    } else {
        msg->as.slow_enhanced.data |= (bit3_data << 11) & 0xF000;
        msg->as.slow_enhanced.id = (bit3_data >> 6) & 0xF;
    }
}

void SENTRx_SlowChannelFSM(SENTRxHandle_t *const handle, uint8_t status) {
    if(handle == NULL)
        return;
    
    handle->slow_channel_buffer_bit2 <<= 1UL;
    handle->slow_channel_buffer_bit2 |= SENTRX_STATUS_BIT2(status) ? 1UL : 0UL;
    handle->slow_channel_buffer_bit3 <<= 1UL;
    handle->slow_channel_buffer_bit3 |= SENTRX_STATUS_BIT3(status) ? 1UL : 0UL;

    switch (handle->base.slow_channel_status)
    {
    case SENT_SLOW_IDLE:
        handle->base.slow_channel_index = 0;
        if(handle->slow_channel_buffer_bit3 & 0x1)
            handle->base.slow_channel_status = SENT_SLOW_RX;
        break;
    
    case SENT_SLOW_RX:
        if(handle->slow_channel_buffer_bit3 & 0x1)
            handle->base.slow_channel_status = SENT_SLOW_RX_ENHANCED;
        else
            handle->base.slow_channel_status = SENT_SLOW_RX_SHORT;
        break;

    case SENT_SLOW_RX_SHORT:
        if(handle->slow_channel_buffer_bit3 & 0x1) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
        } else if(handle->base.slow_channel_index >= 15) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
            SENTRx_ElaborateSlowShort(&handle->slow_msg_buffer, handle->slow_channel_buffer_bit2);
            if(SENT_calc_crc_slow(&handle->slow_msg_buffer) == handle->slow_msg_buffer.as.slow_short.crc && handle->slow_rx_callback)
                handle->slow_rx_callback(handle);
        }
        break;

    case SENT_SLOW_RX_ENHANCED:
        if((handle->base.slow_channel_index < 6 && !(handle->slow_channel_buffer_bit3 & 0x1))
            || (handle->base.slow_channel_index == 6 && handle->slow_channel_buffer_bit3 & 0x1)) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
        } else if(handle->base.slow_channel_index >= 17) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
            SENTRx_ElaborateSlowEnhanced(&handle->slow_msg_buffer, handle->slow_channel_buffer_bit2, handle->slow_channel_buffer_bit3);
            if(SENT_calc_crc_slow(&handle->slow_msg_buffer) == handle->slow_msg_buffer.as.slow_enhanced.crc && handle->slow_rx_callback)
                handle->slow_rx_callback(handle);
        }
        break;
    
    default:
        break;
    }

    ++handle->base.slow_channel_index;
}

void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle) {
    SENTMsg_t msg;
    uint32_t oldCapture;
    uint8_t ticks;
    uint32_t new_tick_to_tim_ratio;
    if(handle->base.htim->Channel == SENTRx_getActiveChannel(handle->base.channel)) {
        oldCapture = handle->capture;
        handle->capture = HAL_TIM_ReadCapturedValue(handle->base.htim, handle->base.channel);
        if(oldCapture == 0)
            return;
        ticks = SENT_TIM_TO_TICKS(handle->capture - oldCapture, handle->last_tick_to_tim_ratio);

        switch(handle->state) {
            case SENTRX_STATE_SYNC:
                handle->msg_buffer.length = 0;
                new_tick_to_tim_ratio = (handle->capture - oldCapture) / SENT_SYNC_TICKS;
                if(new_tick_to_tim_ratio < handle->last_tick_to_tim_ratio*63/64 || new_tick_to_tim_ratio > handle->last_tick_to_tim_ratio*65/64)
                    break;
                
                handle->last_tick_to_tim_ratio = new_tick_to_tim_ratio;
                if(handle->last_tick_to_tim_ratio < handle->base.tim_to_tick_ratio * 0.75 || handle->last_tick_to_tim_ratio > handle->base.tim_to_tick_ratio * 1.25)
                    handle->state = SENTRX_STATE_SYNC;
                else
                    handle->state = SENTRX_STATE_NIBBLES;
                break;
            case SENTRX_STATE_NIBBLES:
                if(ticks < SENT_MIN_NIBBLE_TICK_COUNT * 0.8 || ticks > SENT_MAX_NIBBLE_TICK_COUNT * 1.2) {
                    handle->state = SENTRX_STATE_SYNC;
                }
                break;
            default:
                break;
        }

        handle->msg_buffer.ticks[handle->msg_buffer.length++] = handle->capture - oldCapture;

        if(handle->msg_buffer.length == handle->base.nibble_count) {
            SENT_decodePhysMsg(&handle->base, &msg, &handle->msg_buffer, handle->last_tick_to_tim_ratio);
            if(SENT_calc_crc(&msg) == msg.crc){
                SENTRx_SlowChannelFSM(handle, msg.status_nibble);
                if(handle->rx_callback)
                    handle->rx_callback(handle, &msg);
            }
            handle->state = SENTRX_STATE_SYNC;
        }
    }

}