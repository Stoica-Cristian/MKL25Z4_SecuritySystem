/*
 * keypad_driver.c
 *
 * [KEYPAD DRIVER - 4x4 MATRIX]
 * Features: Background Scanning (ISR), Software Debounce (20ms), Buffer Timeout.
 */

#include "keypad_driver.h"
#include "security_manager.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_clock.h"
#include "MKL25Z4.h"
#include "timer_driver.h"
#include "fsl_debug_console.h"
#include "output_mgr.h"
#include "uart_driver.h"
#include <string.h>

// ============================================================================
// CONFIGURATION
// ============================================================================
#define ROW1_PIN 8U
#define ROW2_PIN 9U
#define ROW3_PIN 10U
#define ROW4_PIN 11U

#define COL1_PIN 2U
#define COL2_PIN 3U
#define COL3_PIN 4U
#define COL4_PIN 5U

#define PASS_LEN 4
#define TIMEOUT_MS 5000

const char key_map[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static volatile char g_pressed_key = 0;     // Validated, Debounced Key Event
static volatile char g_raw_key = 0;         // Immediate scan result
static volatile uint8_t g_stable_count = 0; // Debounce Counter

static char kp_buffer[PASS_LEN + 1];
static uint8_t kp_index = 0;
static uint32_t last_key_time_kp = 0;

// ============================================================================
// INIT
// ============================================================================
void Keypad_Init(void) {
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortE);

    // Configure Row Pins (Outputs)
    PORT_SetPinMux(PORTB, ROW1_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTB, ROW2_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTB, ROW3_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTB, ROW4_PIN, kPORT_MuxAsGpio);

    // Set Rows High (Idle)
    GPIOB->PDDR |= (1U << ROW1_PIN) | (1U << ROW2_PIN) | (1U << ROW3_PIN) | (1U << ROW4_PIN);
    GPIOB->PSOR |= (1U << ROW1_PIN) | (1U << ROW2_PIN) | (1U << ROW3_PIN) | (1U << ROW4_PIN);

    // Configure Col Pins (Inputs with Pull-Up)
    PORTE->PCR[COL1_PIN] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTE->PCR[COL2_PIN] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTE->PCR[COL3_PIN] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTE->PCR[COL4_PIN] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    
    // Ensure inputs
    GPIOE->PDDR &= ~((1U << COL1_PIN) | (1U << COL2_PIN) | (1U << COL3_PIN) | (1U << COL4_PIN));
}

// ============================================================================
// SCANNING LOGIC (ISR Context)
// ============================================================================
/* 
 * Called every 1ms by PIT Timer.
 * Scans one row per tick (Rotation). Complete scan = 4ms.
 * Debounce requirement: 20 consecutive stable scans.
 */
void Keypad_Tick(void) {
    static uint8_t current_row = 0;
    static char stable_key_candidate = 0;
    
    // 1. Read Result directly from inputs (cols)
    char detected_char = 0;
    uint32_t col_val = GPIOE->PDIR;
    
    // Check which Column is LOW (Active)
    if (!(col_val & (1U << COL1_PIN))) detected_char = key_map[current_row][0];
    else if (!(col_val & (1U << COL2_PIN))) detected_char = key_map[current_row][1];
    else if (!(col_val & (1U << COL3_PIN))) detected_char = key_map[current_row][2];
    else if (!(col_val & (1U << COL4_PIN))) detected_char = key_map[current_row][3];
    
    // 2. Disable current Row (Set High)
    switch(current_row) {
        case 0: GPIOB->PSOR = (1U << ROW1_PIN); break;
        case 1: GPIOB->PSOR = (1U << ROW2_PIN); break;
        case 2: GPIOB->PSOR = (1U << ROW3_PIN); break;
        case 3: GPIOB->PSOR = (1U << ROW4_PIN); break;
    }
    
    if (detected_char != 0) g_raw_key = detected_char;

    // 3. Process Stability at end of Scan Cycle (Row 3)
    if (current_row == 3) {
        if (g_raw_key != 0 && g_raw_key == stable_key_candidate) {
            g_stable_count++;
        } else {
             stable_key_candidate = g_raw_key;
             g_stable_count = 0;
        }
        
        // Edge Logic: Only trigger EVENT on new stable press (after 20ms)
        static char last_valid_key = 0;
        if (g_stable_count > 20) {
             if (stable_key_candidate != last_valid_key) {
                  g_pressed_key = stable_key_candidate; 
                  last_valid_key = stable_key_candidate;
             }
        } else {
             if (g_raw_key == 0) last_valid_key = 0; // Release
        }
        
        g_raw_key = 0; // Reset for next sweep
    }

    // 4. Move to Next Row & Enable (Set Low)
    current_row++;
    if (current_row > 3) current_row = 0;

    switch(current_row) {
        case 0: GPIOB->PCOR = (1U << ROW1_PIN); break;
        case 1: GPIOB->PCOR = (1U << ROW2_PIN); break;
        case 2: GPIOB->PCOR = (1U << ROW3_PIN); break;
        case 3: GPIOB->PCOR = (1U << ROW4_PIN); break;
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

char Keypad_GetKeyNonBlocking(void) {
    if (g_pressed_key != 0) {
        char k = g_pressed_key;
        g_pressed_key = 0; // Event Consumed
        return k;
    }
    return 0;
}

int Keypad_CheckPassword(void) {
    // 1. Timeout Check: specific for password entry buffer
    if (kp_index > 0 && IsTimeout(last_key_time_kp, TIMEOUT_MS)) {
        kp_index = 0;
        kp_buffer[0] = 0;
        UART_Printf("\r\n[KEYPAD] TIMEOUT. Buffer Cleared.\r\n");
    }

    char key = Keypad_GetKeyNonBlocking();
    if (key == 0) return 0;
    
    last_key_time_kp = GetTick();
    Buzzer_Beep(30); // Tactile Feedback (Short Beep)
    UART_Printf("\rKEY: %c\r\n", key);

    if (key == '#') {
        kp_index = 0; 
        return 2; // Trigger Signal
    }

    // Append to buffer
    if (kp_index < PASS_LEN) {
        kp_buffer[kp_index++] = key;
    }

    // Validate
    if (kp_index == PASS_LEN) {
        kp_buffer[PASS_LEN] = 0;
        kp_index = 0;
        
        UART_Printf("[ACCESS] PIN Submitted: ****\r\n"); // Hide PIN in logs
        if (Security_CheckPassword(kp_buffer)) return 1;
        else return -1;
    }
    return 0;
}
 