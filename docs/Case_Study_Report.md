# Embedded Systems Case Study: Energy-Saving Smart Streetlight & Roadside Environmental Monitoring Unit with Bluetooth Telemetry

**Course / Subject:** Embedded Systems & Microcontrollers (Sem 5)  
**Target Microcontroller:** STMicroelectronics STM32F401CCUx / STM32F401CCU6 (Black Pill, ARM Cortex-M4 @ 16 MHz)  
**Architecture:** Pure Bare-Metal Register-Level CMSIS (`#include "stm32f4xx.h"`)  
**Development Tool:** Keil MDK-ARM µVision 5  

---

## 1. Executive Summary & Problem Statement

### 1.1 Problem Statement
Municipal street lighting accounts for up to **40% of a city's total electrical budget**. Conventional street lighting systems exhibit three major engineering flaws:
1. **Unnecessary Power Draw:** Luminaires remain at 100% power during twilight, dawn, or clear moonlit nights when reduced illumination would suffice.
2. **Absence of Roadside Environmental Telemetry:** Municipal road authorities lack localized temperature data to flag road overheating (which leads to asphalt rutting and tire blowouts) or freezing road surface hazards.
3. **Lack of Local & Remote Wireless Maintenance:** Technicians cannot easily adjust or inspect lamp states on-site without complex wired access or high-latency dispatches.

### 1.2 Proposed Solution & Assignment Guidelines Compliance
This project designs and implements an autonomous, energy-efficient **Smart Streetlight & Roadside Environmental Monitoring Unit** written in **100% bare-metal register-level C** using `#include "stm32f4xx.h"`.

The design strictly satisfies all assignment criteria:
1. **Wireless Communication:** Integrates an **HC-05 Bluetooth Module** using an asynchronous NVIC-driven interrupt command handler (`USART1_IRQHandler`) for remote smartphone app control.
2. **ADC & Digital Interface:** Dual-channel 12-bit ADC1 sensing for **LM35** temperature and **LDR** ambient light, with digital GPIOs for push-pull warning beacon and internal pull-up inputs.
3. **NVIC Interrupt Handling:** Configured `NVIC_SetPriority(USART1_IRQn, 0)` and `NVIC_EnableIRQ(USART1_IRQn)` in the ARM Cortex-M4 NVIC.
4. **Display / Status Interface:** Dual-layer LED status interface (**Red Hazard Beacon on `PA3`** and **PWM Luminaire Output on `PA0`**), complemented by live Bluetooth serial telemetry.

---

## 2. Hardware Architecture & Complete Pinout Mapping

### 2.1 Microcontroller Specifications
- **Core:** ARM 32-bit Cortex-M4 with Hardware FPU (Single Precision).
- **Core Clock ($f_{\text{CPU}}$):** 16 MHz derived from High-Speed Internal Oscillator (HSI).
- **Target Board / Chip:** STM32F401CCUx (STM32F401CCU6 Black Pill, 48-pin UFQFPN).
- **Supply Voltage:** 3.3V VDD, with 3.3V analog reference voltage ($V_{\text{ref+}}$).

### 2.2 Peripheral Pinout & Register Configurations

| Signal | MCU Pin | Register Function | Configured Bits / Registers | Description |
| :--- | :--- | :--- | :--- | :--- |
| **`TIM2_CH1`** | `PA0` | Alternate Function 1 (PWM) | `GPIOA->MODER |= (2U << 0)`, `GPIOA->AFR[0] |= (1U << 0)` | Streetlight Luminaire Variable Brightness (0–100% duty cycle) |
| **`ADC1_IN1`** | `PA1` | Analog Input | `GPIOA->MODER |= (3U << 2)` | LM35 Precision Temperature Sensor ($10\text{ mV}/^\circ\text{C}$) |
| **`ADC1_IN2`** | `PA2` | Analog Input | `GPIOA->MODER |= (3U << 4)` | LDR Optical Sensor (Ambient Daylight / Darkness Detection) |
| **`RED_LED`** | `PA3` | Push-Pull Output | `GPIOA->MODER |= (1U << 6)`, `GPIOA->ODR` | Red Roadside Hazard Warning Beacon |
| **`USART1_TX`** | `PA9` | Alternate Function 7 (TX) | `GPIOA->MODER |= (2U << 18)`, `GPIOA->AFR[1] |= (7U << 4)` | HC-05 Bluetooth Telemetry Broadcast (9600 Baud, 8-N-1) |
| **`USART1_RX`** | `PA10` | Alternate Function 7 (RX) | `GPIOA->MODER |= (2U << 20)`, `GPIOA->AFR[1] |= (7U << 8)` | HC-05 Bluetooth Command Ingestion Line (Interrupt-driven) |
| **`BTN_UP`** | `PB0` | Input with Internal Pull-Up | `GPIOB->MODER &= ~(3U << 0)`, `GPIOB->PUPDR |= (1U << 0)` | Manual Brightness Step-Up (+5%) |
| **`BTN_DOWN`** | `PB1` | Input with Internal Pull-Up | `GPIOB->MODER &= ~(3U << 2)`, `GPIOB->PUPDR |= (1U << 2)` | Manual Brightness Step-Down (-5%) |

### 2.3 Bill of Materials (BOM)

| Item | Component | Specification | Quantity | Connected Pin | System Function |
| :---: | :--- | :--- | :---: | :---: | :--- |
| **1** | **STM32F401CCU6** | ARM Cortex-M4 MCU Board (Black Pill) | 1 | Core Controller | System brain, ADC sampling, PWM generation, & ISR |
| **2** | **LM35DZ** | Precision Centigrade Temp Sensor | 1 | `PA1` (ADC1_IN1) | Roadside ambient temperature measurement ($10\text{ mV}/^\circ\text{C}$) |
| **3** | **LDR (5mm)** | Light Dependent Resistor (Photocell) | 1 | `PA2` (ADC1_IN2) | Optical illuminance sensing (dusk / night detection) |
| **4** | **HC-05 / HC-06** | UART Bluetooth Transceiver Module | 1 | `PA9` (TX), `PA10` (RX) | Wireless 9600 Baud telemetry broadcast & command ingestion |
| **5** | **White LED** | 5mm High-Brightness LED | 1 | `PA0` (TIM2_CH1) | Streetlight luminaire ($0\dots100\%$ PWM dimmer) |
| **6** | **Red LED** | 5mm Standard Red LED | 1 | `PA3` (GPIO Out) | Over-temperature roadside hazard alert beacon ($>40^\circ\text{C}$) |
| **7** | **Tactile Switches** | 6x6mm Momentary Pushbutton | 2 | `PB0`, `PB1` | Field maintenance brightness step UP/DOWN controls |
| **8** | **Resistors** | $10\text{ k}\Omega, 220\Omega, 330\Omega$ (1/4W) | 3 | Dividers / Current Limit | Signal conditioning and LED protection |
| **9** | **ST-LINK v2** | USB In-Circuit Debugger / Flasher | 1 | SWD Interface | Program flashing and hardware debugging |
| **10** | **Breadboard** | MB-102 Solderless (830 Tie Points) | 1 | Hardware Platform | Component mounting and circuit prototyping |
| **11** | **Jumper Wires** | Male-to-Male & Male-to-Female | 20+ | Interconnects | Breadboard signal routing and module hookup |

---

## 3. Firmware Architecture: `init_port()` & Business Logic

The firmware in [`src/main.c`](file:///C:/My-Files/College/Sem%205/EMBEDDED/casestudy/src/main.c) is cleanly partitioned into a dedicated hardware initialization routine (`init_port()`) and a non-blocking business logic executive in `main()`:

```
+-----------------------------------------------------------------------------+
|                                firmware/main.c                              |
+-----------------------------------------------------------------------------+
|                                                                             |
|  [init_port()]                                                              |
|   1. Clocks: RCC->AHB1ENR (GPIOA/B), APB1ENR (TIM2), APB2ENR (ADC1, USART1) |
|   2. GPIO Modes: PA0(AF1), PA1/PA2(Analog), PA3(Output), PA9/PA10(AF7)     |
|   3. TIM2 PWM: PSC=83, ARR=999, CCMR1 PWM Mode 1, CCER Channel 1 Enable     |
|   4. ADC1: Prescaler=8MHz, SMPR2 84 cycles, ADON=1                          |
|   5. USART1: BRR=0x0683 (9600 Baud), CR1 (TE, RE, RXNEIE), NVIC Enable      |
|   6. SysTick: 1 ms tick at 16 MHz HSI                                       |
|                                                                             |
|  [Interrupt Handlers]                                                       |
|   - SysTick_Handler()   -> Increments ms_ticks counter                      |
|   - USART1_IRQHandler() -> Parses Bluetooth command bytes ('U','D','A','1')|
|                                                                             |
|  [main() Business Logic]                                                    |
|   - Call init_port()                                                        |
|   - Loop:                                                                   |
|     1. 500 ms Task: Read ADC (LM35/LDR), Calc Temp, Thermal Hysteresis,     |
|                     Send Telemetry String over Bluetooth                    |
|     2. 200 ms Task: Read PB0/PB1 buttons with software debounce lockout     |
|     3. Optical Task: Check LDR thresholds (>2500 -> 100% PWM, <2000 -> Auto)|
+-----------------------------------------------------------------------------+
```

---

## 4. Wireless Protocol & Bluetooth Command Specification

The STM32 connects directly to an **HC-05 / HC-06 Bluetooth Module** via USART1. Incoming wireless characters trigger `USART1_IRQHandler()` in the NVIC, which parses commands in real time:

| Bluetooth Character | Hex Code | Action Executed by Firmware |
| :--- | :---: | :--- |
| **`'U'` or `'u'` or `'+'`** | `0x55` / `0x75` | Steps Streetlight PWM brightness **UP** by $+50$ units ($+5\%$) & sets Manual Mode. |
| **`'D'` or `'d'` or `'-'`** | `0x44` / `0x64` | Steps Streetlight PWM brightness **DOWN** by $-50$ units ($-5\%$) & sets Manual Mode. |
| **`'A'` or `'a'`** | `0x41` / `0x61` | Enables **Auto Photocell Mode** (LDR sensor controls lighting). |
| **`'M'` or `'m'`** | `0x4D` / `0x6D` | Enforces **Manual Maintenance Mode** (locks current brightness). |
| **`'1'`** | `0x31` | Forces **100% Full Illumination** (emergency override). |
| **`'0'`** | `0x30` | Forces **0% Luminaire Shutdown** (energy blackout mode). |

---

## 5. Mathematical Models & Signal Conditioning

### 5.1 Roadside Temperature Conversion (LM35)
The LM35 produces an output voltage linearly proportional to Celsius temperature ($10\text{ mV}/^\circ\text{C}$):
$$V_{\text{in}} = \frac{\text{ADC}}{4095} \times 3.3\text{ V}$$
$$\text{Temperature } (^\circ\text{C}) = \frac{\text{ADC} \times 3.3}{4095 \times 0.010} = \frac{\text{ADC} \times 330}{4095}$$

### 5.2 High-Efficiency PWM Dimmer Calculations
Driven by Timer 2 Channel 1 with $f_{\text{CLK}} = 16\text{ MHz}$, Prescaler $\text{PSC} = 83$, and Period $\text{ARR} = 999$:
$$f_{\text{PWM}} = \frac{16{,}000{,}000}{(83 + 1) \times (999 + 1)} = \frac{16{,}000{,}000}{84 \times 1000} \approx 190.48\text{ Hz}$$
$$\text{Duty Cycle } (\%) = \frac{\text{CCR1}}{\text{ARR} + 1} \times 100\% = \frac{\text{PWM}}{1000} \times 100\%$$

---

## 6. Control Stability & Hysteresis Analysis

### 6.1 Thermal Hazard Schmitt Trigger
- **Upper Threshold ($T_{\text{high}}$):** $40^\circ\text{C} \implies \text{GPIOA->ODR} \text{ bit 3} = 1$ (LED ON)
- **Lower Threshold ($T_{\text{low}}$):** $37^\circ\text{C} \implies \text{GPIOA->ODR} \text{ bit 3} = 0$ (LED OFF)
- **Deadband:** In the range $37^\circ\text{C} \le T \le 40^\circ\text{C}$, the output retains its previous state, preventing chatter around the thermal boundary.

### 6.2 Optical Photocell Hysteresis
- **Night / Darkness Transition:** $\text{ADC\_LDR} > 2500$ ($V_{\text{in}} > 2.01\text{ V}$) triggers full illumination ($100\%$ PWM).
- **Daylight Recovery:** $\text{ADC\_LDR} < 2000$ ($V_{\text{in}} < 1.61\text{ V}$) re-engages automatic photocell mode.

---

## 7. Telemetry Format

The unit broadcasts an updated telemetry frame every **500 ms** over USART1:

```text
Temp:38C | Warn LED:OFF | Lights PWM:450
```

- **`Temp`**: Real-time ambient road temperature in °C.
- **`Warn LED`**: Hazard beacon status (`ON` if $>40^\circ\text{C}$, `OFF` if $<37^\circ\text{C}$).
- **`Lights PWM`**: Current luminaire power compare value ($0 = \text{OFF}$, $1000 = 100\%\text{ Full Power}$).
