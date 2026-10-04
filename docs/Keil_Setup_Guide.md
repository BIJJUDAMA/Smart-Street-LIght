# Keil uVision 5 Setup and Hardware Demonstration Guide

This guide provides step-by-step instructions to create a fresh Keil uVision 5 project, compile the bare-metal CMSIS firmware, wire the breadboard hardware, and run benchtop testing for the **Predictive Adaptive Smart Streetlight and Environmental Monitoring Network**.

---

## 1. Prerequisites and Toolchain

1. **Keil MDK-ARM uVision 5** (v5.30 or newer).
2. **STM32F4 Device Family Pack:** `Keil.STM32F4xx_DFP` installed via Keil Pack Installer.
3. **ST-LINK v2 Programmer / In-Circuit Debugger** with ST-LINK USB drivers.
4. **USB-UART or Bluetooth SPP Terminal** (e.g., PuTTY, Tera Term, or Serial Bluetooth Terminal on Android).

---

## 2. Keil uVision 5 Project Configuration

### 2.1 Project Creation
1. Open Keil uVision 5.
2. Click **Project** -> **New uVision Project...**
3. Select your project folder and specify project name: `Predictive_Streetlight`.
4. In the target device selector, search and select:
   ```text
   STMicroelectronics -> STM32F4 Series -> STM32F401 -> STM32F401CCUx
   ```
5. Click **OK**.

### 2.2 Manage Run-Time Environment (RTE)
In the software components window, select strictly minimal bare-metal components:
1. **CMSIS:**
   - Check `CORE`
2. **Device:**
   - Check `Startup`
*(Do NOT select any STM32Cube HAL or middleware libraries. The code is 100% pure CMSIS bare-metal register C.)*

### 2.3 Adding Source File
1. Under the Project workspace tree, expand `Target 1`.
2. Right-click `Source Group 1` and click **Add Existing Files to Group...**
3. Select `src/main.c` and click **Add**, then **Close**.

### 2.4 Target Options (`Alt + F7`)
1. **Target Tab:**
   - **Xtal (MHz):** `16.0`
   - **Floating Point Hardware:** `Single Precision`
2. **Output Tab:**
   - Check `Create HEX File`
3. **Debug Tab:**
   - Select `ST-Link Debugger`
   - Click **Settings** -> Ensure SWD mode is selected and target MCU is detected.
4. Click **OK**.

---

## 3. Comprehensive Breadboard Wiring Guide

### 3.1 Power Supply Distribution
- Connect Black Pill `5V` pin to the breadboard high-voltage rail (supplies PIR sensors, buzzer, LCD backlight, and MAX485).
- Connect Black Pill `3.3V` pin to the low-voltage rail (supplies BH1750, SHT31, LM35, and pushbuttons).
- Connect Black Pill `GND` to the common ground rail.

### 3.2 Subsystem Wiring Connections

| Subsystem | Component Pin | STM32 Pin | Signal Type | External Components Required |
| :--- | :--- | :--- | :--- | :--- |
| **Streetlight S1** | Gate / Anode | `PA0` | TIM2_CH1 PWM | 220 Ω resistor to LED or N-MOSFET gate |
| **Streetlight S2** | Gate / Anode | `PA6` | TIM3_CH1 PWM | 220 Ω resistor to LED or N-MOSFET gate |
| **Streetlight S3** | Gate / Anode | `PA7` | TIM3_CH2 PWM | 220 Ω resistor to LED or N-MOSFET gate |
| **LM35 Temp** | Vout (Pin 2) | `PA1` | ADC1_IN1 | Connect Vs to 3.3V, GND to rail |
| **LDR Sensor** | Signal Out | `PA4` | ADC1_IN4 | Voltage divider with 10 kΩ pull-down |
| **RS-485 Bus** | DI / RO / DE | `PA2`, `PA3`, `PB10` | USART2 + GPIO | MAX485 module (DI->PA2, RO->PA3, DE+RE->PB10) |
| **Bluetooth** | RX / TX | `PA9`, `PA10` | USART1 | HC-05 module (RX->PA9, TX->PA10 via divider) |
| **PIR-A (Motion)**| OUT | `PB0` | Digital Input | Powered by 5V, 3.3V digital logic output |
| **PIR-B (Motion)**| OUT | `PB1` | Digital Input | Powered by 5V, 3.3V digital logic output |
| **I2C Bus** | SCL / SDA | `PB8`, `PB9` | I2C1 Open-Drain | 4.7 kΩ pull-up resistors to 3.3V on bus |
| **Buzzer** | Positive (+) | `PB12` | Digital Output | Active 3.3V/5V buzzer, negative to GND |
| **RGB Status** | R / G / B | `PB13`, `PB14`, `PB15`| Digital Output | 3x 330 Ω resistors to common cathode RGB |
| **Button UP** | Switch Pin | `PA5` | Digital Input | Momentary switch to GND, internal pull-up |
| **Button DOWN** | Switch Pin | `PA8` | Digital Input | Momentary switch to GND, internal pull-up |

---

## 4. Benchtop Demonstration and Test Procedures

### 4.1 Initial Power-Up and Calibration
1. Flash firmware via ST-LINK (`F8` in Keil uVision).
2. The onboard LED on `PC13` begins blinking as a 1 Hz heartbeat.
3. The 16x2 I2C LCD initializes and displays:
   ```text
   SMART STREETLIGHT
   INITIALIZING...
   ```
4. After 1 second, the LCD starts alternating between:
   - **Page 1 (Environment):** Temperature, Humidity, and Ambient Lux.
   - **Page 2 (Lighting Network):** State, Vector Direction, and S1/S2/S3 PWM levels.

### 4.2 Interactive Verification Scenarios

1. **Ambient Lighting Verification:**
   - Cover the BH1750/LDR sensor: Ambient reading drops below 30 Lux. Streetlights S1, S2, and S3 ramp to the baseline 20% idle state.
   - Expose the sensors to ambient room light: Readings rise above 50 Lux. Streetlights automatically extinguish (0% daylight shutoff).

2. **Moving Corridor Transit Verification:**
   - While in night idle mode, swipe your hand from **PIR-A toward PIR-B**:
     - System registers forward approach vector (`DIR: FWD`).
     - Streetlight S1 and S2 immediately jump to 100% full illumination.
     - Streetlight S3 illuminates to 50% as a predictive pre-lit safety buffer.
     - RGB status LED illuminates Blue.
     - After 3 seconds without motion, S1 decays to 20%, followed by S2 and S3 returning to idle.
   - Swipe from **PIR-B toward PIR-A**:
     - System registers reverse approach vector (`DIR: REV`) with reverse corridor progression.

3. **Adverse Weather Trigger Verification:**
   - Gently exhale on the SHT31 sensor to increase localized humidity above 80% RH.
   - The decision engine automatically boosts the idle baseline across all streetlights from 20% to 50% for fog/rain visibility.
   - RGB status LED switches to Yellow.

4. **Frost and Black Ice Hazard Alarm:**
   - Apply cold ice or a cooling element to the LM35 sensor until the temperature drops to 3 °C or lower.
   - The system detects the roadside freezing hazard, triggers a warning chirp on the active buzzer, switches the RGB status LED to Red, and elevates streetlight illumination across all nodes to 100% for driver safety.

5. **Technician Serial Command Terminal:**
   - Connect via Bluetooth terminal at 9600 Baud (8-N-1) to view real-time telemetry frames:
     ```text
     [NODE1] T_LM:2C T_SHT:2.8C H:82% LUX:18lx DIR:FWD S1:100% S2:100% S3:100% STAT:FROST
     ```
   - Send single-byte wireless commands:
     - `'F'` -> Simulates forward vehicle corridor transit.
     - `'R'` -> Simulates reverse vehicle corridor transit.
     - `'U'` / `'+'` -> Steps brightness UP by 5% and engages manual mode.
     - `'D'` / `'-'` -> Steps brightness DOWN by 5% and engages manual mode.
     - `'A'` -> Re-arms automatic adaptive corridor mode.
     - `'1'` -> Emergency full 100% illumination override.
     - `'0'` -> Emergency blackout 0% shutdown.
