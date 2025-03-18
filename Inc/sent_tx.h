#ifndef SENT_TX_H
#define SENT_TX_H

#include "sent.h"

#define SENTTX_NIBBLE_LOW_TICKS   5
#define SENTTX_SYNC_TICKS         (SENT_SYNC_TICKS+SENTTX_NIBBLE_LOW_TICKS)
#define SENTTX_PAUSE_TICKS        (SENT_PAUSE_TICKS+SENTTX_NIBBLE_LOW_TICKS)

/**
 * @brief SENT Transmit Handle structure.
 */
typedef struct SENTTxHandle SENTTxHandle_t;

/**
 * @brief Callback function type for SENT transmission events.
 * @param handle Pointer to the SENTTxHandle_t structure.
 */
typedef void (*SENTTxCallback_t)(SENTTxHandle_t *const handle);

/**
 * @brief Structure representing a SENT transmitter handle.
 */
typedef struct SENTTxHandle {
    SENTHandle_t base;                /**< Base SENT handle. */
    SENTMsg_t *msg_source;            /**< Pointer to the message source. */
    SENTTxCallback_t msg_tx_callback; /**< Callback function for message transmission. */
    SENTSlowMsg_t *slow_msg_source;   /**< Pointer to the slow message source. */
    SENTTxCallback_t slow_msg_tx_callback; /**< Callback function for slow message transmission. */
    SENTPhysMsg_t msg_buffer;         /**< Buffer for storing the physical message. */
    union {
        uint16_t u16[SENT_MAX_NIBBLES+4]; /**< DMA buffer for 16-bit timers. */
        uint32_t u32[SENT_MAX_NIBBLES+4]; /**< DMA buffer for 32-bit timers. */
    } dma_buffer;
    uint8_t msg_buffer_index;         /**< Index for tracking the message buffer. */
} SENTTxHandle_t;

/**
 * @brief Initializes a SENT transmitter handle.
 * 
 * @param handle Pointer to the SENTTxHandle_t structure.
 * @param init Pointer to the SENTHandleInit_t initialization structure.
 * @param msg_source Pointer to the SENT message source.
 * @param msg_tx_callback Callback function for message transmission.
 * @param slow_msg_source Pointer to the SENT slow message source.
 * @param slow_msg_tx_callback Callback function for slow message transmission.
 * 
 * @return 1 on success, 0 on failure.
 */
uint8_t SENTTx_init(SENTTxHandle_t *const handle, SENTHandleInit_t *const init, SENTMsg_t *const msg_source, SENTTxCallback_t msg_tx_callback, SENTSlowMsg_t *const slow_msg_source, SENTTxCallback_t slow_msg_tx_callback);

/**
 * @brief Starts the SENT transmission.
 * 
 * @param handle Pointer to the SENTTxHandle_t structure.
 * 
 * @return 1 on success, 0 on failure.
 */
uint8_t SENTTx_start(SENTTxHandle_t *const handle);

/**
 * @brief Callback function for handling auto-reload events during transmission.
 * 
 * @param handle Pointer to the SENTTxHandle_t structure.
 */
void SENTTx_AutoReloadCallback(SENTTxHandle_t *const handle);

/**
 * @brief Callback function for handling compare events during transmission.
 * 
 * @param handle Pointer to the SENTTxHandle_t structure.
 */
void SENTTx_CompareCallback(SENTTxHandle_t * const handle);

#endif // SENT_TX_H
