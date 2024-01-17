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

uint8_t SENTRx_init(SENTRxHandle_t *const handle, TIM_HandleTypeDef *const htim, uint32_t channel, SENTRxCallback_t rx_callback,  SENTRxCallback_t slow_rx_callback, GPIO_TypeDef *output_port, uint32_t output_pin) {
    if(handle == NULL || htim == NULL)
        return 0;

    handle->base.status = SENT_READY;
    handle->base.index = 0;
    handle->base.htim = htim;
    handle->base.channel = channel;
    handle->rx_callback = rx_callback;
    handle->slow_rx_callback = slow_rx_callback;
    handle->captures[0] = 0;
    handle->captures[1] = 0;
    handle->state = SENTRX_STATE_IDLE;
    handle->output_port = output_port;
    handle->output_pin = output_pin;
    handle->base.slow_channel_status = SENT_SLOW_IDLE;
    handle->slow_channel_buffer_bit2 = 0;
    handle->slow_channel_buffer_bit2 = 0;
    handle->base.slow_channel_index = 0;

    __HAL_TIM_SET_AUTORELOAD(handle->base.htim, TIM_GET_MAX_AUTORELOAD(handle->base.htim));
    __HAL_TIM_CLEAR_FLAG(handle->base.htim, TIM_FLAG_UPDATE);

    HAL_TIM_IC_Start_IT(handle->base.htim, handle->base.channel);
    
    return 1;
}

uint8_t SENTRx_getRxMessage(SENTRxHandle_t *const handle, SENTMsg_t *const message) {
    if(handle == NULL || message == NULL)
        return 0;

    memcpy(message, &handle->message_buffer, sizeof(SENTMsg_t));
    return 1;
}

uint8_t SENTRx_getRxSlowMessage(SENTRxHandle_t *const handle, SENTSlowMsg_t *const message) {
    if(handle == NULL || message == NULL)
        return 0;

    memcpy(message, &handle->slow_message_buffer, sizeof(SENTSlowMsg_t));
    return 1;
}

void SENTRx_ElaborateSlowShort(SENTSlowMsg_t *const msg, uint32_t bit2_data) {
    msg->type = SENT_SLOWTYPE_SHORT;
    msg->as.slow_short.id = (bit2_data >> 12) & 0x0F;
    msg->as.slow_short.data = (bit2_data >> 4) & 0xFF;
    msg->as.slow_short.crc = bit2_data & 0x0F;
}

void SENTRx_ElaborateSlowEnhanced12(SENTSlowMsg_t *const msg, uint32_t bit2_data, uint32_t bit3_data) {
    msg->type = SENT_SLOWTYPE_ENHANCED12;
    msg->as.slow_enhanced.crc = (bit2_data >> 12) & 0x3F;
    msg->as.slow_enhanced.data = bit2_data & 0x0FFF;
    msg->as.slow_enhanced.id = (bit3_data >> 1) & 0x0F;
    msg->as.slow_enhanced.id |= (bit3_data >> 2) & 0xF0;
}

void SENTRx_ElaborateSlowEnhanced16(SENTSlowMsg_t *const msg, uint32_t bit2_data, uint32_t bit3_data) {
    msg->type = SENT_SLOWTYPE_ENHANCED16;
    msg->as.slow_enhanced.crc = (bit2_data >> 16) & 0x3F;
    msg->as.slow_enhanced.data = bit2_data & 0xFFFF;
    msg->as.slow_enhanced.id = (bit3_data >> 1) & 0x0F; // ???????
    msg->as.slow_enhanced.id |= (bit3_data >> 2) & 0xF0;// ???????
}

void SENTRx_SlowChannelFSM(SENTRxHandle_t *const handle, uint8_t status) {
    if(handle == NULL)
        return;
    
    handle->slow_channel_buffer_bit2 <<= 1;
    handle->slow_channel_buffer_bit2 |= SENTRX_STATUS_BIT2(status) ? 1 : 0;
    handle->slow_channel_buffer_bit3 <<= 1;
    handle->slow_channel_buffer_bit3 |= SENTRX_STATUS_BIT3(status) ? 1 : 0;

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
        if(!handle->slow_channel_buffer_bit3) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
        } else if(handle->base.slow_channel_index >= 15) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
            SENTRx_ElaborateSlowShort(&handle->slow_message_buffer, handle->slow_channel_buffer_bit2);
            if(handle->slow_rx_callback)
                handle->slow_rx_callback(handle);
        }
        break;

    case SENT_SLOW_RX_ENHANCED:
        if(handle->base.slow_channel_index == 7 && handle->slow_channel_buffer_bit3)
            handle->base.slow_channel_status = SENT_SLOW_RX_ENHANCED_12;
        else
            handle->base.slow_channel_status = SENT_SLOW_RX_ENHANCED_16;
        break;

    case SENT_SLOW_RX_ENHANCED_12:
        if(handle->base.slow_channel_index >= 17) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
            SENTRx_ElaborateSlowEnhanced12(&handle->slow_message_buffer, handle->slow_channel_buffer_bit2, handle->slow_channel_buffer_bit3);
            if(handle->slow_rx_callback)
                handle->slow_rx_callback(handle);
        }
        break;

    case SENT_SLOW_RX_ENHANCED_16:
        if(handle->base.slow_channel_index >= 21) {
            handle->base.slow_channel_status = SENT_SLOW_IDLE;
            SENTRx_ElaborateSlowEnhanced16(&handle->slow_message_buffer, handle->slow_channel_buffer_bit2, handle->slow_channel_buffer_bit3);
            if(handle->slow_rx_callback)
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
    if(handle->base.htim->Channel == SENTRx_getActiveChannel(handle->base.channel)) {
        if(HAL_GPIO_ReadPin(handle->output_port, handle->output_pin) == GPIO_PIN_SET) {
            handle->captures[1] = HAL_TIM_ReadCapturedValue(handle->base.htim, handle->base.channel);
            return;
        }
        oldCapture = handle->captures[0];
        handle->captures[0] = HAL_TIM_ReadCapturedValue(handle->base.htim, handle->base.channel);

        switch(handle->state) {
            case SENTRX_STATE_IDLE:
                handle->state = SENTRX_STATE_SYNC;
                break;
            case SENTRX_STATE_SYNC:
                handle->base.index = 0;
                handle->tick_unit_time = TIM_TICKS_TO_MS(handle->base.htim, (handle->captures[0] - handle->captures[1])) / SENT_SYNC_TICKS;
                if(handle->tick_unit_time < SENTRX_MIN_TICK_UNIT_MS || handle->tick_unit_time > SENTRX_MAX_TICK_UNIT_MS)
                    handle->state = SENTRX_STATE_SYNC;
                else
                    handle->state = SENTRX_STATE_STATUS;    
                break;
            case SENTRX_STATE_STATUS:
                if(handle->captures[0] - oldCapture > SENT_TICKS_TO_TIM(handle->base.htim, SENT_MAX_NIBBLE_TICK_COUNT * 1.2, handle->tick_unit_time)) {
                    handle->state = SENTRX_STATE_SYNC;
                } else {
                    handle->message_buffer.status_nibble = SENT_TIM_TO_TICKS(handle->base.htim ,handle->captures[0] - oldCapture, handle->tick_unit_time) - SENT_MIN_NIBBLE_TICK_COUNT;
                    handle->message_buffer.data_length = 0;
                    SENTRx_SlowChannelFSM(handle, handle->message_buffer.status_nibble);
                    handle->state = SENTRX_STATE_DATA;
                }
                break;
            case SENTRX_STATE_DATA:
                if(handle->captures[0] - oldCapture > SENT_TICKS_TO_TIM(handle->base.htim, SENT_MAX_NIBBLE_TICK_COUNT * 1.2, handle->tick_unit_time)) {
                    --handle->message_buffer.data_length;
                    handle->state = SENTRX_STATE_SYNC;
                    if(handle->rx_callback)
                        handle->rx_callback(handle);
                } else if(handle->base.index - 2 < 7) {
                    if(handle->base.index - 3 >= 0)
                        handle->message_buffer.data_nibbles[handle->base.index-3] = handle->message_buffer.crc;
                    handle->message_buffer.crc = SENT_TIM_TO_TICKS(handle->base.htim, handle->captures[0] - oldCapture, handle->tick_unit_time) - SENT_MIN_NIBBLE_TICK_COUNT;
                    ++handle->message_buffer.data_length;
                    handle->state = SENTRX_STATE_DATA;
                } else {
                    --handle->message_buffer.data_length;
                    handle->state = SENTRX_STATE_IDLE;
                    if(handle->rx_callback)
                        handle->rx_callback(handle);
                }
                break;
            default:
                break;
        }
        ++handle->base.index;
    }

}