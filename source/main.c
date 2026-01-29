/*
 * main.c
 *
 * [SYSTEM ENTRY POINT]
 * Handles Hardware Initialization, Watchdog (COP) Config, and Main Sleep Loop.
 * Delegates Logic to security_manager.c
 */

#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL25Z4.h"
#include "fsl_debug_console.h"

// Hardware Drivers
#include "pir_driver.h"
#include "rfid_driver.h"
#include "servo_driver.h"
#include "keypad_driver.h"
#include "output_mgr.h"
#include "timer_driver.h"
#include "uart_driver.h"
#include "storage_mgr.h"

// Logic Module
#include "security_manager.h"

int main(void) {
    // ============================================================================
    // 1. BOARD & CLOCK INIT
    // ============================================================================
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    #ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    BOARD_InitDebugConsole();
    #endif

    // ============================================================================
    // 2. PERIPHERAL INIT
    // ============================================================================
    
    // PIT (Periodic Interrupt Timer) - Hard Real-Time 1ms Base
    PIT_Init();

    // Hook into UART0 for Admin Testing
    UART_Bluetooth_Init();

    UART_Printf("\r\n[SYSTEM] *** SECURITY SYSTEM BOOT ***\r\n");

    PIR_Init();     
    RC522_Init();
    Servo_Init(); 
    Keypad_Init();
    Outputs_Init();
    
    // Visual/Audio Confirmation: System Alive
    Output_Startup_Sequence();
    
    UART_Printf("[SYSTEM] Peripherals Initialized. Waiting for Logic...\r\n");

    // ============================================================================
    // 3. LOGIC STARTUP
    // ============================================================================
    Storage_Init(); // Load Config from Flash BEFORE Security Logic
    Security_Init();
    
    // ============================================================================
    // 4. WATCHDOG (COP) ENABLE
    // ============================================================================
    // Safety feature: Resets system if code freezes for >1s
    // COPT = 3 (Long Timeout ~1024ms), COPCLKS = 0 (1kHz LPO)
    SIM->COPC = SIM_COPC_COPT(3) | SIM_COPC_COPCLKS(0);

    // ============================================================================
    // SUPER LOOP (Power Optimized)
    // ============================================================================
    while(1) {
        // A. REFRESH WATCHDOG
        // Service sequence: 0x55 then 0xAA (Must happen < 1s)
        SIM->SRVCOP = 0x55;
        SIM->SRVCOP = 0xAA;

        // B. SLEEP 
        // Wait for next Interrupt (PIT 1ms) to save power
        __WFI(); 
        
        // C. BACKGROUND TASKS (Drivers)
        RFID_Tick(); 

        // D. BUSINESS LOGIC (FSM)
        Security_Update();
    }
    return 0;
}
