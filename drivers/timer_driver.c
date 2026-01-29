/*
 * timer_driver.c
 *
 * PIT-based System Timer.
 * Provides 1ms Interrupts for System Tick and Keypad Scanning.
 */

#include "timer_driver.h"
#include "MKL25Z4.h"
#include "fsl_clock.h"
#include "keypad_driver.h"
#include "output_mgr.h"

static volatile uint32_t g_systemTick = 0;

void PIT_Init(void) {
    // 1. Enable Clock & Module
    SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
    PIT->MCR = 0x00;

    // 2. Stop Timer & Clear Flags
    PIT->CHANNEL[0].TCTRL = 0;
    PIT->CHANNEL[0].TFLG = PIT_TFLG_TIF_MASK;

    // 3. Load Value for 1ms
    uint32_t busClock = CLOCK_GetFreq(kCLOCK_BusClk);
    if (busClock == 0) busClock = 24000000U; // Fallback
    PIT->CHANNEL[0].LDVAL = (busClock / 1000U) - 1U;

    // 4. Enable Interrupts & Timer
    PIT->CHANNEL[0].TCTRL = PIT_TCTRL_TIE_MASK | PIT_TCTRL_TEN_MASK;
    NVIC_SetPriority(PIT_IRQn, 3);
    NVIC_EnableIRQ(PIT_IRQn);
}

void PIT_IRQHandler(void) {
    if (PIT->CHANNEL[0].TFLG & PIT_TFLG_TIF_MASK) {
        PIT->CHANNEL[0].TFLG = PIT_TFLG_TIF_MASK; // Clear Flag
        g_systemTick++;
        
        Keypad_Tick(); // Critical: Scan Matrix every 1ms
        Outputs_Tick(); // Audio Feedback
    }
}

uint32_t GetTick(void) {
    return g_systemTick;
}

uint8_t IsTimeout(uint32_t startTick, uint32_t durationMs) {
    return ((g_systemTick - startTick) >= durationMs);
}
