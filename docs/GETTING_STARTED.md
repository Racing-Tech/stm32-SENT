# Getting Started Guide

This guide will walk you through integrating the STM32 SENT library into your project and configuring it for both receiver and transmitter operation.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Project Integration](#project-integration)
- [Hardware Setup](#hardware-setup)
- [STM32CubeMX Configuration](#stm32cubemx-configuration)
- [Software Integration](#software-integration)
- [Receiver Setup](#receiver-setup)
- [Transmitter Setup](#transmitter-setup)
- [Testing and Validation](#testing-and-validation)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Requirements

- **STM32 microcontroller** with at least one advanced timer (TIM1-TIM8 recommended)
- For RX: Timer with input capture capability
- For TX: Timer with PWM and DMA capability
- **Development board** or custom PCB
- **SENT signal source** (for receiver testing) or **SENT receiver device** (for transmitter testing)
- **Logic analyzer or oscilloscope** (recommended for debugging)

### Software Requirements

- **STM32CubeIDE** or your preferred IDE (Keil, IAR, etc.)
- **STM32CubeMX** for peripheral configuration (optional but recommended)
- **STM32 HAL library** for your specific MCU family
- **ARM GCC toolchain** or ARM Compiler 6

### Knowledge Requirements

- Basic understanding of STM32 HAL
- Familiarity with timers and interrupts
- Understanding of the SENT protocol (recommended)

---

## Project Integration

### Step 1: Add Library Files to Your Project

1. **Copy the library files** to your project:
   ```
   YourProject/
   ├── Core/
   │   ├── Inc/
   │   │   ├── sent.h          (copy here)
   │   │   ├── sent_rx.h       (copy here)
   │   │   ├── sent_tx.h       (copy here)
   │   │   └── timer_utils.h   (copy here)
   │   └── Src/
   │       ├── sent.c          (copy here)
   │       ├── sent_rx.c       (copy here)
   │       ├── sent_tx.c       (copy here)
   │       └── timer_utils.c   (copy here)
   ```

2. **Verify include paths**: Ensure your IDE includes the `Core/Inc/` directory in the include path.

3. **Build the project** to verify all files compile correctly.

### Step 2: Add Required Includes

In your `main.h` file, ensure you have:

```c
#include "stm32f4xx_hal.h"  // Replace with your MCU family
```

In your application code:

```c
#include "sent_rx.h"  // For receiver functionality
// OR
#include "sent_tx.h"  // For transmitter functionality
```

---

## Hardware Setup

### Pin Configuration

#### For Receiver (RX)

- **Timer Input Capture Pin**: Connect your SENT signal source to a timer channel pin
  - Example: PA8 for TIM1_CH1
  - Configure as **Alternate Function** (AF) in GPIO settings
  - **Pull**: No pull-up or pull-down (depends on your signal source)
  - **Speed**: High

#### For Transmitter (TX)

- **Timer PWM Output Pin**: Connect to your SENT receiver device
  - Example: PA8 for TIM1_CH1
  - Configure as **Alternate Function** (AF)
  - **Pull**: No pull
  - **Speed**: High or Very High

### Electrical Considerations

- **Voltage Levels**: Ensure compatibility (typically 5V for automotive SENT)
- **Signal Conditioning**: May need level shifters if using 3.3V MCU with 5V sensors
- **Termination**: Follow SENT specification (typically no termination resistor needed)
- **Grounding**: Ensure proper ground connection between devices

---

## STM32CubeMX Configuration

### For Receiver (RX)

#### Timer Configuration (e.g., TIM1)

1. **Enable Timer**:
   - Select TIM1 in the peripherals tree
   - Set Clock Source to "Internal Clock"

2. **Configure Input Capture Channel**:
   - Channel 1: Set to "Input Capture direct mode"
   - Polarity: "Rising Edge"
   - Prescaler: None (DIV1)
   - Filter: 0 (or small value like 3 for noise rejection)

3. **Configure Timer Parameters**:
   ```
   Prescaler: Calculate based on desired tick unit
   Counter Period (ARR): 65535 (max for 16-bit) or 0xFFFFFFFF (for 32-bit)
   Internal Clock Division: No Division
   Counter Mode: Up
   ```

   **Example Calculation for Tick Unit = 3 μs:**
   - If APB timer clock = 168 MHz
   - Desired tick unit = 3 μs
   - Timer frequency needed = 1/(3 μs) = 333.33 kHz
   - Prescaler = (168 MHz / 333.33 kHz) - 1 = 503

4. **Enable Interrupts**:
   - Check "CC1 interrupt" under NVIC Settings
   - Set appropriate interrupt priority (e.g., priority 5)

5. **DMA** (not needed for RX)

#### GPIO Configuration

- Pin used for input capture: Set to Alternate Function
- Pull: No pull-up/pull-down (or as required by your signal)
- Speed: High

### For Transmitter (TX)

#### Timer Configuration (e.g., TIM1)

1. **Enable Timer**:
   - Select TIM1 in the peripherals tree
   - Set Clock Source to "Internal Clock"

2. **Configure PWM Channel**:
   - Channel 1: Set to "PWM Generation CH1"
   - Mode: PWM mode 1
   - Pulse: Will be set by library
   - Fast Mode: Disable
   - CH Polarity: High

3. **Configure Timer Parameters**:
   ```
   Prescaler: Calculate based on desired tick unit
   Counter Period (ARR): Will be set dynamically by library
   Internal Clock Division: No Division
   Counter Mode: Up
   Auto-reload preload: Enable
   ```

4. **Enable DMA**:
   - Add DMA Request for "TIM1_UP" (Update event)
   - Mode: Normal
   - Direction: Memory to Peripheral
   - Peripheral Increment: Disable
   - Memory Increment: Enable
   - Peripheral Data Width: Half Word (16-bit) or Word (32-bit) based on timer
   - Memory Data Width: Half Word (16-bit) or Word (32-bit) - same as peripheral
   - Priority: High or Very High

5. **Enable Interrupts**:
   - Enable "Update interrupt" under NVIC Settings
   - Enable "Capture Compare 1 interrupt"
   - Enable DMA interrupts if desired
   - Set appropriate priorities

#### GPIO Configuration

- Pin used for PWM output: Set to Alternate Function
- Pull: No pull
- Speed: Very High

### Clock Configuration

Ensure your system clock and timer clocks are configured correctly:
- Set system clock to desired frequency (e.g., 168 MHz for STM32F4)
- Verify APB1 and APB2 prescalers
- Note the actual timer clock frequency (will be used in software configuration)

**Generate Code** after configuration.

---

## Software Integration

### Global Variables and Handles

In your `main.c` or dedicated module:

```c
// For Receiver
SENTRxHandle_t sent_rx_handle;

// For Transmitter
SENTTxHandle_t sent_tx_handle;
SENTMsg_t sent_tx_message;  // Message to transmit
SENTSlowMsg_t sent_slow_message;  // Optional slow channel message
```

---

## Receiver Setup

### Step 1: Implement Callbacks

Create callback functions for message reception:

```c
/**
 * @brief Callback for fast channel messages
 */
void SENT_FastChannelCallback(SENTRxHandle_t *const handle,
                               SENTMsg_t *const msg,
                               void *user_data) {
    // Message is already CRC validated
    // Process the received data

    // Example: Copy to a buffer
    uint8_t status = msg->status_nibble;
    uint8_t data[6];
    memcpy(data, msg->data_nibbles, msg->data_length);

    // Your processing logic here
    // Note: This is called from interrupt context, keep it short!
}

/**
 * @brief Callback for slow channel messages
 */
void SENT_SlowChannelCallback(SENTRxHandle_t *const handle,
                               SENTSlowMsg_t *const msg,
                               void *user_data) {
    // Process slow channel data
    if (msg->type == SENT_SLOWTYPE_ENHANCED12) {
        uint8_t id = msg->as.slow_enhanced.id;
        uint16_t data = msg->as.slow_enhanced.data;
        // Process ID and data
    }
}
```

### Step 2: Initialize SENT Receiver

In your `main()` function, after HAL initialization:

```c
int main(void) {
    // HAL initialization
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();  // Timer for SENT RX

    // Prepare SENT initialization structure
    SENTHandleInit_t sent_init = {
        .htim = &htim1,                    // Timer handle
        .channel = TIM_CHANNEL_1,          // Timer channel
        .has_pause = 0,                    // Set to 1 if pause frame is used
        .data_length = 6,                  // Number of data nibbles
        .tick_unit_time = 0.003,           // 3 μs tick unit (in milliseconds)
        .version = SENT_VERSION_2010       // Protocol version
    };

    // Initialize SENT receiver
    if (!SENTRx_init(&sent_rx_handle, &sent_init,
                     SENT_FastChannelCallback,
                     SENT_SlowChannelCallback,
                     NULL)) {  // user_data pointer (optional)
        // Initialization failed
        Error_Handler();
    }

    // Start SENT receiver
    if (!SENTRx_start(&sent_rx_handle)) {
        // Start failed
        Error_Handler();
    }

    // Main loop
    while (1) {
        // Your application code
        // SENT messages are handled in callbacks
    }
}
```

### Step 3: Connect HAL Callback

Connect the HAL timer input capture callback to SENT library:

```c
/**
 * @brief Timer Input Capture Callback
 * @note This is called by HAL when input capture event occurs
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        // Route to SENT receiver
        SENTRx_InputCaptureCallback(&sent_rx_handle);
    }
}
```

### Step 4: Optional - User Data

You can pass a pointer to your data structure through `user_data`:

```c
typedef struct {
    uint32_t message_count;
    uint8_t last_status;
    uint8_t latest_data[6];
} MyAppData_t;

MyAppData_t app_data = {0};

// During init
SENTRx_init(&sent_rx_handle, &sent_init,
            SENT_FastChannelCallback,
            SENT_SlowChannelCallback,
            &app_data);  // Pass your data

// In callback
void SENT_FastChannelCallback(SENTRxHandle_t *const handle,
                               SENTMsg_t *const msg,
                               void *user_data) {
    MyAppData_t *app = (MyAppData_t *)user_data;
    app->message_count++;
    app->last_status = msg->status_nibble;
    memcpy(app->latest_data, msg->data_nibbles, msg->data_length);
}
```

---

## Transmitter Setup

### Step 1: Initialize Message Data

Create and initialize the message you want to transmit:

```c
SENTMsg_t sent_tx_message;
SENTSlowMsg_t sent_slow_message;  // Optional

// Initialize fast channel message
uint8_t data_nibbles[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
SENT_msg_init(&sent_tx_message, 0x5, data_nibbles, 6, SENT_VERSION_2010);
// CRC is automatically calculated

// Optional: Initialize slow channel message
sent_slow_message.type = SENT_SLOWTYPE_ENHANCED12;
sent_slow_message.as.slow_enhanced.id = 0x42;
sent_slow_message.as.slow_enhanced.data = 0x123;
```

### Step 2: Implement Callbacks (Optional)

Create callbacks to update messages or get notifications:

```c
/**
 * @brief Called after each fast channel message transmission
 */
void SENT_TxCompleteCallback(SENTTxHandle_t *const handle) {
    // Update message data for next transmission
    // Example: Increment first data nibble
    static uint8_t counter = 0;
    handle->msg_source->data_nibbles[0] = counter++;
    counter &= 0x0F;  // Keep in nibble range (0-15)

    // Note: CRC is recalculated automatically before transmission
}

/**
 * @brief Called after slow channel message transmission
 */
void SENT_SlowTxCompleteCallback(SENTTxHandle_t *const handle) {
    // Update slow channel message if needed
}
```

### Step 3: Initialize SENT Transmitter

```c
int main(void) {
    // HAL initialization
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();   // Initialize DMA first!
    MX_TIM1_Init();  // Timer for SENT TX

    // Prepare message
    uint8_t data[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
    SENT_msg_init(&sent_tx_message, 0x5, data, 6, SENT_VERSION_2010);

    // Prepare SENT initialization structure
    SENTHandleInit_t sent_init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,  // 3 μs
        .version = SENT_VERSION_2010
    };

    // Initialize SENT transmitter
    if (!SENTTx_init(&sent_tx_handle,
                     &sent_init,
                     &sent_tx_message,              // Message source pointer
                     SENT_TxCompleteCallback,       // Can be NULL
                     NULL,                          // Slow message (NULL if not used)
                     NULL)) {                       // Slow callback (NULL if not used)
        Error_Handler();
    }

    // Start SENT transmitter
    if (!SENTTx_start(&sent_tx_handle)) {
        Error_Handler();
    }

    // Main loop
    while (1) {
        // Transmission happens automatically via DMA
        // Update message in callback or main loop as needed
    }
}
```

### Step 4: Connect HAL Callbacks

```c
/**
 * @brief Timer Period Elapsed Callback (Auto-Reload)
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_AutoReloadCallback(&sent_tx_handle);
    }
}

/**
 * @brief Timer PWM Pulse Finished Callback
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_CompareCallback(&sent_tx_handle);
    }
}
```

### Step 5: Update Messages Dynamically

You can update messages in the main loop or in callbacks:

```c
// In main loop
while (1) {
    // Update message data
    sent_tx_message.data_nibbles[0] = sensor_read();
    sent_tx_message.data_nibbles[1] = get_temperature();
    // CRC is recalculated automatically before transmission

    HAL_Delay(100);  // Update rate
}
```

---

## Testing and Validation

### Using a Logic Analyzer

1. **Connect** logic analyzer to SENT signal pin
2. **Configure** sampling rate: At least 10x the expected tick rate (e.g., 10 MHz for 3 μs ticks)
3. **Capture** SENT frames
4. **Analyze** timing:
   - Sync pulse: Should be ~56 ticks
   - Data nibbles: 12-27 ticks
   - Verify CRC calculation

### Using an Oscilloscope

1. **Trigger** on the sync pulse (longest pulse)
2. **Measure** pulse widths:
   - Each tick should be consistent (e.g., 3 μs ± tolerance)
   - Sync pulse: 56 ticks × tick_unit
   - Data nibbles: (12 + value) ticks × tick_unit

### Verifying CRC

The library automatically calculates and validates CRC. If callbacks are not being invoked:
- Check that CRC validation is passing
- Verify protocol version matches between TX and RX
- Check tick timing accuracy

### Common Test Points

```c
// In receiver callback
void SENT_FastChannelCallback(SENTRxHandle_t *const handle,
                               SENTMsg_t *const msg,
                               void *user_data) {
    // Toggle LED to indicate message reception
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

    // Log via UART for debugging
    printf("Status: 0x%X, Data: ", msg->status_nibble);
    for (int i = 0; i < msg->data_length; i++) {
        printf("0x%X ", msg->data_nibbles[i]);
    }
    printf("\n");
}
```

---

## Troubleshooting

### Receiver Not Getting Messages

**Problem:** Callbacks are not being called

**Solutions:**
1. **Verify hardware connection**: Check signal is reaching MCU pin
2. **Check timer configuration**: Ensure input capture is enabled
3. **Verify interrupt priority**: Timer IC interrupt should be enabled
4. **Check tick timing**: Use oscilloscope to verify tick unit matches configuration
5. **Verify protocol version**: Must match between TX and RX
6. **Check CRC**: If CRC fails, callback won't be invoked

**Debug Steps:**
```c
// Add debug code in SENTRx_InputCaptureCallback (in sent_rx.c)
// to verify it's being called
void SENTRx_InputCaptureCallback(SENTRxHandle_t *const handle) {
    HAL_GPIO_TogglePin(DEBUG_GPIO_Port, DEBUG_Pin);  // Add this
    // ... rest of function
}
```

### Transmitter Not Generating Correct Signal

**Problem:** Output signal is incorrect or missing

**Solutions:**
1. **Verify DMA configuration**: Must be enabled and configured correctly
2. **Check timer PWM mode**: Should be in PWM mode 1 or 2
3. **Verify timer frequency**: Calculate expected output and measure
4. **Check DMA priority**: Should be high
5. **Verify prescaler calculation**: Tick unit must be correct

**Timing Verification:**
```c
// Expected sync pulse duration
float sync_duration_us = SENT_SYNC_TICKS * tick_unit_time * 1000;
// For tick_unit = 0.003 ms: sync = 56 * 0.003 * 1000 = 168 μs
```

### CRC Errors

**Problem:** Messages failing CRC validation

**Solutions:**
1. **Verify protocol version**: Both ends must use same version
2. **Check tick accuracy**: Timing errors can cause nibble decoding errors
3. **Verify data integrity**: Check signal quality with oscilloscope
4. **Review noise**: Add filtering if electrical noise is present

### Performance Issues

**Problem:** System is slow or missing messages

**Solutions:**
1. **Keep callbacks short**: Move heavy processing to main loop
2. **Use appropriate interrupt priorities**: SENT should have reasonable priority
3. **Optimize data copying**: Use DMA or efficient memory operations
4. **Check CPU load**: Ensure system is not overloaded

### DMA Issues (Transmitter)

**Problem:** DMA not transferring data correctly

**Solutions:**
1. **Initialize DMA before timer**: Call `MX_DMA_Init()` before `MX_TIMx_Init()`
2. **Verify DMA configuration**: Check direction, width, increment settings
3. **Check memory alignment**: Ensure buffers are properly aligned
4. **Verify DMA channel**: Correct DMA channel for timer update event

---

## Next Steps

- Review **[EXAMPLES.md](EXAMPLES.md)** for more complete code examples
- Check **[API_REFERENCE.md](API_REFERENCE.md)** for detailed function documentation
- Read **[ARCHITECTURE.md](ARCHITECTURE.md)** to understand internal implementation

## Additional Resources

- SAE J2716 SENT Protocol Specification
- STM32 Timer documentation for your specific MCU family
- STM32 DMA documentation
- [Main README](../README.md)

---

## Support

If you encounter issues:
1. Check this troubleshooting section
2. Review the examples
3. Verify your hardware setup
4. Check the API reference for correct usage
5. Report issues with detailed information (MCU model, config, observed behavior)
