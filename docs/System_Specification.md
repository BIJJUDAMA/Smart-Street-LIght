# Technical Specification & Register Architecture: Predictive Adaptive Streetlight Network

**Document Reference:** SPEC-STM32F4-STREETLIGHT-V4.0  
**Target MCU:** STM32F401CCUx / STM32F401CCU6 Black Pill, ARM Cortex-M4 @ 16 MHz HSI  
**Architecture:** Pure CMSIS Bare-Metal Register Programming stm32f4xx.h  

---

## 1. Memory-Mapped Peripheral Registers

### 1.1 RCC Reset and Clock Control
- **Base Address:** `0x4002 3800`
- **`RCC->AHB1ENR` (Offset `0x30`):**
  - Bit 0 (`GPIOAEN = 1`): Enables GPIO Port A Clock.
  - Bit 1 (`GPIOBEN = 1`): Enables GPIO Port B Clock.
  - Bit 2 (`GPIOCEN = 1`): Enables GPIO Port C Clock.
- **`RCC->APB1ENR` (Offset `0x40`):**
  - Bit 0 (`TIM2EN = 1`): Enables Timer 2 Clock (16 MHz).
  - Bit 1 (`TIM3EN = 1`): Enables Timer 3 Clock (16 MHz).
  - Bit 21 (`I2C1EN = 1`): Enables I2C1 Clock (16 MHz).
- **`RCC->APB2ENR` (Offset `0x44`):**
  - Bit 4 (`USART1EN = 1`): Enables USART1 Clock (16 MHz).
  - Bit 8 (`ADC1EN = 1`): Enables ADC1 Clock (16 MHz).
  - Bit 14 (`SYSCFGEN = 1`): Enables System Configuration Clock for EXTI.

---

### 1.2 GPIO Port Configurations

#### GPIO Port A (`GPIOA`, Base: `0x4002 0000`)
- **`GPIOA->MODER` (Offset `0x00`):**
  - `PA0` (Bits [1:0] = `00`): Digital Input Mode (`KEY` Button, Auto/Manual Toggle).
  - `PA1` (Bits [3:2] = `11`): Analog Mode (ADC1 Channel 1, LM35 Temp Sensor).
  - `PA2` (Bits [5:4] = `10`): Alternate Function Mode (AF1 -> `TIM2_CH3`, Streetlight 1 PWM).
  - `PA4` (Bits [9:8] = `11`): Analog Mode (ADC1 Channel 4, LDR Lamp-2 Feedback).
  - `PA5` (Bits [11:10] = `00`): Digital Input Mode (`BTN_UP` Technician Step Up).
  - `PA6` (Bits [13:12] = `10`): Alternate Function Mode (AF2 -> `TIM3_CH1`, Streetlight 2 PWM).
  - `PA7` (Bits [15:14] = `10`): Alternate Function Mode (AF2 -> `TIM3_CH2`, Streetlight 3 PWM).
  - `PA8` (Bits [17:16] = `00`): Digital Input Mode (`BTN_DOWN` Technician Step Down).
  - `PA9` (Bits [19:18] = `10`): Alternate Function Mode (AF7 -> `USART1_TX`, Bluetooth).
  - `PA10` (Bits [21:20] = `10`): Alternate Function Mode (AF7 -> `USART1_RX`, Bluetooth).
- **`GPIOA->PUPDR` (Offset `0x0C`):**
  - `PA0` (Bits [1:0] = `01`): Pull-up enabled for active-low onboard button.
  - `PA5` (Bits [11:10] = `01`): Pull-up enabled for active-low step up button.
  - `PA8` (Bits [17:16] = `01`): Pull-up enabled for active-low step down button.
- **`GPIOA->AFR[0]` / `AFR[1]` (Offsets `0x20` / `0x24`):**
  - Pin 2: `AFR0[11:8] = 0001` (AF1 -> TIM2).
  - Pin 6: `AFR0[27:24] = 0010` (AF2 -> TIM3).
  - Pin 7: `AFR0[31:28] = 0010` (AF2 -> TIM3).
  - Pin 9: `AFR1[7:4] = 0111` (AF7 -> USART1).
  - Pin 10: `AFR1[11:8] = 0111` (AF7 -> USART1).

#### GPIO Port B (`GPIOB`, Base: `0x4002 0400`)
- **`GPIOB->MODER` (Offset `0x00`):**
  - `PB0` (Bits [1:0] = `00`): Input Mode (PIR-A Motion Sensor).
  - `PB1` (Bits [3:2] = `00`): Input Mode (PIR-B Motion Sensor).
  - `PB8` (Bits [17:16] = `10`): Alternate Function Mode (AF4 -> `I2C1_SCL`).
  - `PB9` (Bits [19:18] = `10`): Alternate Function Mode (AF4 -> `I2C1_SDA`).
  - `PB12` (Bits [25:24] = `01`): General Purpose Output Mode (Active Buzzer).
  - `PB13` (Bits [27:26] = `01`): General Purpose Output Mode (RGB Status Red).
  - `PB14` (Bits [29:28] = `01`): General Purpose Output Mode (RGB Status Green).
  - `PB15` (Bits [31:30] = `01`): General Purpose Output Mode (RGB Status Blue).
- **`GPIOB->OTYPER` (Offset `0x04`):**
  - Bit 8 = `1`, Bit 9 = `1`: Open-drain output configured for I2C lines.
- **`GPIOB->PUPDR` (Offset `0x0C`):**
  - `PB0` (Bits [1:0] = `10`), `PB1` (Bits [3:2] = `10`): Pull-down active.
  - `PB8` (Bits [17:16] = `01`), `PB9` (Bits [19:18] = `01`): Internal pull-ups active.
- **`GPIOB->AFR[1]` (Offset `0x24`):**
  - Pin 8: `AFR1[3:0] = 0100` (AF4 -> I2C1).
  - Pin 9: `AFR1[7:4] = 0100` (AF4 -> I2C1).

---

### 1.3 Timers & 1.0 kHz Hardware PWM Configuration

Operating on a 16 MHz internal HSI core clock:
$$
f_{\text{PWM}} = \frac{f_{\text{CLK}}}{(\text{PSC} + 1) \times (\text{ARR} + 1)} = \frac{16{,}000{,}000}{(15 + 1) \times (999 + 1)} = 1000\text{ Hz}
$$

#### TIM2 (Streetlight 1 on PA2)
- **Base Address:** `0x4000 0000` (APB1 Bus)
- **`TIM2->PSC`:** `15` (1 MHz counter clock)
- **`TIM2->ARR`:** `999` (1000 discrete duty steps, 1.0 kHz carrier)
- **`TIM2->CCR3`:** Duty cycle register for Streetlight S1 (0 to 1000 on `PA2`)
- **`TIM2->CCMR2`:** `OC3M = 110` (PWM Mode 1) + `OC3PE = 1` (Preload Enable)
- **`TIM2->CCER`:** `CC3E = 1` (Enable Channel 3 output on `PA2`)
- **`TIM2->CR1`:** `ARPE = 1` + `CEN = 1`

#### TIM3 (Streetlights 2 and 3 on PA6, PA7)
- **Base Address:** `0x4000 0400` (APB1 Bus)
- **`TIM3->PSC`:** `15`
- **`TIM3->ARR`:** `999`
- **`TIM3->CCR1`:** Duty cycle register for Streetlight S2 (0 to 1000 on `PA6`)
- **`TIM3->CCR2`:** Duty cycle register for Streetlight S3 (0 to 1000 on `PA7`)
- **`TIM3->CCMR1`:** `OC1M = 110`, `OC1PE = 1`, `OC2M = 110`, `OC2PE = 1`
- **`TIM3->CCER`:** `CC1E = 1` + `CC2E = 1`
- **`TIM3->CR1`:** `ARPE = 1` + `CEN = 1`

---

### 1.4 External Interrupts (EXTI0 and EXTI1 for Dual PIRs)

- **`SYSCFG->EXTICR[0]`:** Maps `PB0` to `EXTI0` and `PB1` to `EXTI1` (Bits [3:0] = `0001`, Bits [7:4] = `0001`).
- **`EXTI->IMR`:** Bits 0 and 1 set to unmask interrupt lines.
- **`EXTI->RTSR`:** Bits 0 and 1 set for rising edge trigger detection.
- **NVIC Priority:** `EXTI0_IRQn` and `EXTI1_IRQn` configured to priority 1.

---

### 1.5 I2C1 Master Bus Interface
- **Base Address:** `0x4000 5400` (APB1 Bus)
- **`I2C1->CR2`:** Peripheral clock frequency = `16` (16 MHz APB1 clock)
- **`I2C1->CCR`:** Standard Mode (100 kHz clock frequency):
$$
\text{CCR} = \frac{f_{\text{PCLK1}}}{2 \times f_{\text{SCL}}} = \frac{16{,}000{,}000}{2 \times 100{,}000} = 80 \quad (\text{0x0050})
$$
- **`I2C1->TRISE`:** Maximum rise time register:
$$
\text{TRISE} = \frac{1000\text{ ns}}{1 / 16\text{ MHz}} + 1 = 16 + 1 = 17 \quad (\text{0x0011})
$$

#### Connected I2C Devices
| Device | 7-bit Address | 8-bit Write | 8-bit Read | Description |
| :--- | :---: | :---: | :---: | :--- |
| **BH1750** | `0x23` | `0x46` | `0x47` | Ambient Lux Sensor |
| **SHT31** | `0x44` | `0x88` | `0x89` | Air Temperature and Humidity Sensor |
| **PCF8574** | `0x27` | `0x4E` | `0x4F` | 16x2 Character LCD Backpack |

---

## 2. Mathematical Models & Transfer Functions

### 2.1 LM35 Road Surface Temperature (with 16x Averaging)
$$
V_{\text{out}} = \frac{\text{ADC}_{\text{avg}}}{4095} \times 3.3\text{ V}
$$
$$
T_{\text{surface}} = \frac{V_{\text{out}}}{0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC}_{\text{avg}} \times 330}{4095}
$$

### 2.2 BH1750 Digital Lux Photometric Conversion
$$
\text{Illuminance (Lux)} = \frac{\text{MSB} \times 256 + \text{LSB}}{1.2}
$$

### 2.3 SHT31 Ambient Air Psychrometric Calculations
$$
T_{\text{air}} = -45 + 175 \times \frac{S_T}{65535}
$$
$$
RH = 100 \times \frac{S_{RH}}{65535}
$$

### 2.4 Psychrometric Dew Point (Magnus Formulation)
$$
\gamma(T_{\text{air}}, RH) = \frac{17.62 \times T_{\text{air}}}{243.12 + T_{\text{air}}} + \ln\left(\frac{RH}{100}\right)
$$
$$
T_{\text{dew}} = \frac{243.12 \times \gamma}{17.62 - \gamma}
$$

### 2.5 Perceptual Gamma 2.2 Correction
Human eye perception of brightness is logarithmic. A linear PWM duty cycle produces an apparent jump from 0% to 20% that appears as 50% brightness. To achieve true perceptual gradation:
$$
\text{Duty}_{\text{PWM}} = 1000 \times \left( \frac{\text{Level}_{\text{perceptual}}}{1000} \right)^{2.2}
$$

---

## 3. Communication Protocol & Telemetry

### 3.1 Bluetooth ASCII Telemetry Frame (1 Hz over USART1)
```text
[N1] T_S:24C T_A:24.5C H:65% LUX:18lx DIR:FWD S1:100% S2:100% S3:50% STAT:ADAPT
```

- **`T_S` / `T_A`:** Surface temp (LM35) and Air temp (SHT31) in °C.
- **`H`:** Relative humidity in %RH.
- **`LUX`:** Ambient illuminance in Lux.
- **`DIR`:** Detected presence vector (`IDLE`, `FWD`, `REV`, `AMBIG`).
- **`S1 / S2 / S3`:** Individual luminaire power percentages.
- **`STAT`:** Policy context (`DAY`, `NORMAL`, `WET_RISK`, `FROST_RISK`, `MANUAL`).

---

## 4. Cooperative Executive Task Timing Budget

| Task Name | Interval | Priority | Execution Time | Functionality |
| :--- | :--- | :---: | :--- | :--- |
| **`SysTick_Handler`** | 1 ms | 0 (Highest) | 0.2 us | Core millisecond timebase |
| **`EXTI0/1_Handler`** | Asynchronous | 1 | 0.8 us | PIR edge timestamp queue |
| **`USART1_Handler`** | Asynchronous | 3 | 1.0 us | RX character queue |
| **`Task_PIR_FSM`** | 10 ms | Super-loop | 2.5 us | Motion sequence validation |
| **`Task_PWM_Fader`** | 25 ms | Super-loop | 4.0 us | Asymmetric slew & gamma mapping |
| **`Task_Buttons`** | 100 ms | Super-loop | 1.5 us | Debouncing for PA0, PA5, PA8 |
| **`Task_Sensors`** | 500 ms | Super-loop | 800 us | ADC avg, BH1750, SHT31, Context |
| **`Task_LCD_Display`**| 1500 ms | Super-loop | 2.2 ms | Non-blocking dual-page render |
