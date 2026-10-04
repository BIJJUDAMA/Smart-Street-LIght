<div align="center">

# STM32-Based Predictive Adaptive Smart Streetlight & Environmental Monitoring System

[![Microcontroller](https://img.shields.io/badge/MCU-STM32F401CCU6%20%28Black%20Pill%29-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f401cc.html)
[![Architecture](https://img.shields.io/badge/Architecture-ARM%20Cortex--M4%20w%2F%20FPU-brightgreen.svg)]()
[![IDE](https://img.shields.io/badge/IDE-Keil%20MDK--ARM%20%2F%20uVision%205-lightgrey.svg)]()
[![Firmware](https://img.shields.io/badge/Firmware-Bare--Metal%20CMSIS%20Register-brightgreen.svg)]()

<p align="center">
  <b>An intelligent, energy-saving municipal streetlight controller and environmental monitoring system built on a single STM32F401CCU6 Black Pill simulating a predictive 3-zone moving illumination corridor.</b>
</p>

</div>

---

The system optimizes municipal street lighting energy consumption by combining real-time environmental context with vector presence tracking. A single STM32F401CCU6 microcontroller drives three independent 1.0 kHz PWM channels (S1, S2, S3) to simulate a multi-zone roadway corridor, adjusting illumination baselines and hold durations based on psychrometric dew point calculations while streaming telemetry over Bluetooth.

---

## Key Features

- **Predictive Moving Illumination Corridor:** Uses dual EXTI-timestamped PIR sensors (PIR-A and PIR-B) to determine direction vectors (forward vs reverse), pre-lighting upcoming streetlights and fading trailing lights as road users clear each zone.
- **Context-Aware Environmental Adaptation:** Environmental sensors establish the baseline policy while motion supplies instantaneous demand:
  - *Daylight:* Luminaires extinguished (0%).
  - *Clear Night:* Base idle brightness maintained at 8% perceptual (20% PWM) to minimize power draw.
  - *Surface Moisture / Fog Risk:* Baseline dynamically elevated to 25% perceptual (50% PWM) with longer pre-lighting and hold times.
  - *Frost & Black Ice Hazard:* Road surface temperature ($\le 3^\circ\text{C}$) paired with psychrometric dew point triggers hazard alerts and maximum pre-lighting.
- **Psychrometric Dew Point Engine:** Computes real-time air dew points using the Magnus formula via hardware floating-point operations.
- **Closed-Loop Optical Feedback:** LDR on `PA4` monitors Streetlight S2 output inside an optical tube to detect luminaire driver faults.
- **1.0 kHz Hardware PWM with Gamma 2.2 Correction:** Drives three independent low-side MOSFET channels using `TIM2_CH3` (`PA2`), `TIM3_CH1` (`PA6`), and `TIM3_CH2` (`PA7`) with asymmetric slew rates for flicker-free dimming.
- **Non-Blocking Architecture:** Operates with zero busy-wait delays, featuring an EXTI interrupt event layer, SysTick cooperative scheduler, and I2C timeout protection.

---

## System Architecture

```text
                                  ENVIRONMENTAL CONTEXT
                   ┌──────────────┬──────────────────┬──────────────┐
                   │              │                  │              │
                 BH1750         SHT31              LM35            LDR
             (Ambient Lux)  (Air Temp+RH)      (Surface Temp)  (S2 Feedback)
                   │              │                  │              │
                   └──────────────┼──────────────────┴──────────────┘
                                  │
PIR-A (Zone 1) ──┐                ▼
                 ├──(EXTI)──> ┌───────────┐
PIR-B (Zone 3) ──┘            │ STM32F401 │
                              │ DECISION  │
Onboard Button (Mode) ──────> │  ENGINE   │
                              └─────┬─────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
  3-CHANNEL 1kHz PWM           16x2 I2C LCD                BLUETOOTH
   STREETLIGHTS                 DASHBOARD                  TELEMETRY
  (S1, S2, S3)                                            & COMMANDS
```

### Moving Illumination Corridor Progression (Forward Pass)

```text
T0 (Idle Baseline):
  S1: [====                ] 20%
  S2: [====                ] 20%
  S3: [====                ] 20%

T1 (PIR-A Triggers - Vehicle Entering Zone 1):
  S1: [====================] 100% (Active zone)
  S2: [====================] 100% (Predicted zone)
  S3: [==========          ] 50%  (Pre-lit zone)

T2 (PIR-B Triggers - Vehicle Entering Zone 2):
  S1: [====                ] 20%  (Decays behind vehicle)
  S2: [====================] 100% (Active zone)
  S3: [====================] 100% (Upcoming zone)

T3 (Restored Baseline after Hold Time):
  S1: [====                ] 20%
  S2: [====                ] 20%
  S3: [====                ] 20%
```

---

## Bill of Materials (BOM)

| Item | Component Description | Quantity | Target Pin / Bus | Purpose |
| :---: | :--- | :---: | :--- | :--- |
| **1** | **STM32F401CCU6 Black Pill** | 1 | Microcontroller Core | 16 MHz ARM Cortex-M4 MCU Board |
| **2** | **ST-Link V2 Programmer** | 1 | SWD (SWDIO, SWCLK) | Firmware flashing and in-circuit debugging |
| **3** | **HC-SR501 PIR Sensors** | 2 | `PB0` (PIR-A), `PB1` (PIR-B) | Presence and direction detection (with tubes) |
| **4** | **BH1750 Lux Sensor (GY-302)** | 1 | `PB8` (SCL), `PB9` (SDA) | Calibrated ambient illumination (Lux) |
| **5** | **LM35 Precision Temp Sensor** | 1 | `PA1` (ADC1_IN1) | Pavement surface temperature probe (5V supply) |
| **6** | **SHT31-D Temp & Humidity** | 1 | `PB8` (SCL), `PB9` (SDA) | Ambient microclimate and psychrometric dew point |
| **7** | **LDR Photocell Module** | 1 | `PA4` (ADC1_IN4) | Streetlight 2 optical feedback verification |
| **8** | **16x2 I2C LCD Display** | 1 | `PB8` (SCL), `PB9` (SDA) | Real-time state and telemetry dashboard |
| **9** | **Bright White LEDs** | 3 | `PA2`, `PA6`, `PA7` | Physical streetlights (S1, S2, S3) |
| **10** | **Logic N-MOSFETs (or Driver)**| 3 | S1, S2, S3 PWM gates | Low-side LED switching switches |
| **11** | **RGB Status LED** | 1 | `PB13` (R), `PB14` (G), `PB15` (B)| Multi-color operating state indicator |
| **12** | **Active Buzzer** | 1 | `PB12` | Single-pulse audible state annunciator |
| **13** | **Pushbuttons (Tactile)** | 2 | `PA5` (UP), `PA8` (DOWN) | Manual brightness override (+5% / -5%) |
| **14** | **HC-05 Bluetooth Module** | 1 | `PA9` (TX), `PA10` (RX) | Wireless technician diagnostic console |
| **15** | **Resistors (220 Ω, 330 Ω, 10 kΩ)**| Assorted | Current limit & pull-ups | Circuit protection and signal conditioning |
| **16** | **Solderless Breadboard** | 1-2 | Circuit Platform | Component mounting and prototyping |
| **17** | **Jumper Wires (M-M, M-F, F-F)** | 40+ | Interconnects | Breadboard signal routing |

---

## Hardware Pinout and Peripheral Mapping

| Signal / Function | MCU Pin | Peripheral | Mode | Hardware Destination |
| :--- | :--- | :--- | :--- | :--- |
| **`MODE_BTN`** | `PA0` | GPIO In | Input w/ Pull-up | Onboard KEY Button (Auto/Manual) |
| **`LM35_TEMP`** | `PA1` | ADC1_IN1 | Analog Input | LM35 Pavement Surface Probe |
| **`S1_PWM`** | `PA2` | TIM2_CH3 | AF Push-Pull (AF1) | Streetlight 1 1.0 kHz PWM Dimmer |
| **`LDR_FB`** | `PA4` | ADC1_IN4 | Analog Input | S2 Optical Output Feedback |
| **`BTN_UP`** | `PA5` | GPIO In | Input w/ Pull-up | Manual Step Up (+5%) |
| **`S2_PWM`** | `PA6` | TIM3_CH1 | AF Push-Pull (AF2) | Streetlight 2 1.0 kHz PWM Dimmer |
| **`S3_PWM`** | `PA7` | TIM3_CH2 | AF Push-Pull (AF2) | Streetlight 3 1.0 kHz PWM Dimmer |
| **`BTN_DOWN`** | `PA8` | GPIO In | Input w/ Pull-up | Manual Step Down (-5%) |
| **`BT_TX`** | `PA9` | USART1_TX | AF Push-Pull (AF7) | HC-05 Bluetooth Telemetry TX |
| **`BT_RX`** | `PA10` | USART1_RX | AF Push-Pull (AF7) | HC-05 Bluetooth Command RX |
| **`PIR_A`** | `PB0` | EXTI0 | Interrupt In | Motion Presence Sensor A (Zone 1) |
| **`PIR_B`** | `PB1` | EXTI1 | Interrupt In | Motion Presence Sensor B (Zone 3) |
| **`I2C1_SCL`** | `PB8` | I2C1_SCL | Open-Drain AF (AF4) | SHT31, BH1750, LCD SCL |
| **`I2C1_SDA`** | `PB9` | I2C1_SDA | Open-Drain AF (AF4) | SHT31, BH1750, LCD SDA |
| **`BUZZER`** | `PB12` | GPIO Out | Digital Output | Active Audible Hazard Alarm |
| **`RGB_RED`** | `PB13` | GPIO Out | Digital Output | RGB Hazard Warning Channel |
| **`RGB_GRN`** | `PB14` | GPIO Out | Digital Output | RGB Normal Operation Channel |
| **`RGB_BLU`** | `PB15` | GPIO Out | Digital Output | RGB Corridor Active Channel |
| **`SYS_LED`** | `PC13` | GPIO Out | Digital Output | Onboard 1 Hz Heartbeat LED |

---

## Mathematical Formulations

### 1. LM35 Temperature Sensor with 16x Oversampling
$$
\text{Temperature (LM35)} = \frac{\text{ADC}_{\text{avg}} \times 3.3\text{ V}}{4095 \times 0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC}_{\text{avg}} \times 330}{4095}
$$

### 2. SHT31 Dew Point Calculation (Magnus Formula)
$$
\gamma(T_{\text{air}}, RH) = \frac{17.62 \times T_{\text{air}}}{243.12 + T_{\text{air}}} + \ln\left(\frac{RH}{100}\right)
$$
$$
T_{\text{dew}} = \frac{243.12 \times \gamma}{17.62 - \gamma}
$$

### 3. 1.0 kHz PWM Carrier Formulation
Operating on TIM2 and TIM3 with a 16 MHz internal HSI core clock, prescaler PSC = 15, and auto-reload ARR = 999:
$$
f_{\text{PWM}} = \frac{f_{\text{CLK}}}{(\text{PSC} + 1) \times (\text{ARR} + 1)} = \frac{16{,}000{,}000}{16 \times 1000} = 1000\text{ Hz}
$$

### 4. Perceptual Gamma 2.2 Slew Transformation
$$
\text{CCR} = 1000 \times \left( \frac{\text{Level}_{\text{perceptual}}}{1000} \right)^{2.2}
$$

---

## Lighting Decision Engine Policy

| Policy Profile | Ambient Lux | Surface / Dew Point Context | S1 Level | S2 Level | S3 Level | RGB Status |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **`DAYLIGHT`** | $> 50\text{ lx}$ | Any | 0% (OFF) | 0% (OFF) | 0% (OFF) | Solid Green |
| **`NORMAL`** | $< 20\text{ lx}$ | $T_{\text{air}} - T_{\text{dew}} > 3.5^\circ\text{C}$ | 8% (Idle) | 8% (Idle) | 8% (Idle) | Solid Green |
| **`WET_RISK`** | $< 20\text{ lx}$ | $T_{\text{air}} - T_{\text{dew}} \le 2.0^\circ\text{C}$ | 25% (Floor) | 25% (Floor) | 25% (Floor) | Yellow |
| **`CORRIDOR_FWD`** | $< 20\text{ lx}$ | PIR-A $\to$ PIR-B Trigger | 100% | 100% | 60% (Pre-lit) | Blue |
| **`CORRIDOR_REV`** | $< 20\text{ lx}$ | PIR-B $\to$ PIR-A Trigger | 60% (Pre-lit) | 100% | 100% | Blue |
| **`FROST_RISK`** | Any | $T_{\text{surface}} \le 3^\circ\text{C} \le T_{\text{dew}}$ | 25% (Floor) | 25% (Floor) | 25% (Floor) | Red + Alert |
| **`MANUAL`** | Any | Onboard Button / Terminal | User % | User % | User % | Magenta |

---

## Telemetry Format

The unit broadcasts an updated telemetry packet every 1000 ms over Bluetooth (USART1 @ 9600 Baud):

```text
[N1] T_S:24C T_A:24.5C H:65% LUX:18lx DIR:FWD S1:100% S2:100% S3:50% STAT:NORMAL
```

- **`T_S` / `T_A`:** Surface temp (LM35) and Air temp (SHT31) in °C.
- **`H`:** Ambient relative humidity in %RH.
- **`LUX`:** Ambient light illuminance in Lux.
- **`DIR`:** Detected presence vector (`IDLE`, `FWD`, `REV`, `AMBIG`).
- **`S1 / S2 / S3`:** Individual luminaire power percentages.
- **`STAT`:** Policy context (`DAY`, `NORMAL`, `WET_RISK`, `FROST_RISK`, `MANUAL`).
