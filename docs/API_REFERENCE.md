# API Reference

Complete API documentation for the STM32 SENT Protocol Library.

## Table of Contents

- [Core Module (sent.h)](#core-module-senth)
  - [Constants and Macros](#constants-and-macros)
  - [Data Types](#data-types)
  - [Functions](#core-functions)
- [Receiver Module (sent_rx.h)](#receiver-module-sent_rxh)
  - [Data Types](#receiver-data-types)
  - [Functions](#receiver-functions)
- [Transmitter Module (sent_tx.h)](#transmitter-module-sent_txh)
  - [Data Types](#transmitter-data-types)
  - [Functions](#transmitter-functions)
- [Timer Utilities (timer_utils.h)](#timer-utilities-timer_utilsh)
  - [Macros](#timer-macros)
  - [Functions](#timer-functions)

---

## Core Module (sent.h)

The core module provides fundamental SENT protocol functionality including message structures, CRC calculation, and encoding/decoding.

### Constants and Macros

#### Timing Constants

```c
#define SENT_MIN_TICK_UNIT_MS   0.003  // Minimum tick unit duration (3 μs)
#define SENT_MAX_TICK_UNIT_MS   0.09   // Maximum tick unit duration (90 μs)
#define SENT_SYNC_TICKS         56     // Synchronization pulse width in ticks
#define SENT_PAUSE_TICKS        77     // Pause frame width in ticks
```

#### Nibble Constants

```c
#define SENT_MIN_NIBBLE_TICK_COUNT  12  // Minimum ticks for a nibble (represents 0)
#define SENT_MAX_NIBBLE_TICK_COUNT  27  // Maximum ticks for a nibble (represents 15)
#define SENT_MAX_NIBBLES            8   // Maximum number of nibbles in a message
```

#### CRC Seeds

```c
#define SENT_CRC4_SEED 0x05  // Seed value for 4-bit CRC (fast channel)
#define SENT_CRC6_SEED 0x15  // Seed value for 6-bit CRC (slow channel)
```

#### Utility Macros

```c
#define SENT_NIBBLE_MASK(data)      ((data) & 0x0f)
#define SENT_STATUS_MASK(status)    ((status) & 0x03)
#define SENT_TICKS_TO_TIM(ticks, unit)  ((uint32_t)roundf((float)(ticks) * (unit)))
#define SENT_TIM_TO_TICKS(ticks, unit)  ((uint32_t)roundf((float)(ticks) / (unit)))
```

**Description:**
- `SENT_NIBBLE_MASK`: Masks a value to 4 bits (0-15)
- `SENT_STATUS_MASK`: Masks a status value to 2 bits (0-3)
- `SENT_TICKS_TO_TIM`: Converts SENT ticks to timer ticks
- `SENT_TIM_TO_TICKS`: Converts timer ticks to SENT ticks

### Data Types

#### Enumerations

##### `SENTHandleStatus_t`

```c
typedef enum {
    SENT_READY,      // Handle is ready for operation
    SENT_TX,         // Handle is in transmit mode
    SENT_RX,         // Handle is in receive mode
    SENT_NOT_READY   // Handle is not initialized
} SENTHandleStatus_t;
```

**Description:** Represents the operational status of a SENT handle.

##### `SENTProtocolVersion_t`

```c
typedef enum {
    SENT_2007 = 1,  // SENT protocol version 2007
    SENT_2008 = 2,  // SENT protocol version 2008
    SENT_2010 = 3,  // SENT protocol version 2010
    SENT_2016 = 4   // SENT protocol version 2016
} SENTProtocolVersion_t;
```

**Description:** Supported SENT protocol versions. Different versions support different features:
- **2007**: Basic fast channel
- **2008**: Fast channel + basic slow channel (SHORT format)
- **2010**: Enhanced 12-bit slow channel
- **2016**: Enhanced 16-bit slow channel

##### `SENTSlowMsgType_t`

```c
typedef enum {
    SENT_SLOWTYPE_SHORT,       // Short slow message (8-bit ID + 4-bit data)
    SENT_SLOWTYPE_ENHANCED12,  // Enhanced 12-bit slow message
    SENT_SLOWTYPE_ENHANCED16   // Enhanced 16-bit slow message
} SENTSlowMsgType_t;
```

**Description:** Types of slow channel messages supported by different protocol versions.

##### `SENTSlowStatus_t`

```c
typedef enum {
    SENT_SLOW_IDLE,            // Slow channel idle
    SENT_SLOW_RX,              // Slow channel receiving
    SENT_SLOW_RX_SHORT,        // Receiving SHORT format
    SENT_SLOW_RX_ENHANCED,     // Receiving ENHANCED format (generic)
    SENT_SLOW_RX_ENHANCED_12,  // Receiving ENHANCED12 format
    SENT_SLOW_RX_ENHANCED_16,  // Receiving ENHANCED16 format
    SENT_SLOW_TX               // Slow channel transmitting
} SENTSlowStatus_t;
```

**Description:** State machine states for slow channel processing.

#### Structures

##### `SENTMsg_t`

```c
typedef struct {
    uint8_t status_nibble;              // Status and communication nibble
    uint8_t data_nibbles[SENT_MAX_NIBBLES]; // Data nibbles (up to 8)
    uint8_t crc;                        // CRC checksum
    uint8_t data_length;                // Number of data nibbles
} SENTMsg_t;
```

**Description:** Logical SENT fast channel message structure.

**Fields:**
- `status_nibble`: Status information (typically 2 bits status + 2 bits reserved)
- `data_nibbles`: Array of data nibbles (each nibble is 4 bits, 0-15)
- `crc`: Calculated CRC value (4 bits)
- `data_length`: Number of valid data nibbles (typically 6)

##### `SENTSlowMsg_t`

```c
typedef struct {
    SENTSlowMsgType_t type;
    union {
        struct {
            uint16_t crc  :4;   // 4-bit CRC
            uint16_t data :8;   // 8-bit message ID
            uint16_t id   :4;   // 4-bit data
        } slow_short;
        struct {
            uint8_t crc;        // 6-bit CRC
            uint16_t data;      // 12 or 16-bit data
            uint8_t id;         // 8-bit message ID
        } slow_enhanced;
    } as;
} SENTSlowMsg_t;
```

**Description:** Slow channel message structure supporting multiple formats.

**Usage:**
```c
SENTSlowMsg_t slow_msg;

// For SHORT format (SENT 2008)
slow_msg.type = SENT_SLOWTYPE_SHORT;
uint8_t id = slow_msg.as.slow_short.id;
uint8_t data = slow_msg.as.slow_short.data;

// For ENHANCED format (SENT 2010/2016)
slow_msg.type = SENT_SLOWTYPE_ENHANCED12;
uint8_t id = slow_msg.as.slow_enhanced.id;
uint16_t data = slow_msg.as.slow_enhanced.data;
```

##### `SENTPhysMsg_t`

```c
typedef struct {
    uint32_t ticks[SENT_TICKS_LEN];  // Array of tick counts
    uint8_t length;                  // Number of valid ticks
} SENTPhysMsg_t;
```

**Description:** Physical SENT message representation with actual tick timings.

**Fields:**
- `ticks`: Array of tick counts (in timer units) for each pulse
- `length`: Number of valid entries in the ticks array

##### `SENTHandleInit_t`

```c
typedef struct {
    TIM_HandleTypeDef *htim;            // Timer handle
    uint32_t channel;                   // Timer channel
    uint8_t has_pause;                  // Whether pause frame is used
    uint16_t message_tick_len;          // Total message length in ticks
    uint8_t data_length;                // Number of data nibbles
    double tick_unit_time;              // Tick unit time in milliseconds
    SENTProtocolVersion_t version;      // Protocol version
} SENTHandleInit_t;
```

**Description:** Initialization parameters for SENT handles.

**Fields:**
- `htim`: Pointer to STM32 HAL timer handle
- `channel`: Timer channel (e.g., `TIM_CHANNEL_1`)
- `has_pause`: Set to 1 if pause frame is included in messages
- `message_tick_len`: Total ticks per message (calculated automatically)
- `data_length`: Number of data nibbles in fast channel (typically 6)
- `tick_unit_time`: Time duration of one tick in milliseconds (3-90 μs)
- `version`: SENT protocol version to use

##### `SENTHandle_t`

```c
typedef struct {
    TIM_HandleTypeDef *htim;            // Timer handle
    uint32_t channel;                   // Timer channel
    uint8_t has_pause;                  // Pause frame enabled flag
    uint16_t message_tick_len;          // Message length in ticks
    uint8_t nibble_count;               // Number of nibbles
    SENTHandleStatus_t status;          // Handle status
    uint8_t index;                      // Current nibble index
    SENTSlowStatus_t slow_channel_status; // Slow channel state
    uint8_t slow_channel_index;         // Slow channel bit index
    uint32_t tim_to_tick_ratio;         // Timer to tick conversion ratio
    SENTProtocolVersion_t version;      // Protocol version
} SENTHandle_t;
```

**Description:** Base SENT handle structure (used internally, extended by RX and TX handles).

### Core Functions

#### `SENT_init()`

```c
uint8_t SENT_init(SENTHandle_t *const handle, SENTHandleInit_t *const init);
```

**Description:** Initializes a base SENT handle with configuration parameters.

**Parameters:**
- `handle`: Pointer to the SENT handle structure to initialize
- `init`: Pointer to initialization configuration structure

**Returns:**
- `1` on successful initialization
- `0` on failure (invalid parameters)

**Usage:**
```c
SENTHandle_t handle;
SENTHandleInit_t init = {
    .htim = &htim1,
    .channel = TIM_CHANNEL_1,
    .has_pause = 0,
    .data_length = 6,
    .tick_unit_time = 0.003,  // 3 μs
    .version = SENT_VERSION_2010
};

if (!SENT_init(&handle, &init)) {
    // Handle initialization error
}
```

**Location:** `Inc/sent.h:101`, `Src/sent.c`

---

#### `SENT_msg_init()`

```c
void SENT_msg_init(SENTMsg_t *const msg, uint8_t status_nibble,
                   uint8_t *const data_nibbles, uint8_t data_length,
                   SENTProtocolVersion_t version);
```

**Description:** Initializes a SENT message structure and calculates its CRC.

**Parameters:**
- `msg`: Pointer to the SENT message structure to initialize
- `status_nibble`: Status nibble value (0-15)
- `data_nibbles`: Pointer to array of data nibbles
- `data_length`: Number of data nibbles (typically 6)
- `version`: Protocol version for CRC calculation

**Usage:**
```c
SENTMsg_t message;
uint8_t data[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};

SENT_msg_init(&message, 0x5, data, 6, SENT_VERSION_2010);
// message.crc is automatically calculated
```

**Location:** `Inc/sent.h:111`, `Src/sent.c`

---

#### `SENT_calc_crc()`

```c
uint8_t SENT_calc_crc(SENTMsg_t *const msg, SENTProtocolVersion_t version);
```

**Description:** Calculates the 4-bit CRC for a SENT fast channel message.

**Parameters:**
- `msg`: Pointer to the SENT message
- `version`: Protocol version (affects CRC calculation method)

**Returns:**
- 4-bit CRC value (0-15)

**Algorithm:**
- Uses lookup table-based CRC4 calculation
- Initial value: `SENT_CRC4_SEED` (0x05)
- Different finalization based on protocol version

**Usage:**
```c
SENTMsg_t msg;
// ... populate message ...
uint8_t crc = SENT_calc_crc(&msg, SENT_VERSION_2010);
msg.crc = crc;
```

**Location:** `Inc/sent.h:119`, `Src/sent.c`

---

#### `SENT_calc_crc_slow()`

```c
uint8_t SENT_calc_crc_slow(SENTSlowMsg_t *const msg, SENTProtocolVersion_t version);
```

**Description:** Calculates the CRC for a SENT slow channel message.

**Parameters:**
- `msg`: Pointer to the SENT slow message
- `version`: Protocol version

**Returns:**
- CRC value (4 bits for SHORT, 6 bits for ENHANCED)

**Usage:**
```c
SENTSlowMsg_t slow_msg;
slow_msg.type = SENT_SLOWTYPE_ENHANCED12;
slow_msg.as.slow_enhanced.id = 0x42;
slow_msg.as.slow_enhanced.data = 0x123;

uint8_t crc = SENT_calc_crc_slow(&slow_msg, SENT_VERSION_2010);
slow_msg.as.slow_enhanced.crc = crc;
```

**Location:** `Inc/sent.h:127`, `Src/sent.c`

---

#### `SENT_encodePhysMsg()`

```c
void SENT_encodePhysMsg(SENTHandle_t *const handle, SENTPhysMsg_t *const dest,
                        SENTMsg_t *const src, uint32_t tim_to_tick_ratio);
```

**Description:** Encodes a logical SENT message into physical tick timings.

**Parameters:**
- `handle`: Pointer to SENT handle
- `dest`: Pointer to destination physical message structure
- `src`: Pointer to source logical message
- `tim_to_tick_ratio`: Conversion ratio from timer ticks to SENT ticks

**Usage:**
```c
SENTHandle_t handle;
SENTMsg_t logical_msg;
SENTPhysMsg_t physical_msg;

SENT_encodePhysMsg(&handle, &physical_msg, &logical_msg,
                   handle.tim_to_tick_ratio);
// physical_msg.ticks[] now contains timer values for each pulse
```

**Location:** `Inc/sent.h:137`, `Src/sent.c`

---

#### `SENT_decodePhysMsg()`

```c
void SENT_decodePhysMsg(SENTHandle_t *const handle, SENTMsg_t *const dest,
                        SENTPhysMsg_t *const src, uint32_t tim_to_tick_ratio);
```

**Description:** Decodes a physical SENT message into a logical message structure.

**Parameters:**
- `handle`: Pointer to SENT handle
- `dest`: Pointer to destination logical message
- `src`: Pointer to source physical message
- `tim_to_tick_ratio`: Conversion ratio from timer ticks to SENT ticks

**Usage:**
```c
SENTHandle_t handle;
SENTPhysMsg_t physical_msg;  // Filled by receiver
SENTMsg_t logical_msg;

SENT_decodePhysMsg(&handle, &logical_msg, &physical_msg,
                   handle.tim_to_tick_ratio);
// logical_msg now contains decoded nibbles and status
```

**Location:** `Inc/sent.h:147`, `Src/sent.c`

---

## Receiver Module (sent_rx.h)

The receiver module implements SENT message reception using STM32 timer input capture.

### Receiver Data Types

#### Constants and Macros

```c
#define SENTRX_TICK_TOLERANCE   0.2  // 20% tolerance for tick timing
#define SENTRX_STATUS_BIT3(status) ((status) & 0x8)  // Extract bit 3
#define SENTRX_STATUS_BIT2(status) ((status) & 0x4)  // Extract bit 2
```

#### Callback Types

##### `SENTRxCallback_t`

```c
typedef void (*SENTRxCallback_t)(SENTRxHandle_t *const handle,
                                  SENTMsg_t *const msg,
                                  void *user_data);
```

**Description:** Callback function type for fast channel message reception.

**Parameters:**
- `handle`: Pointer to the receiver handle
- `msg`: Pointer to the received and decoded message
- `user_data`: User-defined data pointer (passed during initialization)

**Example:**
```c
void MyFastChannelCallback(SENTRxHandle_t *const handle,
                           SENTMsg_t *const msg,
                           void *user_data) {
    // Message has already been CRC validated
    uint8_t status = msg->status_nibble;
    for (int i = 0; i < msg->data_length; i++) {
        process_nibble(msg->data_nibbles[i]);
    }
}
```

##### `SENTRxSlowCallback_t`

```c
typedef void (*SENTRxSlowCallback_t)(SENTRxHandle_t *const handle,
                                      SENTSlowMsg_t *const msg,
                                      void *user_data);
```

**Description:** Callback function type for slow channel message reception.

**Parameters:**
- `handle`: Pointer to the receiver handle
- `msg`: Pointer to the received slow channel message
- `user_data`: User-defined data pointer

**Example:**
```c
void MySlowChannelCallback(SENTRxHandle_t *const handle,
                           SENTSlowMsg_t *const msg,
                           void *user_data) {
    if (msg->type == SENT_SLOWTYPE_ENHANCED12) {
        uint8_t id = msg->as.slow_enhanced.id;
        uint16_t data = msg->as.slow_enhanced.data;
        process_slow_message(id, data);
    }
}
```

#### Enumerations

##### `SENTRxState_t`

```c
typedef enum {
    SENTRX_STATE_SYNC,    // Waiting for synchronization pulse
    SENTRX_STATE_NIBBLES  // Receiving data nibbles
} SENTRxState_t;
```

**Description:** Receiver state machine states.

#### Structures

##### `SENTRxHandle_t`

```c
typedef struct SENTRxHandle {
    SENTHandle_t base;                      // Base SENT handle
    void *user_data;                        // User-defined data pointer
    uint32_t capture;                       // Last captured timer value
    SENTRxState_t state;                    // Current receiver state
    SENTPhysMsg_t msg_buffer;               // Buffer for physical messages
    SENTSlowMsg_t slow_msg_buffer;          // Buffer for slow channel messages
    SENTRxCallback_t rx_callback;           // Fast channel callback
    SENTRxSlowCallback_t slow_rx_callback;  // Slow channel callback
    uint32_t slow_channel_buffer_bit2;      // Slow channel bit buffer (bit 2)
    uint32_t slow_channel_buffer_bit3;      // Slow channel bit buffer (bit 3)
    uint32_t last_tick_to_tim_ratio;        // Last computed tick-to-timer ratio
} SENTRxHandle_t;
```

**Description:** SENT receiver handle structure.

### Receiver Functions

#### `SENTRx_init()`

```c
uint8_t SENTRx_init(SENTRxHandle_t *const handle,
                    SENTHandleInit_t *const init,
                    SENTRxCallback_t rx_callback,
                    SENTRxSlowCallback_t slow_rx_callback,
                    void *user_data);
```

**Description:** Initializes the SENT receiver with timer and callback configuration.

**Parameters:**
- `handle`: Pointer to the SENT receiver handle
- `init`: Pointer to initialization structure
- `rx_callback`: Callback for fast channel messages (can be NULL)
- `slow_rx_callback`: Callback for slow channel messages (can be NULL)
- `user_data`: User-defined pointer passed to callbacks

**Returns:**
- `1` on successful initialization
- `0` on failure

**Usage:**
```c
SENTRxHandle_t rx_handle;
SENTHandleInit_t init = {
    .htim = &htim1,
    .channel = TIM_CHANNEL_1,
    .version = SENT_VERSION_2010,
    .data_length = 6
};

if (!SENTRx_init(&rx_handle, &init, FastCallback, SlowCallback, NULL)) {
    Error_Handler();
}
```

**Location:** `Inc/sent_rx.h:59`, `Src/sent_rx.c`

---

#### `SENTRx_start()`

```c
uint8_t SENTRx_start(SENTRxHandle_t *const handle);
```

**Description:** Starts the SENT receiver by enabling timer input capture.

**Parameters:**
- `handle`: Pointer to the initialized SENT receiver handle

**Returns:**
- `1` on success
- `0` on failure

**Usage:**
```c
if (!SENTRx_start(&rx_handle)) {
    Error_Handler();
}
// Receiver is now active and will call callbacks when messages arrive
```

**Note:** The timer must be configured for input capture mode before calling this function.

**Location:** `Inc/sent_rx.h:67`, `Src/sent_rx.c`

---

#### `SENTRx_InputCaptureCallback()`

```c
void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle);
```

**Description:** Handles timer input capture events. This function should be called from the STM32 HAL timer input capture callback.

**Parameters:**
- `handle`: Pointer to the SENT receiver handle

**Usage:**
```c
// In your HAL callback handler
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTRx_InputCaptureCallback(&rx_handle);
    }
}
```

**Behavior:**
- Captures timer value
- Processes pulse timing
- Runs state machine (SYNC → NIBBLES)
- Decodes messages
- Validates CRC
- Invokes user callbacks when complete messages are received

**Location:** `Inc/sent_rx.h:74`, `Src/sent_rx.c`

---

## Transmitter Module (sent_tx.h)

The transmitter module implements SENT message transmission using STM32 timer PWM with DMA.

### Transmitter Data Types

#### Constants

```c
#define SENTTX_NIBBLE_LOW_TICKS   5   // Low period for each nibble pulse
#define SENTTX_SYNC_TICKS         61  // Sync pulse total (56 + 5)
#define SENTTX_PAUSE_TICKS        82  // Pause pulse total (77 + 5)
```

#### Callback Types

##### `SENTTxCallback_t`

```c
typedef void (*SENTTxCallback_t)(SENTTxHandle_t *const handle);
```

**Description:** Callback function type for transmission completion events.

**Parameters:**
- `handle`: Pointer to the transmitter handle

**Example:**
```c
void TxCompleteCallback(SENTTxHandle_t *const handle) {
    // Called after each message transmission
    // Update message data for next transmission
    handle->msg_source->data_nibbles[0]++;
}
```

#### Structures

##### `SENTTxHandle_t`

```c
typedef struct SENTTxHandle {
    SENTHandle_t base;                      // Base SENT handle
    SENTMsg_t *msg_source;                  // Pointer to message source
    SENTTxCallback_t msg_tx_callback;       // Fast channel callback
    SENTSlowMsg_t *slow_msg_source;         // Pointer to slow message source
    SENTTxCallback_t slow_msg_tx_callback;  // Slow channel callback
    SENTPhysMsg_t msg_buffer;               // Physical message buffer
    union {
        uint16_t u16[SENT_MAX_NIBBLES+4];   // DMA buffer for 16-bit timers
        uint32_t u32[SENT_MAX_NIBBLES+4];   // DMA buffer for 32-bit timers
    } dma_buffer;
    uint8_t msg_buffer_index;               // Message buffer index
} SENTTxHandle_t;
```

**Description:** SENT transmitter handle structure.

### Transmitter Functions

#### `SENTTx_init()`

```c
uint8_t SENTTx_init(SENTTxHandle_t *const handle,
                    SENTHandleInit_t *const init,
                    SENTMsg_t *const msg_source,
                    SENTTxCallback_t msg_tx_callback,
                    SENTSlowMsg_t *const slow_msg_source,
                    SENTTxCallback_t slow_msg_tx_callback);
```

**Description:** Initializes the SENT transmitter with timer and message sources.

**Parameters:**
- `handle`: Pointer to the SENT transmitter handle
- `init`: Pointer to initialization structure
- `msg_source`: Pointer to the message to transmit (updated continuously)
- `msg_tx_callback`: Callback after fast channel transmission (can be NULL)
- `slow_msg_source`: Pointer to slow message (can be NULL if not used)
- `slow_msg_tx_callback`: Callback after slow channel transmission (can be NULL)

**Returns:**
- `1` on successful initialization
- `0` on failure

**Usage:**
```c
SENTTxHandle_t tx_handle;
SENTMsg_t tx_message;
uint8_t data[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};

SENT_msg_init(&tx_message, 0x5, data, 6, SENT_VERSION_2010);

SENTHandleInit_t init = {
    .htim = &htim1,
    .channel = TIM_CHANNEL_1,
    .version = SENT_VERSION_2010,
    .data_length = 6,
    .tick_unit_time = 0.003
};

if (!SENTTx_init(&tx_handle, &init, &tx_message, TxCallback, NULL, NULL)) {
    Error_Handler();
}
```

**Location:** `Inc/sent_tx.h:50`, `Src/sent_tx.c`

---

#### `SENTTx_start()`

```c
uint8_t SENTTx_start(SENTTxHandle_t *const handle);
```

**Description:** Starts SENT transmission using PWM with DMA.

**Parameters:**
- `handle`: Pointer to the initialized SENT transmitter handle

**Returns:**
- `1` on success
- `0` on failure

**Usage:**
```c
if (!SENTTx_start(&tx_handle)) {
    Error_Handler();
}
// Transmission is now continuous via DMA
```

**Note:**
- Timer must be configured for PWM mode with DMA
- DMA must be configured for memory-to-peripheral transfer
- Transmission continues automatically via DMA
- Message updates can be done in the callback

**Location:** `Inc/sent_tx.h:59`, `Src/sent_tx.c`

---

#### `SENTTx_AutoReloadCallback()`

```c
void SENTTx_AutoReloadCallback(SENTTxHandle_t *const handle);
```

**Description:** Handles timer auto-reload events during transmission. Should be called from HAL timer period elapsed callback.

**Parameters:**
- `handle`: Pointer to the SENT transmitter handle

**Usage:**
```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_AutoReloadCallback(&tx_handle);
    }
}
```

**Behavior:**
- Manages message transmission state
- Prepares next message
- Invokes user callbacks
- Updates slow channel state machine

**Location:** `Inc/sent_tx.h:66`, `Src/sent_tx.c`

---

#### `SENTTx_CompareCallback()`

```c
void SENTTx_CompareCallback(SENTTxHandle_t *const handle);
```

**Description:** Handles timer compare events during transmission.

**Parameters:**
- `handle`: Pointer to the SENT transmitter handle

**Usage:**
```c
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_CompareCallback(&tx_handle);
    }
}
```

**Location:** `Inc/sent_tx.h:73`, `Src/sent_tx.c`

---

## Timer Utilities (timer_utils.h)

Utility macros and functions for STM32 timer operations.

### Timer Macros

#### `_M_GET_TIM_APB_PLACEMENT()`

```c
#define _M_GET_TIM_APB_PLACEMENT(__HANDLE__)
```

**Description:** Determines if a timer is on APB1 or APB2 bus.

**Parameters:**
- `__HANDLE__`: Timer handle

**Returns:**
- `1` if timer is on APB1 bus
- `0` if timer is on APB2 bus

**Location:** `Inc/timer_utils.h:24`

---

#### `TIM_GET_FREQ()`

```c
#define TIM_GET_FREQ(TIM)
```

**Description:** Gets the timer operating frequency in Hz.

**Parameters:**
- `TIM`: Timer handle

**Returns:**
- Timer frequency in Hz (as float)

**Usage:**
```c
float freq = TIM_GET_FREQ(&htim1);
// freq contains actual timer counting frequency
```

**Location:** `Inc/timer_utils.h:32`

---

#### `TIM_MS_TO_TICKS()`

```c
#define TIM_MS_TO_TICKS(TIM, MS)
```

**Description:** Converts milliseconds to timer ticks.

**Parameters:**
- `TIM`: Timer handle
- `MS`: Time in milliseconds (float)

**Returns:**
- Number of timer ticks (uint32_t)

**Usage:**
```c
uint32_t ticks = TIM_MS_TO_TICKS(&htim1, 10.5);  // Convert 10.5 ms to ticks
```

**Location:** `Inc/timer_utils.h:41`

---

#### `TIM_TICKS_TO_MS()`

```c
#define TIM_TICKS_TO_MS(TIM, TICKS)
```

**Description:** Converts timer ticks to milliseconds.

**Parameters:**
- `TIM`: Timer handle
- `TICKS`: Number of timer ticks (uint32_t)

**Returns:**
- Time in milliseconds (float)

**Usage:**
```c
float ms = TIM_TICKS_TO_MS(&htim1, 1000);  // Convert 1000 ticks to ms
```

**Location:** `Inc/timer_utils.h:50`

---

#### `TIM_GET_MAX_AUTORELOAD()`

```c
#define TIM_GET_MAX_AUTORELOAD(TIM)
```

**Description:** Gets the maximum auto-reload value for a timer.

**Parameters:**
- `TIM`: Timer handle

**Returns:**
- `UINT32_MAX` for 32-bit timers
- `UINT16_MAX` for 16-bit timers

**Usage:**
```c
uint32_t max_val = TIM_GET_MAX_AUTORELOAD(&htim1);
```

**Location:** `Inc/timer_utils.h:57`

---

#### `TIM_GET_LENGTH()`

```c
#define TIM_GET_LENGTH(TIM)
```

**Description:** Gets the timer counter length in bits.

**Parameters:**
- `TIM`: Timer handle

**Returns:**
- `32` for 32-bit timers
- `16` for 16-bit timers

**Usage:**
```c
uint8_t bits = TIM_GET_LENGTH(&htim1);
```

**Location:** `Inc/timer_utils.h:64`

---

### Timer Functions

#### `TIM_GetInternalClkFreq()`

```c
uint32_t TIM_GetInternalClkFreq(TIM_HandleTypeDef *htim);
```

**Description:** Gets the internal clock frequency of a timer, accounting for APB prescaler settings.

**Parameters:**
- `htim`: Pointer to timer handle

**Returns:**
- Timer clock frequency in Hz

**Usage:**
```c
uint32_t clk_freq = TIM_GetInternalClkFreq(&htim1);
```

**Note:** This function automatically accounts for the APB prescaler doubling rule when APB prescaler is not 1.

**Location:** `Inc/timer_utils.h:72`, `Src/timer_utils.c`

---

## Error Handling

Most initialization and start functions return:
- `1` on success
- `0` on failure

Always check return values:

```c
if (!SENTRx_init(&handle, &init, callback, NULL, NULL)) {
    // Handle initialization error
    Error_Handler();
}

if (!SENTRx_start(&handle)) {
    // Handle start error
    Error_Handler();
}
```

## Thread Safety

This library is not thread-safe. Callbacks are called from interrupt context:
- Keep callbacks short and fast
- Avoid blocking operations
- Use flags and process in main loop if needed
- Be careful with shared data access

## Memory Management

- No dynamic memory allocation used
- All structures are statically allocated by the user
- Message buffers are part of handle structures
- User is responsible for handle lifetime

---

## Quick Reference

### Initialization Flow

**Receiver:**
```c
SENTRxHandle_t handle;
SENTHandleInit_t init = { /* config */ };
SENTRx_init(&handle, &init, fast_cb, slow_cb, user_data);
SENTRx_start(&handle);
// Call SENTRx_InputCaptureCallback() from HAL IC callback
```

**Transmitter:**
```c
SENTTxHandle_t handle;
SENTMsg_t message;
SENTHandleInit_t init = { /* config */ };
SENT_msg_init(&message, status, data, 6, version);
SENTTx_init(&handle, &init, &message, tx_cb, NULL, NULL);
SENTTx_start(&handle);
// Call SENTTx_AutoReloadCallback() from HAL period callback
```

### Common Patterns

**Updating TX message:**
```c
void TxCallback(SENTTxHandle_t *handle) {
    // Modify message for next transmission
    handle->msg_source->data_nibbles[0]++;
    // CRC is recalculated automatically before transmission
}
```

**Processing RX message:**
```c
void RxCallback(SENTRxHandle_t *handle, SENTMsg_t *msg, void *user_data) {
    // Message is already CRC validated
    // Copy data if needed (msg is in handle buffer and will be overwritten)
    memcpy(my_buffer, msg->data_nibbles, msg->data_length);
}
```
