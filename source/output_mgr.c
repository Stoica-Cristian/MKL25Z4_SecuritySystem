/*
 * output_mgr.c
 *
 * [OUTPUT MANAGER]
 * Controls:
 * 1. RGB LED (System Status)
 * 2. Buzzer (PWM Audio, Non-Blocking)
 */

#include "output_mgr.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "MKL25Z4.h"

// ===================================
// LED Conf (PTB3)
// ===================================
#define LED_GPIO    GPIOB
#define LED_PORT    PORTB
#define LED_PIN     3U

// ===================================
// Buzzer Conf (PTA12 - TPM1_CH0)
// ===================================
#define BUZZER_PORT PORTA
#define BUZZER_PIN  12U

static void delay_ms_sw(volatile uint32_t ms) {
    volatile uint32_t i;
    while(ms--) {
        for(i=0; i<4000; i++) __asm("NOP");
    }
}

void Outputs_Init(void) {
    // ------------------------------------------------------------------------
    // 1. LED Init (PTB3 - GPIO)
    // ------------------------------------------------------------------------
    CLOCK_EnableClock(kCLOCK_PortB);
    PORT_SetPinMux(LED_PORT, LED_PIN, kPORT_MuxAsGpio);
    
    gpio_pin_config_t led_config = { kGPIO_DigitalOutput, 1 }; 
    led_config.outputLogic = 0;
    
    GPIO_PinInit(LED_GPIO, LED_PIN, &led_config);

    // ------------------------------------------------------------------------
    // 2. Buzzer Init (PTA12 - TPM1_CH0)
    // ------------------------------------------------------------------------
    // Use TPM1 to avoid resource conflict with Servo (TPM2)
    
    CLOCK_EnableClock(kCLOCK_PortA);
    PORT_SetPinMux(BUZZER_PORT, BUZZER_PIN, kPORT_MuxAlt3); // TPM1_CH0
    
    // Enable TPM1 Clock
    CLOCK_EnableClock(kCLOCK_Tpm1);
    
    // Select Clock Source for TPM (Matches System/Servo)
    CLOCK_SetTpmClock(1U); 

    // Stop TPM1
    TPM1->SC = 0;
    
    // Config for Audio (Prescaler 128)
    // 48MHz / 128 = 375kHz ticks
    TPM1->SC |= TPM_SC_PS(7); 
    
    // Enable CH0 (PTA12) for Edge Aligned PWM
    TPM1->CONTROLS[0].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;
    
    // Start Timer
    TPM1->SC |= TPM_SC_CMOD(1);
}

void LED_Alarm_On(void) {
    GPIO_SetPinsOutput(LED_GPIO, 1U << LED_PIN);
}

void LED_Alarm_Off(void) {
    GPIO_ClearPinsOutput(LED_GPIO, 1U << LED_PIN);
}

void LED_Alarm_Toggle(void) {
    GPIO_TogglePinsOutput(LED_GPIO, 1U << LED_PIN);
}

// ----------------------------------------------------------------------------
// Buzzer Logic (TPM1)
// ----------------------------------------------------------------------------
void Buzzer_On(uint16_t pitch, uint8_t volume) {
    // Limit Volume
    if (volume > 50) volume = 50; 
    if (volume < 1) volume = 1;

    TPM1->MOD = pitch; 
    // Calculate Duty: (MOD * volume) / 100
    // Use Channel 0 for PTA12
    TPM1->CONTROLS[0].CnV = (pitch * volume) / 100;
}

void Buzzer_Off(void) {
    TPM1->CONTROLS[0].CnV = 0;
}

// Non-Blocking State
static volatile uint32_t g_buzzerTimeout = 0;

void Buzzer_Beep(int duration_ms) {
    Buzzer_On(1000, 50);
    g_buzzerTimeout = duration_ms; // Set Timer (Non-blocking)
}

// Called from PIT_IRQHandler (1ms)
void Outputs_Tick(void) {
    if (g_buzzerTimeout > 0) {
        g_buzzerTimeout--;
        if (g_buzzerTimeout == 0) {
            Buzzer_Off();
        }
    }
}

void Output_Startup_Sequence(void) {
    // 3 Quick Beeps + Flashes
    for(int i=0; i<3; i++) {
        LED_Alarm_On();
        Buzzer_On(2000, 20); // High pitch, low vol
        delay_ms_sw(100);
        
        LED_Alarm_Off();
        Buzzer_Off();
        delay_ms_sw(100);
    }
}
