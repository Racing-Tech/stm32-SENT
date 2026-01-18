# STM32 SENT Protocol Library

A complete implementation of the SENT (Single Edge Nibble Transmission) protocol for STM32 microcontrollers, providing both transmitter and receiver functionality with support for multiple protocol versions.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [File Structure](#file-structure)
- [Protocol Versions](#protocol-versions)
- [Hardware Requirements](#hardware-requirements)
- [Dependencies](#dependencies)
- [License](#license)
- [Contributing](#contributing)

## Overview

This library implements the SENT (Single Edge Nibble Transmission) protocol, a point-to-point communication standard commonly used in automotive applications for transmitting sensor data. The library is designed for STM32 microcontrollers and integrates seamlessly with STM32 HAL (Hardware Abstraction Layer).

**Author:** Racing Tech (Squadra Corse firmware team)
**License:** MIT
**Copyright:** 2024

### What is SENT?

SENT is a unidirectional communication protocol that transmits data using pulse-width modulation. It consists of:
- **Fast Channel**: Primary data transmission with status nibble + up to 6 data nibbles + CRC
- **Slow Channel**: Optional auxiliary data transmission for configuration or diagnostic information
- **Synchronization**: Automatic frame synchronization using fixed-width sync pulses

## Features

### Core Features
- **Full SENT Protocol Support**: Implements complete SENT specification including fast and slow channels
- **Multiple Protocol Versions**: Supports SENT 2007, 2008, 2010, and 2016 standards
- **Bidirectional Communication**: Separate transmitter (TX) and receiver (RX) implementations
- **CRC Validation**: Automatic CRC4 and CRC6 calculation and validation
- **Flexible Architecture**: Callback-based design for easy integration

### Receiver (RX) Features
- Input capture-based reception using STM32 timers
- Automatic synchronization detection with configurable tolerance
- Separate callbacks for fast and slow channel messages
- State machine-based robust message decoding
- Support for SHORT, ENHANCED12, and ENHANCED16 slow channel formats

### Transmitter (TX) Features
- DMA-based transmission for efficient CPU usage
- PWM output using STM32 timers
- Automatic message encoding and transmission
- Support for both 16-bit and 32-bit timers
- Callback notifications on transmission completion

### Timer Utilities
- Automatic timer clock frequency detection
- APB1/APB2 bus placement detection
- Conversion macros for ticks/time calculations
- Support for different timer resolutions

## Quick Start

### Basic Receiver Example

```c
#include "sent_rx.h"

// Callback for fast channel messages
void FastChannelCallback(SENTMsg_t *const msg, void *user_data) {
    // Process received fast channel data
    uint8_t status = msg->status_nibble;
    uint8_t *data = msg->data_nibbles;
    // Your processing logic here
}

// Callback for slow channel messages
void SlowChannelCallback(SENTSlowMsg_t *const msg, void *user_data) {
    // Process received slow channel data
    uint16_t slow_data = msg->data;
    // Your processing logic here
}

int main(void) {
    // Initialize HAL and peripherals
    HAL_Init();
    SystemClock_Config();
    MX_TIM1_Init();  // Configure TIM1 for input capture

    // Initialize SENT receiver
    SENTRxHandle_t sent_rx;
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .version = SENT_VERSION_2010
    };

    if (SENTRx_init(&sent_rx, &init, FastChannelCallback, SlowChannelCallback, NULL) == 0) {
        Error_Handler();
    }

    // Start receiving
    SENTRx_start(&sent_rx);

    while (1) {
        // Main loop - callbacks handle received data
    }
}

// Timer input capture callback (call from HAL_TIM_IC_CaptureCallback)
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTRx_InputCaptureCallback(&sent_rx);
    }
}
```

### Basic Transmitter Example

```c
#include "sent_tx.h"

SENTMsg_t tx_message;
SENTSlowMsg_t slow_message;

// Callback when message transmission completes
void TxCompleteCallback(void *user_data) {
    // Update message data for next transmission
    // This is called after each message is sent
}

int main(void) {
    // Initialize HAL and peripherals
    HAL_Init();
    SystemClock_Config();
    MX_TIM1_Init();  // Configure TIM1 for PWM with DMA

    // Prepare message
    uint8_t data[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
    SENT_msg_init(&tx_message, 0x5, data, 6, SENT_VERSION_2010);

    // Initialize SENT transmitter
    SENTTxHandle_t sent_tx;
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .version = SENT_VERSION_2010
    };

    if (SENTTx_init(&sent_tx, &init, &tx_message, TxCompleteCallback, NULL, NULL) == 0) {
        Error_Handler();
    }

    // Start transmitting (uses DMA for continuous transmission)
    SENTTx_start(&sent_tx);

    while (1) {
        // Main loop - transmission happens via DMA and callbacks
    }
}
```

## Documentation

For detailed information, please refer to:

- **[GETTING_STARTED.md](docs/GETTING_STARTED.md)** - Step-by-step setup guide and configuration
- **[API_REFERENCE.md](docs/API_REFERENCE.md)** - Complete API documentation for all functions
- **[EXAMPLES.md](docs/EXAMPLES.md)** - Comprehensive usage examples and use cases
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - Internal design and architecture documentation

## File Structure

```
stm32-SENT/
├── Inc/                          # Header files
│   ├── sent.h                   # Core SENT protocol definitions and utilities
│   ├── sent_rx.h                # SENT receiver API and structures
│   ├── sent_tx.h                # SENT transmitter API and structures
│   └── timer_utils.h            # Timer utility macros and functions
├── Src/                          # Implementation files
│   ├── sent.c                   # Core SENT protocol implementation
│   ├── sent_rx.c                # SENT receiver implementation
│   ├── sent_tx.c                # SENT transmitter implementation
│   └── timer_utils.c            # Timer utility implementation
├── docs/                         # Documentation
│   ├── API_REFERENCE.md         # Detailed API reference
│   ├── GETTING_STARTED.md       # Setup and configuration guide
│   ├── EXAMPLES.md              # Usage examples
│   └── ARCHITECTURE.md          # Design and architecture
├── README.md                     # This file
└── LICENSE                       # MIT License
```

### Module Descriptions

#### Core Module (`sent.h`/`sent.c`)
Provides fundamental SENT protocol functionality:
- Message structure definitions (`SENTMsg_t`, `SENTSlowMsg_t`, `SENTPhysMsg_t`)
- CRC calculation for both fast and slow channels
- Message encoding and decoding between logical and physical formats
- Protocol version management
- Base handle initialization

#### Receiver Module (`sent_rx.h`/`sent_rx.c`)
Implements SENT message reception:
- Input capture-based pulse timing measurement
- Automatic synchronization and frame detection
- State machine for robust message decoding
- Separate processing for fast and slow channels
- Callback notifications on message reception

#### Transmitter Module (`sent_tx.h`/`sent_tx.c`)
Implements SENT message transmission:
- PWM output with DMA for efficient transmission
- Automatic message encoding and formatting
- Slow channel state machine for auxiliary data
- Callback notifications on transmission events
- Support for continuous or single-shot transmission

#### Timer Utilities (`timer_utils.h`/`timer_utils.c`)
STM32 HAL integration utilities:
- Timer clock frequency calculation
- APB bus placement detection
- Time/tick conversion macros
- Timer resolution detection

## Protocol Versions

This library supports multiple SENT protocol versions:

| Version | Year | Features | CRC |
|---------|------|----------|-----|
| `SENT_VERSION_2007` | 2007 | Basic fast channel | CRC4 |
| `SENT_VERSION_2008` | 2008 | Fast + slow channel | CRC4 + CRC6 |
| `SENT_VERSION_2010` | 2010 | Enhanced slow channel (12-bit) | CRC4 + CRC6 |
| `SENT_VERSION_2016` | 2016 | Enhanced slow channel (16-bit) | CRC4 + CRC6 |

### Slow Channel Message Types

- **SHORT**: 8-bit ID + 4-bit data (SENT 2008)
- **ENHANCED12**: 8-bit ID + 12-bit data (SENT 2010)
- **ENHANCED16**: 8-bit ID + 16-bit data (SENT 2016)

## Hardware Requirements

### For Receiver (RX)
- STM32 microcontroller with HAL library
- One timer with input capture capability
- Timer channel configured for input capture mode
- Input pin connected to SENT signal source

**Recommended Timer Configuration:**
- Input capture on rising edge
- Prescaler configured for adequate resolution (tick = 3-90 μs)
- Channel configured for capture events

### For Transmitter (TX)
- STM32 microcontroller with HAL library
- One timer with PWM capability and DMA support
- Timer channel configured for PWM output mode
- DMA channel configured for memory-to-peripheral transfer

**Recommended Timer Configuration:**
- PWM mode 1 or 2
- DMA burst mode for compare register updates
- Auto-reload and compare values managed by library
- Output pin configured as alternate function

### Timing Requirements
- **Tick Unit**: 3-90 μs (library calculates automatically)
- **Sync Pulse**: 56 ticks
- **Pause Pulse**: 77 ticks (optional)
- **Data Nibble**: 12-27 ticks (representing values 0-15)

## Dependencies

- **STM32 HAL Library**: Required for timer and DMA operations
- **ARM CMSIS**: Standard Cortex-M headers
- **C Standard Library**: Standard C functions (minimal usage)

**Compiler Requirements:**
- C99 or later
- ARM GCC or ARM Compiler 6

**Tested Platforms:**
- STM32F4 series
- STM32F7 series
- Should work with any STM32 series with compatible timers

## Integration Steps

1. **Add Files**: Copy `Inc/` and `Src/` directories to your project
2. **Configure Timer**: Set up timer peripheral using STM32CubeMX or manually
3. **Include Headers**: Include `sent_rx.h` or `sent_tx.h` in your application
4. **Initialize**: Call initialization functions with proper configuration
5. **Implement Callbacks**: Provide callback functions for message handling
6. **Start Operation**: Call start functions to begin RX/TX operation
7. **Handle Events**: Call appropriate callbacks from HAL interrupt handlers

See [GETTING_STARTED.md](docs/GETTING_STARTED.md) for detailed integration instructions.

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.

```
Copyright (c) 2024 Racing Tech - Squadra Corse firmware team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

## Contributing

Contributions are welcome! This library was developed for the Racing Tech Squadra Corse team, but we're happy to accept improvements and bug fixes.

### How to Contribute
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Make your changes with clear commit messages
4. Test thoroughly on actual hardware
5. Submit a pull request

### Coding Standards
- Follow existing code style (K&R with modifications)
- Document all public APIs with doxygen-style comments
- Keep functions focused and modular
- Test on real hardware before submitting
- Update documentation for any API changes

### Reporting Issues
Please report bugs and feature requests through the issue tracker. Include:
- STM32 model and HAL version
- Code snippet demonstrating the issue
- Expected vs actual behavior
- Any relevant error messages

## Support and Contact

For questions, issues, or contributions:
- **Issues**: Use the GitHub issue tracker
- **Team**: Racing Tech - Squadra Corse firmware team
- **Repository**: [GitHub Repository URL]

## Acknowledgments

This library implements the SENT protocol as specified by the SAE J2716 standard. Special thanks to the Racing Tech Squadra Corse team for development and testing.
