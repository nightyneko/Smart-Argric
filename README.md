# Project: Smart Plant IoT System

## Overview
This is an IoT-based environmental control system designed to maintain optimal conditions for plants. It monitors temperature, pressure, humidity, light intensity, and soil moisture, while controlling actuators like a fan, water pump, and OLED display. The system is integrated into a central dashboard via Node-RED using the MQTT protocol.

## Key Features
- **Intelligent Health Scoring:** Calculates a real-time "Plant Health %" using weighted biological factors (50% Soil, 35% Temp, 15% Light).
- **Advanced Smoothing:** Implements Moving Average Filters (Size 5) on all sensors to prevent jitter.
- **Hardware Failsafe:** Detects pump hardware errors or empty reservoirs if soil moisture doesn't improve after 5 minutes of active irrigation.
- **EMI Resilience:** Custom UART error handling and EIE bit management to prevent system lockups caused by motor noise.
- **Dynamic Calibration:** Remote commands allow for "Dry" and "Wet" calibration of the soil sensor on-the-fly.
- **Visual Feedback:** 128x64 OLED display showing sensor telemetry, current state, and a dynamic "Plant Avatar" (Happy/Sad).

## System Architecture

### 1. Main Controller: STM32L432KC
The STM32 acts as the "brain" of the system, handling real-time sensor data acquisition and hardware control.
- **Sensors:**
  - **BMP280:** Temperature and Pressure (I2C3).
  - **LDR:** Light intensity (ADC1_IN6).
  - **Soil Moisture:** Raw capacitive/resistive probe (ADC1_IN5).
- **Actuators:**
  - **Fan (PB1):** Cooling control.
  - **Water Pump (PB0):** Irrigation control.
  - **Plant Light (PA4):** Toggle-based growth light (100ms pulse logic).
  - **OLED (SSD1306):** Local status display (I2C1).

### 2. Communication Bridge: ESP32-S
Serves as a transparent MQTT bridge.
- **Protocol:** MQTT @ 1883.
- **Serial Connection:** UART @ **152000 baud** (Custom high-speed).
- **Telemetry Topic:** `stm32/sensors` (JSON format).
- **Command Topic:** `stm32/commands` (Raw ID:Value strings).

### 3. Backend: Node-RED
Handles data logging, remote UI dashboarding, and manual override logic.

## Command Reference (Node-RED -> STM32)
Commands are sent as strings to the `stm32/commands` topic in the format `ID` or `ID:Value`.

| ID | Command | Description |
| :--- | :--- | :--- |
| **0 / 1** | Auto Mode | Disable (0) or Enable (1) automatic regulation. |
| **2 / 3** | Pump Control | Force Pump OFF (2) or ON (3). Switches to Manual. |
| **4 / 5** | Fan Control | Force Fan OFF (4) or ON (5). Switches to Manual. |
| **6 / 7** | Light Control | Force Light OFF (6) or ON (7). Switches to Manual. |
| **8 / 9** | Calibration | Calibrate Dry Point (8) or Wet Point (9). |
| **10** | Reset Failsafe | Clears the "Pump Critical" error state. |
| **11 - 16** | Thresholds | Set Soil Low/High, Temp Low/High, Light Low/High. |
| **17 / 18** | Day/Night | Set Day Mode (17) or Night Mode (18) for light logic. |

---

# Hardware Pin Configuration

## 1. STM32L432KC Connections

| Component | STM32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **Soil Moisture** | `PA0` | ADC1_IN5 | Analog Raw. |
| **LDR Sensor** | `PA1` | ADC1_IN6 | Analog Raw. |
| **Water Pump** | `PB0` | Output | Active High. |
| **Cooling Fan** | `PB1` | Output | Active High. |
| **Plant Light** | `PA4` | Output | **Pulse Toggle** (100ms HIGH-LOW). |
| **BMP280** | `I2C3` (PA7/PB4) | I2C | Temp/Pressure. |
| **OLED (SSD1306)** | `I2C1` (PA9/PA10) | I2C | Local UI. |
| **ESP32 Link** | `UART1` | 152000 baud | TX/RX to Bridge. |

> **Warning:** Use Optocouplers or Flyback Diodes for the Pump and Fan to protect the STM32 from inductive spikes.

## 2. ESP32-S Connections
- **RXD2 (GPIO 25)** -> STM32 UART1_TX
- **TXD2 (GPIO 26)** -> STM32 UART1_RX
- **GND** -> Common Ground mandatory.
