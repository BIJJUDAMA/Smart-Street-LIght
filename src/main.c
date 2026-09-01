/**
  ******************************************************************************
  * @file           : main.c
  * @project        : Smart Streetlight & Roadside Environmental Monitoring Unit
  * @target         : STM32F401CCUx (Black Pill, ARM Cortex-M4 @ 16 MHz)
  * @architecture   : Pure CMSIS Bare-Metal Register Programming (stm32f4xx.h)
  *
  * Hardware Pinout Summary:
  *   - PA0  : TIM2_CH1 (PWM Output for Streetlight LED Dimmer, 0-100%)
  *   - PA1  : ADC1_IN1 (LM35 Roadside Temperature Sensor, 10mV/°C)
  *   - PA2  : ADC1_IN2 (LDR Ambient Light Sensor)
  *   - PA3  : GPIO Output (Red Hazard Over-Temperature Warning Beacon)
  *   - PA9  : USART1_TX (Bluetooth Wireless Telemetry @ 9600 Baud)
  *   - PA10 : USART1_RX (Bluetooth Command Receiver via NVIC Interrupt)
  *   - PB0  : GPIO Input Pull-Up (Manual Brightness UP Button)
  *   - PB1  : GPIO Input Pull-Up (Manual Brightness DOWN Button)
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * GLOBAL VARIABLES & CALIBRATION THRESHOLDS
 * ============================================================================= */
volatile uint32_t ms_ticks = 0;             /* Millisecond counter updated by SysTick ISR */
volatile int pwm_value = 0;                 /* Current PWM duty cycle (0 = OFF, 1000 = 100%) */
volatile uint8_t auto_light_mode = 1;       /* 1 = Automatic LDR control, 0 = Manual override */
volatile uint8_t bluetooth_rx_byte = 0;     /* Stores last received Bluetooth command character */

/* Hysteresis & Sensor Thresholds */
const int DARK_THRESHOLD   = 2500;          /* LDR ADC threshold to turn ON lights at night/tunnel */
const int BRIGHT_THRESHOLD = 2000;          /* LDR ADC threshold to re-enable auto daylight mode */
const int TEMP_HIGH_ALERT  = 40;            /* Road temperature threshold to turn ON Hazard LED (>40°C) */
const int TEMP_LOW_RESET   = 37;            /* Hysteresis reset threshold to turn OFF Hazard LED (<37°C) */

/* Function Prototypes */
void init_port(void);
uint32_t ADC_ReadChannel(uint8_t channel);
void USART1_SendString(const char *str);
uint32_t GetTick(void);

/* =============================================================================
 * HARDWARE INITIALIZATION ROUTINE (init_port)
 * ============================================================================= */
void init_port(void) {
    /* -------------------------------------------------------------
     * Step 1: Enable Peripheral Bus Clocks via RCC Registers
     * ------------------------------------------------------------- */
    RCC->AHB1ENR |= (1U << 0);  // Enable GPIOA Clock (bit 0)
    RCC->AHB1ENR |= (1U << 1);  // Enable GPIOB Clock (bit 1)
    RCC->APB1ENR |= (1U << 0);  // Enable TIM2 Timer Clock (bit 0)
    RCC->APB2ENR |= (1U << 8);  // Enable ADC1 Clock (bit 8)
    RCC->APB2ENR |= (1U << 4);  // Enable USART1 Clock (bit 4)

    /* -------------------------------------------------------------
     * Step 2: Configure GPIO Pin Modes (MODER, AFR, PUPDR)
     * ------------------------------------------------------------- */
    // PA0: Configure as Alternate Function (AF1) for TIM2 Channel 1 PWM output
    GPIOA->MODER &= ~(3U << 0);
    GPIOA->MODER |=  (2U << 0);       // MODER0 = 10 (Alternate Function)
    GPIOA->AFR[0] |= (1U << 0);       // AFR0 = 0001 (AF1 -> TIM2_CH1)

    // PA1 & PA2: Configure as Analog Inputs for LM35 (CH1) and LDR (CH2)
    GPIOA->MODER |= (3U << 2);        // MODER1 = 11 (Analog Mode for PA1)
    GPIOA->MODER |= (3U << 4);        // MODER2 = 11 (Analog Mode for PA2)

    // PA3: Configure as General Purpose Push-Pull Output for Red Hazard LED
    GPIOA->MODER &= ~(3U << 6);
    GPIOA->MODER |=  (1U << 6);       // MODER3 = 01 (Digital Output)
    GPIOA->ODR   &= ~(1U << 3);       // Ensure LED starts in OFF state

    // PA9 (TX) & PA10 (RX): Configure as Alternate Function (AF7) for USART1
    GPIOA->MODER &= ~((3U << 18) | (3U << 20));
    GPIOA->MODER |=  ((2U << 18) | (2U << 20)); // MODER9=10, MODER10=10 (AF mode)
    GPIOA->AFR[1] |= ((7U << 4) | (7U << 8));   // AFR9=AF7, AFR10=AF7 (USART1)

    // PB0 & PB1: Configure as Digital Inputs with Internal Pull-Up for Pushbuttons
    GPIOB->MODER &= ~((3U << 0) | (3U << 2));   // MODER0=00, MODER1=00 (Input mode)
    GPIOB->PUPDR &= ~((3U << 0) | (3U << 2));   // Clear existing pull settings
    GPIOB->PUPDR |=  ((1U << 0) | (1U << 2));   // PUPDR0=01, PUPDR1=01 (Pull-Up active)

    /* -------------------------------------------------------------
     * Step 3: Configure Timer 2 for 190.5 Hz PWM (Streetlight Dimmer)
     * Calculation: Frequency = 16 MHz / ((PSC+1) * (ARR+1))
     *                        = 16,000,000 / (84 * 1000) = 190.48 Hz
     * ------------------------------------------------------------- */
    TIM2->PSC   = 83;                         // Prescaler = 83 (Divides 16 MHz to 190.48 kHz)
    TIM2->ARR   = 999;                        // Auto-Reload = 999 (Sets 1000 discrete dimming steps)
    TIM2->CCR1  = 0;                          // Initialize Capture/Compare to 0 (0% duty cycle)
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM2->CCMR1 |= (6U << 4) | (1U << 3);     // OC1M = 110 (PWM Mode 1) + Preload Enable
    TIM2->CCER  |= (1U << 0);                 // Enable Output on Channel 1 (CC1E)
    TIM2->CR1   |= (1U << 7) | (1U << 0);     // Enable Auto-Reload Preload (ARPE) + Start Counter (CEN)

    /* -------------------------------------------------------------
     * Step 4: Configure 12-Bit Analog-to-Digital Converter (ADC1)
     * ------------------------------------------------------------- */
    ADC->CCR    = 0;                          // ADC Prescaler = PCLK2 / 2 (8 MHz ADC clock)
    ADC1->CR1   = 0;                          // 12-bit resolution, single conversion mode
    ADC1->CR2   = 0;                          // Clear control register 2
    ADC1->SMPR2 |= (4U << 3) | (4U << 6);     // Sample time = 84 cycles for Channel 1 and Channel 2
    ADC1->CR2   |= (1U << 0);                 // Enable ADC power (ADON = 1)
    while ((ADC1->CR2 & (1U << 0)) == 0) {}   // Wait until ADC internal stabilization completes

    /* -------------------------------------------------------------
     * Step 5: Configure USART1 for Wireless Bluetooth (9600 Baud, 8-N-1)
     * Calculation: Baud Rate = 16 MHz / (16 * 9600) = 104.1875
     *              Mantissa = 104 (0x68), Fraction = 0.1875 * 16 = 3 (0x3) -> BRR = 0x0683
     * ------------------------------------------------------------- */
    USART1->BRR = (104U << 4) | (3U & 0x0F);  // Load calculated Baud Rate Register
    USART1->CR1 = (1U << 3) | (1U << 2) |     // Enable Transmitter (TE) and Receiver (RE)
                  (1U << 5) | (1U << 13);     // Enable RX Interrupt (RXNEIE) and USART (UE)

    /* Enable USART1 Global Interrupt in ARM Cortex-M4 NVIC */
    NVIC_SetPriority(USART1_IRQn, 0);         // Set highest priority (Priority 0)
    NVIC_EnableIRQ(USART1_IRQn);              // Enable interrupt line in NVIC

    /* -------------------------------------------------------------
     * Step 6: Configure SysTick for 1 ms Monotonic Timebase
     * ------------------------------------------------------------- */
    SysTick_Config(16000000 / 1000);          // 16,000 ticks per millisecond @ 16 MHz HSI
}

/* =============================================================================
 * HELPER FUNCTIONS
 * ============================================================================= */

/**
  * @brief  Selects and converts a single analog channel from ADC1.
  * @param  channel: ADC channel number (1 for LM35 on PA1, 2 for LDR on PA2)
  * @retval 12-bit digital conversion result (0 - 4095)
  */
uint32_t ADC_ReadChannel(uint8_t channel) {
    ADC1->SQR3 = channel & 0x1F;              // Set desired channel in 1st conversion sequence rank
    ADC1->CR2 |= (1U << 30);                  // Trigger software conversion start (SWSTART = 1)
    while (!(ADC1->SR & (1U << 1))) {}        // Poll until End of Conversion (EOC bit 1) is set
    return ADC1->DR;                          // Reading data register clears EOC flag automatically
}

/**
  * @brief  Transmits a null-terminated ASCII string over USART1.
  */
void USART1_SendString(const char *str) {
    while (*str) {
        while (!(USART1->SR & (1U << 7))) {}  // Wait until Transmit Data Register Empty (TXE bit 7)
        USART1->DR = (*str++ & 0xFF);         // Load next character into data register
    }
}

/**
  * @brief  SysTick Interrupt Service Routine: increments millisecond tick counter.
  */
void SysTick_Handler(void) {
    ms_ticks++;
}

/**
  * @brief  Returns total elapsed milliseconds since system startup.
  */
uint32_t GetTick(void) {
    return ms_ticks;
}

/**
  * @brief  USART1 Interrupt Service Routine: Ingests & executes Bluetooth commands.
  */
void USART1_IRQHandler(void) {
    // Check if Receive Not Empty (RXNE bit 5) triggered the interrupt
    if (USART1->SR & (1U << 5)) {
        bluetooth_rx_byte = (uint8_t)(USART1->DR & 0xFF); // Read received byte

        // Parse Wireless Bluetooth Command Set
        switch (bluetooth_rx_byte) {
            case 'U': case 'u': case '+':     // Step Brightness UP by +5% (+50 counts)
                auto_light_mode = 0;          // Engage manual technician override
                if (pwm_value < 999) pwm_value += 50;
                TIM2->CCR1 = (uint32_t)pwm_value;
                break;

            case 'D': case 'd': case '-':     // Step Brightness DOWN by -5% (-50 counts)
                auto_light_mode = 0;          // Engage manual technician override
                if (pwm_value > 0) pwm_value -= 50;
                TIM2->CCR1 = (uint32_t)pwm_value;
                break;

            case 'A': case 'a':               // Enable Automatic LDR Photocell Mode
                auto_light_mode = 1;
                break;

            case 'M': case 'm':               // Lock to Manual Mode
                auto_light_mode = 0;
                break;

            case '1':                         // Emergency Override: Force 100% Full Illumination
                auto_light_mode = 0;
                pwm_value = 1000;
                TIM2->CCR1 = 999;
                break;

            case '0':                         // Blackout Override: Turn OFF Streetlight (0%)
                auto_light_mode = 0;
                pwm_value = 0;
                TIM2->CCR1 = 0;
                break;

            default:
                break;
        }
    }
}

/* =============================================================================
 * MAIN APPLICATION & NON-BLOCKING BUSINESS LOGIC
 * ============================================================================= */
int main(void) {
    /* Timing tracking variables for non-blocking scheduling */
    uint32_t last_telemetry_tick = 0;
    uint32_t last_button_tick = 0;

    /* Sensor data containers */
    uint32_t adc_temp_raw = 0;
    uint32_t adc_ldr_raw = 0;
    uint32_t temperature_c = 0;
    char tx_buffer[100];

    // Initialize all hardware peripherals & ports
    init_port();

    // Main Cooperative Executive Super-Loop
    while (1) {

        /* -------------------------------------------------------------
         * Task 1: Periodic 500 ms Environmental Sensing & Telemetry
         * ------------------------------------------------------------- */
        if (GetTick() - last_telemetry_tick >= 500) {
            last_telemetry_tick = GetTick();

            // 1. Read LM35 Temperature Sensor on PA1 (ADC Channel 1)
            adc_temp_raw = ADC_ReadChannel(1);

            // 2. Read LDR Ambient Light Sensor on PA2 (ADC Channel 2)
            adc_ldr_raw = ADC_ReadChannel(2);

            // 3. Convert 12-bit ADC code to Temperature in °C:
            //    Formula: Temp = (ADC_RAW * 3.3V) / (4095 * 10mV/°C) = (ADC_RAW * 330) / 4095
            temperature_c = (adc_temp_raw * 330) / 4095;

            // 4. Thermal Hazard Hysteresis Alerting (Red LED on PA3):
            //    Turns ON when road temperature exceeds 40°C, turns OFF when it drops below 37°C
            if (temperature_c > TEMP_HIGH_ALERT) {
                GPIOA->ODR |= (1U << 3);       // Set PA3 HIGH (Hazard Alert LED ON)
            } else if (temperature_c < TEMP_LOW_RESET) {
                GPIOA->ODR &= ~(1U << 3);      // Set PA3 LOW (Hazard Alert LED OFF)
            }

            // 5. Format and broadcast live telemetry packet over Bluetooth (USART1)
            const char *led_status = (GPIOA->ODR & (1U << 3)) ? "ON" : "OFF";
            sprintf(tx_buffer, "Temp:%uC | Warn LED:%s | Lights PWM:%d\r\n",
                    (unsigned int)temperature_c, led_status, pwm_value);
            USART1_SendString(tx_buffer);
        }

        /* -------------------------------------------------------------
         * Task 2: Debounced 200 ms Pushbutton Manual Override
         * ------------------------------------------------------------- */
        if (GetTick() - last_button_tick > 200) {
            // Button UP (PB0 active-LOW): Increase streetlight brightness by +5%
            if (!(GPIOB->IDR & (1U << 0))) {
                auto_light_mode = 0;           // Switch to manual mode
                if (pwm_value < 999) pwm_value += 50;
                TIM2->CCR1 = (uint32_t)pwm_value;
                last_button_tick = GetTick();
            }
            // Button DOWN (PB1 active-LOW): Decrease streetlight brightness by -5%
            else if (!(GPIOB->IDR & (1U << 1))) {
                auto_light_mode = 0;           // Switch to manual mode
                if (pwm_value > 0) pwm_value -= 50;
                TIM2->CCR1 = (uint32_t)pwm_value;
                last_button_tick = GetTick();
            }
        }

        /* -------------------------------------------------------------
         * Task 3: Automatic Optical Dusk/Night Illumination
         * ------------------------------------------------------------- */
        // When darkness is detected (LDR > 2500) and in Auto Mode -> Drive luminaire to 100%
        if (adc_ldr_raw > DARK_THRESHOLD && auto_light_mode == 1) {
            TIM2->CCR1 = 999;
            pwm_value = 1000;
        }
        // When bright daylight returns (LDR < 2000) -> Re-arm automatic photocell mode
        else if (adc_ldr_raw < BRIGHT_THRESHOLD) {
            auto_light_mode = 1;
        }
    }
}
