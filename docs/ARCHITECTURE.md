# Architecture and Design

Internal architecture documentation for the STM32 SENT Protocol Library.

## Table of Contents

- [Overview](#overview)
- [Design Philosophy](#design-philosophy)
- [Module Architecture](#module-architecture)
- [Data Flow](#data-flow)
- [State Machines](#state-machines)
- [Timing and Synchronization](#timing-and-synchronization)
- [CRC Implementation](#crc-implementation)
- [Memory Management](#memory-management)
- [Performance Considerations](#performance-considerations)
- [Extending the Library](#extending-the-library)

---

## Overview

The STM32 SENT library is designed as a modular, efficient, and portable implementation of the SENT (Single Edge Nibble Transmission) protocol. The architecture emphasizes:

- **Separation of concerns** - Core, RX, and TX modules are independent
- **Hardware abstraction** - Uses STM32 HAL for portability
- **Interrupt-driven operation** - Minimal CPU overhead
- **Zero dynamic allocation** - All memory is statically allocated
- **Callback-based design** - Flexible application integration

### Key Components

```
┌─────────────────────────────────────────────────┐
│              Application Layer                  │
│  (User callbacks, message processing)           │
└────────────┬──────────────────┬─────────────────┘
             │                  │
┌────────────▼────────┐  ┌──────▼──────────────────┐
│   SENT RX Module    │  │   SENT TX Module        │
│  (Input Capture)    │  │   (PWM + DMA)           │
│  sent_rx.c/.h       │  │   sent_tx.c/.h          │
└────────────┬────────┘  └──────┬──────────────────┘
             │                  │
             └────────┬─────────┘
                      │
           ┌──────────▼──────────┐
           │   Core SENT Module  │
           │   (Protocol logic)  │
           │   sent.c/.h         │
           └──────────┬──────────┘
                      │
           ┌──────────▼──────────┐
           │   Timer Utilities   │
           │  timer_utils.c/.h   │
           └──────────┬──────────┘
                      │
           ┌──────────▼──────────┐
           │    STM32 HAL        │
           │  (Hardware layer)   │
           └─────────────────────┘
```

---

## Design Philosophy

### 1. Modularity

The library is split into four independent modules:

- **Core Module** (`sent.c`/`sent.h`): Protocol-agnostic SENT functionality
- **Receiver Module** (`sent_rx.c`/`sent_rx.h`): Reception-specific implementation
- **Transmitter Module** (`sent_tx.c`/`sent_tx.h`): Transmission-specific implementation
- **Timer Utilities** (`timer_utils.c`/`timer_utils.h`): STM32-specific helpers

Each module can be used independently (e.g., RX-only or TX-only applications).

### 2. Portability

- Uses STM32 HAL exclusively for hardware access
- No direct register manipulation (except in timer_utils for clock detection)
- Easily portable to different STM32 families
- Clear abstraction between protocol logic and hardware

### 3. Efficiency

- **Interrupt-driven**: Minimal CPU usage, operations triggered by hardware events
- **DMA-based TX**: Transmission happens in background without CPU intervention
- **Zero-copy** where possible: Direct buffer manipulation
- **No dynamic allocation**: All structures statically allocated by user

### 4. Safety

- CRC validation before invoking callbacks
- Synchronization tolerance checking
- Error detection at multiple levels
- Handle status tracking prevents misuse

---

## Module Architecture

### Core Module (sent.c/sent.h)

**Responsibilities:**
- Define SENT protocol data structures
- Implement CRC calculation (CRC4 and CRC6)
- Encode/decode between logical and physical messages
- Manage base handle initialization
- Protocol version handling

**Key Design Decisions:**

1. **Separation of Logical and Physical Messages**
   - `SENTMsg_t`: Logical message (nibbles, status, CRC)
   - `SENTPhysMsg_t`: Physical message (tick timings)
   - Conversion handled by encode/decode functions

2. **CRC Lookup Tables**
   ```c
   // CRC4 lookup table for fast calculation
   static const uint8_t crc4_lookup[16] = { /* ... */ };

   // CRC6 lookup table for slow channel
   static const uint8_t crc6_lookup[64] = { /* ... */ };
   ```
   - Trade memory for speed
   - Constant-time CRC calculation
   - Tables computed at compile time

3. **Protocol Version Support**
   - Enum-based version selection
   - Version affects CRC finalization
   - Version affects slow channel format

**Core Data Flow:**
```
Application
    ↓
Logical Message (SENTMsg_t)
    ↓ encode
Physical Message (SENTPhysMsg_t) - timer tick values
    ↓
Hardware Timer
```

### Receiver Module (sent_rx.c/sent_rx.h)

**Responsibilities:**
- Input capture event handling
- Pulse timing measurement
- Synchronization detection
- State machine for message decoding
- Slow channel bit extraction
- CRC validation
- Callback invocation

**Architecture:**

```
Timer Input Capture IRQ
         ↓
HAL_TIM_IC_CaptureCallback()
         ↓
SENTRx_InputCaptureCallback()
         ↓
    ┌────────────────────┐
    │  State Machine     │
    │                    │
    │  SYNC → NIBBLES    │
    └────────────────────┘
         ↓
    Decode Physical Msg
         ↓
    Validate CRC
         ↓
    Invoke User Callback
```

**State Machine:**

```c
typedef enum {
    SENTRX_STATE_SYNC,      // Looking for sync pulse
    SENTRX_STATE_NIBBLES    // Receiving data nibbles
} SENTRxState_t;
```

**State Transitions:**
1. **SYNC State**:
   - Wait for pulse ≈ 56 ticks (within tolerance)
   - Calculate tick-to-timer ratio
   - Transition to NIBBLES

2. **NIBBLES State**:
   - Capture each nibble pulse
   - Convert timer ticks to nibble values (0-15)
   - Extract slow channel bits (bits 2 and 3 of status)
   - After last nibble: decode, validate CRC, callback
   - Return to SYNC

**Synchronization Detection:**
```c
// Check if captured pulse is a sync pulse (≈56 ticks)
float tick_duration_ms = TIM_TICKS_TO_MS(htim, capture_diff);

if (tick_duration_ms >= SENT_SYNC_TICKS * SENTRX_MIN_TICK_UNIT_MS &&
    tick_duration_ms <= SENT_SYNC_TICKS * SENTRX_MAX_TICK_UNIT_MS) {
    // This is a sync pulse!
    // Calculate tick-to-timer ratio for this message
}
```

**Slow Channel Processing:**
- Bits extracted from status nibble (bits 2 and 3)
- Accumulated in 32-bit buffers
- Separate state machine tracks slow channel assembly
- CRC validated before slow callback invocation

**Location References:**
- Receiver init: `sent_rx.c:SENTRx_init()`
- Input capture handler: `sent_rx.c:SENTRx_InputCaptureCallback()`
- State machine: `sent_rx.c:SENTRx_InputCaptureCallback()` (lines handling state transitions)

### Transmitter Module (sent_tx.c/sent_tx.h)

**Responsibilities:**
- PWM timer configuration
- DMA-based transmission
- Message encoding
- Slow channel state machine
- Callback invocation on transmission events

**Architecture:**

```
Application updates message
         ↓
SENTTx_AutoReloadCallback()
         ↓
    Encode Message
         ↓
    Update DMA Buffer
         ↓
┌────────────────────┐
│   DMA Transfer     │
│  (Automatic PWM    │
│   pulse widths)    │
└────────────────────┘
         ↓
    PWM Output
         ↓
    Transmission Complete
         ↓
    Invoke User Callback
```

**DMA-Based Transmission:**

The transmitter uses a clever DMA technique:

1. **DMA updates timer CCR register** (compare/capture register)
2. Each DMA transfer changes the PWM pulse width
3. Auto-reload triggers next DMA transfer
4. No CPU intervention needed during transmission

```c
// DMA buffer contains pulse widths for each tick
union {
    uint16_t u16[SENT_MAX_NIBBLES+4];  // For 16-bit timers
    uint32_t u32[SENT_MAX_NIBBLES+4];  // For 32-bit timers
} dma_buffer;

// DMA configured as:
// - Memory-to-Peripheral
// - Increment memory address
// - Triggered by timer update event
```

**Message Preparation:**
```c
// Encode logical message to physical ticks
SENT_encodePhysMsg(handle, &physical_msg, logical_msg, tim_to_tick_ratio);

// Convert to timer values and store in DMA buffer
for (each tick) {
    dma_buffer[i] = tick_value;
}

// Start DMA transfer
HAL_TIM_PWM_Start_DMA(htim, channel, dma_buffer, length);
```

**Slow Channel Integration:**
- Status nibble bits [3:2] carry slow channel data
- Slow channel state machine progresses with each message
- Automatic bit stuffing into status nibble
- Slow callback invoked when complete message assembled

**Location References:**
- Transmitter init: `sent_tx.c:SENTTx_init()`
- Auto-reload handler: `sent_tx.c:SENTTx_AutoReloadCallback()`
- Compare handler: `sent_tx.c:SENTTx_CompareCallback()`
- Slow channel FSM: `sent_tx.c:SENTTx_SlowChannelFSM()`

### Timer Utilities Module

**Responsibilities:**
- Determine timer clock frequency
- Account for APB prescaler effects
- Provide time/tick conversion macros
- Detect timer capabilities (16-bit vs 32-bit)

**APB Prescaler Handling:**

STM32 has a special rule: when APB prescaler ≠ 1, timer clock is 2× APB clock.

```c
uint32_t TIM_GetInternalClkFreq(TIM_HandleTypeDef *htim) {
    uint32_t apb_freq;
    uint32_t apb_prescaler;

    if (timer_on_apb1) {
        apb_freq = HAL_RCC_GetPCLK1Freq();
        apb_prescaler = /* get APB1 prescaler */;
    } else {
        apb_freq = HAL_RCC_GetPCLK2Freq();
        apb_prescaler = /* get APB2 prescaler */;
    }

    // Apply doubling rule if prescaler != 1
    if (apb_prescaler != 1) {
        return apb_freq * 2;
    }
    return apb_freq;
}
```

---

## Data Flow

### Receiver Data Flow

```
SENT Signal (Physical)
         ↓
    GPIO Input Pin
         ↓
    Timer Input Capture
         ↓
Capture Interrupt (HAL_TIM_IC_CaptureCallback)
         ↓
SENTRx_InputCaptureCallback()
         ↓
┌──────────────────────────────┐
│  Measure pulse width         │
│  (current - previous capture)│
└──────────────────────────────┘
         ↓
┌──────────────────────────────┐
│  State Machine               │
│  SYNC: Detect sync pulse     │
│  NIBBLES: Decode nibbles     │
└──────────────────────────────┘
         ↓
Store in SENTPhysMsg_t buffer
         ↓
SENT_decodePhysMsg()
         ↓
SENTMsg_t (logical message)
         ↓
CRC Validation (SENT_calc_crc)
         ↓
User Callback (if CRC valid)
         ↓
Application Processing
```

### Transmitter Data Flow

```
Application Data
         ↓
Update SENTMsg_t
         ↓
Timer Update Event
         ↓
SENTTx_AutoReloadCallback()
         ↓
┌──────────────────────────────┐
│  Read message from source    │
│  Update slow channel bits    │
└──────────────────────────────┘
         ↓
SENT_calc_crc() - Calculate CRC
         ↓
SENT_encodePhysMsg()
         ↓
SENTPhysMsg_t (tick timings)
         ↓
┌──────────────────────────────┐
│  Convert to timer values     │
│  Store in DMA buffer         │
└──────────────────────────────┘
         ↓
DMA Transfer (automatic)
         ↓
Timer CCR Update (each tick)
         ↓
PWM Output (physical signal)
         ↓
Transmission Complete
         ↓
User Callback (optional)
```

---

## State Machines

### Receiver State Machine

```
                ┌─────────────┐
                │   INIT      │
                └──────┬──────┘
                       │
                       ↓
    ┌──────────────────────────────────┐
    │         SENTRX_STATE_SYNC        │
    │  • Wait for sync pulse (~56 ticks)│
    │  • Calculate tick-to-timer ratio │
    │  • Reset nibble counter          │
    └──────────┬───────────────────────┘
               │ Sync detected
               ↓
    ┌──────────────────────────────────┐
    │       SENTRX_STATE_NIBBLES       │
    │  • Capture each nibble pulse     │
    │  • Convert timing to nibble value│
    │  • Extract slow channel bits     │
    │  • Increment nibble counter      │
    └──────────┬───────────────────────┘
               │ All nibbles received
               ↓
    ┌──────────────────────────────────┐
    │      Message Processing          │
    │  • Decode physical message       │
    │  • Validate CRC                  │
    │  • Invoke fast channel callback  │
    │  • Process slow channel if ready │
    └──────────┬───────────────────────┘
               │
               ↓
         (Return to SYNC)
```

### Slow Channel State Machine (Receiver)

```
    IDLE
     ↓
    RX (receiving bits)
     ↓
  ┌──────────────┐
  │  Bit counter │
  │  reaches     │
  │  threshold   │
  └──────┬───────┘
         ↓
    ┌────────────────┐
    │ Determine type │
    │ (SHORT/ENH12/  │
    │  ENH16)        │
    └────┬───────────┘
         ↓
    ┌────────────────┐
    │ Extract ID,    │
    │ data, CRC      │
    └────┬───────────┘
         ↓
    ┌────────────────┐
    │ Validate CRC   │
    └────┬───────────┘
         ↓ (if valid)
    ┌────────────────┐
    │ Invoke slow    │
    │ channel        │
    │ callback       │
    └────┬───────────┘
         ↓
       IDLE
```

### Slow Channel State Machine (Transmitter)

```
    IDLE
     ↓
    TX (transmitting)
     ↓
  ┌──────────────────┐
  │  Stuff bits into │
  │  status nibble   │
  │  [3:2]           │
  └──────┬───────────┘
         │
    ┌────▼────────────┐
    │ Bit counter     │
    │ increments      │
    └────┬────────────┘
         │ All bits sent
         ↓
    ┌────────────────┐
    │ Invoke slow    │
    │ TX callback    │
    └────┬───────────┘
         ↓
       IDLE
```

---

## Timing and Synchronization

### Tick Unit Calculation

The SENT protocol uses a "tick" as the base timing unit. All pulse widths are multiples of this tick.

**Tick Unit:**
- Specification: 3-90 μs (0.003-0.09 ms)
- Typical: 3 μs

**Timer Configuration:**

For a desired tick unit of 3 μs:

```
Timer Frequency = 1 / tick_unit
                = 1 / 3 μs
                = 333.33 kHz

Prescaler = (Timer Clock / Timer Frequency) - 1

Example: 168 MHz clock
Prescaler = (168 MHz / 333.33 kHz) - 1
          = 503
```

**Library Calculation:**

```c
// User specifies tick_unit_time in ms
init.tick_unit_time = 0.003;  // 3 μs

// Library calculates tim_to_tick_ratio
tim_to_tick_ratio = (1.0 / tick_unit_time) / TIM_GET_FREQ(htim);

// Convert timer ticks to SENT ticks
sent_ticks = timer_ticks / tim_to_tick_ratio;
```

### Synchronization Tolerance

The receiver allows 20% tolerance for timing:

```c
#define SENTRX_TICK_TOLERANCE   0.2  // 20%
```

This accounts for:
- Clock drift between TX and RX
- Jitter in signal transmission
- Timer resolution limitations

**Sync Detection:**
```c
// Expected sync duration
expected_sync = SENT_SYNC_TICKS * tick_unit_time;  // 56 ticks

// Tolerance range
min_sync = expected_sync * (1 - SENTRX_TICK_TOLERANCE);  // 80%
max_sync = expected_sync * (1 + SENTRX_TICK_TOLERANCE);  // 120%

// Pulse accepted as sync if within range
if (pulse_duration >= min_sync && pulse_duration <= max_sync) {
    // Valid sync
}
```

### Message Timing

**Fast Channel Message Structure:**
```
┌──────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ SYNC │ S&C │  D0 │  D1 │  D2 │  D3 │  D4 │  D5 │ CRC │PAUSE│
└──────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
  56t    12-27  12-27  12-27  12-27  12-27  12-27  12-27  12-27  77t
        (variable based on nibble value 0-15)

Total: 56 + (8 nibbles × avg 19.5 ticks) + 77 (if pause)
     = 56 + 156 + 77 = 289 ticks minimum
```

**Example: 3 μs tick, 6 data nibbles**
- Min duration: (56 + 12×8) × 3 μs = 456 μs
- Max duration: (56 + 27×8) × 3 μs = 816 μs
- With pause: add 77×3 = 231 μs

---

## CRC Implementation

### CRC4 (Fast Channel)

**Algorithm:** 4-bit CRC with polynomial and seed

```c
uint8_t SENT_calc_crc(SENTMsg_t *const msg, SENTProtocolVersion_t version) {
    uint8_t crc = SENT_CRC4_SEED;  // 0x05

    // Process status nibble
    crc = crc4_lookup[crc ^ msg->status_nibble];

    // Process each data nibble
    for (int i = 0; i < msg->data_length; i++) {
        crc = crc4_lookup[crc ^ msg->data_nibbles[i]];
    }

    // Version-specific finalization
    if (version == SENT_2010 || version == SENT_2016) {
        crc = crc4_lookup[crc ^ 0x0];  // Extra round
    }

    return crc & 0x0F;
}
```

**Lookup Table:**
```c
static const uint8_t crc4_lookup[16] = {
    0x0, 0xD, 0x7, 0xA, 0xE, 0x3, 0x9, 0x4,
    0x5, 0x8, 0x2, 0xF, 0xB, 0x6, 0xC, 0x1
};
```

### CRC6 (Slow Channel)

Similar structure but 6-bit CRC with different polynomial:

```c
uint8_t SENT_calc_crc_slow(SENTSlowMsg_t *const msg, ...) {
    uint8_t crc = SENT_CRC6_SEED;  // 0x15

    // Process message ID and data based on type
    // ...

    return crc & 0x3F;  // 6 bits
}
```

**CRC Validation Flow:**
```
Message Received
      ↓
Extract CRC from message
      ↓
Calculate expected CRC
      ↓
Compare: received == calculated
      ↓
┌──────────┬───────────┐
│  Match   │  Mismatch │
│          │           │
│ Invoke   │  Discard  │
│ Callback │  message  │
└──────────┴───────────┘
```

---

## Memory Management

### Static Allocation Only

The library uses zero dynamic memory allocation:

- All handles allocated by user
- All buffers are part of handle structures
- No malloc/free calls
- Predictable memory usage

**Memory Layout Example:**

```c
// User allocates handles
SENTRxHandle_t rx_handle;  // On stack or global

// Handle contains all necessary buffers
struct SENTRxHandle {
    SENTHandle_t base;             // ~40 bytes
    SENTPhysMsg_t msg_buffer;      // ~48 bytes
    SENTSlowMsg_t slow_msg_buffer; // ~16 bytes
    uint32_t slow_channel_buffers; // ~8 bytes
    // ... other fields
};
// Total: ~120 bytes per receiver
```

### Buffer Management

**Receiver Buffers:**
- `msg_buffer`: Temporary storage for physical message
- `slow_msg_buffer`: Assembly area for slow channel
- `slow_channel_buffer_bit2/3`: Bit accumulation

**Transmitter Buffers:**
- `msg_buffer`: Encoded physical message
- `dma_buffer`: DMA transfer buffer (aligned)

**Alignment:**
DMA buffers are properly aligned via union:
```c
union {
    uint16_t u16[...];  // Naturally 2-byte aligned
    uint32_t u32[...];  // Naturally 4-byte aligned
} dma_buffer;
```

---

## Performance Considerations

### CPU Usage

**Receiver:**
- Interrupt per pulse (9-10 per message for 6 data nibbles)
- Each interrupt: ~50-100 cycles of processing
- Total per message: < 1000 cycles
- At 1 kHz message rate: < 0.1% CPU on 100 MHz MCU

**Transmitter:**
- DMA handles transmission automatically
- CPU only involved at message boundaries
- Interrupt when message completes
- Minimal CPU overhead

### Interrupt Priority

Recommended priorities (lower number = higher priority):

```
Priority 0-4:   Critical system (SysTick, etc.)
Priority 5-7:   SENT timers (moderate priority)
Priority 8-10:  Other peripherals
Priority 11-15: Low priority tasks
```

SENT should have moderate priority to ensure timing accuracy while not blocking critical interrupts.

### Optimization Techniques

1. **Lookup Tables for CRC**: O(1) per nibble vs O(n) calculation
2. **DMA for TX**: Zero CPU during transmission
3. **Inline Functions**: Timer utility macros are inline
4. **Minimal Validation**: Only essential checks in hot paths
5. **Callback Architecture**: User controls processing burden

---

## Extending the Library

### Adding New Protocol Versions

1. **Add enum value:**
   ```c
   typedef enum {
       // ...
       SENT_2024 = 5  // New version
   } SENTProtocolVersion_t;
   ```

2. **Update CRC calculation:**
   ```c
   uint8_t SENT_calc_crc(...) {
       // ...
       if (version == SENT_2024) {
           // New CRC finalization
       }
   }
   ```

3. **Update slow channel handling if needed**

### Adding Custom Message Types

Extend `SENTSlowMsg_t`:

```c
typedef struct {
    SENTSlowMsgType_t type;
    union {
        // ... existing types ...
        struct {
            // Your custom format
        } slow_custom;
    } as;
} SENTSlowMsg_t;
```

### Porting to Non-STM32 Platforms

1. **Replace STM32 HAL calls** in:
   - `sent_rx.c`: Timer input capture functions
   - `sent_tx.c`: Timer PWM and DMA functions
   - `timer_utils.c`: Clock frequency calculation

2. **Keep core module unchanged** - protocol logic is portable

3. **Create platform-specific timer utilities**

---

## Implementation Notes

### Key Files and Functions

**Receiver Critical Path:**
- `sent_rx.c:SENTRx_InputCaptureCallback()` - Main ISR handler
- `sent_rx.c:SENTRx_SlowChannelFSM()` - Slow channel processing
- `sent.c:SENT_decodePhysMsg()` - Message decoding

**Transmitter Critical Path:**
- `sent_tx.c:SENTTx_AutoReloadCallback()` - Message preparation
- `sent_tx.c:SENTTx_SlowChannelFSM()` - Slow bit stuffing
- `sent.c:SENT_encodePhysMsg()` - Message encoding

**Core Functions:**
- `sent.c:SENT_calc_crc()` - Fast channel CRC (line ~120)
- `sent.c:SENT_calc_crc_slow()` - Slow channel CRC (line ~180)
- `sent.c:SENT_init()` - Base initialization (line ~80)

### Design Tradeoffs

**Memory vs Speed:**
- Chose lookup tables for CRC (64 bytes for 4-bit, 128 bytes for 6-bit)
- Alternative: Calculate CRC inline (slower but zero memory)

**Flexibility vs Simplicity:**
- Callback-based design adds indirection
- Alternative: Polling-based (simpler but less flexible)

**Validation vs Performance:**
- CRC validated before callback invocation
- Could expose unvalidated messages for advanced users

---

## Summary

The STM32 SENT library architecture provides:

✅ **Modular design** - Independent RX, TX, and core modules
✅ **Efficient operation** - Interrupt + DMA driven
✅ **Zero allocation** - All memory statically allocated
✅ **Portable** - HAL-based hardware abstraction
✅ **Extensible** - Easy to add versions and features
✅ **Well-tested** - Clear state machines and validation

For implementation details, see the source code with inline comments.
For usage examples, see [EXAMPLES.md](EXAMPLES.md).
For API details, see [API_REFERENCE.md](API_REFERENCE.md).
