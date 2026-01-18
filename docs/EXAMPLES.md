# SENT Library Examples

Complete, ready-to-use code examples for various SENT protocol use cases.

## Table of Contents

- [Basic Examples](#basic-examples)
  - [Simple Receiver](#simple-receiver)
  - [Simple Transmitter](#simple-transmitter)
- [Advanced Examples](#advanced-examples)
  - [Receiver with Data Processing](#receiver-with-data-processing)
  - [Transmitter with Sensor Data](#transmitter-with-sensor-data)
  - [Bidirectional Communication](#bidirectional-communication)
  - [Slow Channel Usage](#slow-channel-usage)
- [Complete Applications](#complete-applications)
  - [Temperature Sensor Receiver](#temperature-sensor-receiver)
  - [Multi-Channel Data Transmitter](#multi-channel-data-transmitter)
  - [Diagnostic Data via Slow Channel](#diagnostic-data-via-slow-channel)
- [Integration Examples](#integration-examples)
  - [FreeRTOS Integration](#freertos-integration)
  - [Multiple SENT Channels](#multiple-sent-channels)
- [Testing and Debugging](#testing-and-debugging)
  - [Loopback Test](#loopback-test)
  - [Signal Quality Monitoring](#signal-quality-monitoring)

---

## Basic Examples

### Simple Receiver

Minimal receiver example for receiving SENT fast channel messages.

```c
/**
 * @file simple_receiver.c
 * @brief Minimal SENT receiver example
 */

#include "main.h"
#include "sent_rx.h"
#include <stdio.h>

// External handles from main.c
extern TIM_HandleTypeDef htim1;

// SENT receiver handle
SENTRxHandle_t sent_rx;

// Fast channel callback
void FastChannelCallback(SENTRxHandle_t *const handle,
                        SENTMsg_t *const msg,
                        void *user_data) {
    // Toggle LED on message reception
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

    // Print received data
    printf("Status: 0x%X | Data: ", msg->status_nibble);
    for (int i = 0; i < msg->data_length; i++) {
        printf("0x%X ", msg->data_nibbles[i]);
    }
    printf("\n");
}

// Initialize SENT receiver
void SENT_Receiver_Init(void) {
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,  // 3 microseconds
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx, &init, FastChannelCallback, NULL, NULL)) {
        Error_Handler();
    }

    if (!SENTRx_start(&sent_rx)) {
        Error_Handler();
    }
}

// HAL Timer Input Capture Callback
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTRx_InputCaptureCallback(&sent_rx);
    }
}

// Main function
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_USART2_UART_Init();  // For printf

    SENT_Receiver_Init();

    while (1) {
        // Main loop - messages handled in callback
        HAL_Delay(1000);
    }
}
```

---

### Simple Transmitter

Minimal transmitter example for sending SENT fast channel messages.

```c
/**
 * @file simple_transmitter.c
 * @brief Minimal SENT transmitter example
 */

#include "main.h"
#include "sent_tx.h"

// External handles from main.c
extern TIM_HandleTypeDef htim1;

// SENT transmitter handle and message
SENTTxHandle_t sent_tx;
SENTMsg_t tx_message;

// Transmission complete callback
void TxCompleteCallback(SENTTxHandle_t *const handle) {
    // Update message for next transmission
    static uint8_t counter = 0;
    handle->msg_source->data_nibbles[0] = counter++;
    counter &= 0x0F;  // Keep in 0-15 range

    // Toggle LED on each transmission
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
}

// Initialize SENT transmitter
void SENT_Transmitter_Init(void) {
    // Prepare message data
    uint8_t data[6] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5};
    SENT_msg_init(&tx_message, 0x5, data, 6, SENT_VERSION_2010);

    // Initialize transmitter
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,  // 3 microseconds
        .version = SENT_VERSION_2010
    };

    if (!SENTTx_init(&sent_tx, &init, &tx_message, TxCompleteCallback, NULL, NULL)) {
        Error_Handler();
    }

    if (!SENTTx_start(&sent_tx)) {
        Error_Handler();
    }
}

// HAL Timer Callbacks
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_AutoReloadCallback(&sent_tx);
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_CompareCallback(&sent_tx);
    }
}

// Main function
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();   // DMA must be initialized first!
    MX_TIM1_Init();

    SENT_Transmitter_Init();

    while (1) {
        // Transmission happens automatically via DMA
        HAL_Delay(1000);
    }
}
```

---

## Advanced Examples

### Receiver with Data Processing

Receiver that processes and validates received data.

```c
/**
 * @file advanced_receiver.c
 * @brief SENT receiver with data processing and validation
 */

#include "main.h"
#include "sent_rx.h"
#include <string.h>

// Data structure for application
typedef struct {
    uint32_t message_count;
    uint32_t error_count;
    uint8_t last_status;
    uint16_t sensor_values[6];
    uint8_t data_valid;
} SensorData_t;

SensorData_t sensor_data = {0};
SENTRxHandle_t sent_rx;

// Fast channel callback with processing
void ProcessFastChannel(SENTRxHandle_t *const handle,
                       SENTMsg_t *const msg,
                       void *user_data) {
    SensorData_t *data = (SensorData_t *)user_data;

    // Increment message counter
    data->message_count++;

    // Extract status nibble
    data->last_status = msg->status_nibble;

    // Validate status (example: bits [1:0] should be 0b01 for valid data)
    if ((data->last_status & 0x03) != 0x01) {
        data->error_count++;
        data->data_valid = 0;
        return;
    }

    // Process data nibbles - convert to 12-bit values
    // Example: Two nibbles form one 12-bit sensor value
    for (int i = 0; i < 3; i++) {
        uint8_t high_nibble = msg->data_nibbles[i * 2];
        uint8_t low_nibble = msg->data_nibbles[i * 2 + 1];
        data->sensor_values[i] = (high_nibble << 8) | (low_nibble << 4);
    }

    data->data_valid = 1;

    // Trigger application event
    // osThreadFlagsSet(processing_task_id, DATA_READY_FLAG);  // For RTOS
}

// Slow channel callback
void ProcessSlowChannel(SENTRxHandle_t *const handle,
                       SENTSlowMsg_t *const msg,
                       void *user_data) {
    // Process diagnostic or configuration data
    if (msg->type == SENT_SLOWTYPE_ENHANCED12) {
        uint8_t msg_id = msg->as.slow_enhanced.id;
        uint16_t msg_data = msg->as.slow_enhanced.data;

        switch (msg_id) {
            case 0x01:  // Temperature
                // Process temperature data
                break;
            case 0x02:  // Diagnostic status
                // Process diagnostic data
                break;
            default:
                break;
        }
    }
}

// Initialize receiver
void SENT_Advanced_Receiver_Init(void) {
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx, &init,
                     ProcessFastChannel,
                     ProcessSlowChannel,
                     &sensor_data)) {  // Pass application data
        Error_Handler();
    }

    if (!SENTRx_start(&sent_rx)) {
        Error_Handler();
    }
}

// Application function to get sensor data
uint8_t GetSensorData(uint16_t *values, uint8_t *status) {
    if (!sensor_data.data_valid) {
        return 0;  // Data not valid
    }

    // Copy data in critical section
    __disable_irq();
    memcpy(values, sensor_data.sensor_values, sizeof(sensor_data.sensor_values));
    *status = sensor_data.last_status;
    __enable_irq();

    return 1;  // Success
}

// Get statistics
void GetStatistics(uint32_t *msg_count, uint32_t *err_count) {
    __disable_irq();
    *msg_count = sensor_data.message_count;
    *err_count = sensor_data.error_count;
    __enable_irq();
}
```

---

### Transmitter with Sensor Data

Transmitter that sends real sensor readings.

```c
/**
 * @file sensor_transmitter.c
 * @brief SENT transmitter with real sensor data
 */

#include "main.h"
#include "sent_tx.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

SENTTxHandle_t sent_tx;
SENTMsg_t tx_message;

// Sensor data structure
typedef struct {
    uint16_t pressure;      // 12-bit value
    uint16_t temperature;   // 12-bit value
    uint16_t voltage;       // 12-bit value
    uint8_t status;         // 4-bit status
} SensorReadings_t;

SensorReadings_t sensor_readings = {0};

// Read sensors (example)
void ReadSensors(void) {
    // Read ADC channels (example)
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_value = HAL_ADC_GetValue(&hadc1);

    // Convert to sensor values (example scaling)
    sensor_readings.pressure = (adc_value * 4095) / 4096;  // 12-bit
    sensor_readings.temperature = 2048;  // Example value
    sensor_readings.voltage = 3300;      // Example value

    // Update status
    sensor_readings.status = 0x5;  // Normal operation
}

// Update SENT message with sensor data
void UpdateSentMessage(void) {
    // Pack 12-bit values into nibbles
    // Pressure: nibbles 0-2 (12 bits = 3 nibbles)
    tx_message.data_nibbles[0] = (sensor_readings.pressure >> 8) & 0x0F;
    tx_message.data_nibbles[1] = (sensor_readings.pressure >> 4) & 0x0F;
    tx_message.data_nibbles[2] = (sensor_readings.pressure >> 0) & 0x0F;

    // Temperature: nibbles 3-5
    tx_message.data_nibbles[3] = (sensor_readings.temperature >> 8) & 0x0F;
    tx_message.data_nibbles[4] = (sensor_readings.temperature >> 4) & 0x0F;
    tx_message.data_nibbles[5] = (sensor_readings.temperature >> 0) & 0x0F;

    // Status nibble
    tx_message.status_nibble = sensor_readings.status;

    // CRC is recalculated automatically by library before transmission
}

// Transmission callback
void SensorTxCallback(SENTTxHandle_t *const handle) {
    // Read sensors and update message
    ReadSensors();
    UpdateSentMessage();
}

// Initialize transmitter
void SENT_Sensor_Transmitter_Init(void) {
    // Initial message
    uint8_t initial_data[6] = {0};
    SENT_msg_init(&tx_message, 0x5, initial_data, 6, SENT_VERSION_2010);

    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTTx_init(&sent_tx, &init, &tx_message, SensorTxCallback, NULL, NULL)) {
        Error_Handler();
    }

    if (!SENTTx_start(&sent_tx)) {
        Error_Handler();
    }
}

// HAL callbacks
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_AutoReloadCallback(&sent_tx);
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_CompareCallback(&sent_tx);
    }
}
```

---

### Bidirectional Communication

System with both transmitter and receiver.

```c
/**
 * @file bidirectional.c
 * @brief Bidirectional SENT communication example
 */

#include "main.h"
#include "sent_rx.h"
#include "sent_tx.h"

extern TIM_HandleTypeDef htim1;  // For TX
extern TIM_HandleTypeDef htim2;  // For RX

SENTRxHandle_t sent_rx;
SENTTxHandle_t sent_tx;
SENTMsg_t tx_message;

// Receive callback - echo received data
void RxCallback(SENTRxHandle_t *const handle,
               SENTMsg_t *const msg,
               void *user_data) {
    // Echo received data by updating TX message
    memcpy(tx_message.data_nibbles, msg->data_nibbles, msg->data_length);
    tx_message.status_nibble = msg->status_nibble;
}

// Transmit callback
void TxCallback(SENTTxHandle_t *const handle) {
    // Transmission complete
}

// Initialize bidirectional communication
void SENT_Bidirectional_Init(void) {
    // Initialize receiver on TIM2
    SENTHandleInit_t rx_init = {
        .htim = &htim2,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx, &rx_init, RxCallback, NULL, NULL)) {
        Error_Handler();
    }

    // Initialize transmitter on TIM1
    uint8_t initial_data[6] = {0};
    SENT_msg_init(&tx_message, 0x0, initial_data, 6, SENT_VERSION_2010);

    SENTHandleInit_t tx_init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTTx_init(&sent_tx, &tx_init, &tx_message, TxCallback, NULL, NULL)) {
        Error_Handler();
    }

    // Start both
    if (!SENTRx_start(&sent_rx)) {
        Error_Handler();
    }

    if (!SENTTx_start(&sent_tx)) {
        Error_Handler();
    }
}

// HAL callbacks for both timers
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        SENTRx_InputCaptureCallback(&sent_rx);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_AutoReloadCallback(&sent_tx);
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTTx_CompareCallback(&sent_tx);
    }
}
```

---

### Slow Channel Usage

Using slow channel for configuration and diagnostics.

```c
/**
 * @file slow_channel.c
 * @brief SENT slow channel example for diagnostics
 */

#include "main.h"
#include "sent_tx.h"

extern TIM_HandleTypeDef htim1;

SENTTxHandle_t sent_tx;
SENTMsg_t tx_message;
SENTSlowMsg_t slow_message;

// Device configuration
typedef struct {
    uint8_t device_id;
    uint16_t serial_number;
    uint8_t firmware_version;
    uint16_t calibration_value;
} DeviceConfig_t;

DeviceConfig_t config = {
    .device_id = 0x42,
    .serial_number = 12345,
    .firmware_version = 0x10,
    .calibration_value = 0xABC
};

uint8_t slow_msg_index = 0;

// Fast channel callback
void FastTxCallback(SENTTxHandle_t *const handle) {
    // Update fast channel data (sensor readings)
    static uint16_t sensor_value = 0;
    handle->msg_source->data_nibbles[0] = (sensor_value >> 8) & 0x0F;
    handle->msg_source->data_nibbles[1] = (sensor_value >> 4) & 0x0F;
    handle->msg_source->data_nibbles[2] = (sensor_value >> 0) & 0x0F;
    sensor_value++;
}

// Slow channel callback - cycle through config messages
void SlowTxCallback(SENTTxHandle_t *const handle) {
    // Cycle through different configuration messages
    slow_msg_index = (slow_msg_index + 1) % 4;

    handle->slow_msg_source->type = SENT_SLOWTYPE_ENHANCED12;

    switch (slow_msg_index) {
        case 0:  // Device ID
            handle->slow_msg_source->as.slow_enhanced.id = 0x01;
            handle->slow_msg_source->as.slow_enhanced.data = config.device_id;
            break;

        case 1:  // Serial number (low)
            handle->slow_msg_source->as.slow_enhanced.id = 0x02;
            handle->slow_msg_source->as.slow_enhanced.data = config.serial_number & 0xFFF;
            break;

        case 2:  // Serial number (high)
            handle->slow_msg_source->as.slow_enhanced.id = 0x03;
            handle->slow_msg_source->as.slow_enhanced.data = (config.serial_number >> 12) & 0xFFF;
            break;

        case 3:  // Firmware version
            handle->slow_msg_source->as.slow_enhanced.id = 0x04;
            handle->slow_msg_source->as.slow_enhanced.data = config.firmware_version;
            break;
    }

    // CRC calculated automatically by library
}

// Initialize with slow channel
void SENT_SlowChannel_Init(void) {
    // Initialize fast channel message
    uint8_t data[6] = {0};
    SENT_msg_init(&tx_message, 0x5, data, 6, SENT_VERSION_2010);

    // Initialize slow channel message
    slow_message.type = SENT_SLOWTYPE_ENHANCED12;
    slow_message.as.slow_enhanced.id = 0x01;
    slow_message.as.slow_enhanced.data = config.device_id;

    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    // Initialize with both fast and slow channels
    if (!SENTTx_init(&sent_tx, &init,
                     &tx_message, FastTxCallback,
                     &slow_message, SlowTxCallback)) {
        Error_Handler();
    }

    if (!SENTTx_start(&sent_tx)) {
        Error_Handler();
    }
}
```

---

## Complete Applications

### Temperature Sensor Receiver

Complete application for receiving temperature sensor data.

```c
/**
 * @file temperature_sensor_app.c
 * @brief Complete temperature sensor receiver application
 */

#include "main.h"
#include "sent_rx.h"
#include <stdio.h>

// Temperature sensor SENT receiver
SENTRxHandle_t temp_sensor_rx;

// Application data
typedef struct {
    float temperature_celsius;
    uint8_t sensor_status;
    uint32_t valid_messages;
    uint32_t invalid_messages;
    uint8_t new_data_available;
} TempSensorData_t;

TempSensorData_t temp_data = {0};

// Convert raw SENT data to temperature
float ConvertToTemperature(uint16_t raw_value) {
    // Example conversion: 12-bit value, range -40°C to +150°C
    // Formula: temp = (raw / 4095) * 190 - 40
    return ((float)raw_value / 4095.0f) * 190.0f - 40.0f;
}

// Fast channel callback
void TempSensorCallback(SENTRxHandle_t *const handle,
                       SENTMsg_t *const msg,
                       void *user_data) {
    TempSensorData_t *data = (TempSensorData_t *)user_data;

    // Check status nibble for sensor errors
    uint8_t status = msg->status_nibble & 0x03;

    if (status != 0x01) {  // 0x01 = normal operation
        data->invalid_messages++;
        return;
    }

    // Extract 12-bit temperature value from first 3 nibbles
    uint16_t raw_temp = (msg->data_nibbles[0] << 8) |
                        (msg->data_nibbles[1] << 4) |
                        (msg->data_nibbles[2]);

    // Convert to temperature
    data->temperature_celsius = ConvertToTemperature(raw_temp);
    data->sensor_status = status;
    data->valid_messages++;
    data->new_data_available = 1;
}

// Initialize temperature sensor receiver
void TempSensor_Init(void) {
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&temp_sensor_rx, &init,
                     TempSensorCallback, NULL, &temp_data)) {
        Error_Handler();
    }

    if (!SENTRx_start(&temp_sensor_rx)) {
        Error_Handler();
    }

    printf("Temperature sensor initialized\n");
}

// Get latest temperature reading
uint8_t TempSensor_GetReading(float *temperature) {
    if (!temp_data.new_data_available) {
        return 0;  // No new data
    }

    __disable_irq();
    *temperature = temp_data.temperature_celsius;
    temp_data.new_data_available = 0;
    __enable_irq();

    return 1;  // Success
}

// Main application
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_USART2_UART_Init();

    TempSensor_Init();

    float temperature;

    while (1) {
        if (TempSensor_GetReading(&temperature)) {
            printf("Temperature: %.2f°C\n", temperature);
            printf("Valid messages: %lu, Invalid: %lu\n",
                   temp_data.valid_messages,
                   temp_data.invalid_messages);
        }

        HAL_Delay(1000);
    }
}
```

---

### Multi-Channel Data Transmitter

Transmitter sending multiple sensor channels.

```c
/**
 * @file multi_sensor_tx.c
 * @brief Multi-channel sensor data transmitter
 */

#include "main.h"
#include "sent_tx.h"

SENTTxHandle_t sent_tx;
SENTMsg_t tx_message;

// Sensor channels
typedef struct {
    uint16_t channel1;  // Pressure
    uint16_t channel2;  // Temperature
    uint16_t channel3;  // Humidity
    uint8_t  status;
} MultiSensorData_t;

MultiSensorData_t sensors = {0};

// Read all sensors
void ReadAllSensors(void) {
    // Read pressure sensor (example: 0-4095 = 0-100 kPa)
    sensors.channel1 = ReadPressureSensor();

    // Read temperature sensor (example: 0-4095 = -40 to +125°C)
    sensors.channel2 = ReadTemperatureSensor();

    // Read humidity sensor (example: 0-4095 = 0-100% RH)
    sensors.channel3 = ReadHumiditySensor();

    // Update status (bit 0: error flag, bit 1: calibration mode)
    sensors.status = 0x01;  // Normal operation
}

// Update SENT message with all sensor data
void UpdateMultiSensorMessage(void) {
    // Pack three 12-bit values into six nibbles
    // Each 12-bit value uses 3 nibbles

    // Channel 1 (nibbles 0-1: MSB 8 bits)
    tx_message.data_nibbles[0] = (sensors.channel1 >> 8) & 0x0F;
    tx_message.data_nibbles[1] = (sensors.channel1 >> 4) & 0x0F;

    // Channel 2 (nibbles 2-3: MSB 8 bits)
    tx_message.data_nibbles[2] = (sensors.channel2 >> 8) & 0x0F;
    tx_message.data_nibbles[3] = (sensors.channel2 >> 4) & 0x0F;

    // Channel 3 (nibbles 4-5: MSB 8 bits)
    tx_message.data_nibbles[4] = (sensors.channel3 >> 8) & 0x0F;
    tx_message.data_nibbles[5] = (sensors.channel3 >> 4) & 0x0F;

    // Status
    tx_message.status_nibble = sensors.status;
}

// TX callback
void MultiSensorTxCallback(SENTTxHandle_t *const handle) {
    ReadAllSensors();
    UpdateMultiSensorMessage();
}

// Initialize multi-sensor transmitter
void MultiSensor_Tx_Init(void) {
    uint8_t initial_data[6] = {0};
    SENT_msg_init(&tx_message, 0x1, initial_data, 6, SENT_VERSION_2010);

    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTTx_init(&sent_tx, &init, &tx_message,
                     MultiSensorTxCallback, NULL, NULL)) {
        Error_Handler();
    }

    if (!SENTTx_start(&sent_tx)) {
        Error_Handler();
    }
}

// Example sensor read functions (implement based on your hardware)
uint16_t ReadPressureSensor(void) {
    // Read from ADC, SPI, I2C, etc.
    return 2048;  // Example value
}

uint16_t ReadTemperatureSensor(void) {
    return 1500;  // Example value
}

uint16_t ReadHumiditySensor(void) {
    return 3000;  // Example value
}
```

---

## Integration Examples

### FreeRTOS Integration

Integrating SENT library with FreeRTOS.

```c
/**
 * @file freertos_integration.c
 * @brief SENT library integration with FreeRTOS
 */

#include "main.h"
#include "sent_rx.h"
#include "cmsis_os.h"

// FreeRTOS handles
osThreadId_t sent_task_handle;
osMessageQueueId_t sent_queue_handle;

// SENT receiver
SENTRxHandle_t sent_rx;

// Message structure for queue
typedef struct {
    uint8_t status;
    uint8_t data[6];
    uint8_t length;
} SENTQueueMsg_t;

// SENT callback - runs in interrupt context
void SENTRxCallback_ISR(SENTRxHandle_t *const handle,
                        SENTMsg_t *const msg,
                        void *user_data) {
    SENTQueueMsg_t queue_msg;

    // Copy data to queue message
    queue_msg.status = msg->status_nibble;
    memcpy(queue_msg.data, msg->data_nibbles, msg->data_length);
    queue_msg.length = msg->data_length;

    // Send to queue (from ISR)
    osMessageQueuePut(sent_queue_handle, &queue_msg, 0, 0);
}

// SENT processing task
void SENT_ProcessingTask(void *argument) {
    SENTQueueMsg_t received_msg;

    while (1) {
        // Wait for message from queue
        if (osMessageQueueGet(sent_queue_handle, &received_msg,
                             NULL, osWaitForever) == osOK) {

            // Process message in task context (not ISR)
            ProcessSENTMessage(&received_msg);

            // Update display, log data, etc.
        }
    }
}

// Process SENT message (runs in task context)
void ProcessSENTMessage(SENTQueueMsg_t *msg) {
    // Heavy processing can be done here
    // Not constrained by ISR timing requirements

    // Example: Log to SD card
    // Example: Update display
    // Example: Trigger control actions
}

// Initialize SENT with RTOS
void SENT_RTOS_Init(void) {
    // Create message queue
    sent_queue_handle = osMessageQueueNew(10, sizeof(SENTQueueMsg_t), NULL);

    if (sent_queue_handle == NULL) {
        Error_Handler();
    }

    // Initialize SENT receiver
    SENTHandleInit_t init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx, &init, SENTRxCallback_ISR, NULL, NULL)) {
        Error_Handler();
    }

    if (!SENTRx_start(&sent_rx)) {
        Error_Handler();
    }

    // Create processing task
    const osThreadAttr_t task_attributes = {
        .name = "SENTTask",
        .stack_size = 512,
        .priority = osPriorityNormal,
    };

    sent_task_handle = osThreadNew(SENT_ProcessingTask, NULL, &task_attributes);

    if (sent_task_handle == NULL) {
        Error_Handler();
    }
}
```

---

### Multiple SENT Channels

Managing multiple SENT receivers/transmitters.

```c
/**
 * @file multiple_channels.c
 * @brief Managing multiple SENT channels
 */

#include "main.h"
#include "sent_rx.h"

#define NUM_SENT_CHANNELS 3

// Multiple receiver handles
SENTRxHandle_t sent_rx_channels[NUM_SENT_CHANNELS];

// Application data for each channel
typedef struct {
    uint32_t message_count;
    uint8_t last_status;
    uint8_t latest_data[6];
} ChannelData_t;

ChannelData_t channel_data[NUM_SENT_CHANNELS] = {0};

// Generic callback with channel identification
void SENT_ChannelCallback(SENTRxHandle_t *const handle,
                         SENTMsg_t *const msg,
                         void *user_data) {
    ChannelData_t *data = (ChannelData_t *)user_data;

    data->message_count++;
    data->last_status = msg->status_nibble;
    memcpy(data->latest_data, msg->data_nibbles, msg->data_length);
}

// Initialize multiple channels
void SENT_MultiChannel_Init(void) {
    // Channel 0: TIM1_CH1
    SENTHandleInit_t init0 = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx_channels[0], &init0,
                     SENT_ChannelCallback, NULL, &channel_data[0])) {
        Error_Handler();
    }

    // Channel 1: TIM2_CH1
    SENTHandleInit_t init1 = {
        .htim = &htim2,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx_channels[1], &init1,
                     SENT_ChannelCallback, NULL, &channel_data[1])) {
        Error_Handler();
    }

    // Channel 2: TIM3_CH1
    SENTHandleInit_t init2 = {
        .htim = &htim3,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx_channels[2], &init2,
                     SENT_ChannelCallback, NULL, &channel_data[2])) {
        Error_Handler();
    }

    // Start all channels
    for (int i = 0; i < NUM_SENT_CHANNELS; i++) {
        if (!SENTRx_start(&sent_rx_channels[i])) {
            Error_Handler();
        }
    }
}

// HAL callback - route to correct channel
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        SENTRx_InputCaptureCallback(&sent_rx_channels[0]);
    } else if (htim->Instance == TIM2) {
        SENTRx_InputCaptureCallback(&sent_rx_channels[1]);
    } else if (htim->Instance == TIM3) {
        SENTRx_InputCaptureCallback(&sent_rx_channels[2]);
    }
}

// Get data from specific channel
uint8_t GetChannelData(uint8_t channel_id, uint8_t *data_out) {
    if (channel_id >= NUM_SENT_CHANNELS) {
        return 0;  // Invalid channel
    }

    __disable_irq();
    memcpy(data_out, channel_data[channel_id].latest_data, 6);
    __enable_irq();

    return 1;
}
```

---

## Testing and Debugging

### Loopback Test

Test transmitter and receiver on same MCU.

```c
/**
 * @file loopback_test.c
 * @brief SENT loopback test for verification
 */

#include "main.h"
#include "sent_rx.h"
#include "sent_tx.h"

// Test statistics
typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t match_count;
    uint32_t error_count;
} LoopbackStats_t;

LoopbackStats_t stats = {0};

SENTTxHandle_t sent_tx;
SENTRxHandle_t sent_rx;
SENTMsg_t tx_message;

uint8_t expected_data[6] = {0};

// RX callback - verify received data matches transmitted
void LoopbackRxCallback(SENTRxHandle_t *const handle,
                       SENTMsg_t *const msg,
                       void *user_data) {
    stats.rx_count++;

    // Verify data matches expected
    if (memcmp(msg->data_nibbles, expected_data, 6) == 0) {
        stats.match_count++;
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    } else {
        stats.error_count++;
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    }
}

// TX callback - update test pattern
void LoopbackTxCallback(SENTTxHandle_t *const handle) {
    stats.tx_count++;

    // Update expected data
    memcpy(expected_data, handle->msg_source->data_nibbles, 6);

    // Generate new test pattern
    for (int i = 0; i < 6; i++) {
        handle->msg_source->data_nibbles[i] = (stats.tx_count + i) & 0x0F;
    }
}

// Initialize loopback test
void Loopback_Test_Init(void) {
    // Note: TX output must be physically connected to RX input

    // Initialize TX
    uint8_t data[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
    SENT_msg_init(&tx_message, 0x5, data, 6, SENT_VERSION_2010);

    SENTHandleInit_t tx_init = {
        .htim = &htim1,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTTx_init(&sent_tx, &tx_init, &tx_message,
                     LoopbackTxCallback, NULL, NULL)) {
        Error_Handler();
    }

    // Initialize RX
    SENTHandleInit_t rx_init = {
        .htim = &htim2,
        .channel = TIM_CHANNEL_1,
        .has_pause = 0,
        .data_length = 6,
        .tick_unit_time = 0.003,
        .version = SENT_VERSION_2010
    };

    if (!SENTRx_init(&sent_rx, &rx_init,
                     LoopbackRxCallback, NULL, NULL)) {
        Error_Handler();
    }

    // Start RX first, then TX
    if (!SENTRx_start(&sent_rx)) {
        Error_Handler();
    }

    if (!SENTTx_start(&sent_tx)) {
        Error_Handler();
    }

    printf("Loopback test started\n");
}

// Print test results
void PrintLoopbackStats(void) {
    printf("TX: %lu, RX: %lu, Match: %lu, Errors: %lu\n",
           stats.tx_count, stats.rx_count,
           stats.match_count, stats.error_count);

    float success_rate = (float)stats.match_count / (float)stats.rx_count * 100.0f;
    printf("Success rate: %.2f%%\n", success_rate);
}
```

---

These examples provide a comprehensive foundation for using the SENT library in various applications. Refer to the [API Reference](API_REFERENCE.md) for detailed function documentation and the [Getting Started Guide](GETTING_STARTED.md) for setup instructions.
