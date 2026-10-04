# Embedded Systems Case Study: Predictive Adaptive Smart Streetlight and Environmental Monitoring System

**Course:** Embedded Systems and Microcontrollers  
**Target Microcontroller:** STMicroelectronics STM32F401CCU6 Black Pill, ARM Cortex-M4 @ 16 MHz HSI  
**Architecture:** Pure Bare-Metal Register-Level CMSIS using stm32f4xx.h  
**Development Tool:** Keil MDK-ARM uVision 5  

---

## 1. Executive Summary and Problem Statement

### 1.1 Problem Statement
Municipal street lighting networks consume substantial electrical energy when running at fixed 100% capacity throughout the night. Conventional installations present three major engineering challenges:
1. **Inflexible Power Dissipation:** Luminaires remain energized at full power during low-traffic periods when lower baseline illumination would suffice.
2. **Absence of Environmental Context:** Streetlights do not adapt to surface-moisture condensation, dense fog, or freezing road temperatures where contrast and braking distances degrade.
3. **Lack of Predictive Inter-Zone Coordination:** Isolated streetlights switch abruptly after a target passes rather than dynamically illuminating ahead of an approaching road user.

### 1.2 Proposed Embedded Solution
This project implements a **Predictive Adaptive Smart Streetlight and Environmental Monitoring System** using a single STM32F401CCU6 Black Pill microcontroller running bare-metal CMSIS C firmware.

The design is built on the engineering principle:
> **Motion supplies demand. Environment establishes the operating lighting policy.**

```text
                     ENVIRONMENTAL CONTEXT
             ┌──────────────┬──────────────────┬──────────────┐
             │              │                  │              │
           BH1750         SHT31              LM35            LDR
       (Ambient Lux)   (Air Temp+RH)     (Surface Temp)  (S2 Feedback)
             │              │                  │              │
             └──────────────┼──────────────────┴──────────────┘
                            │
PIR-A (Entry Zone 1) ──┐    ▼
                       ├──> ┌───────────┐
PIR-B (Exit Zone 3) ───┘    │ STM32F401 │
   (Presence Demand)        │ DECISION  │
                            │  ENGINE   │
Onboard Button (Mode) ────> │           │
                            └─────┬─────┘
                                  │
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
  3-CHANNEL PWM              16x2 I2C LCD             BLUETOOTH
   STREETLIGHTS               DASHBOARD               TELEMETRY
  (S1, S2, S3)                                       & COMMANDS
```

The STM32 controls three independent 1.0 kHz PWM channels driving Streetlights S1, S2, and S3 (with S2 functioning as the predicted middle zone). Dual edge-triggered PIR sensors evaluate presence and direction vectors, while the environmental engine computes psychrometric dew points to establish baseline dimming, pre-lighting intensity, and hold times.

---

## 2. Hardware Architecture and Pinout Mapping

### 2.1 Microcontroller Specifications
- **Core:** ARM 32-bit Cortex-M4 with single-precision hardware FPU.
- **Clock Source:** 16 MHz internal High-Speed Internal (HSI) oscillator.
- **Supply Voltage:** 3.3V VDD, 3.3V analog reference (VREF+).
- **Target Board:** STM32F401CCU6 Black Pill (UFQFPN48 package).

### 2.2 Peripheral Pinout Configuration

| Signal | MCU Pin | Peripheral Instance | Mode | Connected Subsystem |
| :--- | :--- | :--- | :--- | :--- |
| **MODE_BTN** | `PA0` | GPIO Input Pull-up | Digital In | Onboard KEY Button (Auto/Manual Toggle) |
| **LM35_TEMP** | `PA1` | ADC1_IN1 | Analog In | Pavement Surface Temperature Probe (5V rail) |
| **S1_PWM** | `PA2` | TIM2_CH3 (AF1) | AF Push-Pull | Streetlight 1 Luminaire (1 kHz PWM 0-100%) |
| **LDR_FB** | `PA4` | ADC1_IN4 | Analog In | Streetlight 2 Optical Feedback Sensor |
| **BTN_UP** | `PA5` | GPIO Input Pull-up | Digital In | Field Technician Manual Step Up (+5%) |
| **S2_PWM** | `PA6` | TIM3_CH1 (AF2) | AF Push-Pull | Streetlight 2 Luminaire (Predicted Zone) |
| **S3_PWM** | `PA7` | TIM3_CH2 (AF2) | AF Push-Pull | Streetlight 3 Luminaire (1 kHz PWM 0-100%) |
| **BTN_DOWN** | `PA8` | GPIO Input Pull-up | Digital In | Field Technician Manual Step Down (-5%) |
| **BT_TX** | `PA9` | USART1_TX (AF7) | AF Push-Pull | HC-05 Bluetooth Telemetry Transmitter |
| **BT_RX** | `PA10` | USART1_RX (AF7) | AF Push-Pull | HC-05 Bluetooth Command Receiver (ISR) |
| **PIR_A** | `PB0` | GPIO In + EXTI0 | Interrupt In | Motion Presence Sensor A (Zone 1) |
| **PIR_B** | `PB1` | GPIO In + EXTI1 | Interrupt In | Motion Presence Sensor B (Zone 3) |
| **I2C1_SCL** | `PB8` | I2C1_SCL (AF4) | Open-Drain AF | Shared Clock for SHT31, BH1750, 16x2 LCD |
| **I2C1_SDA** | `PB9` | I2C1_SDA (AF4) | Open-Drain AF | Shared Data for SHT31, BH1750, 16x2 LCD |
| **BUZZER** | `PB12` | GPIO Output | Digital Out | Audible Hazard Annunciator |
| **RGB_RED** | `PB13` | GPIO Output | Digital Out | RGB Alert Channel (Frost / Hazard) |
| **RGB_GRN** | `PB14` | GPIO Output | Digital Out | RGB Normal Adaptive Operation Channel |
| **RGB_BLU** | `PB15` | GPIO Output | Digital Out | RGB Active Corridor Wave Channel |
| **SYS_LED** | `PC13` | GPIO Output | Digital Out | Onboard 1 Hz Diagnostic Heartbeat |

---

## 3. Mathematical Formulations & Signal Models

### 3.1 LM35 Pavement Surface Temperature (with 16x Averaging)
The LM35 produces 10 mV/°C relative to analog ground:
$$
V_{\text{out}} = \frac{\text{ADC}_{\text{avg}}}{4095} \times 3.3\text{ V}
$$
$$
T_{\text{surface}} = \frac{V_{\text{out}}}{0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC}_{\text{avg}} \times 330}{4095}
$$

### 3.2 BH1750 Photometric Lux Calculation
$$
\text{Illuminance (Lux)} = \frac{\text{MSB} \times 256 + \text{LSB}}{1.2}
$$

### 3.3 SHT31 Psychrometric Air Temperature and Humidity
$$
T_{\text{air}} = -45 + 175 \times \frac{S_T}{65535}
$$
$$
RH = 100 \times \frac{S_{RH}}{65535}
$$

### 3.4 Psychrometric Dew Point (Magnus Formula)
$$
\gamma(T_{\text{air}}, RH) = \frac{17.62 \times T_{\text{air}}}{243.12 + T_{\text{air}}} + \ln\left(\frac{RH}{100}\right)
$$
$$
T_{\text{dew}} = \frac{243.12 \times \gamma}{17.62 - \gamma}
$$

### 3.5 Gamma 2.2 Perceptual Slew Transformation
Human vision responds non-linearly to luminous flux. The system transforms linear perceptual levels ($0\dots1000$) to timer compare values using a Gamma 2.2 profile:
$$
\text{CCR} = 1000 \times \left( \frac{\text{Level}_{\text{perceptual}}}{1000} \right)^{2.2}
$$

---

## 4. Decision Engine & Lighting Policy

### 4.1 Environmental Policy Profiles

| Policy Profile | Context Criteria | Baseline Floor | Pre-Lit Target | Full Target | Hold Duration |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **`DAYLIGHT`** | BH1750 $> 50\text{ lx}$ | 0% (OFF) | 0% | 0% | — |
| **`NORMAL`** | BH1750 $< 20\text{ lx}$, $T_{\text{air}} - T_{\text{dew}} > 3.5^\circ\text{C}$ | 8% (20% raw) | 60% | 100% | 4000 ms |
| **`WET_RISK`** | $T_{\text{air}} - T_{\text{dew}} \le 2.0^\circ\text{C}$ (or $RH \ge 88\%$) | 25% (50% raw) | 100% | 100% | 7000 ms |
| **`FROST_RISK`**| $T_{\text{surface}} \le 3.0^\circ\text{C}$ and $T_{\text{surface}} \le T_{\text{dew}}$ | 25% (50% raw) | 100% | 100% | 10000 ms |

### 4.2 Moving Corridor Progression (Forward Transit)

```text
T0 (Idle Baseline):
  S1 = Base Floor (8% / 25%)
  S2 = Base Floor
  S3 = Base Floor

T1 (PIR-A Trip -> Entry into Zone 1):
  S1 -> 100% (Active Zone)
  S2 -> 100% (Predicted Zone)
  S3 -> Pre-lit Target (60% / 100%)

T2 (PIR-B Trip -> Progression into Zone 2 & 3):
  S1 -> Asymmetric fade to Base Floor
  S2 -> 100%
  S3 -> 100%

T3 (Target Clears Corridor):
  S2 -> Asymmetric fade to Base Floor
  S3 -> Holds 100% for Hold Duration, then fades to Base Floor
```

---

## 5. Diagnostic Subsystems & Fail-Safe Operation

1. **Closed-Loop Lamp Feedback Check:**  
   When Streetlight S2 is commanded to $\ge 80\%$, the LDR on `PA4` is sampled. If measured light remains low, the system flags `LAMP2_FAULT` on the LCD and telemetry without disrupting adaptive operation.
2. **Fail-Bright Optical Architecture:**  
   If the BH1750 sensor disconnects or NACKs on I2C1, the system defaults to night mode rather than failing dark, ensuring road safety.
3. **PIR Sensor Blanking & Crosstalk Isolation:**  
   EXTI ISRs reject events for the first 45 seconds after boot while HC-SR501 sensors stabilize. Collimation tubes restrict each sensor's conical angle to $30^\circ$, preventing false reverse triggers.
