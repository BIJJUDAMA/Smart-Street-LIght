# Technical Specification & Register Architecture: Smart Streetlight Unit

**Document Reference:** SPEC-STM32F4-STREETLIGHT-V2.0  
**Target MCU:** STM32F401CCUx / STM32F401CCU6 (Black Pill, ARM Cortex-M4 @ 16 MHz)  
**Architecture:** Pure CMSIS Bare-Metal Register Programming (`stm32f4xx.h`)  

---

## 1. Memory-Mapped Peripheral Registers

### 1.1 RCC (Reset and Clock Control)
- **Base Address:** `0x4002 3800`
- **`RCC->AHB1ENR` (Offset `0x30`):**
  - Bit 0 (`GPIOAEN = 1`): Enables GPIO Port A Clock.
  - Bit 1 (`GPIOBEN = 1`): Enables GPIO Port B Clock.
- **`RCC->APB1ENR` (Offset `0x40`):**
  - Bit 0 (`TIM2EN = 1`): Enables Timer 2 Clock ($16\text{ MHz}$).
- **`RCC->APB2ENR` (Offset `0x44`):**
  - Bit 4 (`USART1EN = 1`): Enables USART1 Clock ($16\text{ MHz}$).
  - Bit 8 (`ADC1EN = 1`): Enables ADC1 Clock ($16\text{ MHz}$).

---

### 1.2 GPIO Port Configurations (GPIOA & GPIOB)
- **`GPIOA->MODER` (Offset `0x00`):**
  - `PA0` (Bits [1:0] = `10`): Alternate Function Mode (AF1 -> `TIM2_CH1`).
  - `PA1` (Bits [3:2] = `11`): Analog Mode (LM35 Temp Sensor).
  - `PA2` (Bits [5:4] = `11`): Analog Mode (LDR Light Sensor).
  - `PA3` (Bits [7:6] = `01`): General Purpose Output Mode (Red Hazard LED).
  - `PA9` (Bits [19:18] = `10`): Alternate Function Mode (AF7 -> `USART1_TX`).
  - `PA10` (Bits [21:20] = `10`): Alternate Function Mode (AF7 -> `USART1_RX`).
- **`GPIOA->AFR[0]` / `AFR[1]` (Offsets `0x20` / `0x24`):**
  - Pin 0: `AFR0[3:0] = 0001` (AF1 -> TIM2).
  - Pin 9: `AFR1[7:4] = 0111` (AF7 -> USART1).
  - Pin 10: `AFR1[11:8] = 0111` (AF7 -> USART1).
- **`GPIOB->MODER` & `GPIOB->PUPDR` (Offset `0x00` & `0x0C`):**
  - `PB0` (Bits [1:0] = `00` input, PUPDR = `01` pull-up): `BTN_UP`.
  - `PB1` (Bits [3:2] = `00` input, PUPDR = `01` pull-up): `BTN_DOWN`.

---

### 1.3 TIM2 (General Purpose 32-bit Timer - PWM Output)
- **Base Address:** `0x4000 0000` (APB1 Bus)
- **`TIM2->PSC`:** `83` ($16\text{ MHz} / 84 = 190.48\text{ kHz}$)
- **`TIM2->ARR`:** `999` ($190.48\text{ kHz} / 1000 = 190.48\text{ Hz}$)
- **`TIM2->CCR1`:** Variable $0 \dots 1000$ (0% to 100% duty cycle)
- **`TIM2->CCMR1`:** `OC1M = 110` (PWM Mode 1) + `OC1PE = 1` (Preload Enable)
- **`TIM2->CCER`:** `CC1E = 1` (Enable Channel 1 output on `PA0`)
- **`TIM2->CR1`:** `ARPE = 1` (Auto-reload preload) + `CEN = 1` (Counter enable)

---

### 1.4 ADC1 (Analog-to-Digital Converter)
- **Base Address:** `0x4001 2000` (APB2 Bus)
- **`ADC->CCR`:** Prescaler = $f_{\text{PCLK2}} / 2 = 8\text{ MHz}$
- **`ADC1->SMPR2`:** Bits [5:3] = `100` (84 cycles for CH1), Bits [8:6] = `100` (84 cycles for CH2)
- **`ADC1->CR2`:** Bit 0 (`ADON = 1`) to power on ADC, Bit 30 (`SWSTART = 1`) to trigger conversion
- **`ADC1->SQR3`:** Written with Channel 1 (LM35) or Channel 2 (LDR) prior to `SWSTART`
- **`ADC1->SR`:** Bit 1 (`EOC`) polled for conversion completion
- **`ADC1->DR`:** Reads 12-bit digital conversion result ($0 \dots 4095$)

---

### 1.5 USART1 (Bluetooth Wireless Interface)
- **Base Address:** `0x4001 1000` (APB2 Bus)
- **`USART1->BRR`:** `0x0683` (9600 Baud at 16 MHz clock: Mantissa = 104, Fraction = 3)
- **`USART1->CR1`:**
  - Bit 13 (`UE = 1`): USART Enable
  - Bit 3 (`TE = 1`): Transmitter Enable
  - Bit 2 (`RE = 1`): Receiver Enable
  - Bit 5 (`RXNEIE = 1`): RX Not Empty Interrupt Enable
- **NVIC Integration:** `NVIC_SetPriority(USART1_IRQn, 0)`, `NVIC_EnableIRQ(USART1_IRQn)`

---

## 2. Wireless Command Ingestion Protocol

| Command Byte | ASCII Character | Function | Register Action |
| :---: | :---: | :--- | :--- |
| `0x55` | `'U'` | Brightness Step UP | `TIM2->CCR1 += 50`, `auto_light_mode = 0` |
| `0x44` | `'D'` | Brightness Step DOWN | `TIM2->CCR1 -= 50`, `auto_light_mode = 0` |
| `0x41` | `'A'` | Enable Auto Photocell Mode | `auto_light_mode = 1` |
| `0x4D` | `'M'` | Lock to Manual Mode | `auto_light_mode = 0` |
| `0x31` | `'1'` | Force Full 100% Illumination | `TIM2->CCR1 = 999`, `pwm_value = 1000` |
| `0x30` | `'0'` | Force 0% Shutdown | `TIM2->CCR1 = 0`, `pwm_value = 0` |

---

## 3. Timing Budget & CPU Utilization Analysis

| Routine / Task | Frequency | Execution Time (est.) | CPU Duty Cycle | Trigger Source |
| :--- | :--- | :--- | :--- | :--- |
| **SysTick Handler** | 1000 Hz (1 ms) | ~0.2 µs | 0.02% | Hardware SysTick Exception |
| **USART1 RX ISR (Bluetooth)** | Asynchronous | ~0.8 µs | <0.01% | NVIC `USART1_IRQn` |
| **ADC Polling & Math** | 2 Hz (500 ms) | ~25 µs | 0.005% | Monotonic Tick Comparison |
| **Button Debounce Poll** | 5 Hz (200 ms) | ~1.5 µs | 0.001% | Monotonic Tick Comparison |
| **Idle Super-Loop** | Continuous | - | >99.9% | Free-running core loop |
