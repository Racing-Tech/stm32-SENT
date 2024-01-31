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

uint8_t SENTRx_init(SENTRxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel, SENTRxCallback_t rx_callback,  SENTRxCallback_t slow_rx_callback) {
    if(handle == NULL || htim == NULL)
        return 0;

    SENT_init(&handle->base, htim, channel, 0);

    handle->rx_callback = rx_callback;
    handle->slow_rx_callback = slow_rx_callback;
    handle->slow_channel_buffer_bit2 = 0;
    handle->slow_channel_buffer_bit3 = 0;
    handle->capture = 0;
    handle->state = SENTRX_STATE_IDLE;

    __HAL_TIM_SET_AUTORELOAD(handle->base.htim, TIM_GET_MAX_AUTORELOAD(handle->base.htim));
    __HAL_TIM_CLEAR_FLAG(handle->base.htim, TIM_FLAG_UPDATE);

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
    uint32_t oldCapture;
    uint8_t ticks;
    if(handle->base.htim->Channel == SENTRx_getActiveChannel(handle->base.channel)) {
        oldCapture = handle->capture;
        handle->capture = HAL_TIM_ReadCapturedValue(handle->base.htim, handle->base.channel);
        ticks = SENT_TIM_TO_TICKS(handle->capture - oldCapture, handle->base.tim_to_tick_ratio);

        switch(handle->state) {
            case SENTRX_STATE_IDLE:
                handle->state = SENTRX_STATE_SYNC;
                break;
            case SENTRX_STATE_SYNC:
                handle->base.index = 0;
                handle->base.tim_to_tick_ratio = (handle->capture - oldCapture) / SENT_SYNC_TICKS;
                if(handle->base.tim_to_tick_ratio < TIM_MS_TO_TICKS(handle->base.htim, SENTRX_MIN_TICK_UNIT_MS) || handle->base.tim_to_tick_ratio > TIM_MS_TO_TICKS(handle->base.htim, SENTRX_MAX_TICK_UNIT_MS))
                    handle->state = SENTRX_STATE_SYNC;
                else
                    handle->state = SENTRX_STATE_STATUS;    
                break;
            case SENTRX_STATE_STATUS:
                if(ticks > SENT_MAX_NIBBLE_TICK_COUNT * 1.2) {
                    handle->state = SENTRX_STATE_SYNC;
                } else {
                    handle->msg_buffer.status_nibble = ticks - SENT_MIN_NIBBLE_TICK_COUNT;
                    handle->msg_buffer.data_length = 0;
                    handle->state = SENTRX_STATE_DATA;
                }
                break;
            case SENTRX_STATE_DATA:
                if(handle->base.index - 2 < 7) {
                    if(handle->base.index - 3 >= 0)
                        handle->msg_buffer.data_nibbles[handle->base.index-3] = handle->msg_buffer.crc;
                    handle->msg_buffer.crc = ticks - SENT_MIN_NIBBLE_TICK_COUNT;
                    ++handle->msg_buffer.data_length;
                    handle->state = SENTRX_STATE_DATA;
                    break;
                }
                
                if(ticks > SENT_MAX_NIBBLE_TICK_COUNT * 1.2) {
                    handle->state = SENTRX_STATE_SYNC;
                } else {
                    handle->state = SENTRX_STATE_IDLE;
                }

                --handle->msg_buffer.data_length;
                if(SENT_calc_crc(&handle->msg_buffer) == handle->msg_buffer.crc && handle->rx_callback) {
                    SENTRx_SlowChannelFSM(handle, handle->msg_buffer.status_nibble);
                    handle->rx_callback(handle);
                }
                break;
            default:
                break;
        }
        ++handle->base.index;
    }

}