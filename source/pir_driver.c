/*
 * pir_driver.c
 *
 * [MOTION SENSOR DRIVER - HC-SR501]
 * Logic: Interrupt on Rising Edge (Movement Detected).
 */

#include "pir_driver.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_clock.h"
#include "MKL25Z4.h"

// Configuration: PTA5 (Input)
#define PIR_GPIO      GPIOA
#define PIR_PORT      PORTA
#define PIR_PIN       5U

static volatile bool g_pirDetected = false;

void PIR_Init(void) {
    CLOCK_EnableClock(kCLOCK_PortA);

    port_pin_config_t pir_port_options = {0};
    pir_port_options.pullSelect = kPORT_PullDown; // Internal weak Pull-Down
    pir_port_options.slewRate = kPORT_SlowSlewRate;
    pir_port_options.mux = kPORT_MuxAsGpio;

    // Configure Interrupt for Rising Edge (Motion started)
    PORT_SetPinConfig(PIR_PORT, PIR_PIN, &pir_port_options);
    PORT_SetPinInterruptConfig(PIR_PORT, PIR_PIN, kPORT_InterruptRisingEdge);

    gpio_pin_config_t pir_config = {
        kGPIO_DigitalInput,
        0,
    };
    GPIO_PinInit(PIR_GPIO, PIR_PIN, &pir_config);
    
    // Enable IRQ
    NVIC_SetPriority(PORTA_IRQn, 3);
    EnableIRQ(PORTA_IRQn);
}

bool PIR_Read(void) {
    // Read Pin. If 1 -> Motion Detected.
    return (GPIO_ReadPinInput(PIR_GPIO, PIR_PIN) == 1U);
}

bool PIR_CheckTriggered(void) {
    if (g_pirDetected) {
        g_pirDetected = false; // Clear on read
        return true;
    }
    return false;
}

// ISR Handler name predefined in startup code
void PORTA_IRQHandler(void) {
    // Check if PTA5 caused the interrupt
    if (PORT_GetPinsInterruptFlags(PIR_PORT) & (1U << PIR_PIN)) {
        // Clear Flag
        PORT_ClearPinsInterruptFlags(PIR_PORT, (1U << PIR_PIN));
        
        // Set Logic Flag
        g_pirDetected = true;
    }
}
