/**
  ******************************************************************************
  * @file           : main.c
  * @project        : Predictive Adaptive Smart Streetlight & Environmental System
  * @target         : STM32F401CCUx (Black Pill, ARM Cortex-M4 @ 16 MHz HSI)
  * @architecture   : Pure CMSIS Bare-Metal Register Programming (stm32f4xx.h)
  *
  * Hardware Pinout Summary:
  *   - PA0  : GPIO Input Pull-Up (Onboard KEY Button: Auto / Manual Mode Toggle)
  *   - PA1  : ADC1_IN1  (LM35 Analog Pavement Temperature Sensor, 10mV/°C, 5V rail)
  *   - PA2  : TIM2_CH3  (PWM Output for Streetlight 1 / S1, 1.0 kHz Carrier)
  *   - PA4  : ADC1_IN4  (LDR Streetlight 2 Lamp Optical Output Feedback Sensor)
  *   - PA5  : GPIO Input Pull-Up (Manual Technician Button STEP UP +5%)
  *   - PA6  : TIM3_CH1  (PWM Output for Streetlight 2 / S2, Predicted Middle Zone)
  *   - PA7  : TIM3_CH2  (PWM Output for Streetlight 3 / S3, 1.0 kHz Carrier)
  *   - PA8  : GPIO Input Pull-Up (Manual Technician Button STEP DOWN -5%)
  *   - PA9  : USART1_TX (Bluetooth Telemetry @ 9600 Baud)
  *   - PA10 : USART1_RX (Bluetooth Command Receiver via NVIC Interrupt)
  *   - PB0  : EXTI0     (PIR-A Zone 1 Motion Sensor, Edge Interrupt)
  *   - PB1  : EXTI1     (PIR-B Zone 3 Motion Sensor, Edge Interrupt)
  *   - PB8  : I2C1_SCL  (Open-Drain Clock for SHT31, BH1750, 16x2 LCD)
  *   - PB9  : I2C1_SDA  (Open-Drain Data for SHT31, BH1750, 16x2 LCD)
  *   - PB12 : GPIO Output (Active Buzzer Alert Annunciator)
  *   - PB13 : GPIO Output (RGB Status Beacon - Red Channel)
  *   - PB14 : GPIO Output (RGB Status Beacon - Green Channel)
  *   - PB15 : GPIO Output (RGB Status Beacon - Blue Channel)
  *   - PC13 : GPIO Output (Onboard Diagnostic 1 Hz Heartbeat LED)
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* =============================================================================
 * CONSTANTS, I2C SLAVE ADDRESSES & POLICY DEFINITIONS
 * ============================================================================= */
#define BH1750_I2C_ADDR         (0x23 << 1)     /* 7-bit 0x23 -> 8-bit Write: 0x46, Read: 0x47 */
#define SHT31_I2C_ADDR          (0x44 << 1)     /* 7-bit 0x44 -> 8-bit Write: 0x88, Read: 0x89 */
#define LCD_I2C_ADDR            (0x27 << 1)     /* PCF8574 backpack (0x27 or 0x3F) */

/* PCF8574 LCD Control Bits */
#define LCD_BACKLIGHT           0x08
#define LCD_ENABLE              0x04
#define LCD_RW                  0x02
#define LCD_RS                  0x01

/* Optical Thresholds */
#define LUX_NIGHT_ON            20              /* Turn on streetlights when ambient < 20 lx */
#define LUX_DAY_OFF             50              /* Turn off streetlights when ambient > 50 lx */

/* Motion State Machine Timing */
#define PIR_WARMUP_MS           45000           /* 45s power-on stabilization for HC-SR501 */
#define PIR_MIN_INTERVAL_MS     1200            /* Refractory holdoff to reject analog re-triggers */
#define PIR_TRANSIT_TIMEOUT_MS  8000            /* Maximum inter-sensor transit window */

/* Perceptual Lighting Policy Levels (0 - 1000) */
typedef struct {
    uint16_t floor;                             /* Base idle brightness floor */
    uint16_t prelit;                            /* Upcoming pre-lit brightness */
    uint16_t full;                              /* Active target brightness */
    uint32_t hold_ms;                           /* Active illumination dwell time */
    uint32_t trail_ms;                          /* Trailing light decay delay */
} Policy_t;

static const Policy_t POLICY_NORMAL    = {  80,  600, 1000,  4000, 1500 };
static const Policy_t POLICY_WET_RISK  = { 250, 1000, 1000,  7000, 3000 };
static const Policy_t POLICY_FROST     = { 250, 1000, 1000, 10000, 4000 };

/* Environmental Context */
typedef enum {
    CONTEXT_NORMAL = 0,
    CONTEXT_WET_RISK,
    CONTEXT_FROST_RISK
} EnvContext_t;

/* Motion State */
typedef enum {
    MOTION_IDLE = 0,
    MOTION_ENTRY_FWD,
    MOTION_CONFIRMED_FWD,
    MOTION_ENTRY_REV,
    MOTION_CONFIRMED_REV,
    MOTION_AMBIGUOUS
} MotionState_t;

/* Direction */
typedef enum {
    DIR_NONE = 0,
    DIR_FORWARD,
    DIR_REVERSE,
    DIR_AMBIGUOUS
} Direction_t;

/* Operating Mode */
typedef enum {
    MODE_AUTO = 0,
    MODE_MANUAL
} SystemMode_t;

/* =============================================================================
 * GLOBAL VARIABLES
 * ============================================================================= */
volatile uint32_t ms_ticks = 0;                 /* Monotonic tick updated by SysTick ISR */
volatile uint8_t pir_a_raw_event = 0;           /* Flag set by EXTI0 ISR */
volatile uint8_t pir_b_raw_event = 0;           /* Flag set by EXTI1 ISR */
volatile uint8_t uart_rx_char = 0;              /* Received Bluetooth character */
volatile uint8_t uart_rx_flag = 0;

/* Operational States */
SystemMode_t current_mode = MODE_AUTO;
EnvContext_t current_context = CONTEXT_NORMAL;
MotionState_t motion_state = MOTION_IDLE;
Direction_t motion_direction = DIR_NONE;
uint8_t is_night = 0;
uint16_t manual_level = 500;

/* Timing Timestamps */
uint32_t pir_a_trigger_tick = 0;
uint32_t pir_b_trigger_tick = 0;
uint32_t motion_event_start = 0;
uint32_t buzzer_off_tick = 0;

/* Sensor Measurements */
uint32_t sensor_lux = 100;
float sensor_temp_air = 25.0f;
float sensor_humidity = 50.0f;
float sensor_dew_point = 14.0f;
float sensor_temp_surface = 25.0f;
uint16_t sensor_ldr_feedback = 0;

/* Diagnostic Flags */
uint8_t fault_bh1750 = 0;
uint8_t fault_sht31 = 0;
uint8_t fault_lamp2 = 0;

/* PWM Duty Levels (Perceptual 0 - 1000) */
uint16_t target_s1 = 0, current_s1 = 0;
uint16_t target_s2 = 0, current_s2 = 0;
uint16_t target_s3 = 0, current_s3 = 0;

/* Gamma 2.2 Lookup Table (Maps perceptual 0-1000 to timer CCR 0-999) */
static uint16_t gamma_lut[101];

/* Function Prototypes */
void init_port(void);
uint32_t GetTick(void);
void DelayMs(uint32_t ms);
void GammaLUT_Init(void);

/* Hardware Drivers */
uint32_t ADC_ReadOversampled(uint8_t channel, uint8_t samples);
void USART1_SendString(const char *str);
void Set_RGB(uint8_t r, uint8_t g, uint8_t b);
void Trigger_Buzzer(uint32_t duration_ms);

/* I2C Engine */
void I2C1_Init(void);
void I2C1_BusRecover(void);
uint8_t I2C1_Start(void);
void I2C1_Stop(void);
uint8_t I2C1_WriteByte(uint8_t data);
uint8_t I2C1_ReadAck(void);
uint8_t I2C1_ReadNack(void);

/* Sensor Drivers */
uint8_t BH1750_ReadLux(uint32_t *lux_out);
uint8_t SHT31_Read(float *temp_out, float *hum_out);
void LCD_Init(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Print(const char *str);

/* Decision & Corridor Engine */
void Calculate_DewPoint(void);
void Evaluate_PIR_FSM(uint32_t now);
void Lighting_Decide(uint32_t now);
void Update_Fader(void);

/* =============================================================================
 * HARDWARE INITIALIZATION ROUTINE (init_port)
 * ============================================================================= */
void init_port(void) {
    /* -------------------------------------------------------------------------
     * Step 1: Configure SysTick Monotonic Timebase FIRST (Priority 0)
     * ------------------------------------------------------------------------- */
    SysTick_Config(16000000 / 1000);            /* 1 ms tick @ 16 MHz HSI */
    NVIC_SetPriority(SysTick_IRQn, 0);

    /* -------------------------------------------------------------------------
     * Step 2: Enable Peripheral Bus Clocks
     * ------------------------------------------------------------------------- */
    RCC->AHB1ENR |= (1U << 0) | (1U << 1) | (1U << 2); /* GPIOA, GPIOB, GPIOC */
    RCC->APB1ENR |= (1U << 0) | (1U << 1) | (1U << 21); /* TIM2, TIM3, I2C1 */
    RCC->APB2ENR |= (1U << 4) | (1U << 8) | (1U << 14); /* USART1, ADC1, SYSCFG */

    /* -------------------------------------------------------------------------
     * Step 3: Configure GPIO Port A Pins
     * ------------------------------------------------------------------------- */
    /* PA0: Onboard KEY Button (Auto/Manual Toggle, Input with Pull-Up) */
    GPIOA->MODER &= ~(3U << 0);
    GPIOA->PUPDR &= ~(3U << 0);
    GPIOA->PUPDR |=  (1U << 0);

    /* PA1: ADC1_IN1 (LM35 Pavement Temp Sensor, Analog Mode) */
    GPIOA->MODER |=  (3U << 2);

    /* PA2: TIM2_CH3 PWM (AF1 -> Streetlight 1) */
    GPIOA->MODER &= ~(3U << 4);
    GPIOA->MODER |=  (2U << 4);
    GPIOA->AFR[0] |= (1U << 8);                 /* AFR2 = AF1 (TIM2_CH3) */

    /* PA4: ADC1_IN4 (LDR Optical Feedback on S2, Analog Mode) */
    GPIOA->MODER |=  (3U << 8);

    /* PA5 & PA8: Tactile Buttons UP & DOWN (Input with Pull-Up) */
    GPIOA->MODER &= ~((3U << 10) | (3U << 16));
    GPIOA->PUPDR &= ~((3U << 10) | (3U << 16));
    GPIOA->PUPDR |=  ((1U << 10) | (1U << 16));

    /* PA6: TIM3_CH1 PWM (AF2 -> Streetlight 2) */
    GPIOA->MODER &= ~(3U << 12);
    GPIOA->MODER |=  (2U << 12);
    GPIOA->AFR[0] |= (2U << 24);                /* AFR6 = AF2 */

    /* PA7: TIM3_CH2 PWM (AF2 -> Streetlight 3) */
    GPIOA->MODER &= ~(3U << 14);
    GPIOA->MODER |=  (2U << 14);
    GPIOA->AFR[0] |= (2U << 28);                /* AFR7 = AF2 */

    /* PA9 (TX) & PA10 (RX): USART1 (AF7 -> Bluetooth) */
    GPIOA->MODER &= ~((3U << 18) | (3U << 20));
    GPIOA->MODER |=  ((2U << 18) | (2U << 20));
    GPIOA->AFR[1] |= ((7U << 4) | (7U << 8));

    /* -------------------------------------------------------------------------
     * Step 4: Configure GPIO Port B & Port C Pins
     * ------------------------------------------------------------------------- */
    /* PB0 (PIR-A) & PB1 (PIR-B): Inputs with Pull-Down for EXTI */
    GPIOB->MODER &= ~((3U << 0) | (3U << 2));
    GPIOB->PUPDR &= ~((3U << 0) | (3U << 2));
    GPIOB->PUPDR |=  ((2U << 0) | (2U << 2));

    /* PB8 (SCL) & PB9 (SDA): I2C1 Open-Drain with Pull-Up */
    GPIOB->MODER &= ~((3U << 16) | (3U << 18));
    GPIOB->MODER |=  ((2U << 16) | (2U << 18));
    GPIOB->OTYPER |= (1U << 8) | (1U << 9);
    GPIOB->PUPDR  |= (1U << 16) | (1U << 18);
    GPIOB->AFR[1] |= ((4U << 0) | (4U << 4));

    /* PB12 (Buzzer), PB13 (Red), PB14 (Green), PB15 (Blue): Outputs */
    GPIOB->MODER &= ~((3U << 24) | (3U << 26) | (3U << 28) | (3U << 30));
    GPIOB->MODER |=  ((1U << 24) | (1U << 26) | (1U << 28) | (1U << 30));
    GPIOB->ODR   &= ~((1U << 12) | (1U << 13) | (1U << 14) | (1U << 15));

    /* PC13: Onboard LED (Active-LOW) */
    GPIOC->MODER &= ~(3U << 26);
    GPIOC->MODER |=  (1U << 26);
    GPIOC->ODR   |=  (1U << 13);                /* Default OFF */

    /* -------------------------------------------------------------------------
     * Step 5: Configure External Interrupts (EXTI0 and EXTI1 for PIRs)
     * ------------------------------------------------------------------------- */
    SYSCFG->EXTICR[0] &= ~((0x0F << 0) | (0x0F << 4));
    SYSCFG->EXTICR[0] |=  ((0x01 << 0) | (0x01 << 4)); /* Map PB0 -> EXTI0, PB1 -> EXTI1 */

    EXTI->IMR  |= (1U << 0) | (1U << 1);        /* Unmask EXTI0 and EXTI1 */
    EXTI->RTSR |= (1U << 0) | (1U << 1);        /* Trigger on Rising Edge */
    EXTI->FTSR &= ~((1U << 0) | (1U << 1));

    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_SetPriority(EXTI1_IRQn, 1);
    NVIC_EnableIRQ(EXTI1_IRQn);

    /* -------------------------------------------------------------------------
     * Step 6: Configure 1.0 kHz Hardware PWM Timers (TIM2 and TIM3)
     * Calculation: 16 MHz / ((15+1) * (999+1)) = 1000.0 Hz
     * ------------------------------------------------------------------------- */
    /* TIM2: Streetlight 1 on PA2 (Channel 3) */
    TIM2->PSC   = 15;
    TIM2->ARR   = 999;
    TIM2->CCR3  = 0;
    TIM2->CCMR2 &= ~TIM_CCMR2_OC3M;
    TIM2->CCMR2 |= (6U << 4) | (1U << 3);       /* PWM Mode 1 + Preload */
    TIM2->CCER  |= (1U << 8);                   /* Enable CH3 output (CC3E) */
    TIM2->CR1   |= (1U << 7) | (1U << 0);       /* ARPE + Counter Enable */

    /* TIM3: Streetlight 2 (CH1 on PA6) & Streetlight 3 (CH2 on PA7) */
    TIM3->PSC   = 15;
    TIM3->ARR   = 999;
    TIM3->CCR1  = 0;
    TIM3->CCR2  = 0;
    TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC2M);
    TIM3->CCMR1 |= (6U << 4)  | (1U << 3);      /* CH1 PWM Mode 1 + Preload */
    TIM3->CCMR1 |= (6U << 12) | (1U << 11);     /* CH2 PWM Mode 1 + Preload */
    TIM3->CCER  |= (1U << 0)  | (1U << 4);      /* Enable CH1 and CH2 outputs */
    TIM3->CR1   |= (1U << 7)  | (1U << 0);      /* ARPE + Counter Enable */

    /* -------------------------------------------------------------------------
     * Step 7: Configure ADC1 for LM35 & LDR Acquisition
     * ------------------------------------------------------------------------- */
    ADC->CCR    = 0;                            /* Clock = PCLK2 / 2 = 8 MHz */
    ADC1->CR1   = 0;
    ADC1->CR2   = 0;
    ADC1->SMPR2 |= (4U << 3) | (4U << 12);      /* 84 cycles for CH1 (PA1) and CH4 (PA4) */
    ADC1->CR2   |= (1U << 0);                   /* Enable ADC (ADON = 1) */
    DelayMs(2);

    /* -------------------------------------------------------------------------
     * Step 8: Configure USART1 for Bluetooth (9600 Baud, 8-N-1)
     * ------------------------------------------------------------------------- */
    USART1->BRR = (104U << 4) | (3U & 0x0F);    /* 16 MHz / (16 * 9600) = 104.1875 */
    USART1->CR1 = (1U << 3) | (1U << 2) | (1U << 5) | (1U << 13); /* TE, RE, RXNEIE, UE */
    NVIC_SetPriority(USART1_IRQn, 3);
    NVIC_EnableIRQ(USART1_IRQn);

    /* -------------------------------------------------------------------------
     * Step 9: Initialize I2C1 and Compute Gamma LUT
     * ------------------------------------------------------------------------- */
    I2C1_Init();
    GammaLUT_Init();
}

/* =============================================================================
 * SYSTICK, TIMING & GAMMA CORRECTION
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

void GammaLUT_Init(void) {
    /* Precompute Gamma 2.2 curve for perceptual 0-100% (101 steps) */
    for (int i = 0; i <= 100; i++) {
        float norm = (float)i / 100.0f;
        float gamma_val = powf(norm, 2.2f);
        gamma_lut[i] = (uint16_t)(gamma_val * 999.0f + 0.5f);
    }
}

uint16_t Map_Gamma(uint16_t perceptual_0_to_1000) {
    int index = perceptual_0_to_1000 / 10;
    if (index > 100) index = 100;
    return gamma_lut[index];
}

/* =============================================================================
 * INTERRUPT SERVICE ROUTINES (ISRs: Non-Blocking Event Push Only)
 * ============================================================================= */
void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1U << 0)) {
        EXTI->PR = (1U << 0);                   /* Clear pending bit */
        pir_a_raw_event = 1;                    /* Queue event */
    }
}

void EXTI1_IRQHandler(void) {
    if (EXTI->PR & (1U << 1)) {
        EXTI->PR = (1U << 1);                   /* Clear pending bit */
        pir_b_raw_event = 1;                    /* Queue event */
    }
}

void USART1_IRQHandler(void) {
    if (USART1->SR & (1U << 5)) {               /* RXNE flag set */
        uart_rx_char = (uint8_t)(USART1->DR & 0xFF);
        uart_rx_flag = 1;                       /* Queue character */
    }
}

/* =============================================================================
 * ADC OVERSAMPLED DRIVER
 * ============================================================================= */
uint32_t ADC_ReadOversampled(uint8_t channel, uint8_t samples) {
    uint32_t sum = 0;
    ADC1->SQR3 = channel & 0x1F;

    for (uint8_t i = 0; i < samples; i++) {
        ADC1->CR2 |= (1U << 30);                /* SWSTART */
        uint32_t timeout = 10000;
        while (!(ADC1->SR & (1U << 1)) && --timeout) {}
        sum += ADC1->DR;
    }
    return (samples > 0) ? (sum / samples) : 0;
}

/* =============================================================================
 * USART1 SERIAL OUTPUT & INDICATORS
 * ============================================================================= */
void USART1_SendString(const char *str) {
    while (*str) {
        while (!(USART1->SR & (1U << 7))) {}    /* Wait TXE */
        USART1->DR = (*str++ & 0xFF);
    }
}

void Set_RGB(uint8_t r, uint8_t g, uint8_t b) {
    if (r) GPIOB->ODR |= (1U << 13); else GPIOB->ODR &= ~(1U << 13);
    if (g) GPIOB->ODR |= (1U << 14); else GPIOB->ODR &= ~(1U << 14);
    if (b) GPIOB->ODR |= (1U << 15); else GPIOB->ODR &= ~(1U << 15);
}

void Trigger_Buzzer(uint32_t duration_ms) {
    GPIOB->ODR |= (1U << 12);                   /* Buzzer ON */
    buzzer_off_tick = GetTick() + duration_ms;
}

/* =============================================================================
 * HARDENED I2C1 MASTER DRIVER
 * ============================================================================= */
void I2C1_BusRecover(void) {
    /* Clock SCL 9 times to release stuck slave SDA line */
    GPIOB->MODER &= ~((3U << 16) | (3U << 18));
    GPIOB->MODER |=  ((1U << 16) | (1U << 18)); /* General purpose output */
    GPIOB->ODR |= (1U << 16) | (1U << 18);

    for (int i = 0; i < 9; i++) {
        GPIOB->ODR &= ~(1U << 16);              /* SCL LOW */
        DelayMs(1);
        GPIOB->ODR |=  (1U << 16);              /* SCL HIGH */
        DelayMs(1);
    }
    /* Return to AF4 mode */
    GPIOB->MODER &= ~((3U << 16) | (3U << 18));
    GPIOB->MODER |=  ((2U << 16) | (2U << 18));
}

void I2C1_Init(void) {
    I2C1_BusRecover();

    I2C1->CR1 |= (1U << 15);                    /* Software reset */
    DelayMs(1);
    I2C1->CR1 &= ~(1U << 15);

    I2C1->CR2   = 16;                           /* 16 MHz APB1 */
    I2C1->CCR   = 80;                           /* 100 kHz Standard Mode */
    I2C1->TRISE = 17;
    I2C1->CR1  |= (1U << 0);                    /* Peripheral Enable */
}

uint8_t I2C1_Start(void) {
    I2C1->CR1 |= (1U << 8);                     /* Generate START */
    uint32_t timeout = 10000;
    while (!(I2C1->SR1 & (1U << 0)) && --timeout) {}
    return (timeout > 0);
}

void I2C1_Stop(void) {
    I2C1->CR1 |= (1U << 9);                     /* Generate STOP */
}

uint8_t I2C1_WriteByte(uint8_t data) {
    I2C1->DR = data;
    uint32_t timeout = 10000;
    while (!(I2C1->SR1 & ((1U << 1) | (1U << 7))) && --timeout) {
        if (I2C1->SR1 & (1U << 10)) {           /* Acknowledge Failure (AF) */
            I2C1->SR1 &= ~(1U << 10);
            I2C1_Stop();
            return 0;
        }
    }
    if (I2C1->SR1 & (1U << 1)) {
        (void)I2C1->SR1;                        /* Clear ADDR flag */
        (void)I2C1->SR2;
    }
    return (timeout > 0);
}

uint8_t I2C1_ReadAck(void) {
    I2C1->CR1 |= (1U << 10);                    /* ACK = 1 */
    uint32_t timeout = 10000;
    while (!(I2C1->SR1 & (1U << 6)) && --timeout) {}
    return (uint8_t)I2C1->DR;
}

uint8_t I2C1_ReadNack(void) {
    I2C1->CR1 &= ~(1U << 10);                   /* ACK = 0 */
    I2C1->CR1 |= (1U << 9);                     /* STOP */
    uint32_t timeout = 10000;
    while (!(I2C1->SR1 & (1U << 6)) && --timeout) {}
    return (uint8_t)I2C1->DR;
}

/* =============================================================================
 * SENSOR DRIVERS (BH1750, SHT31, LCD 16x2)
 * ============================================================================= */

/* BH1750 Lux Sensor */
uint8_t BH1750_ReadLux(uint32_t *lux_out) {
    if (!I2C1_Start()) return 0;
    if (!I2C1_WriteByte(BH1750_I2C_ADDR)) { I2C1_Stop(); return 0; }
    if (!I2C1_WriteByte(0x10)) { I2C1_Stop(); return 0; } /* Continuous H-Mode */
    I2C1_Stop();

    DelayMs(20);

    if (!I2C1_Start()) return 0;
    if (!I2C1_WriteByte(BH1750_I2C_ADDR | 0x01)) { I2C1_Stop(); return 0; }
    uint8_t msb = I2C1_ReadAck();
    uint8_t lsb = I2C1_ReadNack();

    *lux_out = (uint32_t)(((uint32_t)msb << 8) | lsb) * 10 / 12;
    return 1;
}

/* SHT31 Air Temperature & Humidity */
uint8_t SHT31_Read(float *temp_out, float *hum_out) {
    if (!I2C1_Start()) return 0;
    if (!I2C1_WriteByte(SHT31_I2C_ADDR)) { I2C1_Stop(); return 0; }
    if (!I2C1_WriteByte(0x24)) { I2C1_Stop(); return 0; }
    if (!I2C1_WriteByte(0x00)) { I2C1_Stop(); return 0; }
    I2C1_Stop();

    DelayMs(20);

    if (!I2C1_Start()) return 0;
    if (!I2C1_WriteByte(SHT31_I2C_ADDR | 0x01)) { I2C1_Stop(); return 0; }
    uint8_t t_msb = I2C1_ReadAck();
    uint8_t t_lsb = I2C1_ReadAck();
    (void)I2C1_ReadAck();                       /* Temperature CRC */
    uint8_t h_msb = I2C1_ReadAck();
    uint8_t h_lsb = I2C1_ReadNack();

    uint16_t raw_t = ((uint16_t)t_msb << 8) | t_lsb;
    uint16_t raw_h = ((uint16_t)h_msb << 8) | h_lsb;

    *temp_out = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *hum_out  = 100.0f * ((float)raw_h / 65535.0f);
    return 1;
}

/* 16x2 I2C LCD via PCF8574 */
void LCD_WriteNibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT;
    if (rs) data |= LCD_RS;

    if (I2C1_Start()) {
        I2C1_WriteByte(LCD_I2C_ADDR);
        I2C1_WriteByte(data | LCD_ENABLE);
        DelayMs(1);
        I2C1_WriteByte(data & ~LCD_ENABLE);
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
    LCD_SendCommand(0x06);                      /* Auto-increment */
    LCD_SendCommand(0x01);                      /* Clear */
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
 * PSYCHROMETRIC DEW POINT & DECISION ENGINES
 * ============================================================================= */
void Calculate_DewPoint(void) {
    /* Magnus Formula for psychrometric dew point */
    if (sensor_humidity < 1.0f) sensor_humidity = 1.0f;
    float gamma = (17.62f * sensor_temp_air) / (243.12f + sensor_temp_air) + logf(sensor_humidity / 100.0f);
    sensor_dew_point = (243.12f * gamma) / (17.62f - gamma);

    /* Determine Environmental Policy Context */
    if (sensor_temp_surface <= 3.0f && sensor_temp_surface <= sensor_dew_point) {
        current_context = CONTEXT_FROST_RISK;
    } else if ((sensor_temp_air - sensor_dew_point) <= 2.0f || sensor_humidity >= 88.0f) {
        current_context = CONTEXT_WET_RISK;
    } else {
        current_context = CONTEXT_NORMAL;
    }
}

/* PIR Edge-Triggered Direction State Machine */
void Evaluate_PIR_FSM(uint32_t now) {
    if (now < PIR_WARMUP_MS) return;            /* 45s power-on stabilization */

    if (pir_a_raw_event) {
        pir_a_raw_event = 0;
        /* Glitch filter: verify PB0 is still HIGH */
        if (GPIOB->IDR & (1U << 0)) {
            if (now - pir_a_trigger_tick >= PIR_MIN_INTERVAL_MS) {
                pir_a_trigger_tick = now;

                if (motion_state == MOTION_IDLE) {
                    motion_state = MOTION_ENTRY_FWD;
                    motion_direction = DIR_FORWARD;
                    motion_event_start = now;
                    Trigger_Buzzer(60);
                } else if (motion_state == MOTION_ENTRY_REV) {
                    uint32_t dt = now - pir_b_trigger_tick;
                    if (dt >= 250 && dt <= PIR_TRANSIT_TIMEOUT_MS) {
                        motion_state = MOTION_CONFIRMED_REV;
                        motion_direction = DIR_REVERSE;
                        motion_event_start = now;
                    }
                }
            }
        }
    }

    if (pir_b_raw_event) {
        pir_b_raw_event = 0;
        /* Glitch filter: verify PB1 is still HIGH */
        if (GPIOB->IDR & (1U << 1)) {
            if (now - pir_b_trigger_tick >= PIR_MIN_INTERVAL_MS) {
                pir_b_trigger_tick = now;

                if (motion_state == MOTION_IDLE) {
                    motion_state = MOTION_ENTRY_REV;
                    motion_direction = DIR_REVERSE;
                    motion_event_start = now;
                    Trigger_Buzzer(60);
                } else if (motion_state == MOTION_ENTRY_FWD) {
                    uint32_t dt = now - pir_a_trigger_tick;
                    if (dt >= 250 && dt <= PIR_TRANSIT_TIMEOUT_MS) {
                        motion_state = MOTION_CONFIRMED_FWD;
                        motion_direction = DIR_FORWARD;
                        motion_event_start = now;
                    }
                }
            }
        }
    }
}

/* Pure Decision Function: Computes S1, S2, S3 Target Duty Cycles */
void Lighting_Decide(uint32_t now) {
    if (current_mode == MODE_MANUAL) {
        target_s1 = manual_level;
        target_s2 = manual_level;
        target_s3 = manual_level;
        Set_RGB(1, 0, 1);                       /* Magenta (Manual) */
        return;
    }

    /* Ambient Day/Night Determination (Fail-Bright if BH1750 faulted) */
    if (fault_bh1750) {
        is_night = 1;                           /* Safety default */
    } else {
        if (sensor_lux >= LUX_DAY_OFF) is_night = 0;
        else if (sensor_lux <= LUX_NIGHT_ON) is_night = 1;
    }

    if (!is_night) {
        target_s1 = 0;
        target_s2 = 0;
        target_s3 = 0;
        Set_RGB(0, 1, 0);                       /* Solid Green (Daylight) */
        motion_state = MOTION_IDLE;
        motion_direction = DIR_NONE;
        return;
    }

    /* Select Active Policy Profile */
    const Policy_t *p = (current_context == CONTEXT_FROST_RISK) ? &POLICY_FROST :
                        (current_context == CONTEXT_WET_RISK)   ? &POLICY_WET_RISK : &POLICY_NORMAL;

    /* Evaluate Timed Staged Corridor Wave */
    uint32_t elapsed = now - motion_event_start;

    if (motion_state == MOTION_ENTRY_FWD) {
        target_s1 = p->full;
        target_s2 = p->full;                    /* S2 predicted zone */
        target_s3 = p->prelit;
        Set_RGB(0, 0, 1);                       /* Blue (Corridor Wave) */
        if (elapsed > p->hold_ms) {
            motion_state = MOTION_IDLE;
            motion_direction = DIR_NONE;
        }
    } else if (motion_state == MOTION_CONFIRMED_FWD) {
        target_s1 = p->floor;                   /* Trailing node decays */
        target_s2 = (elapsed < p->trail_ms) ? p->full : p->floor;
        target_s3 = p->full;
        Set_RGB(0, 0, 1);
        if (elapsed > p->hold_ms) {
            motion_state = MOTION_IDLE;
            motion_direction = DIR_NONE;
        }
    } else if (motion_state == MOTION_ENTRY_REV) {
        target_s3 = p->full;
        target_s2 = p->full;
        target_s1 = p->prelit;
        Set_RGB(0, 0, 1);
        if (elapsed > p->hold_ms) {
            motion_state = MOTION_IDLE;
            motion_direction = DIR_NONE;
        }
    } else if (motion_state == MOTION_CONFIRMED_REV) {
        target_s3 = p->floor;
        target_s2 = (elapsed < p->trail_ms) ? p->full : p->floor;
        target_s1 = p->full;
        Set_RGB(0, 0, 1);
        if (elapsed > p->hold_ms) {
            motion_state = MOTION_IDLE;
            motion_direction = DIR_NONE;
        }
    } else {
        /* Idle Baseline State */
        target_s1 = p->floor;
        target_s2 = p->floor;
        target_s3 = p->floor;
        if (current_context == CONTEXT_FROST_RISK) Set_RGB(1, 0, 0); /* Red Warning */
        else if (current_context == CONTEXT_WET_RISK) Set_RGB(1, 1, 0); /* Yellow */
        else Set_RGB(0, 1, 0);                  /* Green */
    }
}

/* Asymmetric PWM Soft-Ramping with Gamma Slew */
void Update_Fader(void) {
    const int step_up = 40;                     /* Fast attack (250 ms to full) */
    const int step_down = 15;                   /* Slow decay (1600 ms to floor) */

    /* S1 Fader */
    if (current_s1 < target_s1) {
        current_s1 = (current_s1 + step_up > target_s1) ? target_s1 : current_s1 + step_up;
    } else if (current_s1 > target_s1) {
        current_s1 = (current_s1 - step_down < target_s1) ? target_s1 : current_s1 - step_down;
    }

    /* S2 Fader */
    if (current_s2 < target_s2) {
        current_s2 = (current_s2 + step_up > target_s2) ? target_s2 : current_s2 + step_up;
    } else if (current_s2 > target_s2) {
        current_s2 = (current_s2 - step_down < target_s2) ? target_s2 : current_s2 - step_down;
    }

    /* S3 Fader */
    if (current_s3 < target_s3) {
        current_s3 = (current_s3 + step_up > target_s3) ? target_s3 : current_s3 + step_up;
    } else if (current_s3 > target_s3) {
        current_s3 = (current_s3 - step_down < target_s3) ? target_s3 : current_s3 - step_down;
    }

    /* Load Gamma-Mapped Values into Timer Compare Registers */
    TIM2->CCR3 = Map_Gamma(current_s1);
    TIM3->CCR1 = Map_Gamma(current_s2);
    TIM3->CCR2 = Map_Gamma(current_s3);
}

/* =============================================================================
 * MAIN EXECUTIVE COOPERATIVE SCHEDULER
 * ============================================================================= */
int main(void) {
    uint32_t last_sensor_tick = 0;
    uint32_t last_fader_tick = 0;
    uint32_t last_button_tick = 0;
    uint32_t last_display_tick = 0;
    uint32_t last_heartbeat_tick = 0;
    uint8_t display_page = 0;

    char lcd_line[17];
    char tx_buf[120];

    /* Initialize Hardware Core */
    init_port();
    LCD_Init();

    /* Splash Screen */
    LCD_SetCursor(0, 0);
    LCD_Print("SMART STREETLGT ");
    LCD_SetCursor(1, 0);
    LCD_Print("INITIALIZING... ");
    DelayMs(1000);

    while (1) {
        uint32_t now = GetTick();

        /* ---------------------------------------------------------------------
         * Task 1: 1 Hz Diagnostic Heartbeat (PC13)
         * --------------------------------------------------------------------- */
        if (now - last_heartbeat_tick >= 500) {
            last_heartbeat_tick = now;
            GPIOC->ODR ^= (1U << 13);
        }

        /* ---------------------------------------------------------------------
         * Task 2: Buzzer Auto-Shutoff
         * --------------------------------------------------------------------- */
        if (buzzer_off_tick && (now >= buzzer_off_tick)) {
            GPIOB->ODR &= ~(1U << 12);
            buzzer_off_tick = 0;
        }

        /* ---------------------------------------------------------------------
         * Task 3: Bluetooth Ingestion & Simulation Interface
         * --------------------------------------------------------------------- */
        if (uart_rx_flag) {
            uart_rx_flag = 0;
            switch (uart_rx_char) {
                case 'M': case 'm':             /* Toggle Auto/Manual Mode */
                    current_mode = (current_mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
                    break;
                case 'U': case 'u': case '+':   /* Step UP +5% */
                    current_mode = MODE_MANUAL;
                    if (manual_level < 950) manual_level += 50; else manual_level = 1000;
                    break;
                case 'D': case 'd': case '-':   /* Step DOWN -5% */
                    current_mode = MODE_MANUAL;
                    if (manual_level > 50) manual_level -= 50; else manual_level = 0;
                    break;
                case 'A': case 'a':             /* Engage Auto Mode */
                    current_mode = MODE_AUTO;
                    break;
                case 'F': case 'f':             /* Simulate Forward Transit Edge */
                    pir_a_raw_event = 1;
                    break;
                case 'R': case 'r':             /* Simulate Reverse Transit Edge */
                    pir_b_raw_event = 1;
                    break;
                default:
                    break;
            }
        }

        /* ---------------------------------------------------------------------
         * Task 4: Pushbutton Debouncing (PA0 Mode, PA5 Up, PA8 Down)
         * --------------------------------------------------------------------- */
        if (now - last_button_tick >= 100) {
            last_button_tick = now;
            /* PA0: Onboard KEY Button (Auto/Manual Toggle) */
            if (!(GPIOA->IDR & (1U << 0))) {
                current_mode = (current_mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
                Trigger_Buzzer(50);
            }
            /* PA5: Step UP */
            else if (!(GPIOA->IDR & (1U << 5))) {
                current_mode = MODE_MANUAL;
                if (manual_level < 950) manual_level += 50; else manual_level = 1000;
            }
            /* PA8: Step DOWN */
            else if (!(GPIOA->IDR & (1U << 8))) {
                current_mode = MODE_MANUAL;
                if (manual_level > 50) manual_level -= 50; else manual_level = 0;
            }
        }

        /* ---------------------------------------------------------------------
         * Task 5: Dual PIR Motion FSM Evaluator (10 ms)
         * --------------------------------------------------------------------- */
        Evaluate_PIR_FSM(now);

        /* ---------------------------------------------------------------------
         * Task 6: Environmental Sensing & Decision Core (500 ms)
         * --------------------------------------------------------------------- */
        if (now - last_sensor_tick >= 500) {
            last_sensor_tick = now;

            /* Sample LM35 Surface Probe (16x Oversampled on PA1) */
            uint32_t adc_lm35 = ADC_ReadOversampled(1, 16);
            sensor_temp_surface = (float)(adc_lm35 * 330) / 4095.0f;

            /* Sample LDR Optical Feedback on PA4 */
            sensor_ldr_feedback = ADC_ReadOversampled(4, 4);

            /* Sample BH1750 Ambient Lux */
            uint32_t lux_val = 0;
            if (BH1750_ReadLux(&lux_val)) {
                sensor_lux = lux_val;
                fault_bh1750 = 0;
            } else {
                fault_bh1750 = 1;
            }

            /* Sample SHT31 Air Temp & Humidity */
            float t_air = 0.0f, hum = 0.0f;
            if (SHT31_Read(&t_air, &hum)) {
                sensor_temp_air = t_air;
                sensor_humidity = hum;
                fault_sht31 = 0;
            } else {
                fault_sht31 = 1;
            }

            /* Compute Dew Point & Evaluate Lighting Policy */
            Calculate_DewPoint();
            Lighting_Decide(now);

            /* Closed-Loop Lamp-2 Optical Feedback Diagnostic */
            if (current_s2 >= 800 && sensor_ldr_feedback < 100) {
                fault_lamp2 = 1;
            } else {
                fault_lamp2 = 0;
            }

            /* Stream 1 Hz Telemetry over Bluetooth (USART1) */
            const char *dir_name = (motion_direction == DIR_FORWARD) ? "FWD" :
                                   (motion_direction == DIR_REVERSE) ? "REV" : "IDL";
            const char *ctx_name = (current_mode == MODE_MANUAL)           ? "MANUAL" :
                                   (!is_night)                             ? "DAY" :
                                   (current_context == CONTEXT_FROST_RISK) ? "FROST" :
                                   (current_context == CONTEXT_WET_RISK)   ? "WET" : "NORMAL";

            snprintf(tx_buf, sizeof(tx_buf),
                     "[N1] T_S:%.1fC T_A:%.1fC H:%.0f%% LUX:%u DIR:%s S1:%u%% S2:%u%% S3:%u%% STAT:%s\r\n",
                     sensor_temp_surface, sensor_temp_air, sensor_humidity,
                     (unsigned int)sensor_lux, dir_name,
                     current_s1 / 10, current_s2 / 10, current_s3 / 10, ctx_name);
            USART1_SendString(tx_buf);
        }

        /* ---------------------------------------------------------------------
         * Task 7: Asymmetric PWM Slew Fader (25 ms)
         * --------------------------------------------------------------------- */
        if (now - last_fader_tick >= 25) {
            last_fader_tick = now;
            Update_Fader();
        }

        /* ---------------------------------------------------------------------
         * Task 8: Non-Blocking 16x2 Character LCD Dashboard (1500 ms)
         * --------------------------------------------------------------------- */
        if (now - last_display_tick >= 1500) {
            last_display_tick = now;
            display_page = !display_page;

            if (now < PIR_WARMUP_MS) {
                /* Display Startup Sensor Warm-Up Countdown */
                LCD_SetCursor(0, 0);
                snprintf(lcd_line, sizeof(lcd_line), "SMART STREETLGT ");
                LCD_Print(lcd_line);

                LCD_SetCursor(1, 0);
                snprintf(lcd_line, sizeof(lcd_line), "PIR WARMUP: %02ds ", (int)((PIR_WARMUP_MS - now) / 1000));
                LCD_Print(lcd_line);
            } else if (display_page == 0) {
                /* Page 1: Environmental Telemetry */
                LCD_SetCursor(0, 0);
                snprintf(lcd_line, sizeof(lcd_line), "Ts:%.0fC Ta:%.0fC    ", sensor_temp_surface, sensor_temp_air);
                LCD_Print(lcd_line);

                LCD_SetCursor(1, 0);
                snprintf(lcd_line, sizeof(lcd_line), "H:%.0f%% Lux:%-4u  ", sensor_humidity, (unsigned int)sensor_lux);
                LCD_Print(lcd_line);
            } else {
                /* Page 2: Corridor & Streetlight State */
                const char *dir_lbl = (motion_direction == DIR_FORWARD) ? "FWD" :
                                      (motion_direction == DIR_REVERSE) ? "REV" : "IDL";
                LCD_SetCursor(0, 0);
                if (fault_lamp2) {
                    snprintf(lcd_line, sizeof(lcd_line), "DIR:%s *LAMP2 FLT*", dir_lbl);
                } else if (current_context == CONTEXT_FROST_RISK) {
                    snprintf(lcd_line, sizeof(lcd_line), "DIR:%s *FROST*   ", dir_lbl);
                } else {
                    snprintf(lcd_line, sizeof(lcd_line), "DIR:%s %-10s", dir_lbl,
                             (current_mode == MODE_MANUAL) ? "MANUAL" : (is_night ? "NIGHT" : "DAY"));
                }
                LCD_Print(lcd_line);

                LCD_SetCursor(1, 0);
                snprintf(lcd_line, sizeof(lcd_line), "1:%-2u 2:%-2u 3:%-2u%% ",
                         current_s1 / 10, current_s2 / 10, current_s3 / 10);
                LCD_Print(lcd_line);
            }
        }
    }
}
