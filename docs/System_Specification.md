# Technical Specification & Register Architecture: Predictive Adaptive Streetlight Network

**Document Reference:** SPEC-STM32F4-STREETLIGHT-V3.0  
**Target MCU:** STM32F401CCUx / STM32F401CCU6 Black Pill, ARM Cortex-M4 @ 16 MHz  
**Architecture:** Pure CMSIS Bare-Metal Register Programming `stm32f4xx.h`  

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
  - Bit 17 (`USART2EN = 1`): Enables USART2 Clock (16 MHz).
  - Bit 21 (`I2C1EN = 1`): Enables I2C1 Clock (16 MHz).
- **`RCC->APB2ENR` (Offset `0x44`):**
  - Bit 4 (`USART1EN = 1`): Enables USART1 Clock (16 MHz).
  - Bit 8 (`ADC1EN = 1`): Enables ADC1 Clock (16 MHz).

---

### 1.2 GPIO Port Configurations

#### GPIO Port A (`GPIOA`, Base: `0x4002 0000`)
- **`GPIOA->MODER` (Offset `0x00`):**
  - `PA0` (Bits [1:0] = `10`): Alternate Function Mode (AF1 -> `TIM2_CH1`, Streetlight 1 PWM).
  - `PA1` (Bits [3:2] = `11`): Analog Mode (ADC1 Channel 1, LM35 Temp Sensor).
  - `PA2` (Bits [5:4] = `10`): Alternate Function Mode (AF7 -> `USART2_TX`, RS-485 Bus).
  - `PA3` (Bits [7:6] = `10`): Alternate Function Mode (AF7 -> `USART2_RX`, RS-485 Bus).
  - `PA4` (Bits [9:8] = `11`): Analog Mode (ADC1 Channel 4, LDR Sensor).
  - `PA5` (Bits [11:10] = `00`): Digital Input Mode (`BTN_UP` Technician Step Up).
  - `PA6` (Bits [13:12] = `10`): Alternate Function Mode (AF2 -> `TIM3_CH1`, Streetlight 2 PWM).
  - `PA7` (Bits [15:14] = `10`): Alternate Function Mode (AF2 -> `TIM3_CH2`, Streetlight 3 PWM).
  - `PA8` (Bits [17:16] = `00`): Digital Input Mode (`BTN_DOWN` Technician Step Down).
  - `PA9` (Bits [19:18] = `10`): Alternate Function Mode (AF7 -> `USART1_TX`, Bluetooth).
  - `PA10` (Bits [21:20] = `10`): Alternate Function Mode (AF7 -> `USART1_RX`, Bluetooth).
- **`GPIOA->PUPDR` (Offset `0x0C`):**
  - `PA5` (Bits [11:10] = `01`): Pull-up enabled for active-low button.
  - `PA8` (Bits [17:16] = `01`): Pull-up enabled for active-low button.
- **`GPIOA->AFR[0]` / `AFR[1]` (Offsets `0x20` / `0x24`):**
  - Pin 0: `AFR0[3:0] = 0001` (AF1 -> TIM2).
  - Pin 2: `AFR0[11:8] = 0111` (AF7 -> USART2).
  - Pin 3: `AFR0[15:12] = 0111` (AF7 -> USART2).
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
  - `PB10` (Bits [21:20] = `01`): General Purpose Output Mode (RS-485 DE/RE control).
  - `PB12` (Bits [25:24] = `01`): General Purpose Output Mode (Active Buzzer).
  - `PB13` (Bits [27:26] = `01`): General Purpose Output Mode (RGB Status Red).
  - `PB14` (Bits [29:28] = `01`): General Purpose Output Mode (RGB Status Green).
  - `PB15` (Bits [31:30] = `01`): General Purpose Output Mode (RGB Status Blue).
- **`GPIOB->OTYPER` (Offset `0x04`):**
  - Bit 8 = `1`, Bit 9 = `1`: Open-drain output configured for I2C lines.
- **`GPIOB->PUPDR` (Offset `0x0C`):**
  - `PB0` (Bits [1:0] = `10`): Pull-down enabled for active-high PIR sensor.
  - `PB1` (Bits [3:2] = `10`): Pull-down enabled for active-high PIR sensor.
  - `PB8` (Bits [17:16] = `01`), `PB9` (Bits [19:18] = `01`): Internal pull-ups enabled for I2C bus.
- **`GPIOB->AFR[1]` (Offset `0x24`):**
  - Pin 8: `AFR1[3:0] = 0100` (AF4 -> I2C1).
  - Pin 9: `AFR1[7:4] = 0100` (AF4 -> I2C1).

---

### 1.3 Timers and Multi-Channel PWM Control

#### TIM2 General Purpose 32-bit Timer: Streetlight 1
- **Base Address:** `0x4000 0000` (APB1 Bus)
- **`TIM2->PSC`:** `83` (Clock division to 190.48 kHz)
- **`TIM2->ARR`:** `999` (1000 discrete dimming steps, PWM frequency = 190.48 Hz)
- **`TIM2->CCR1`:** Duty cycle register for Streetlight S1 (0 to 1000)
- **`TIM2->CCMR1`:** `OC1M = 110` (PWM Mode 1) + `OC1PE = 1` (Preload Enable)
- **`TIM2->CCER`:** `CC1E = 1` (Enable Channel 1 output on `PA0`)
- **`TIM2->CR1`:** `ARPE = 1` + `CEN = 1`

#### TIM3 General Purpose 16-bit Timer: Streetlights 2 and 3
- **Base Address:** `0x4000 0400` (APB1 Bus)
- **`TIM3->PSC`:** `83`
- **`TIM3->ARR`:** `999` (PWM frequency = 190.48 Hz)
- **`TIM3->CCR1`:** Duty cycle register for Streetlight S2 (0 to 1000 on `PA6`)
- **`TIM3->CCR2`:** Duty cycle register for Streetlight S3 (0 to 1000 on `PA7`)
- **`TIM3->CCMR1`:** `OC1M = 110`, `OC1PE = 1`, `OC2M = 110`, `OC2PE = 1`
- **`TIM3->CCER`:** `CC1E = 1` + `CC2E = 1`
- **`TIM3->CR1`:** `ARPE = 1` + `CEN = 1`

---

### 1.4 ADC1 Analog-to-Digital Converter
- **Base Address:** `0x4001 2000` (APB2 Bus)
- **`ADC->CCR`:** Prescaler = PCLK2 / 2 = 8 MHz
- **`ADC1->SMPR2`:** Bits [5:3] = `100` (84 cycles for CH1), Bits [14:12] = `100` (84 cycles for CH4)
- **`ADC1->CR2`:** Bit 0 (`ADON = 1`) power enable, Bit 30 (`SWSTART = 1`) conversion trigger
- **`ADC1->SQR3`:** Channel selection register (Channel 1 for LM35, Channel 4 for LDR)
- **`ADC1->SR`:** Bit 1 (`EOC`) polled for conversion completion flag
- **`ADC1->DR`:** 12-bit conversion output register (0 to 4095)

---

### 1.5 I2C1 Master Bus Interface
- **Base Address:** `0x4000 5400` (APB1 Bus)
- **`I2C1->CR1`:** Software reset, peripheral enable (`PE = 1`), start generation (`START = 1`), stop generation (`STOP = 1`), acknowledge enable (`ACK = 1`)
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
| **BH1750** | `0x23` | `0x46` | `0x47` | Ambient Light Lux Sensor |
| **SHT31** | `0x44` | `0x88` | `0x89` | Temperature and Humidity Sensor |
| **PCF8574** | `0x27` | `0x4E` | `0x4F` | 16x2 Character LCD Backpack |

---

### 1.6 USART1 Bluetooth and USART2 RS-485 Interfaces
- **USART1 Base Address:** `0x4001 1000` (APB2 Bus)
  - Baud Rate: 9600 Baud @ 16 MHz clock, `BRR = 0x0683`
  - Control: `TE = 1`, `RE = 1`, `RXNEIE = 1`, `UE = 1`
  - NVIC Vector: `USART1_IRQn` priority 0
- **USART2 Base Address:** `0x4000 4400` (APB1 Bus)
  - Baud Rate: 9600 Baud @ 16 MHz clock, `BRR = 0x0683`
  - Control: `TE = 1`, `RE = 1`, `UE = 1`
  - Direction Control: `PB10` driven HIGH for TX mode, LOW for RX mode

---

## 2. Sensor Mathematical Transfer Functions

### 2.1 LM35 Precision Analog Temperature Sensor
The LM35 outputs a calibrated analog voltage of 10 mV/°C:
$$
V_{\text{out}} = \frac{\text{ADC}_{\text{raw}}}{4095} \times 3.3\text{ V}
$$
$$
T_{\text{LM35}} = \frac{V_{\text{out}}}{0.010\text{ V}/^\circ\text{C}} = \frac{\text{ADC}_{\text{raw}} \times 330}{4095}
$$

### 2.2 BH1750 Digital Lux Sensor
The BH1750 measures calibrated human-eye photometric illuminance in Lux:
$$
\text{Illuminance (Lux)} = \frac{\text{MSB} \times 256 + \text{LSB}}{1.2}
$$

### 2.3 SHT31 Digital Temperature and Humidity Sensor
The SHT31 returns 16-bit raw words for temperature and relative humidity:
$$
T_{\text{SHT31}} = -45 + 175 \times \frac{S_T}{65535}
$$
$$
RH = 100 \times \frac{S_{RH}}{65535}
$$

### 2.4 Motion Direction and Velocity Estimation
Two PIR sensors are placed along the road corridor separated by distance $d$:
$$
\Delta t = t_{\text{PIR\_B}} - t_{\text{PIR\_A}}
$$
$$
\text{Direction} = 
\begin{cases} 
\text{FORWARD} & \text{if } \Delta t > 0 \text{ and } \Delta t \le T_{\text{window}} \\ 
\text{REVERSE} & \text{if } \Delta t < 0 \text{ and } |\Delta t| \le T_{\text{window}} 
\end{cases}
$$
$$
v = \frac{d}{|\Delta t|}
$$

---

## 3. Communication Protocol Frame Structures

### 3.1 Bluetooth ASCII Telemetry Frame
Broadcast every 500 ms over USART1:
```text
[NODE1] T_LM:32C T_SHT:32.4C H:68% LUX:45lx DIR:FWD S1:100% S2:100% S3:50% STAT:ADAPT
```

### 3.2 RS-485 Inter-Node Network Packet
Fixed 6-byte binary frame broadcast across the RS-485 bus:
```text
Byte 0: 0xAA (Sync Header)
Byte 1: Source Node ID (0x01 = Node 1)
Byte 2: Target Node ID (0x02 = Node 2, 0xFF = Broadcast)
Byte 3: Command Code (0x10 = Approach, 0x11 = Corridor Pre-light, 0x20 = Hazard)
Byte 4: Estimated Speed (cm/s, 0-255)
Byte 5: Longitudinal Checksum (XOR of Bytes 0-4)
```

---

## 4. Cooperative Executive Task Timing Budget

| Task Name | Periodicity | Priority | Typical Execution | Trigger Source |
| :--- | :--- | :---: | :--- | :--- |
| **SysTick Handler** | 1 ms | High | 0.2 us | Core Exception |
| **PIR Edge Evaluator** | 10 ms | High | 1.2 us | Super-Loop |
| **PWM Soft-Ramp Fader** | 30 ms | Medium | 2.5 us | Super-Loop |
| **Button Debounce Poll** | 100 ms | Low | 1.8 us | Super-Loop |
| **ADC and I2C Sensing** | 500 ms | Medium | 850 us | Super-Loop |
| **Decision Policy Engine** | 500 ms | Medium | 8.0 us | Super-Loop |
| **LCD and Telemetry Push** | 1000 ms | Low | 12.5 ms | Super-Loop |
