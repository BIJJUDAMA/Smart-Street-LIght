<div align="center">

# STM32-Based Predictive Adaptive Smart Streetlight & Environmental Monitoring Network

[![Microcontroller](https://img.shields.io/badge/MCU-STM32F401CCU6%20%28Black%20Pill%29-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f401cc.html)
[![Architecture](https://img.shields.io/badge/Architecture-ARM%20Cortex--M4%20w%2F%20FPU-brightgreen.svg)]()
[![IDE](https://img.shields.io/badge/IDE-Keil%20MDK--ARM%20%2F%20uVision%205-lightgrey.svg)]()
[![Firmware](https://img.shields.io/badge/Firmware-Bare--Metal%20CMSIS%20Register-brightgreen.svg)]()

<p align="center">
  <b>An intelligent, energy-saving municipal streetlight network and environmental monitoring unit built on a single STM32F401CCU6 Black Pill simulating a distributed 3-node moving illumination corridor.</b>
</p>

</div>

---

The system reduces municipal electrical grid power draw by up to 70% by automating streetlight intensity with real-time ambient optical sensing, vector motion tracking, and weather-aware policy adaptation. A single STM32F401CCU6 microcontroller controls three independent PWM-dimmed streetlights (S1, S2, S3) to simulate a multi-pole road corridor, continuously measuring ambient microclimate parameters and streaming live telemetry over RS-485 and Bluetooth.

---

## Key Features

- **Autonomous Predictive Illumination Corridor:** Uses dual PIR sensors (PIR-A and PIR-B) to calculate motion direction (forward vs reverse) and vehicle transit speed, illuminating ahead of vehicles to create a smooth travelling light corridor before fading back to idle.
- **Context-Aware Environmental Adaptation:** Ambient sensors determine the lighting baseline, while motion dictates instantaneous demand:
  - *Daylight:* Luminaires extinguished (0%).
  - *Clear Night:* Base idle brightness maintained at 20% to save energy.
  - *Adverse Weather (High Humidity / Rain / Fog):* Base idle automatically elevated to 50% for driver safety.
  - *Active Transit:* Approach streetlights ramp to 100%, and downstream nodes pre-light to 50%.
- **Dual-Technology Sensor Redundancy:**
  - *Temperature:* Analog LM35 (road surface temperature) cross-referenced with digital SHT31 (ambient air temperature and relative humidity).
  - *Optical:* Analog LDR (fast twilight thresholding) paired with digital BH1750 (calibrated photometric lux).
- **Multi-Channel Hardware PWM Dimming:** Drives three independent low-side MOSFET streetlight channels using Timer 2 (`TIM2_CH1`) and Timer 3 (`TIM3_CH1`, `TIM3_CH2`) at 190.5 Hz for smooth, non-flickering dimming.
- **Multi-Device I2C Shared Bus:** Connects digital SHT31, BH1750, and 16x2 character LCD over a single bare-metal I2C master bus (`PB8`/`PB9`).
- **Distributed Network Simulation:** Includes MAX485 transceiver for inter-pole RS-485 communication alongside an HC-05 Bluetooth module for mobile technician diagnostics.
- **Field Safety and Hazard Watchdog:** Features an RGB status beacon, active buzzer hazard alarms, and tactile pushbuttons with non-blocking software debounce for manual brightness overrides.

---

## System Architecture

```text
                                  ENVIRONMENT CONTEXT
                   ┌──────────────┬──────────────────┬──────────────┐
                   │              │                  │              │
                 LM35            LDR               SHT31          BH1750
             (Analog Temp)  (Analog Light)     (Digital Hum)  (Digital Lux)
                   │              │                  │              │
                   └──────────────┴────────┬─────────┴──────────────┘
                                           │
PIR-A (Approach) ───┐                      ▼
                    ├──────────────> ┌───────────┐
PIR-B (Direction) ──┘   (Demand)     │ STM32F401 │
                                     │ DECISION  │
Technician Buttons / Bluetooth ────> │  ENGINE   │
                                     └─────┬─────┘
                                           │
                   ┌───────────────────────┼───────────────────────┐
                   ▼                       ▼                       ▼
            3-CHANNEL PWM             RS-485 BUS                16x2 LCD
             STREETLIGHTS          INTER-POLE COORD          RGB STATUS &
              (S1, S2, S3)          + BLUETOOTH                 BUZZER
```

### Moving Illumination Corridor Progression

```text
Idle Corridor (Clear Night):
  S1: 20% ──────────── S2: 20% ──────────── S3: 20%

Vehicle Approach Detected Moving Forward (PIR-A -> PIR-B):
  S1: 100% ─────────── S2: 100% ─────────── S3: 50% (Pre-lit)

Vehicle Advances Through Mid-Corridor:
  S1: 20% (Decay) ──── S2: 100% ─────────── S3: 100%

Corridor Reset:
  S1: 20% ──────────── S2: 20% ──────────── S3: 20%
```

---

## Bill of Materials (BOM)

| Item | Component Description | Quantity | Target Pin / Bus | Purpose |
| :---: | :--- | :---: | :--- | :--- |
| **1** | **STM32F401CCU6 Black Pill** | 1 | Microcontroller Core | 84 MHz ARM Cortex-M4 MCU Board |
| **2** | **ST-Link V2 Programmer** | 1 | SWD (SWDIO, SWCLK) | Firmware flashing and in-circuit debugging |
| **3** | **HC-SR501 PIR Motion Sensors** | 2 | `PB0` (PIR-A), `PB1` (PIR-B) | Approach and direction detection |
| **4** | **LDR Photocell Module** | 1 | `PA4` (ADC1_IN4) | Analog day/night twilight sensing |
| **5** | **BH1750 Lux Sensor (GY-302)** | 1 | `PB8` (SCL), `PB9` (SDA) | Calibrated ambient illumination (Lux) |
| **6** | **LM35 Precision Temp Sensor** | 1 | `PA1` (ADC1_IN1) | Analog road surface temperature (10 mV/°C) |
| **7** | **SHT31-D Temp & Humidity** | 1 | `PB8` (SCL), `PB9` (SDA) | Microclimate and fog/rain monitoring |
| **8** | **16x2 I2C LCD Display** | 1 | `PB8` (SCL), `PB9` (SDA) | Real-time state and telemetry dashboard |
| **9** | **Bright White LEDs** | 3 | `PA0`, `PA6`, `PA7` | Physical streetlights (S1, S2, S3) |
| **10** | **Logic N-MOSFETs (or Driver)**| 3 | S1, S2, S3 PWM gates | Low-side LED switching switches |
| **11** | **RGB Status LED** | 1 | `PB13` (R), `PB14` (G), `PB15` (B)| Multi-color operating state indicator |
| **12** | **Active Buzzer** | 1 | `PB12` | High-temperature and intruder alarm |
| **13** | **Pushbuttons (Tactile)** | 2 | `PA5` (UP), `PA8` (DOWN) | Manual brightness override |
| **14** | **MAX485 Module** | 1 | `PA2` (TX), `PA3` (RX), `PB10` (DE)| Inter-node RS-485 bus simulation |
| **15** | **HC-05 Bluetooth Module** | 1 | `PA9` (TX), `PA10` (RX) | Wireless technician diagnostic console |
| **16** | **Resistors (220 Ω, 330 Ω, 10 kΩ)**| Assorted | Current limit & pull-ups | Circuit protection and signal conditioning |
| **17** | **Solderless Breadboard** | 1-2 | Circuit Platform | Component mounting and breadboarding |
| **18** | **Jumper Wires (M-M, M-F, F-F)** | 40+ | Interconnects | Breadboard signal routing |

---

## Hardware Pinout and Peripheral Mapping

| Signal / Function | MCU Pin | Peripheral | Mode | Hardware Destination |
| :--- | :--- | :--- | :--- | :--- |
| **`S1_PWM`** | `PA0` | TIM2_CH1 | AF Push-Pull (AF1) | Streetlight 1 LED PWM Dimmer |
| **`S2_PWM`** | `PA6` | TIM3_CH1 | AF Push-Pull (AF2) | Streetlight 2 LED PWM Dimmer |
| **`S3_PWM`** | `PA7` | TIM3_CH2 | AF Push-Pull (AF2) | Streetlight 3 LED PWM Dimmer |
| **`LM35_TEMP`** | `PA1` | ADC1_IN1 | Analog Input | LM35 Temperature Sensor |
| **`LDR_LIGHT`** | `PA4` | ADC1_IN4 | Analog Input | LDR Photocell Voltage Divider |
| **`RS485_TX`** | `PA2` | USART2_TX | AF Push-Pull (AF7) | MAX485 Driver Input (DI) |
| **`RS485_RX`** | `PA3` | USART2_RX | AF Push-Pull (AF7) | MAX485 Receiver Output (RO) |
| **`RS485_DE`** | `PB10` | GPIO Out | Digital Output | MAX485 DE & RE Enable Line |
| **`BT_TX`** | `PA9` | USART1_TX | AF Push-Pull (AF7) | HC-05 Bluetooth Telemetry TX |
| **`BT_RX`** | `PA10` | USART1_RX | AF Push-Pull (AF7) | HC-05 Bluetooth Command RX |
| **`PIR_A`** | `PB0` | GPIO In | Digital Input w/ Pull-down | Motion Sensor A (Approach) |
| **`PIR_B`** | `PB1` | GPIO In | Digital Input w/ Pull-down | Motion Sensor B (Direction) |
| **`I2C1_SCL`** | `PB8` | I2C1_SCL | Open-Drain AF (AF4) | SHT31, BH1750, LCD SCL |
| **`I2C1_SDA`** | `PB9` | I2C1_SDA | Open-Drain AF (AF4) | SHT31, BH1750, LCD SDA |
| **`BUZZER`** | `PB12` | GPIO Out | Digital Output | Active Audible Hazard Alarm |
| **`RGB_RED`** | `PB13` | GPIO Out | Digital Output | RGB Hazard Warning Channel |
| **`RGB_GRN`** | `PB14` | GPIO Out | Digital Output | RGB Normal Operation Channel |
| **`RGB_BLU`** | `PB15` | GPIO Out | Digital Output | RGB Corridor Active Channel |
| **`BTN_UP`** | `PA5` | GPIO In | Input w/ Pull-up | Manual Brightness Step Up (+5%) |
| **`BTN_DOWN`** | `PA8` | GPIO In | Input w/ Pull-up | Manual Brightness Step Down (-5%) |
| **`SYS_LED`** | `PC13` | GPIO Out | Digital Output | Onboard Heartbeat LED |

---

## Mathematical Formulations

### 1. LM35 Temperature Sensor Calculation
The LM35 produces an analog output of 10 mV/°C relative to the 3.3V reference:
$$
\text{Temperature (LM35)} = \frac{\text{ADC}_{\text{raw}} \times 3.3\text{ V}}{4095 \times 0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC}_{\text{raw}} \times 330}{4095}
$$

### 2. BH1750 Ambient Light Calculation
The BH1750 internal ADC integrates photometric flux density into calibrated Lux:
$$
\text{Illuminance (Lux)} = \frac{\text{Code}_{\text{raw}}}{1.2}
$$

### 3. SHT31 Temperature and Humidity Calculations
$$
\text{Temperature (SHT31)} = -45 + 175 \times \frac{S_T}{65535}
$$
$$
\text{Relative Humidity (\%RH)} = 100 \times \frac{S_{RH}}{65535}
$$

### 4. PWM Dimmer Frequency Calculation
Operating on TIM2 and TIM3 with a 16 MHz core clock, prescaler PSC = 83, and auto-reload ARR = 999:
$$
f_{\text{PWM}} = \frac{f_{\text{CLK}}}{(\text{PSC} + 1) \times (\text{ARR} + 1)} = \frac{16{,}000{,}000}{84 \times 1000} = 190.48\text{ Hz}
$$

### 5. Motion Direction and Speed Estimation
Given sensor physical separation distance d and trigger time difference $\Delta t$:
$$
\Delta t = t_{\text{PIR\_B}} - t_{\text{PIR\_A}}
$$
$$
v = \frac{d}{|\Delta t|}
$$

---

## Lighting Decision Engine Policy

| Condition | Ambient Lux | Humidity | Motion Event | S1 | S2 | S3 | RGB Status |
| :--- | :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **Daylight** | > 50 lx | Any | Don't Care | 0% | 0% | 0% | Solid Green |
| **Night Normal** | < 30 lx | < 80% RH | None | 20% | 20% | 20% | Dim Green |
| **Night Adverse** | < 30 lx | >= 80% RH | None | 50% | 50% | 50% | Yellow |
| **Vehicle Transit (Forward)** | < 30 lx | Any | PIR-A -> PIR-B | 100% | 100% | 50% | Blue |
| **Vehicle Transit (Reverse)** | < 30 lx | Any | PIR-B -> PIR-A | 50% | 100% | 100% | Blue |
| **Thermal Hazard** | Any | Any | Any | Hold | Hold | Hold | Red Blinking + Buzzer |
| **Manual Override** | Any | Any | Any | User% | User% | User% | Magenta |

---

## Telemetry Format

The unit broadcasts an updated telemetry packet every 500 ms over Bluetooth (USART1) and RS-485 (USART2):

```text
[NODE1] T_LM:31C T_SHT:31.4C H:68% LUX:22lx DIR:FWD S1:100% S2:100% S3:50% STAT:ADAPT
```

- **`T_LM` / `T_SHT`:** Road surface analog temp and ambient digital temp in °C.
- **`H`:** Ambient relative humidity in %RH.
- **`LUX`:** Ambient light illuminance in Lux.
- **`DIR`:** Detected vehicle motion direction (`IDLE`, `FWD`, `REV`).
- **`S1 / S2 / S3`:** Individual luminaire power levels.
- **`STAT`:** Operating state (`DAY`, `ADAPT`, `POOR`, `MANUAL`, `HAZARD`).

---

## Repository Structure

```text
├── src/                        # Standalone Source Code
│   └── main.c                  # 100% self-contained bare-metal CMSIS C firmware
└── docs/                       # Engineering Reports & Specifications
    ├── Case_Study_Report.md    # Full academic case study & technical analysis
    ├── Keil_Setup_Guide.md     # Step-by-step Keil uVision 5 build and wiring guide
    └── System_Specification.md # Hardware register mapping and protocol specifications
```

---

## Build and Flashing Instructions

1. Open **Keil uVision 5** and open or create a project targeting **STM32F401CCUx**.
2. In Manage Run-Time Environment, select **CMSIS -> CORE** and **Device -> Startup** (No HAL required).
3. Add [`src/main.c`](file:///C:/My-Files/College/Sem%205/EMBEDDED/casestudy/src/main.c) into your Source Group.
4. Press **F7** to compile and build the HEX binary.
5. Connect your **ST-LINK v2** to the STM32F401 Black Pill (`SWDIO`, `SWCLK`, `3.3V`, `GND`) and press **F8** to flash.
6. Open any serial terminal at **9600 Baud (8-N-1)** to view real-time telemetry and send test commands (`'F'` for forward motion, `'R'` for reverse motion, `'U'`/`'D'` for manual brightness adjustment, `'A'` for auto mode).
