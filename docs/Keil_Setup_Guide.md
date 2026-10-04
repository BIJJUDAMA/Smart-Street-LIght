# Keil uVision 5 Setup and Hardware Demonstration Guide

This guide provides step-by-step instructions to create a fresh Keil uVision 5 project, compile the bare-metal CMSIS firmware, wire the breadboard hardware, and run benchtop testing for the **Predictive Adaptive Smart Streetlight and Environmental Monitoring System**.

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
3. Name your project: `Predictive_Streetlight`.
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
   - Select `ST-Link Debugger` -> Click **Settings** -> Ensure SWD mode is selected.
4. Click **OK**.

---

## 3. Hardware Connections & Breadboard Setup

### 3.1 Power Supply Distribution
- Connect Black Pill `5V` pin to the breadboard high-voltage rail (supplies PIR sensors, LM35, buzzer, LCD backlight).
- Connect Black Pill `3.3V` pin to the low-voltage rail (supplies BH1750, SHT31, and pull-up resistors).
- Connect Black Pill `GND` to the common ground rail.

### 3.2 Subsystem Pin Mapping

| Subsystem | Component Pin | STM32 Pin | Signal Type | External Components Required |
| :--- | :--- | :--- | :--- | :--- |
| **MODE Button** | Onboard KEY | `PA0` | Digital Input | Internal pull-up (active LOW) |
| **LM35 Pavement** | Vout (Pin 2) | `PA1` | ADC1_IN1 | Connect Vs to 5V, GND to rail |
| **Streetlight S1** | Gate / Anode | `PA2` | TIM2_CH3 PWM | 220 Ω resistor to LED or N-MOSFET gate |
| **LDR Feedback** | Signal Out | `PA4` | ADC1_IN4 | 10 kΩ divider, inside tube aimed at S2 |
| **Button UP** | Switch Pin | `PA5` | Digital Input | Momentary switch to GND, internal pull-up |
| **Streetlight S2** | Gate / Anode | `PA6` | TIM3_CH1 PWM | 220 Ω resistor to LED or N-MOSFET gate |
| **Streetlight S3** | Gate / Anode | `PA7` | TIM3_CH2 PWM | 220 Ω resistor to LED or N-MOSFET gate |
| **Button DOWN** | Switch Pin | `PA8` | Digital Input | Momentary switch to GND, internal pull-up |
| **Bluetooth TX** | RX Pin | `PA9` | USART1_TX | Connect to HC-05 RX via resistor divider |
| **Bluetooth RX** | TX Pin | `PA10` | USART1_RX | Connect directly to HC-05 TX |
| **PIR-A (Zone 1)**| OUT Pin | `PB0` | EXTI0 (Int) | Powered by 5V, output to PB0 with tube |
| **PIR-B (Zone 3)**| OUT Pin | `PB1` | EXTI1 (Int) | Powered by 5V, output to PB1 with tube |
| **I2C Bus Clock** | SCL Pin | `PB8` | I2C1_SCL | 4.7 kΩ pull-up to 3.3V on bus |
| **I2C Bus Data** | SDA Pin | `PB9` | I2C1_SDA | 4.7 kΩ pull-up to 3.3V on bus |
| **Buzzer** | Positive (+) | `PB12` | Digital Output | Active buzzer, negative to GND |
| **RGB Status** | R / G / B | `PB13`, `PB14`, `PB15`| Digital Output | 3x 330 Ω resistors to common cathode RGB |
| **Heartbeat LED** | Onboard LED | `PC13` | Digital Output | Onboard active-low LED |

### 3.3 Optical Isolation & Collimation Tips
1. **PIR Field of View:** Place a 5 cm black heat-shrink or cardboard tube around PIR-A and PIR-B domes to narrow detection cones to $30^\circ$, preventing cross-triggering.
2. **BH1750 Shielding:** Position the BH1750 ambient sensor facing away from or shielded from Streetlights S1, S2, and S3 to prevent optical feedback oscillation.

---

## 4. Benchtop Demonstration and Test Procedures

### 4.1 Initial Power-Up (45s Warm-up)
1. Flash firmware via ST-LINK (`F8` in Keil uVision).
2. The onboard LED on `PC13` begins blinking as a 1 Hz heartbeat.
3. The 16x2 LCD initializes and displays a 45-second PIR sensor stabilization countdown:
   ```text
   SMART STREETLGT
   PIR WARMUP: 42s
   ```

### 4.2 Interactive Verification Scenarios

1. **Daylight Mode (0:00 - 0:30):**
   - Expose BH1750 to ambient room light (> 50 Lux).
   - LCD displays `DAYLIGHT | LUX: 120 lx`.
   - All three streetlights remain completely off (0%).

2. **Clear Night Baseline (0:30 - 1:00):**
   - Cover BH1750 with a dark cap (< 20 Lux).
   - Streetlights S1, S2, and S3 smoothly fade up to the idle baseline (8% perceptual / 20% PWM).
   - RGB status LED illuminates solid Green.

3. **Forward Moving Corridor (1:00 - 1:30):**
   - Slowly swipe hand past PIR-A toward PIR-B ($t_A \to t_B$).
   - S1 and S2 ramp to 100% full brightness; S3 pre-lights to 60%.
   - RGB LED switches to Blue; Buzzer gives a confirmation chirp.
   - S1 fades to baseline while S2 and S3 hold at 100%, followed by sequential decay back to idle.

4. **Reverse Moving Corridor (1:30 - 2:00):**
   - Swipe hand past PIR-B toward PIR-A ($t_B \to t_A$).
   - System mirrors the wave in reverse: S3 and S2 illuminate to 100%, and S1 pre-lights.

5. **Adverse Weather Response (2:00 - 2:30):**
   - Gently exhale on the SHT31 sensor (humidity rises above 88% RH, reducing dew point spread).
   - LCD displays `NIGHT WET`, and the baseline brightness elevates across all nodes to 25% perceptual.
   - RGB status LED switches to Yellow.

6. **Frost and Black Ice Hazard (2:30 - 2:45):**
   - Touch an ice pack to the LM35 sensor ($\le 3.0^\circ\text{C}$).
   - LCD displays `FROST RISK`, RGB flashes Red, and hold times extend for road safety.

7. **Closed-Loop Lamp Feedback Test:**
   - Disconnect the LED at S2 while commanded ON; the system flags `LAMP2_FAULT` on the LCD and Bluetooth console.
