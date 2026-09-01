<div align="center">

# 🏙️ Energy-Saving Smart Streetlight & Roadside Environmental Monitoring Unit

[![Microcontroller](https://img.shields.io/badge/MCU-STM32F401CCU6%20%28Black%20Pill%29-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f401cc.html)
[![Architecture](https://img.shields.io/badge/Architecture-ARM%20Cortex--M4%20w%2F%20FPU-brightgreen.svg)]()
[![IDE](https://img.shields.io/badge/IDE-Keil%20MDK--ARM%20%2F%20µVision%205-lightgrey.svg)]()
[![CMSIS](https://img.shields.io/badge/Firmware-Bare--Metal%20CMSIS%20Register-brightgreen.svg)]()

<p align="center">
  <b>An intelligent, energy-efficient municipal streetlight controller and environmental watchdog built on the STM32F401CCUx ARM Cortex-M4 microcontroller.</b>
</p>

</div>

---

The system reduces municipal electrical grid load by automating streetlight brightness with real-time ambient optical sensing (LDR), provides on-pole maintenance override capabilities, monitors roadside surface/ambient thermal conditions (LM35) to trigger hazard alerts with hysteresis protection, and streams live telemetry to a municipal monitoring console over Bluetooth (USART1).

---

## 🌟 Key Features

- **💡 Autonomous Optical Dimming (PWM):** Uses an LDR (Light Dependent Resistor) circuit to automatically detect dusk/night conditions and drive the streetlight luminaire to 100% brightness via high-frequency Pulse Width Modulation (`TIM2_CH1` @ 190.5 Hz).
- **🛠️ Manual Field Maintenance Override:** On-pole tactile push buttons (`BTN_UP` / `BTN_DOWN`) allow maintenance technicians and emergency crews to adjust lamp intensity in 5% steps ($0\%\dots100\%$) with a 200 ms non-blocking software debounce lockout.
- **🌡️ Roadside Thermal Hazard Watchdog:** Samples an analog LM35 precision temperature sensor to monitor asphalt/ambient road temperatures, triggering a **Hazard Beacon (Red LED)** when temperatures exceed safe operating limits ($>40^\circ\text{C}$).
- **🛡️ Noise-Immune Hysteresis Control:** Employs a $3^\circ\text{C}$ thermal deadband ($37^\circ\text{C} \dots 40^\circ\text{C}$) and a 500-count optical hysteresis zone ($2000 \dots 2500$ ADC counts) to eliminate relay/LED chattering caused by transient shadows or thermal noise.
- **📡 Wireless Bluetooth Telemetry & Control:** Transmits structured ASCII telemetry packets every 500 ms and parses incoming wireless single-byte commands via NVIC UART interrupt (`USART1` @ 9600 Baud).
- **⏱️ Deterministic Bare-Metal Scheduling:** Engineered purely at register level without busy-wait delays, using the hardware SysTick monotonic counter for multi-rate scheduling.

---

## 📦 Bill of Materials (BOM)

| Item | Component Description | Quantity | Target Pin / Connection | Purpose |
| :---: | :--- | :---: | :---: | :--- |
| **1** | **STM32F401CCU6 (Black Pill)** | 1 | Microcontroller Core | 84 MHz ARM Cortex-M4 Development Board |
| **2** | **LM35 Precision Temp Sensor** | 1 | `PA1` (ADC1_IN1), 3.3V, GND | Measures asphalt & ambient road surface temp ($10\text{ mV}/^\circ\text{C}$) |
| **3** | **LDR (Photocell Sensor)** | 1 | `PA2` (ADC1_IN2) | Optical sensing (detects dusk, tunnels, and darkness) |
| **4** | **HC-05 / HC-06 Bluetooth Module** | 1 | `PA9` (TX), `PA10` (RX), 3.3V/5V | Wireless telemetry streaming & smartphone command reception |
| **5** | **Streetlight Luminaire (White/Yellow LED)** | 1 | `PA0` (TIM2_CH1) via $220\Omega$ resistor | Variable brightness streetlight dimmer ($0\dots100\%$ PWM) |
| **6** | **Red Hazard Warning LED** | 1 | `PA3` via $330\Omega$ resistor | Road thermal hazard alert beacon ($>40^\circ\text{C}$) |
| **7** | **Tactile Pushbuttons (6x6mm)** | 2 | `PB0` (Step UP), `PB1` (Step DOWN) | Manual field maintenance brightness controls |
| **8** | **Resistors ($10\text{ k}\Omega, 220\Omega, 330\Omega$)** | 3 | LDR Divider ($10\text{k}$), LEDs ($220/330\Omega$) | Voltage divider and current limiting |
| **9** | **ST-LINK v2 USB Programmer** | 1 | `SWDIO`, `SWCLK`, 3.3V, GND | Firmware flashing and in-circuit debugging |
| **10** | **MB-102 Solderless Breadboard (830 Points)** | 1 | Circuit Platform | Prototyping and component mounting |
| **11** | **Jumper Wires (Male-Male & Male-Female)** | 20+ | Interconnects | Breadboard signal routing and sensor hookup |

---

## 📌 Hardware Pinout & Peripheral Configuration

| Signal / Label | MCU Pin | Peripheral Instance | Mode / Direction | Hardware Function |
| :--- | :--- | :--- | :--- | :--- |
| **`TIM2_CH1`** | `PA0` | Timer 2 Channel 1 | Alternate Function (PWM) | Variable Duty Cycle Luminaire Dimmer (0–100%) |
| **`ADC1_IN1`** | `PA1` | ADC1 (Channel 1) | Analog Input | LM35 Precision Roadside Temperature Sensor |
| **`ADC1_IN2`** | `PA2` | ADC1 (Channel 2) | Analog Input | LDR Ambient Optical Photocell Sensor |
| **`RED_LED`** | `PA3` | GPIO Port A | Push-Pull Output | Roadside Thermal Hazard Warning Beacon |
| **`USART1_TX`** | `PA9` | USART1 | AF Push-Pull (TX) | Serial Telemetry Broadcast (9600-8N1) |
| **`USART1_RX`** | `PA10` | USART1 | AF Push-Pull (RX) | Diagnostic / Command Ingestion Line |
| **`BTN_UP`** | `PB0` | GPIO Port B | Input w/ Pull-Up | Manual Dimmer Brightness Step-Up (+5%) |
| **`BTN_DOWN`** | `PB1` | GPIO Port B | Input w/ Pull-Up | Manual Dimmer Brightness Step-Down (-5%) |

---

## 📐 System Architecture

```
                  +-----------------------------------------------+
                  |       STM32F401CCUx (Black Pill) MCU          |
                  |                                               |
[LM35 Temp Sensor] --> PA1 (ADC1_IN1)             PA0 (TIM2_CH1)  |--> [PWM Streetlight LED]
                  |                                               |
[LDR Light Sensor]--> PA2 (ADC1_IN2)             PA3 (GPIO_Out)  |--> [Hazard Warning Beacon]
                  |                                               |
[BTN_UP Pushbutton]-> PB0 (GPIO_In, Pull-up)     PA9 (USART1_TX) |--> [Municipal Telemetry]
                  |                                               |
[BTN_DOWN Pushbtn]--> PB1 (GPIO_In, Pull-up)     PA10(USART1_RX) |<-- [Diagnostic Serial Console]
                  +-----------------------------------------------+
```

---

## 🔬 Mathematical Formulations

### 1. Temperature Sensor (LM35) Calculation
The LM35 outputs a linear voltage of $10\text{ mV}/^\circ\text{C}$ relative to the $3.3\text{ V}$ ($3300\text{ mV}$) reference:
$$\text{Temperature } (^\circ\text{C}) = \frac{\text{ADC} \times 3.3\text{ V}}{4095 \times 0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC} \times 330}{4095}$$

### 2. PWM Frequency Formulation
Configured on TIM2 with an internal $16\text{ MHz}$ core clock:
$$f_{\text{PWM}} = \frac{f_{\text{CLK}}}{(\text{PSC} + 1) \times (\text{ARR} + 1)} = \frac{16{,}000{,}000}{(83 + 1) \times (999 + 1)} \approx 190.48\text{ Hz}$$

---

## 📊 Telemetry Format

The unit broadcasts an updated telemetry frame every **500 ms** over USART1:

```text
Temp:38C | Warn LED:OFF | Lights PWM:450
```

- **`Temp`**: Real-time ambient road temperature in °C.
- **`Warn LED`**: Hazard indicator state (`ON` if $>40^\circ\text{C}$, `OFF` if $<37^\circ\text{C}$).
- **`Lights PWM`**: Current luminaire power compare value ($0 = \text{OFF}$, $1000 = 100\%\text{ Full Power}$).

---

## 📁 Repository Structure

```
├── src/                        # Standalone Source Code
│   └── main.c                  # 100% self-contained bare-metal C source code
└── docs/                       # Comprehensive Engineering Case Study Reports
    ├── Case_Study_Report.md    # Full academic & industrial case study report
    ├── Keil_Setup_Guide.md     # Step-by-step Keil5 fresh project creation guide
    └── System_Specification.md # Hardware datasheet, registers, & state machine
```

---

## 🛠️ Build Guide

1. **Creating a Fresh Keil5 Project:**
   - Create a new project in Keil µVision 5 for **STM32F401CCUx** (or `STM32F401CC`).
   - Under Manage Run-Time Environment, check only **CMSIS Core** and **Device Startup** (No HAL required).
   - Copy [`src/main.c`](file:///C:/My-Files/College/Sem%205/EMBEDDED/casestudy/src/main.c) into your project.
   - In Target Options (`Alt + F7`) $\to$ **Output**, check **`Create HEX File`**.
   - Build (`F7`) to generate your `.hex` binary executable.
2. **Flashing the Hardware:** Flash the generated `.hex` binary onto your STM32F401CCUx microcontroller using an ST-LINK v2 programmer.
3. **Execution:** Connect an HC-05 Bluetooth module to USART1 (`PA9`/`PA10`) or open a serial terminal at 9600 Baud to view live telemetry and send Bluetooth wireless commands (`'U'`, `'D'`, `'A'`, `'1'`, `'0'`).
