#ifndef SENT_RX_H
#define SENT_RX_H
/**
 * @file sent_rx.h
 * @brief SENT Protocol Receiver Header File
 *
 * This file contains the definitions and function prototypes for the SENT receiver module.
 */

#include "sent.h"

/** @brief Extracts bit 3 from the status byte. */
#define SENTRX_STATUS_BIT3(status) ((status) & 0x8)
/** @brief Extracts bit 2 from the status byte. */
#define SENTRX_STATUS_BIT2(status) ((status) & 0x4)

/** @brief Tolerance for SENT tick timing. */
#define SENTRX_TICK_TOLERANCE   0.2
/** @brief Minimum tick unit duration in milliseconds. */
#define SENTRX_MIN_TICK_UNIT_MS (SENT_MIN_TICK_UNIT_MS*(1-SENTRX_TICK_TOLERANCE))
/** @brief Maximum tick unit duration in milliseconds. */
#define SENTRX_MAX_TICK_UNIT_MS (SENT_MAX_TICK_UNIT_MS*(1+SENTRX_TICK_TOLERANCE))

/** @brief SENT receiver state machine states. */
typedef enum {
    SENTRX_STATE_SYNC,   /**< Synchronization state */
    SENTRX_STATE_NIBBLES /**< Nibble reception state */
} SENTRxState_t;

/** @brief SENT receiver handle structure. */
typedef struct SENTRxHandle {
    SENTHandle_t base;                /**< Base SENT handle */
    void *user_data;                   /**< User-defined data pointer */
    uint32_t capture;                  /**< Last captured timer value */
    SENTRxState_t state;               /**< Current receiver state */
    SENTPhysMsg_t msg_buffer;          /**< Buffer for physical messages */
    SENTSlowMsg_t slow_msg_buffer;     /**< Buffer for slow channel messages */
    SENTRxCallback_t rx_callback;      /**< Callback for standard messages */
    SENTRxSlowCallback_t slow_rx_callback; /**< Callback for slow messages */
    uint32_t slow_channel_buffer_bit2; /**< Bit buffer for slow channel (bit 2) */
    uint32_t slow_channel_buffer_bit3; /**< Bit buffer for slow channel (bit 3) */
    uint32_t last_tick_to_tim_ratio;   /**< Last computed tick-to-timer ratio */
} SENTRxHandle_t;

/**
 * @brief Initializes the SENT receiver.
 *
 * @param handle Pointer to the SENT receiver handle.
 * @param init Pointer to the initialization structure.
 * @param rx_callback Callback function for received messages.
 * @param slow_rx_callback Callback function for received slow messages.
 * @param user_data Pointer to user-defined data.
 * @return 1 on success, 0 on failure.
 */
uint8_t SENTRx_init(SENTRxHandle_t *const handle, SENTHandleInit_t *const init, SENTRxCallback_t rx_callback, SENTRxSlowCallback_t slow_rx_callback, void *user_data);

/**
 * @brief Starts the SENT receiver.
 *
 * @param handle Pointer to the SENT receiver handle.
 * @return 1 on success, 0 on failure.
 */
uint8_t SENTRx_start(SENTRxHandle_t *const handle);

/**
 * @brief Input capture callback function for SENT reception.
 *
 * @param handle Pointer to the SENT receiver handle.
 */
void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle);

#endif // SENT_RX_H
