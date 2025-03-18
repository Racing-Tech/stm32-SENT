#include "sent_tx.h"

uint8_t SENTTx_init(SENTTxHandle_t *const handle, SENTHandleInit_t *const init, SENTMsg_t *const msg_source, SENTTxCallback_t msg_tx_callback, SENTSlowMsg_t *const slow_msg_source, SENTTxCallback_t slow_msg_tx_callback) {
    if(handle == NULL || init == NULL || init->htim == NULL)
        return 0;

    SENT_init(&handle->base, init);

    handle->msg_source = msg_source;
    handle->msg_tx_callback = msg_tx_callback;
    handle->slow_msg_source = slow_msg_source;
    handle->slow_msg_tx_callback = slow_msg_tx_callback;
    handle->msg_buffer_index = 0;

    return 1;
}

void _load_dma_buf(SENTTxHandle_t *const handle) {
    for(int i=0; i<handle->msg_buffer.length; ++i) {
        if(IS_TIM_32B_COUNTER_INSTANCE(handle->base.htim))
            handle->dma_buffer.u32[i] = handle->msg_buffer.ticks[i];
        else
            handle->dma_buffer.u16[i] = handle->msg_buffer.ticks[i];
    }
}

uint8_t SENTTx_start(SENTTxHandle_t *const handle) {
    if(handle == NULL)
        return 0;

    if(handle->base.status != SENT_READY)
        return 0;

    handle->base.status = SENT_TX;
    handle->base.index = 0;
    SENT_encodePhysMsg(&handle->base, &handle->msg_buffer, handle->msg_source, handle->base.tim_to_tick_ratio);

    if(handle->slow_msg_source != NULL) {
        handle->base.slow_channel_status = SENT_SLOW_TX;
    }

    __HAL_TIM_SET_COMPARE(handle->base.htim, handle->base.channel, SENT_TICKS_TO_TIM(SENTTX_NIBBLE_LOW_TICKS, handle->base.tim_to_tick_ratio));
    __HAL_TIM_SET_COUNTER(handle->base.htim, 0);
    __HAL_TIM_ENABLE_IT(handle->base.htim, TIM_IT_UPDATE);

    _load_dma_buf(handle);

    HAL_TIM_Base_Start_DMA(handle->base.htim, handle->dma_buffer.u32, handle->msg_buffer.length);
    HAL_TIM_PWM_Start_IT(handle->base.htim, handle->base.channel);


    return 1;
}

void SENTTx_SlowChannelFSM(SENTTxHandle_t *const handle, SENTPhysMsg_t *const msg, SENTSlowMsg_t *const slow_msg) {
    uint32_t status_ticks = SENT_STATUS_MASK(SENT_TIM_TO_TICKS(msg->ticks[1], handle->base.tim_to_tick_ratio)-SENT_MIN_NIBBLE_TICK_COUNT);
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
            if(handle->base.slow_channel_index == 0)
                status_ticks |= (1 << 3);
            
            status_ticks |= ((*((uint16_t*)(&slow_msg->as.slow_short)) >> (15-handle->base.slow_channel_index)) << 2) & 0x4;

            status_ticks += SENT_MIN_NIBBLE_TICK_COUNT;

            if(handle->base.has_pause)
                msg->ticks[msg->length-1] -= SENT_TICKS_TO_TIM(status_ticks, handle->base.tim_to_tick_ratio) - msg->ticks[1];
            msg->ticks[1] = SENT_TICKS_TO_TIM(status_ticks, handle->base.tim_to_tick_ratio);

            if(handle->base.slow_channel_index >= 15) {
                handle->base.slow_channel_index = 0;
                handle->base.slow_channel_status = SENT_SLOW_TX;
                if(handle->slow_msg_tx_callback)
                    handle->slow_msg_tx_callback(handle);
                return;
            }

            break;
        case SENT_SLOWTYPE_ENHANCED16:
        case SENT_SLOWTYPE_ENHANCED12:
            if(handle->base.slow_channel_index < 6)
                status_ticks |= (1 << 3);
            else if(handle->base.slow_channel_index == 7)
                status_ticks |= slow_msg->type == SENT_SLOWTYPE_ENHANCED12 ? 0 : (1<<3);
            else if(handle->base.slow_channel_index >= 8 && handle->base.slow_channel_index < 12) {
                if(slow_msg->type == SENT_SLOWTYPE_ENHANCED12)
                    status_ticks |= ((slow_msg->as.slow_enhanced.id >> (7+8-handle->base.slow_channel_index)) << 3) & 0x8;
                else
                    status_ticks |= ((slow_msg->as.slow_enhanced.id >> (3+8-handle->base.slow_channel_index)) << 3) & 0x8;
            }
            else if(handle->base.slow_channel_index >= 13 && handle->base.slow_channel_index < 17) {
                if(slow_msg->type == SENT_SLOWTYPE_ENHANCED12)
                    status_ticks |= ((slow_msg->as.slow_enhanced.id >> (3+13-handle->base.slow_channel_index)) << 3) & 0x8;
                else
                    status_ticks |= ((slow_msg->as.slow_enhanced.data >> (15+13-handle->base.slow_channel_index)) << 3) & 0x8;
            }
            
            if(handle->base.slow_channel_index < 6)
                status_ticks |= ((slow_msg->as.slow_enhanced.crc >> (5-handle->base.slow_channel_index)) << 2) & 0x4;
            else
                status_ticks |= ((slow_msg->as.slow_enhanced.data >> (11+6-handle->base.slow_channel_index)) << 2) & 0x4;

            status_ticks += SENT_MIN_NIBBLE_TICK_COUNT;

            if(handle->base.has_pause)
                msg->ticks[msg->length-1] -= SENT_TICKS_TO_TIM(status_ticks, handle->base.tim_to_tick_ratio) - msg->ticks[1];
            msg->ticks[1] = SENT_TICKS_TO_TIM(status_ticks, handle->base.tim_to_tick_ratio);

            if(handle->base.slow_channel_index >= 17) {
                handle->base.slow_channel_index = 0;
                handle->base.slow_channel_status = SENT_SLOW_TX;
                if(handle->slow_msg_tx_callback)
                    handle->slow_msg_tx_callback(handle);
                return;
            }
            break;
        }
        ++handle->base.slow_channel_index;
        break;
    }
}

void SENTTx_AutoReloadCallback(SENTTxHandle_t *const handle) {
    if(handle == NULL)
        return;
    
    if(handle->base.index >= handle->msg_buffer.length) {
        handle->base.index = 0;
        if(handle->msg_tx_callback)
            handle->msg_tx_callback(handle);
        handle->msg_buffer_index = !handle->msg_buffer_index;
    }

    if(handle->base.index == 0) {
        if(handle->slow_msg_source)
            SENTTx_SlowChannelFSM(handle, &handle->msg_buffer, handle->slow_msg_source);
            
        _load_dma_buf(handle);

        SENT_encodePhysMsg(&handle->base, &handle->msg_buffer, handle->msg_source, handle->base.tim_to_tick_ratio);
    }
}

void SENTTx_CompareCallback(SENTTxHandle_t *const handle) {
    if(handle == NULL)
        return;
    
    ++handle->base.index;
}
