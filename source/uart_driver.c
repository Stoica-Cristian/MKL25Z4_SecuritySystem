/*
 * uart_driver.c
 *
 * [BLUETOOTH DRIVER]
 * Driver for HC-05 Module using UART2 (Interrupt-based).
 */

#include "uart_driver.h"
#include "admin_mgr.h"
#include "fsl_uart.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "MKL25Z4.h"
#include <stdio.h>
#include <stdarg.h>

#define TARGET_UART UART2
#define TARGET_IRQ  UART2_IRQn

#define RX_BUFFER_SIZE 64

static char rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_index = 0;

void UART_Printf(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    // Send 
    UART_WriteBlocking(TARGET_UART, (uint8_t*)buf, strlen(buf));
}

void UART_Bluetooth_Init(void) {
    // 1. Enable Clocks
    CLOCK_EnableClock(kCLOCK_PortD);
    CLOCK_EnableClock(kCLOCK_Uart2);

    // 2. Configure Pins: PTD2=RX, PTD3=TX (Alt 3 for UART2)
    PORT_SetPinMux(PORTD, 2U, kPORT_MuxAlt3); 
    PORT_SetPinMux(PORTD, 3U, kPORT_MuxAlt3);

    // 3. Configure UART2 for HC-05 (9600 Baud)
    uart_config_t config;
    UART_GetDefaultConfig(&config);
    config.baudRate_Bps = 9600; 
    config.enableTx = true;
    config.enableRx = true;
    
    // Initialize using Bus Clock
    UART_Init(TARGET_UART, &config, CLOCK_GetFreq(kCLOCK_BusClk));

    // 4. Enable RX Interrupt
    UART_EnableInterrupts(TARGET_UART, kUART_RxDataRegFullInterruptEnable);
    EnableIRQ(TARGET_IRQ);
}

// Interrupt Handler for UART2
void UART2_IRQHandler(void) {
    uint8_t data;
    uint32_t flags = UART_GetStatusFlags(TARGET_UART);

    // Check if RX Full
    if ((flags & kUART_RxDataRegFullFlag) && !(flags & kUART_FramingErrorFlag)) {
        data = UART_ReadByte(TARGET_UART);
        
        // Echo back to Phone (Optional, helps verify connection)
        UART_WriteByte(TARGET_UART, data); 

        // Handle Backspace/Delete
        if (data == 0x08 || data == 0x7F) {
            if (rx_index > 0) rx_index--;
            return;
        }

        // Handle Enter (\r or \n)
        if (data == '\r' || data == '\n') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = 0; // Null terminate
                Admin_ProcessCommand(rx_buffer); // Process Loop
                rx_index = 0; // Reset
            }
        } 
        else {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = (char)data;
            } else {
                rx_index = 0; // Overflow protection
            }
        }
    }
    
    // Clear functional errors (OR, NF, FE, PF)
    if (flags & (kUART_FramingErrorFlag | kUART_RxOverrunFlag | kUART_NoiseErrorFlag | kUART_ParityErrorFlag)) {
         // Clear all error flags
         UART_ClearStatusFlags(TARGET_UART, kUART_FramingErrorFlag | kUART_RxOverrunFlag | kUART_NoiseErrorFlag | kUART_ParityErrorFlag);
    }
}
