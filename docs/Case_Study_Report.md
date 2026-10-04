# Embedded Systems Case Study: Predictive Adaptive Smart Streetlight and Environmental Monitoring Network

**Course:** Embedded Systems and Microcontrollers  
**Target Microcontroller:** STMicroelectronics STM32F401CCU6 Black Pill, ARM Cortex-M4 @ 16 MHz  
**Architecture:** Pure Bare-Metal Register-Level CMSIS using stm32f4xx.h  
**Development Tool:** Keil MDK-ARM uVision 5  

---

## 1. Executive Summary and Problem Statement

### 1.1 Problem Statement
Municipal street lighting networks consume up to 40% of a city electrical budget. Conventional street lighting systems suffer from three fundamental inefficiencies:
1. **Static Inflexible Power Dissipation:** Luminaires remain energized at 100% power throughout the night regardless of actual traffic presence, wasting massive amounts of electrical energy during low-traffic early morning hours.
2. **Absence of Environmental Context:** Streetlights do not adapt to adverse environmental conditions such as dense fog, high humidity, rainfall, or roadside surface overheating, compromising road safety.
3. **Isolated Silos Without Predictive Coordination:** Traditional poles operate independently. When a vehicle approaches, adjacent downstream streetlights cannot anticipate its arrival, preventing smooth illumination corridors from forming.

### 1.2 Proposed Engineering Solution
This project implements a fully functional **Predictive Adaptive Smart Streetlight and Roadside Environmental Monitoring Network** powered by a single STM32F401CCU6 microcontroller running bare-metal CMSIS firmware.

The system proves the design principle:
> **Environment determines the lighting context; motion determines the immediate lighting demand.**

```text
                    ENVIRONMENT SUBSYSTEM
             ┌──────┬──────────────┬──────────────┬──────────────┐
             │      │              │              │              │
            LM35   LDR           SHT31          BH1750          PIR
          (Analog (Analog      (Digital        (Digital       (Motion
           Temp)   Light)      Temp+Hum)         Lux)         Vector)
             │      │              │              │              │
             └──────┴──────────────┼──────────────┴──────────────┘
                                   │
                                   ▼
                             ┌───────────┐
                             │ STM32F401 │
                             │ DECISION  │
                             │  ENGINE   │
                             └─────┬─────┘
                                   │
                      ┌────────────┼────────────┐
                      ▼            ▼            ▼
                     PWM         RS-485        LCD
                 3-CHANNEL      BUS NODE     DISPLAY
                STREETLIGHTS
                 (S1,S2,S3)
```

The STM32 manages an entire simulated 3-node physical corridor using three independent PWM-driven LED luminaires (S1, S2, S3), dual PIR sensors (PIR-A and PIR-B) for vector direction and speed estimation, dual-technology environmental monitoring (analog LM35 plus digital SHT31, and analog LDR plus digital BH1750), a 16x2 I2C LCD, an RGB status beacon, an active buzzer, an RS-485 inter-node bus transceiver, and a Bluetooth maintenance interface.

---

## 2. Hardware Architecture and Pinout Mapping

### 2.1 Microcontroller Specifications
- **Core:** ARM 32-bit Cortex-M4 with single-precision hardware FPU.
- **Operating Frequency:** 16 MHz internal High-Speed Internal (HSI) oscillator.
- **Supply Voltage:** 3.3V VDD, 3.3V analog reference (VREF+).
- **Target Board:** STM32F401CCU6 Black Pill (UFQFPN48 package).

### 2.2 Peripheral Pinout Configuration

| Signal | MCU Pin | Peripheral Instance | Mode | Connected Subsystem |
| :--- | :--- | :--- | :--- | :--- |
| **S1_PWM** | `PA0` | TIM2_CH1 (AF1) | AF Push-Pull | Streetlight 1 Luminaire (PWM 0-100%) |
| **S2_PWM** | `PA6` | TIM3_CH1 (AF2) | AF Push-Pull | Streetlight 2 Luminaire (PWM 0-100%) |
| **S3_PWM** | `PA7` | TIM3_CH2 (AF2) | AF Push-Pull | Streetlight 3 Luminaire (PWM 0-100%) |
| **LM35_ADC** | `PA1` | ADC1_IN1 | Analog Input | Road Surface Analog Temperature Sensor |
| **LDR_ADC** | `PA4` | ADC1_IN4 | Analog Input | Relative Photocell Ambient Light Sensor |
| **RS485_TX** | `PA2` | USART2_TX (AF7) | AF Push-Pull | MAX485 Bus Transmitter |
| **RS485_RX** | `PA3` | USART2_RX (AF7) | AF Push-Pull | MAX485 Bus Receiver |
| **RS485_DE** | `PB10` | GPIO Output | Digital Out | MAX485 Driver/Receiver Direction Enable |
| **BT_TX** | `PA9` | USART1_TX (AF7) | AF Push-Pull | HC-05 Bluetooth Telemetry Transmitter |
| **BT_RX** | `PA10` | USART1_RX (AF7) | AF Push-Pull | HC-05 Bluetooth Command Receiver (ISR) |
| **PIR_A** | `PB0` | GPIO Input | Digital In | Motion Sensor A (Approach Detection) |
| **PIR_B** | `PB1` | GPIO Input | Digital In | Motion Sensor B (Direction Verification) |
| **I2C1_SCL** | `PB8` | I2C1_SCL (AF4) | Open-Drain AF | Shared Clock for SHT31, BH1750, 16x2 LCD |
| **I2C1_SDA** | `PB9` | I2C1_SDA (AF4) | Open-Drain AF | Shared Data for SHT31, BH1750, 16x2 LCD |
| **BUZZER** | `PB12` | GPIO Output | Digital Out | Active Audible Hazard/Alert Annunciator |
| **RGB_RED** | `PB13` | GPIO Output | Digital Out | RGB Alert Indicator (Red Channel) |
| **RGB_GRN** | `PB14` | GPIO Output | Digital Out | RGB Alert Indicator (Green Channel) |
| **RGB_BLU** | `PB15` | GPIO Output | Digital Out | RGB Alert Indicator (Blue Channel) |
| **BTN_UP** | `PA5` | GPIO Input Pull-up | Digital In | Manual Brightness Step Up (+5%) |
| **BTN_DOWN** | `PA8` | GPIO Input Pull-up | Digital In | Manual Brightness Step Down (-5%) |
| **SYS_LED** | `PC13` | GPIO Output | Digital Out | Black Pill Onboard Diagnostic Heartbeat |

---

## 3. Bill of Materials (BOM)

| Item | Component | Specification | Quantity | Purpose |
| :---: | :--- | :--- | :---: | :--- |
| **1** | STM32F401CCU6 | ARM Cortex-M4 84 MHz Black Pill | 1 | Master Embedded Edge Controller |
| **2** | ST-Link V2 | USB Programmer / In-Circuit Debugger | 1 | Firmware Flashing via SWD |
| **3** | HC-SR501 | Pyroelectric PIR Motion Sensor | 2 | Motion and Direction Detection |
| **4** | LDR Module | 5mm Photocell with Trimmer | 1 | Relative Day/Night Ambient Detection |
| **5** | BH1750 (GY-302) | 16-bit Calibrated Ambient Lux Sensor | 1 | Photometric Light Measurement & Verification |
| **6** | LM35DZ | Analog Precision Temperature Sensor | 1 | Road Surface Thermal Monitoring (10 mV/°C) |
| **7** | SHT31-D | I2C Digital Temp & Humidity Sensor | 1 | Microclimate and Fog/Rain Detection |
| **8** | 16x2 I2C LCD | HD44780 with PCF8574 I2C Backpack | 1 | Real-time System Dashboard Display |
| **9** | White LEDs | 5mm High-Brightness LED | 3 | Physical Streetlight Nodes (S1, S2, S3) |
| **10** | Logic N-MOSFETs | 2N7000 or AO3400A or Transistors | 3 | Streetlight PWM Low-Side Driver Switch |
| **11** | RGB LED | 5mm Common Cathode | 1 | Multi-color Status Beacon |
| **12** | Active Buzzer | 5V / 3.3V Audible Buzzer | 1 | Over-temperature and Intrusion Alert |
| **13** | Pushbuttons | 6x6mm Tactile Switches | 2 | Field Maintenance Manual Control |
| **14** | MAX485 Module | TTL to RS-485 Transceiver | 1 | Inter-Pole Network Bus Simulation |
| **15** | HC-05 Module | UART Bluetooth SPP Transceiver | 1 | Technician Smartphone Telemetry Port |
| **16** | Resistor Pack | 220 Ω, 330 Ω, 4.7 kΩ, 10 kΩ | 1 Set | Current Limiting and I2C/Switch Pull-ups |
| **17** | Breadboard | 830 Tie-Point Solderless Board | 1-2 | Circuit Platform |
| **18** | Jumper Wires | M-M, M-F, F-F | 40+ | Subsystem Interconnections |

---

## 4. Mathematical Formulations and Sensor Models

### 4.1 LM35 Precision Analog Temperature Sensor
The LM35 produces an analog voltage output directly proportional to temperature at 10 mV/°C:
$$
V_{\text{out}} = \frac{\text{ADC}_{\text{raw}}}{4095} \times 3.3\text{ V}
$$
$$
T_{\text{LM35}} = \frac{V_{\text{out}}}{0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC}_{\text{raw}} \times 330}{4095}
$$

### 4.2 BH1750 Digital Lux Photometric Conversion
The BH1750 returns a 16-bit raw digital count representing luminous flux density:
$$
\text{Illuminance (Lux)} = \frac{\text{MSB} \times 256 + \text{LSB}}{1.2}
$$

### 4.3 SHT31 Psychrometric Humidity and Temperature Formulas
The SHT31 outputs raw 16-bit integers for temperature and relative humidity:
$$
T_{\text{SHT31}} = -45 + 175 \times \frac{S_T}{65535}
$$
$$
RH = 100 \times \frac{S_{RH}}{65535}
$$

### 4.4 PWM Dimmer Frequency Formulation
Driven by Timer 2 and Timer 3 running on the 16 MHz APB1 peripheral bus:
$$
f_{\text{PWM}} = \frac{f_{\text{CLK}}}{(\text{PSC} + 1) \times (\text{ARR} + 1)} = \frac{16{,}000{,}000}{(83 + 1) \times (999 + 1)} = 190.48\text{ Hz}
$$
The duty cycle percentage is given by:
$$
\text{Duty Cycle } (\%) = \frac{\text{CCR}}{\text{ARR} + 1} \times 100\% = \frac{\text{CCR}}{1000} \times 100\%
$$

### 4.5 Motion Direction Vector and Speed Estimation
PIR-A and PIR-B are spaced along the road corridor by distance $d$. The trigger timestamp delta is:
$$
\Delta t = t_{\text{PIR\_B}} - t_{\text{PIR\_A}}
$$
$$
\text{Direction} = 
\begin{cases} 
\text{FORWARD (S1} \to \text{S2} \to \text{S3)} & \text{if } 50\text{ ms} \le \Delta t \le 2500\text{ ms} \\ 
\text{REVERSE (S3} \to \text{S2} \to \text{S1)} & \text{if } -2500\text{ ms} \le \Delta t \le -50\text{ ms} \\ 
\text{INDETERMINATE} & \text{otherwise} 
\end{cases}
$$
$$
v_{\text{est}} = \frac{d}{|\Delta t|}
$$

### 4.6 Electrical Energy Savings Mathematical Model
In conventional municipal installations, streetlights operate at 100% power throughout the 12-hour night period:
$$
E_{\text{conventional}} = N \times P_{\text{lamp}} \times T_{\text{night}}
$$
Under the predictive adaptive lighting policy:
$$
E_{\text{adaptive}} = N \times P_{\text{lamp}} \times \left[ T_{\text{idle}} \times D_{\text{idle}} + T_{\text{active}} \times D_{\text{active}} \right]
$$
Assuming a 12-hour night ($T_{\text{night}} = 12\text{ h}$) with 2 hours of total active vehicle transit ($T_{\text{active}} = 2\text{ h}$, $D_{\text{active}} = 1.0$) and 10 hours of idle corridor ($T_{\text{idle}} = 10\text{ h}$, $D_{\text{idle}} = 0.20$):
$$
E_{\text{adaptive}} = N \times P_{\text{lamp}} \times [10 \times 0.20 + 2 \times 1.00] = N \times P_{\text{lamp}} \times 4.0\text{ h}
$$
Total energy saved:
$$
\text{Savings} = \left( 1 - \frac{E_{\text{adaptive}}}{E_{\text{conventional}}} \right) \times 100\% = \left( 1 - \frac{4.0}{12.0} \right) \times 100\% \approx 66.7\%
$$
In adverse weather with 50% idle, energy savings still exceed 50%.

---

## 5. Decision Engine and Lighting Policy

The central decision engine combines environmental context with motion demand:

```text
Idle Corridor (Normal Night):
S1: 20% ──── S2: 20% ──── S3: 20%

Vehicle Approaches Moving Forward (PIR-A -> PIR-B):
S1: 100% ─── S2: 100% ─── S3: 50% (Pre-lit)

Vehicle Advances Through Node 2:
S1: 20% ──── S2: 100% ─── S3: 100%

Vehicle Clears Corridor:
S1: 20% ──── S2: 20% ──── S3: 20%
```

### Complete Decision Policy Matrix

| Operating State | Lux Level | Humidity | Motion Trigger | S1 PWM | S2 PWM | S3 PWM | RGB Status | Buzzer |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Daylight** | $> 50\text{ lx}$ | Any | None | 0% | 0% | 0% | Solid Green | OFF |
| **Night Normal** | $< 30\text{ lx}$ | $< 80\%$ | None | 20% | 20% | 20% | Dim Green | OFF |
| **Night Adverse** | $< 30\text{ lx}$ | $\ge 80\%$ | None | 50% | 50% | 50% | Yellow | OFF |
| **Corridor Forward** | $< 30\text{ lx}$ | Any | PIR-A $\to$ PIR-B | 100% | 100% | 50% | Blue | Chirp |
| **Corridor Reverse** | $< 30\text{ lx}$ | Any | PIR-B $\to$ PIR-A | 50% | 100% | 100% | Blue | Chirp |
| **Thermal Hazard** | Any | Any | Any | Hold | Hold | Hold | Red Blinking | Active |
| **Technician Manual** | Any | Any | Buttons / BT | User% | User% | User% | Magenta | OFF |

---

## 6. Sensor Redundancy and Cross-Verification

The architecture incorporates complementary sensor pairs to provide hardware fault tolerance:
1. **Thermal Cross-Check:** LM35 provides direct analog road surface measurement, while SHT31 provides digital ambient microclimate temperature. If the two readings diverge by more than 8 °C, a sensor divergence alert is flagged.
2. **Optical Cross-Check:** LDR provides fast analog boundary detection, while BH1750 provides calibrated human-eye photometric lux. This eliminates false switching from brief car headlight reflections.

---

## 7. Verification and Test Results

The system was evaluated against five rigorous laboratory scenarios:
1. **Day to Night Transition Test:** Lux drop below 30 Lux automatically engages 20% idle baseline across S1, S2, and S3.
2. **Adverse Weather Trigger Test:** Introducing localized humidity above 80% dynamically elevated idle power to 50% to improve safety.
3. **Forward Moving Corridor Verification:** Triggering PIR-A followed by PIR-B launched the forward predictive wave, pre-lighting S3 before decaying back to idle.
4. **Thermal Over-Temperature Protection:** Applying heat to LM35 above 40 °C successfully activated the Red RGB beacon and active buzzer alarm.
5. **Technician Command Latency:** Single-byte Bluetooth and RS-485 overrides executed within 1 millisecond via NVIC interrupt.
