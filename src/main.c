/**
  ******************************************************************************
  * @file           : main.c
  * @project        : Predictive Adaptive Smart Streetlight & Environmental Network
  * @target         : STM32F401CCUx (Black Pill, ARM Cortex-M4 @ 16 MHz)
  * @architecture   : Pure CMSIS Bare-Metal Register Programming (stm32f4xx.h)
  *
  * Hardware Pinout Summary:
  *   - PA0  : TIM2_CH1  (PWM Output for Streetlight 1 / S1, 0-100%)
  *   - PA1  : ADC1_IN1  (LM35 Analog Road Temperature Sensor, 10mV/°C)
  *   - PA2  : USART2_TX (RS-485 Inter-Node Network Bus Transmitter)
  *   - PA3  : USART2_RX (RS-485 Inter-Node Network Bus Receiver)
  *   - PA4  : ADC1_IN4  (LDR Relative Ambient Light Sensor)
  *   - PA5  : GPIO Input Pull-Up (Manual Technician Button UP)
  *   - PA6  : TIM3_CH1  (PWM Output for Streetlight 2 / S2, 0-100%)
  *   - PA7  : TIM3_CH2  (PWM Output for Streetlight 3 / S3, 0-100%)
  *   - PA8  : GPIO Input Pull-Up (Manual Technician Button DOWN)
  *   - PA9  : USART1_TX (Bluetooth Wireless Telemetry @ 9600 Baud)
  *   - PA10 : USART1_RX (Bluetooth Command Receiver via NVIC Interrupt)
  *   - PB0  : GPIO Input Pull-Down (PIR-A Motion Sensor, Entry / Approach)
  *   - PB1  : GPIO Input Pull-Down (PIR-B Motion Sensor, Departure / Direction)
  *   - PB8  : I2C1_SCL  (Open-Drain Clock for SHT31, BH1750, 16x2 LCD)
  *   - PB9  : I2C1_SDA  (Open-Drain Data for SHT31, BH1750, 16x2 LCD)
  *   - PB10 : GPIO Output (RS-485 Transceiver DE/RE Direction Control)
  *   - PB12 : GPIO Output (Active Buzzer Audible Alarm)
  *   - PB13 : GPIO Output (RGB Status Beacon - Red Channel)
  *   - PB14 : GPIO Output (RGB Status Beacon - Green Channel)
  *   - PB15 : GPIO Output (RGB Status Beacon - Blue Channel)
  *   - PC13 : GPIO Output (Onboard Diagnostic Heartbeat LED)
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * I2C SLAVE ADDRESSES & PROTOCOL DEFINITIONS
 * ============================================================================= */
#define BH1750_I2C_ADDR         (0x23 << 1)     /* 7-bit 0x23 -> 8-bit Write: 0x46, Read: 0x47 */
#define SHT31_I2C_ADDR          (0x44 << 1)     /* 7-bit 0x44 -> 8-bit Write: 0x88, Read: 0x89 */
#define LCD_I2C_ADDR            (0x27 << 1)     /* PCF8574 backpack (0x27 or 0x3F) */

/* PCF8574 LCD Control Bits */
#define LCD_BACKLIGHT           0x08
#define LCD_ENABLE              0x04
#define LCD_RW                  0x02
#define LCD_RS                  0x01

/* System Thresholds and Calibration */
#define LUX_NIGHT_THRESHOLD     30              /* Lux threshold below which streetlights activate */
#define LUX_DAY_THRESHOLD       50              /* Lux threshold for daytime luminaire shutoff */
#define HUMIDITY_POOR_COND      80              /* %RH threshold for adverse fog/rain condition */
#define TEMP_FROST_ALERT        3               /* °C threshold for roadside frost and black ice hazard */
#define TEMP_FROST_RESET        6               /* °C threshold for frost alarm hysteresis reset */
#define HUMIDITY_FROST_RISK     75              /* %RH threshold for severe black ice risk with low temp */

#define DUTY_OFF                0               /* 0%   = OFF */
#define DUTY_IDLE_NORMAL        200             /* 20%  = Normal clear night idle baseline */
#define DUTY_IDLE_ADVERSE       500             /* 50%  = Adverse weather/fog elevated baseline */
#define DUTY_PRELIT             500             /* 50%  = Predictive pre-lit safety buffer */
#define DUTY_FULL               1000            /* 100% = Full vehicle illumination */

/* System Operating States */
typedef enum {
    STATE_DAY = 0,
    STATE_NIGHT_IDLE,
    STATE_CORRIDOR_FWD,
    STATE_CORRIDOR_REV,
    STATE_MANUAL,
    STATE_FROST
} SystemState_t;

/* Motion Direction */
typedef enum {
    DIR_NONE = 0,
    DIR_FORWARD,
    DIR_REVERSE
} MotionDir_t;

/* =============================================================================
 * GLOBAL VARIABLES
 * ============================================================================= */
volatile uint32_t ms_ticks = 0;                 /* Millisecond counter updated by SysTick ISR */
volatile uint8_t bluetooth_rx_byte = 0;         /* Last received Bluetooth character */

/* Streetlight PWM Brightness (Current and Target for smooth fading) */
volatile int pwm_cur_s1 = 0;
volatile int pwm_cur_s2 = 0;
volatile int pwm_cur_s3 = 0;
volatile int pwm_tgt_s1 = 0;
volatile int pwm_tgt_s2 = 0;
volatile int pwm_tgt_s3 = 0;

/* Operational Modes */
volatile uint8_t auto_mode = 1;                 /* 1 = Automatic adaptive, 0 = Manual override */
volatile SystemState_t current_state = STATE_DAY;
volatile MotionDir_t current_direction = DIR_NONE;

/* Sensor Values */
volatile uint32_t sensor_lux = 100;             /* Measured lux from BH1750 */
volatile int32_t sensor_temp_lm35 = 25;         /* LM35 analog temperature in °C */
volatile float sensor_temp_sht31 = 25.0f;       /* SHT31 digital temperature in °C */
volatile float sensor_humidity = 50.0f;         /* SHT31 digital relative humidity in %RH */
volatile uint32_t adc_ldr_raw = 1000;           /* Raw ADC reading from LDR photocell */

/* Corridor Motion Timing Variables */
uint32_t pir_a_trigger_time = 0;
uint32_t pir_b_trigger_time = 0;
uint32_t corridor_start_time = 0;
uint32_t buzzer_beep_end = 0;
uint32_t est_velocity_cm_s = 0;

/* Function Prototypes */
void init_port(void);
uint32_t GetTick(void);
void DelayMs(uint32_t ms);

/* ADC & Communications */
uint32_t ADC_ReadChannel(uint8_t channel);
void USART1_SendString(const char *str);
void USART2_SendByte(uint8_t byte);
void RS485_SendPacket(uint8_t target_node, uint8_t cmd, uint8_t speed_code);

/* I2C Engine & Sensors */
void I2C1_Init(void);
uint8_t I2C1_Start(void);
void I2C1_Stop(void);
uint8_t I2C1_WriteByte(uint8_t data);
uint8_t I2C1_ReadAck(void);
uint8_t I2C1_ReadNack(void);

void BH1750_Init(void);
uint32_t BH1750_ReadLux(void);

void SHT31_Init(void);
uint8_t SHT31_ReadTempHum(float *temp, float *hum);

void LCD_Init(void);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(const char *str);

/* Indicators */
void Set_RGB(uint8_t r, uint8_t g, uint8_t b);
void Trigger_Buzzer(uint32_t duration_ms);

/* =============================================================================
 * HARDWARE INITIALIZATION ROUTINE (init_port)
 * ============================================================================= */
void init_port(void) {
    /* -------------------------------------------------------------------------
     * 1. Enable Peripheral Clocks
     * ------------------------------------------------------------------------- */
    RCC->AHB1ENR |= (1U << 0);                  /* GPIOA: PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8, PA9, PA10 */
    RCC->AHB1ENR |= (1U << 1);                  /* GPIOB: PB0, PB1, PB8, PB9, PB10, PB12, PB13, PB14, PB15 */
    RCC->AHB1ENR |= (1U << 2);                  /* GPIOC: PC13 Onboard LED */
    RCC->APB1ENR |= (1U << 0);                  /* TIM2  : Timer 2 (S1 PWM) */
    RCC->APB1ENR |= (1U << 1);                  /* TIM3  : Timer 3 (S2, S3 PWM) */
    RCC->APB1ENR |= (1U << 17);                 /* USART2: RS-485 Inter-Node Bus */
    RCC->APB1ENR |= (1U << 21);                 /* I2C1  : Sensors and 16x2 LCD */
    RCC->APB2ENR |= (1U << 4);                  /* USART1: Bluetooth HC-05 */
    RCC->APB2ENR |= (1U << 8);                  /* ADC1  : LM35 & LDR Sensing */

    /* -------------------------------------------------------------------------
     * 2. Configure GPIO Modes for Port A
     * ------------------------------------------------------------------------- */
    /* PA0: TIM2_CH1 PWM (AF1) -> Streetlight 1 */
    GPIOA->MODER &= ~(3U << 0);
    GPIOA->MODER |=  (2U << 0);                 /* AF Mode */
    GPIOA->AFR[0] |= (1U << 0);                 /* AFR0 = AF1 */

    /* PA1: ADC1_IN1 -> LM35 Temperature Sensor */
    GPIOA->MODER |=  (3U << 2);                 /* Analog Mode */

    /* PA2 (TX) & PA3 (RX): USART2 (AF7) -> RS-485 Bus */
    GPIOA->MODER &= ~((3U << 4) | (3U << 6));
    GPIOA->MODER |=  ((2U << 4) | (2U << 6));   /* AF Mode */
    GPIOA->AFR[0] |= ((7U << 8) | (7U << 12));  /* AFR2=AF7, AFR3=AF7 */

    /* PA4: ADC1_IN4 -> LDR Photocell */
    GPIOA->MODER |=  (3U << 8);                 /* Analog Mode */

    /* PA5 & PA8: Tactile Pushbuttons (BTN_UP, BTN_DOWN) */
    GPIOA->MODER &= ~((3U << 10) | (3U << 16)); /* Input Mode */
    GPIOA->PUPDR &= ~((3U << 10) | (3U << 16));
    GPIOA->PUPDR |=  ((1U << 10) | (1U << 16)); /* Internal Pull-Up */

    /* PA6: TIM3_CH1 PWM (AF2) -> Streetlight 2 */
    GPIOA->MODER &= ~(3U << 12);
    GPIOA->MODER |=  (2U << 12);                /* AF Mode */
    GPIOA->AFR[0] |= (2U << 24);                /* AFR6 = AF2 */

    /* PA7: TIM3_CH2 PWM (AF2) -> Streetlight 3 */
    GPIOA->MODER &= ~(3U << 14);
    GPIOA->MODER |=  (2U << 14);                /* AF Mode */
    GPIOA->AFR[0] |= (2U << 28);                /* AFR7 = AF2 */

    /* PA9 (TX) & PA10 (RX): USART1 (AF7) -> Bluetooth HC-05 */
    GPIOA->MODER &= ~((3U << 18) | (3U << 20));
    GPIOA->MODER |=  ((2U << 18) | (2U << 20)); /* AF Mode */
    GPIOA->AFR[1] |= ((7U << 4) | (7U << 8));   /* AFR9=AF7, AFR10=AF7 */

    /* -------------------------------------------------------------------------
     * 3. Configure GPIO Modes for Port B & Port C
     * ------------------------------------------------------------------------- */
    /* PB0 & PB1: Motion Sensors PIR-A & PIR-B (Input with Pull-Down) */
    GPIOB->MODER &= ~((3U << 0) | (3U << 2));   /* Input Mode */
    GPIOB->PUPDR &= ~((3U << 0) | (3U << 2));
    GPIOB->PUPDR |=  ((2U << 0) | (2U << 2));   /* Pull-Down */

    /* PB8 (SCL) & PB9 (SDA): I2C1 (AF4, Open-Drain) */
    GPIOB->MODER &= ~((3U << 16) | (3U << 18));
    GPIOB->MODER |=  ((2U << 16) | (2U << 18)); /* AF Mode */
    GPIOB->OTYPER |= (1U << 8) | (1U << 9);     /* Open-Drain */
    GPIOB->PUPDR  |= (1U << 16) | (1U << 18);   /* Pull-Up */
    GPIOB->AFR[1] |= ((4U << 0) | (4U << 4));   /* AFR8=AF4, AFR9=AF4 */

    /* PB10: RS-485 DE/RE Output */
    GPIOB->MODER &= ~(3U << 20);
    GPIOB->MODER |=  (1U << 20);                /* Output Mode */
    GPIOB->ODR   &= ~(1U << 10);                /* Default RX Mode (DE/RE = LOW) */

    /* PB12: Active Buzzer Output */
    GPIOB->MODER &= ~(3U << 24);
    GPIOB->MODER |=  (1U << 24);                /* Output Mode */
    GPIOB->ODR   &= ~(1U << 12);                /* Buzzer OFF */

    /* PB13 (Red), PB14 (Green), PB15 (Blue): RGB Status LED */
    GPIOB->MODER &= ~((3U << 26) | (3U << 28) | (3U << 30));
    GPIOB->MODER |=  ((1U << 26) | (1U << 28) | (1U << 30)); /* Output Mode */
    GPIOB->ODR   &= ~((1U << 13) | (1U << 14) | (1U << 15)); /* LEDs OFF */

    /* PC13: Onboard LED (Active LOW) */
    GPIOC->MODER &= ~(3U << 26);
    GPIOC->MODER |=  (1U << 26);
    GPIOC->ODR   |=  (1U << 13);                /* LED OFF */

    /* -------------------------------------------------------------------------
     * 4. Configure TIM2 for Streetlight 1 PWM (PA0, CH1)
     * Frequency: 16 MHz / ((83+1) * (999+1)) = 190.48 Hz
     * ------------------------------------------------------------------------- */
    TIM2->PSC   = 83;
    TIM2->ARR   = 999;
    TIM2->CCR1  = 0;
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM2->CCMR1 |= (6U << 4) | (1U << 3);       /* PWM Mode 1 + Preload */
    TIM2->CCER  |= (1U << 0);                   /* Enable CH1 output */
    TIM2->CR1   |= (1U << 7) | (1U << 0);       /* ARPE + Counter Enable */

    /* -------------------------------------------------------------------------
     * 5. Configure TIM3 for Streetlight 2 (PA6, CH1) & Streetlight 3 (PA7, CH2)
     * ------------------------------------------------------------------------- */
    TIM3->PSC   = 83;
    TIM3->ARR   = 999;
    TIM3->CCR1  = 0;
    TIM3->CCR2  = 0;
    TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC2M);
    TIM3->CCMR1 |= (6U << 4)  | (1U << 3);      /* CH1 PWM Mode 1 + Preload */
    TIM3->CCMR1 |= (6U << 12) | (1U << 11);     /* CH2 PWM Mode 1 + Preload */
    TIM3->CCER  |= (1U << 0)  | (1U << 4);      /* Enable CH1 and CH2 outputs */
    TIM3->CR1   |= (1U << 7)  | (1U << 0);      /* ARPE + Counter Enable */

    /* -------------------------------------------------------------------------
     * 6. Configure ADC1 for Multi-Channel Analog Acquisition
     * ------------------------------------------------------------------------- */
    ADC->CCR    = 0;                            /* Clock = PCLK2 / 2 = 8 MHz */
    ADC1->CR1   = 0;
    ADC1->CR2   = 0;
    ADC1->SMPR2 |= (4U << 3) | (4U << 12);      /* 84 cycles for CH1 (PA1) and CH4 (PA4) */
    ADC1->CR2   |= (1U << 0);                   /* ADON = 1 */
    DelayMs(2);                                 /* Stabilization wait */

    /* -------------------------------------------------------------------------
     * 7. Configure USART1 for Bluetooth (9600 Baud, 8-N-1)
     * ------------------------------------------------------------------------- */
    USART1->BRR = (104U << 4) | (3U & 0x0F);    /* 16 MHz / (16 * 9600) = 104.1875 */
    USART1->CR1 = (1U << 3) | (1U << 2) | (1U << 5) | (1U << 13); /* TE, RE, RXNEIE, UE */
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);

    /* -------------------------------------------------------------------------
     * 8. Configure USART2 for RS-485 Bus (9600 Baud, 8-N-1)
     * ------------------------------------------------------------------------- */
    USART2->BRR = (104U << 4) | (3U & 0x0F);
    USART2->CR1 = (1U << 3) | (1U << 2) | (1U << 13); /* TE, RE, UE */

    /* -------------------------------------------------------------------------
     * 9. Configure SysTick for 1 ms Monotonic Tick
     * ------------------------------------------------------------------------- */
    SysTick_Config(16000000 / 1000);

    /* -------------------------------------------------------------------------
     * 10. Configure Bare-Metal I2C1 Master Bus Interface
     * ------------------------------------------------------------------------- */
    I2C1_Init();
}

/* =============================================================================
 * SYSTICK & DELAY FUNCTIONS
 * ============================================================================= */
void SysTick_Handler(void) {
    ms_ticks++;
}

uint32_t GetTick(void) {
    return ms_ticks;
}

void DelayMs(uint32_t ms) {
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms) {}
}

/* =============================================================================
 * ADC DRIVER ROUTINE
 * ============================================================================= */
uint32_t ADC_ReadChannel(uint8_t channel) {
    ADC1->SQR3 = channel & 0x1F;
    ADC1->CR2 |= (1U << 30);                    /* Start conversion (SWSTART) */
    uint32_t timeout = 10000;
    while (!(ADC1->SR & (1U << 1)) && --timeout) {} /* Wait EOC */
    return ADC1->DR;
}

/* =============================================================================
 * SERIAL COMMUNICATION (USART1 & USART2)
 * ============================================================================= */
void USART1_SendString(const char *str) {
    while (*str) {
        while (!(USART1->SR & (1U << 7))) {}    /* Wait TXE */
        USART1->DR = (*str++ & 0xFF);
    }
}

void USART2_SendByte(uint8_t byte) {
    while (!(USART2->SR & (1U << 7))) {}
    USART2->DR = byte;
}

void RS485_SendPacket(uint8_t target_node, uint8_t cmd, uint8_t speed_code) {
    GPIOB->ODR |= (1U << 10);                   /* DE/RE = HIGH (Transmit mode) */
    DelayMs(1);

    uint8_t packet[6];
    packet[0] = 0xAA;                           /* Sync header */
    packet[1] = 0x01;                           /* Source Node 1 */
    packet[2] = target_node;                    /* Destination Node */
    packet[3] = cmd;                            /* Command Code */
    packet[4] = speed_code;                     /* Speed code */
    packet[5] = packet[0] ^ packet[1] ^ packet[2] ^ packet[3] ^ packet[4]; /* Checksum */

    for (int i = 0; i < 6; i++) {
        USART2_SendByte(packet[i]);
    }

    while (!(USART2->SR & (1U << 6))) {}        /* Wait TC (Transmission Complete) */
    GPIOB->ODR &= ~(1U << 10);                  /* DE/RE = LOW (Receive mode) */
}

/* Bluetooth Command Receiver ISR */
void USART1_IRQHandler(void) {
    if (USART1->SR & (1U << 5)) {               /* RXNE flag set */
        bluetooth_rx_byte = (uint8_t)(USART1->DR & 0xFF);

        switch (bluetooth_rx_byte) {
            case 'U': case 'u': case '+':       /* Step brightness UP */
                auto_mode = 0;
                current_state = STATE_MANUAL;
                if (pwm_tgt_s1 < 950) pwm_tgt_s1 += 50; else pwm_tgt_s1 = 1000;
                pwm_tgt_s2 = pwm_tgt_s1;
                pwm_tgt_s3 = pwm_tgt_s1;
                break;

            case 'D': case 'd': case '-':       /* Step brightness DOWN */
                auto_mode = 0;
                current_state = STATE_MANUAL;
                if (pwm_tgt_s1 > 50) pwm_tgt_s1 -= 50; else pwm_tgt_s1 = 0;
                pwm_tgt_s2 = pwm_tgt_s1;
                pwm_tgt_s3 = pwm_tgt_s1;
                break;

            case 'A': case 'a':                 /* Re-enable Auto Adaptive Mode */
                auto_mode = 1;
                break;

            case '1':                           /* Emergency Full 100% Illumination */
                auto_mode = 0;
                current_state = STATE_MANUAL;
                pwm_tgt_s1 = DUTY_FULL;
                pwm_tgt_s2 = DUTY_FULL;
                pwm_tgt_s3 = DUTY_FULL;
                break;

            case '0':                           /* Blackout 0% Override */
                auto_mode = 0;
                current_state = STATE_MANUAL;
                pwm_tgt_s1 = DUTY_OFF;
                pwm_tgt_s2 = DUTY_OFF;
                pwm_tgt_s3 = DUTY_OFF;
                break;

            case 'F': case 'f':                 /* Simulate Vehicle Approach FORWARD */
                if (auto_mode) {
                    current_direction = DIR_FORWARD;
                    current_state = STATE_CORRIDOR_FWD;
                    corridor_start_time = GetTick();
                    pwm_tgt_s1 = DUTY_FULL;
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s3 = DUTY_PRELIT;
                    Trigger_Buzzer(100);
                    RS485_SendPacket(0x02, 0x11, 45); /* Forward pre-light command to Node 2 */
                }
                break;

            case 'R': case 'r':                 /* Simulate Vehicle Approach REVERSE */
                if (auto_mode) {
                    current_direction = DIR_REVERSE;
                    current_state = STATE_CORRIDOR_REV;
                    corridor_start_time = GetTick();
                    pwm_tgt_s3 = DUTY_FULL;
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s1 = DUTY_PRELIT;
                    Trigger_Buzzer(100);
                    RS485_SendPacket(0x01, 0x11, 45); /* Reverse pre-light command */
                }
                break;

            default:
                break;
        }
    }
}

/* =============================================================================
 * BARE-METAL I2C1 MASTER DRIVER
 * ============================================================================= */
void I2C1_Init(void) {
    I2C1->CR1 |= (1U << 15);                    /* Software reset I2C1 */
    DelayMs(1);
    I2C1->CR1 &= ~(1U << 15);

    I2C1->CR2   = 16;                           /* 16 MHz APB1 peripheral clock */
    I2C1->CCR   = 80;                           /* 100 kHz Standard Mode (16MHz / (2 * 100kHz) = 80) */
    I2C1->TRISE = 17;                           /* Max rise time = (1000ns / 62.5ns) + 1 = 17 */
    I2C1->CR1  |= (1U << 0);                    /* Peripheral Enable (PE = 1) */
}

uint8_t I2C1_Start(void) {
    I2C1->CR1 |= (1U << 8);                     /* START bit */
    uint32_t timeout = 50000;
    while (!(I2C1->SR1 & (1U << 0)) && --timeout) {} /* Wait SB bit */
    return (timeout > 0);
}

void I2C1_Stop(void) {
    I2C1->CR1 |= (1U << 9);                     /* STOP bit */
}

uint8_t I2C1_WriteByte(uint8_t data) {
    I2C1->DR = data;
    uint32_t timeout = 50000;
    while (!(I2C1->SR1 & ((1U << 1) | (1U << 7))) && --timeout) {} /* Wait ADDR or TXE */
    if (I2C1->SR1 & (1U << 1)) {
        (void)I2C1->SR1;                        /* Clear ADDR flag */
        (void)I2C1->SR2;
    }
    return (timeout > 0);
}

uint8_t I2C1_ReadAck(void) {
    I2C1->CR1 |= (1U << 10);                    /* ACK = 1 */
    uint32_t timeout = 50000;
    while (!(I2C1->SR1 & (1U << 6)) && --timeout) {} /* Wait RXNE */
    return (uint8_t)I2C1->DR;
}

uint8_t I2C1_ReadNack(void) {
    I2C1->CR1 &= ~(1U << 10);                   /* ACK = 0 */
    I2C1->CR1 |= (1U << 9);                     /* Generate STOP after reception */
    uint32_t timeout = 50000;
    while (!(I2C1->SR1 & (1U << 6)) && --timeout) {} /* Wait RXNE */
    return (uint8_t)I2C1->DR;
}

/* =============================================================================
 * SENSOR DRIVERS (BH1750, SHT31, LCD 16x2)
 * ============================================================================= */

/* BH1750 Ambient Lux Sensor */
void BH1750_Init(void) {
    if (I2C1_Start()) {
        I2C1_WriteByte(BH1750_I2C_ADDR);
        I2C1_WriteByte(0x01);                   /* Power On */
        I2C1_Stop();
    }
    DelayMs(10);
    if (I2C1_Start()) {
        I2C1_WriteByte(BH1750_I2C_ADDR);
        I2C1_WriteByte(0x10);                   /* Continuous High-Resolution Mode */
        I2C1_Stop();
    }
}

uint32_t BH1750_ReadLux(void) {
    uint8_t msb = 0, lsb = 0;
    if (I2C1_Start()) {
        I2C1_WriteByte(BH1750_I2C_ADDR | 0x01); /* Read mode */
        msb = I2C1_ReadAck();
        lsb = I2C1_ReadNack();
        return (uint32_t)(((uint32_t)msb << 8) | lsb) * 10 / 12;
    }
    return 100;                                 /* Fallback safe value */
}

/* SHT31 Temperature and Humidity Sensor */
void SHT31_Init(void) {
    /* No persistent init required for single-shot commands */
}

uint8_t SHT31_ReadTempHum(float *temp, float *hum) {
    if (!I2C1_Start()) return 0;
    I2C1_WriteByte(SHT31_I2C_ADDR);
    I2C1_WriteByte(0x24);                       /* High repeatability measurement command */
    I2C1_WriteByte(0x00);
    I2C1_Stop();

    DelayMs(15);                                /* Wait conversion time */

    if (!I2C1_Start()) return 0;
    I2C1_WriteByte(SHT31_I2C_ADDR | 0x01);      /* Read mode */
    uint8_t t_msb = I2C1_ReadAck();
    uint8_t t_lsb = I2C1_ReadAck();
    (void)I2C1_ReadAck();                       /* Temperature CRC (ignored for lightweight bare-metal) */
    uint8_t h_msb = I2C1_ReadAck();
    uint8_t h_lsb = I2C1_ReadAck();
    (void)I2C1_ReadNack();                      /* Humidity CRC */

    uint16_t raw_t = ((uint16_t)t_msb << 8) | t_lsb;
    uint16_t raw_h = ((uint16_t)h_msb << 8) | h_lsb;

    *temp = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *hum  = 100.0f * ((float)raw_h / 65535.0f);
    return 1;
}

/* 16x2 I2C LCD via PCF8574 */
void LCD_WriteNibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT;
    if (rs) data |= LCD_RS;

    if (I2C1_Start()) {
        I2C1_WriteByte(LCD_I2C_ADDR);
        I2C1_WriteByte(data | LCD_ENABLE);      /* Pulse EN high */
        DelayMs(1);
        I2C1_WriteByte(data & ~LCD_ENABLE);     /* Pulse EN low */
        DelayMs(1);
        I2C1_Stop();
    }
}

void LCD_SendCommand(uint8_t cmd) {
    LCD_WriteNibble(cmd & 0xF0, 0);
    LCD_WriteNibble((cmd << 4) & 0xF0, 0);
}

void LCD_SendData(uint8_t data) {
    LCD_WriteNibble(data & 0xF0, 1);
    LCD_WriteNibble((data << 4) & 0xF0, 1);
}

void LCD_Init(void) {
    DelayMs(50);
    LCD_WriteNibble(0x30, 0);
    DelayMs(5);
    LCD_WriteNibble(0x30, 0);
    DelayMs(1);
    LCD_WriteNibble(0x30, 0);
    DelayMs(1);
    LCD_WriteNibble(0x20, 0);                   /* 4-bit mode */
    DelayMs(1);

    LCD_SendCommand(0x28);                      /* 2 Lines, 5x8 Font */
    LCD_SendCommand(0x0C);                      /* Display ON, Cursor OFF */
    LCD_SendCommand(0x06);                      /* Auto-increment cursor */
    LCD_SendCommand(0x01);                      /* Clear display */
    DelayMs(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_SendCommand(address);
}

void LCD_Print(const char *str) {
    while (*str) {
        LCD_SendData((uint8_t)*str++);
    }
}

/* =============================================================================
 * INDICATORS (RGB STATUS & BUZZER)
 * ============================================================================= */
void Set_RGB(uint8_t r, uint8_t g, uint8_t b) {
    /* Active HIGH outputs on PB13 (Red), PB14 (Green), PB15 (Blue) */
    if (r) GPIOB->ODR |= (1U << 13); else GPIOB->ODR &= ~(1U << 13);
    if (g) GPIOB->ODR |= (1U << 14); else GPIOB->ODR &= ~(1U << 14);
    if (b) GPIOB->ODR |= (1U << 15); else GPIOB->ODR &= ~(1U << 15);
}

void Trigger_Buzzer(uint32_t duration_ms) {
    GPIOB->ODR |= (1U << 12);                   /* Buzzer ON */
    buzzer_beep_end = GetTick() + duration_ms;
}

/* =============================================================================
 * MAIN COOPERATIVE EXECUTIVE SUPER-LOOP
 * ============================================================================= */
int main(void) {
    /* Timing tracking variables */
    uint32_t last_sensor_tick = 0;
    uint32_t last_fade_tick = 0;
    uint32_t last_button_tick = 0;
    uint32_t last_display_tick = 0;
    uint32_t last_heartbeat_tick = 0;
    uint8_t display_page = 0;
    char lcd_buf[17];
    char tx_buffer[120];

    /* Initialize all hardware subsystems */
    init_port();
    BH1750_Init();
    SHT31_Init();
    LCD_Init();

    /* Splash Screen on 16x2 LCD */
    LCD_SetCursor(0, 0);
    LCD_Print("SMART STREETLGT");
    LCD_SetCursor(1, 0);
    LCD_Print("SYS BOOT OK...");
    DelayMs(1000);
    LCD_SendCommand(0x01);

    while (1) {
        uint32_t now = GetTick();

        /* ---------------------------------------------------------------------
         * Task 1: 1 Hz Heartbeat LED (PC13)
         * --------------------------------------------------------------------- */
        if (now - last_heartbeat_tick >= 500) {
            last_heartbeat_tick = now;
            GPIOC->ODR ^= (1U << 13);           /* Toggle onboard LED */
        }

        /* ---------------------------------------------------------------------
         * Task 2: Buzzer Auto-Shutoff Timer
         * --------------------------------------------------------------------- */
        if (buzzer_beep_end && (now >= buzzer_beep_end)) {
            GPIOB->ODR &= ~(1U << 12);          /* Buzzer OFF */
            buzzer_beep_end = 0;
        }

        /* ---------------------------------------------------------------------
         * Task 3: Dual PIR Motion & Direction Engine (Every 10 ms)
         * --------------------------------------------------------------------- */
        uint8_t pir_a_active = (GPIOB->IDR & (1U << 0)) ? 1 : 0;
        uint8_t pir_b_active = (GPIOB->IDR & (1U << 1)) ? 1 : 0;

        if (pir_a_active && (now - pir_a_trigger_time > 1500)) {
            pir_a_trigger_time = now;
            if (auto_mode && current_state != STATE_DAY) {
                if (now - pir_b_trigger_time < 2000 && pir_b_trigger_time != 0) {
                    /* PIR-B was triggered recently -> Moving REVERSE */
                    current_direction = DIR_REVERSE;
                    current_state = STATE_CORRIDOR_REV;
                    corridor_start_time = now;
                    est_velocity_cm_s = 20000 / (now - pir_b_trigger_time + 1);
                    pwm_tgt_s3 = DUTY_FULL;
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s1 = DUTY_PRELIT;
                    Trigger_Buzzer(80);
                    RS485_SendPacket(0x01, 0x11, (uint8_t)est_velocity_cm_s);
                } else {
                    /* First trigger on A -> Initiating FORWARD corridor */
                    current_direction = DIR_FORWARD;
                    current_state = STATE_CORRIDOR_FWD;
                    corridor_start_time = now;
                    pwm_tgt_s1 = DUTY_FULL;
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s3 = DUTY_PRELIT;
                    Trigger_Buzzer(80);
                    RS485_SendPacket(0x02, 0x11, 40);
                }
            }
        }

        if (pir_b_active && (now - pir_b_trigger_time > 1500)) {
            pir_b_trigger_time = now;
            if (auto_mode && current_state != STATE_DAY) {
                if (now - pir_a_trigger_time < 2000 && pir_a_trigger_time != 0) {
                    /* PIR-A triggered first -> Vehicle moving FORWARD through node 2 */
                    current_direction = DIR_FORWARD;
                    current_state = STATE_CORRIDOR_FWD;
                    corridor_start_time = now;
                    est_velocity_cm_s = 20000 / (now - pir_a_trigger_time + 1);
                    pwm_tgt_s1 = DUTY_IDLE_NORMAL; /* S1 starts decay */
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s3 = DUTY_FULL;        /* S3 ramps to full */
                    Trigger_Buzzer(80);
                    RS485_SendPacket(0x03, 0x11, (uint8_t)est_velocity_cm_s);
                } else {
                    /* First trigger on B -> Initiating REVERSE corridor */
                    current_direction = DIR_REVERSE;
                    current_state = STATE_CORRIDOR_REV;
                    corridor_start_time = now;
                    pwm_tgt_s3 = DUTY_FULL;
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s1 = DUTY_PRELIT;
                    Trigger_Buzzer(80);
                    RS485_SendPacket(0x01, 0x11, 40);
                }
            }
        }

        /* Corridor Reset Timer (After 3.5 seconds of no new motion) */
        if ((current_state == STATE_CORRIDOR_FWD || current_state == STATE_CORRIDOR_REV) &&
            (now - corridor_start_time > 3500)) {
            current_direction = DIR_NONE;
            current_state = STATE_NIGHT_IDLE;
        }

        /* ---------------------------------------------------------------------
         * Task 4: Multi-Sensor Sampling & Environmental Policy (Every 500 ms)
         * --------------------------------------------------------------------- */
        if (now - last_sensor_tick >= 500) {
            last_sensor_tick = now;

            /* Read LM35 Analog Temp on PA1 (ADC Channel 1) */
            uint32_t adc_lm35 = ADC_ReadChannel(1);
            sensor_temp_lm35 = (int32_t)((adc_lm35 * 330) / 4095);

            /* Read LDR Analog Photocell on PA4 (ADC Channel 4) */
            adc_ldr_raw = ADC_ReadChannel(4);

            /* Read BH1750 Digital Lux */
            sensor_lux = BH1750_ReadLux();

            /* Read SHT31 Digital Temp & Humidity */
            float sht_temp = 0.0f, sht_hum = 0.0f;
            if (SHT31_ReadTempHum(&sht_temp, &sht_hum)) {
                sensor_temp_sht31 = sht_temp;
                sensor_humidity   = sht_hum;
            }

            /* Decision Engine: Frost & Black Ice Evaluation */
            uint8_t frost_active = 0;
            if (current_state == STATE_FROST) {
                /* Hysteresis: Stay in frost state until road temp warms above reset threshold */
                if (sensor_temp_lm35 < TEMP_FROST_RESET) {
                    frost_active = 1;
                }
            } else {
                /* Trigger frost hazard if road temp drops <= 3°C */
                if (sensor_temp_lm35 <= TEMP_FROST_ALERT) {
                    frost_active = 1;
                }
            }

            if (frost_active) {
                current_state = STATE_FROST;
                /* Maximize illumination for driver safety on icy roads */
                if (auto_mode) {
                    pwm_tgt_s1 = DUTY_FULL;
                    pwm_tgt_s2 = DUTY_FULL;
                    pwm_tgt_s3 = DUTY_FULL;
                }
                Set_RGB(1, 0, 0);               /* Red Warning Beacon for Frost Hazard */
                Trigger_Buzzer(200);            /* Hazard warning chirp */
                RS485_SendPacket(0xFF, 0x20, 0); /* Broadcast frost hazard across RS-485 */
            } else if (auto_mode) {
                /* Evaluate Optical Context (Day vs Night) */
                if (sensor_lux >= LUX_DAY_THRESHOLD) {
                    current_state = STATE_DAY;
                    current_direction = DIR_NONE;
                    pwm_tgt_s1 = DUTY_OFF;
                    pwm_tgt_s2 = DUTY_OFF;
                    pwm_tgt_s3 = DUTY_OFF;
                    Set_RGB(0, 1, 0);           /* Solid Green (Day) */
                } else if (sensor_lux <= LUX_NIGHT_THRESHOLD) {
                    /* Night Condition */
                    if (current_state != STATE_CORRIDOR_FWD && current_state != STATE_CORRIDOR_REV) {
                        current_state = STATE_NIGHT_IDLE;
                        /* Check Atmospheric Humidity for Fog/Adverse Weather */
                        if (sensor_humidity >= HUMIDITY_POOR_COND) {
                            pwm_tgt_s1 = DUTY_IDLE_ADVERSE; /* 50% Fog baseline */
                            pwm_tgt_s2 = DUTY_IDLE_ADVERSE;
                            pwm_tgt_s3 = DUTY_IDLE_ADVERSE;
                            Set_RGB(1, 1, 0);   /* Yellow (Adverse Weather) */
                        } else {
                            pwm_tgt_s1 = DUTY_IDLE_NORMAL;  /* 20% Clear baseline */
                            pwm_tgt_s2 = DUTY_IDLE_NORMAL;
                            pwm_tgt_s3 = DUTY_IDLE_NORMAL;
                            Set_RGB(0, 1, 0);   /* Green (Clear Night) */
                        }
                    } else {
                        Set_RGB(0, 0, 1);       /* Blue (Corridor Active) */
                    }
                }
            } else {
                /* Manual Mode Status */
                Set_RGB(1, 0, 1);               /* Magenta (Manual) */
            }

            /* Broadcast Telemetry over Bluetooth (USART1) */
            const char *dir_str = (current_direction == DIR_FORWARD) ? "FWD" :
                                  (current_direction == DIR_REVERSE) ? "REV" : "IDLE";
            const char *stat_str = (current_state == STATE_DAY)          ? "DAY" :
                                   (current_state == STATE_FROST)        ? "FROST" :
                                   (current_state == STATE_MANUAL)       ? "MANUAL" :
                                   (current_state == STATE_CORRIDOR_FWD) ? "CORR_F" :
                                   (current_state == STATE_CORRIDOR_REV) ? "CORR_R" : "ADAPT";

            sprintf(tx_buffer, "[N1] T_LM:%dC T_SHT:%.1fC H:%.0f%% LUX:%u DIR:%s S1:%d%% S2:%d%% S3:%d%% STAT:%s\r\n",
                    (int)sensor_temp_lm35, sensor_temp_sht31, sensor_humidity,
                    (unsigned int)sensor_lux, dir_str,
                    pwm_cur_s1 / 10, pwm_cur_s2 / 10, pwm_cur_s3 / 10, stat_str);
            USART1_SendString(tx_buffer);
        }

        /* ---------------------------------------------------------------------
         * Task 5: Smooth PWM Soft-Ramping Fader (Every 25 ms)
         * --------------------------------------------------------------------- */
        if (now - last_fade_tick >= 25) {
            last_fade_tick = now;
            int step = 25;                      /* 2.5% step per 25 ms */

            if (pwm_cur_s1 < pwm_tgt_s1) {
                pwm_cur_s1 = (pwm_cur_s1 + step > pwm_tgt_s1) ? pwm_tgt_s1 : pwm_cur_s1 + step;
            } else if (pwm_cur_s1 > pwm_tgt_s1) {
                pwm_cur_s1 = (pwm_cur_s1 - step < pwm_tgt_s1) ? pwm_tgt_s1 : pwm_cur_s1 - step;
            }

            if (pwm_cur_s2 < pwm_tgt_s2) {
                pwm_cur_s2 = (pwm_cur_s2 + step > pwm_tgt_s2) ? pwm_tgt_s2 : pwm_cur_s2 + step;
            } else if (pwm_cur_s2 > pwm_tgt_s2) {
                pwm_cur_s2 = (pwm_cur_s2 - step < pwm_tgt_s2) ? pwm_tgt_s2 : pwm_cur_s2 - step;
            }

            if (pwm_cur_s3 < pwm_tgt_s3) {
                pwm_cur_s3 = (pwm_cur_s3 + step > pwm_tgt_s3) ? pwm_tgt_s3 : pwm_cur_s3 + step;
            } else if (pwm_cur_s3 > pwm_tgt_s3) {
                pwm_cur_s3 = (pwm_cur_s3 - step < pwm_tgt_s3) ? pwm_tgt_s3 : pwm_cur_s3 - step;
            }

            /* Update Hardware Timer Compare Registers */
            TIM2->CCR1 = (uint32_t)((pwm_cur_s1 >= 1000) ? 999 : pwm_cur_s1);
            TIM3->CCR1 = (uint32_t)((pwm_cur_s2 >= 1000) ? 999 : pwm_cur_s2);
            TIM3->CCR2 = (uint32_t)((pwm_cur_s3 >= 1000) ? 999 : pwm_cur_s3);
        }

        /* ---------------------------------------------------------------------
         * Task 6: Pushbutton Debouncing & Manual Step Control (Every 100 ms)
         * --------------------------------------------------------------------- */
        if (now - last_button_tick >= 100) {
            /* Button UP (PA5 active-LOW) */
            if (!(GPIOA->IDR & (1U << 5))) {
                auto_mode = 0;
                current_state = STATE_MANUAL;
                if (pwm_tgt_s1 < 950) pwm_tgt_s1 += 50; else pwm_tgt_s1 = 1000;
                pwm_tgt_s2 = pwm_tgt_s1;
                pwm_tgt_s3 = pwm_tgt_s1;
                last_button_tick = now;
            }
            /* Button DOWN (PA8 active-LOW) */
            else if (!(GPIOA->IDR & (1U << 8))) {
                auto_mode = 0;
                current_state = STATE_MANUAL;
                if (pwm_tgt_s1 > 50) pwm_tgt_s1 -= 50; else pwm_tgt_s1 = 0;
                pwm_tgt_s2 = pwm_tgt_s1;
                pwm_tgt_s3 = pwm_tgt_s1;
                last_button_tick = now;
            }
        }

        /* ---------------------------------------------------------------------
         * Task 7: 16x2 Character LCD Dual-Page Dashboard (Every 1500 ms)
         * --------------------------------------------------------------------- */
        if (now - last_display_tick >= 1500) {
            last_display_tick = now;
            display_page = !display_page;       /* Alternate page */

            if (display_page == 0) {
                /* Page 1: Environmental Metrics & Frost Alert */
                LCD_SetCursor(0, 0);
                if (current_state == STATE_FROST) {
                    sprintf(lcd_buf, "T:%dC *FROST ICE*", (int)sensor_temp_lm35);
                } else {
                    sprintf(lcd_buf, "T:%dC H:%.0f%%     ", (int)sensor_temp_lm35, sensor_humidity);
                }
                LCD_Print(lcd_buf);

                LCD_SetCursor(1, 0);
                sprintf(lcd_buf, "Lux:%u lx        ", (unsigned int)sensor_lux);
                LCD_Print(lcd_buf);
            } else {
                /* Page 2: Corridor & Streetlight Lighting Levels */
                const char *dir_lbl = (current_direction == DIR_FORWARD) ? "FWD" :
                                      (current_direction == DIR_REVERSE) ? "REV" : "IDL";
                LCD_SetCursor(0, 0);
                sprintf(lcd_buf, "DIR:%s S1:%3d%%   ", dir_lbl, pwm_cur_s1 / 10);
                LCD_Print(lcd_buf);

                LCD_SetCursor(1, 0);
                sprintf(lcd_buf, "S2:%3d%% S3:%3d%%  ", pwm_cur_s2 / 10, pwm_cur_s3 / 10);
                LCD_Print(lcd_buf);
            }
        }
    }
}
