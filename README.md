# Project: Smart Plant IoT System

## Overview
This is an IoT-based environmental control system designed to maintain optimal conditions for plants. It monitors temperature, pressure, humidity, light intensity, and soil moisture, while controlling actuators like a fan, water pump, and OLED display. The system is integrated into a central dashboard via Node-RED using the MQTT protocol.

## System Architecture

### 1. Main Controller: STM32L432KC
The STM32 acts as the "brain" of the system, handling real-time sensor data acquisition and hardware control.
- **Sensors:**
  - **BMP280:** Measures temperature, atmospheric pressure, and humidity (connected via I2C3).
  - **LDR (Light Dependent Resistor):** Measures ambient light intensity (connected via ADC1 Channel 6 / PA1).
  - **Soil Moisture Sensor:** Measures the water content in the soil (connected via ADC1 Channel 5 / PA0).
- **Actuators:**
  - **Fan:** Controls temperature and airflow (GPIO controlled).
  - **Water Pump:** Automates irrigation based on soil moisture levels (GPIO controlled).
  - **OLED (SSD1306):** Displays local real-time sensor data (connected via I2C1).
- **Communication:**
  - Sends sensor data to ESP32 via UART1.
  - Receives remote commands from ESP32 via UART1.

### 2. Communication Bridge: ESP32-S
The ESP32 serves as a transparent bridge between the STM32 and the internet.
- **Protocol:** MQTT.
- **Topics:**
  - `stm32/sensors`: ESP32 publishes sensor data strings received from STM32 to this topic.
  - `stm32/commands`: ESP32 subscribes to this topic and forwards any incoming commands from Node-RED to the STM32 via UART.
- **Connection:** Connects to a local WiFi network and a Mosquitto MQTT broker.

### 3. Backend & UI: Node-RED
Node-RED provides the user interface and logic for remote monitoring and control.
- **Monitoring:** Subscribes to `stm32/sensors` to display real-time gauges and charts.
- **Control:** Sends commands (e.g., "PUMP_ON", "FAN_OFF") to the `stm32/commands` topic.
- **Automation:** Can implement complex logic (e.g., historical data logging, alerts).

## Project Structure
- `Core/`: STM32CubeIDE project source code (C).
- `esp32_src/`: ESP32 bridge source code (Arduino/C++).
- `node-red/`: Node-RED flow configuration files.

## Target Environment Goals
- **Temperature:** Maintain within a safe range using the fan.
- **Soil Moisture:** Prevent drying out by triggering the water pump.
- **Monitoring:** Provide full visibility of the plant's status via both the local OLED and the Node-RED dashboard.

# Hardware Pin Configuration

This document outlines the pin connections for the Smart Plant IoT System, covering both the main STM32L432KC controller and the ESP32-S MQTT bridge.

## 1. STM32L432KC Connections

The STM32 acts as the main controller. Wire your sensors and actuators to the following pins:

| Component | STM32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **Soil Moisture Sensor** | `PA0` | Analog Input (ADC1_IN5) | Reads raw moisture level (0-4095). |
| **LDR (Light Sensor)** | `PA1` | Analog Input (ADC1_IN6) | Reads ambient light level (0-4095). |
| **Water Pump** | `PB0` | Digital Output | Triggered low/high to turn pump on/off. (Use a relay or transistor). |
| **Cooling Fan** | `PB1` | Digital Output | Triggered low/high to turn fan on/off. (Use a relay or transistor). |
| **Plant Light** | `PA4` | Digital Output | Triggered low/high to turn light on/off. (Use a relay or transistor). |
| **BMP280 Sensor** | `I2C3` (SCL/SDA) | I2C Communication | Temp, Humidity, Pressure data. Connect to hardware I2C3 pins. |
| **OLED Display** | `I2C1` (SCL/SDA) | I2C Communication | SSD1306 Local display. Connect to hardware I2C1 pins. |
| **ESP32 RX** | `UART1_TX` (e.g., PA9) | Serial TX | Sends JSON telemetry to the ESP32. |
| **ESP32 TX** | `UART1_RX` (e.g., PA10) | Serial RX | Receives commands from the ESP32. |

> **Note on Actuators:** Do not connect motors, fans, or high-power lights directly to the STM32 GPIO pins. The STM32 outputs 3.3V at low current. You **must** use a Relay Module, MOSFET, or Motor Driver board between the STM32 pins (`PB0`, `PB1`, `PA4`) and the actual actuators.

---

## 2. ESP32-S Connections

The ESP32 acts as a transparent serial-to-MQTT bridge. 

| Component | ESP32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **STM32 UART1_TX** | `GPIO 25` (RXD2) | Serial RX | Receives telemetry JSON from the STM32. |
| **STM32 UART1_RX** | `GPIO 26` (TXD2) | Serial TX | Sends remote commands to the STM32. |

> **Wiring Note:** Ensure that the STM32 and ESP32 share a common **GND** (Ground) connection so the serial communication works reliably.

---

## Power Requirements
- **STM32 & ESP32:** 3.3V logic (Can generally be powered via USB during dev, or a 5V/3.3V step-down module).
- **Actuators (Pump, Fan, Light):** Likely require an external power supply (e.g., 5V, 9V, or 12V depending on the specific hardware you bought) connected through your relays/transistors.
