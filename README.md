# SENTRx - SENT Protocol Receiver

## Overview
SENTRx is a library for receiving and decoding data using the SENT (Single Edge Nibble Transmission) protocol. It is implemented in C and designed to work with STM32 microcontrollers using the HAL library for timer input capture.

## Features
- Supports decoding SENT fast and slow channel messages.
- Implements input capture via STM32 HAL timers.
- Provides callback mechanisms for received messages.
- Supports both standard and enhanced slow channel messages.
- Includes automatic synchronization and error checking via CRC validation.

## File Structure
- `sent_rx.h` - Header file containing the SENTRx API, data structures, and macros.
- `sent_rx.c` - Implementation of the SENTRx receiver functions.

## SENTRx Data Structures

### `SENTRxHandle_t`
A handle structure containing:
- Base SENT handle (`SENTHandle_t`)
- User data pointer
- Capture timing data
- Receiver state machine
- Message buffers for fast and slow channels
- Callback functions for received messages
- Buffers for slow channel bit collection

### Callbacks
- `SENTRxCallback_t` - Called when a fast channel message is received.
- `SENTRxSlowCallback_t` - Called when a slow channel message is received.

## API Functions

### `uint8_t SENTRx_init(SENTRxHandle_t *const handle, SENTHandleInit_t *const init, SENTRxCallback_t rx_callback, SENTRxSlowCallback_t slow_rx_callback, void *user_data);`
Initializes the SENT receiver with the given timer and callbacks.

### `uint8_t SENTRx_start(SENTRxHandle_t *const handle);`
Starts the SENT receiver by enabling timer input capture.

### `void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle);`
Handles incoming pulse capture events, processes nibbles, and triggers the state machine.

## SENT Protocol Handling
### Synchronization
- The receiver synchronizes based on a predefined pulse width corresponding to the SENT sync frame.

### Fast Channel Processing
- Nibbles are collected and converted into a full message.
- CRC validation is performed before invoking the callback.

### Slow Channel Processing
- Uses a state machine to collect slow channel data bits.
- Supports both short and enhanced slow messages.

## Usage Example
```c
void SENT_MessageCallback(SENTMsg_t *const msg, void *user_data) {
    // Process fast channel message
}

void SENT_SlowMessageCallback(SENTSlowMsg_t *const msg, void *user_data) {
    // Process slow channel message
}

SENTRxHandle_t sent_rx;
SENTHandleInit_t init = { .htim = &htim1, .channel = TIM_CHANNEL_1 };

if (SENTRx_init(&sent_rx, &init, SENT_MessageCallback, SENT_SlowMessageCallback, NULL)) {
    SENTRx_start(&sent_rx);
}
```

## Dependencies
- STM32 HAL library (for timer handling)
- `sent.h` (base SENT protocol definitions and utilities)

## License
This project is released under the MIT License.

