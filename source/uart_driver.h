#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>

// Initialize UART (Enable Interrupts)
void UART_Bluetooth_Init(void);

// Helper to Tick input if not fully IRQ based (Optional)

// Send Formatted String to Bluetooth (PRINTF replacement)
void UART_Printf(const char* fmt, ...);

#endif // UART_DRIVER_H
