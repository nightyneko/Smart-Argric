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
