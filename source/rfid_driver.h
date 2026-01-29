/*
 * rfid_driver.h
 *
 * Driver for RC522 RFID Module.
 */

#ifndef RFID_DRIVER_H
#define RFID_DRIVER_H

#include <stdint.h>

// Initialize RFID (SPI, Pins, Chip)
void RC522_Init(void);

// Non-blocking Tick for FSM (Call every loop)
void RFID_Tick(void);

// Non-blocking Check. Returns: 0(None), 1(Card Detected)
// Clears state after reading result.
int RFID_GetLastScanResult(void);
// Returns status: 1 (New Card), 0 (None)
int RFID_CheckScan(void);

// Returns the last scanned 4-byte UID (0x11223344)
uint32_t RFID_GetLastUID(void); // Alias

uint8_t ReadReg(uint8_t addr);

#define PICC_REQIDL    0x26
#define PICC_REQALL    0x52

#endif // RFID_DRIVER_H


