# MKL25Z4 Security System

A robust embedded security system built on the **NXP FRDM-MKL25Z4**. Features real-time state management via FSM, secure PIN/RFID authentication, Bluetooth remote administration, and brute-force protection with persistent storage integration.

## Components & Pinout

| Component | Function | Pins (MKL25Z) |
|-----------|----------|---------------|
| **RC522 RFID** | Card Authentication | PTC4-7 (SPI0), PTC0 (RST) |
| **4x4 Keypad** | PIN Entry | PTB8-11 (Rows), PTE2-5 (Cols) |
| **HC-05** | Bluetooth Admin | PTD2 (RX), PTD3 (TX) - UART2 |
| **HC-SR501** | Motion Sensor | PTA5 (GPIO Interrupt) |
| **SG90 Servo** | Locking Mechanism | PTB2 (PWM) |
| **Buzzer** | Alarm/Feedback | PTA12 (PWM) |
| **RGB LED** | Status Indicator | PTB3 (External) |

## Features

- **Dual Authentication**: 4-digit PIN (Keypad) or RFID Card (Mifare 1K).
- **Persistent Storage**: Settings (PIN, Admin Pass, Allowed UIDs) are saved in the microcontroller's internal Flash memory, so they remain after a restart.
- **Alarm Logic**: Includes Entry/Exit delays and a brute-force lockout mechanism (siren triggers after 3 failed attempts).
- **Remote Admin**: Bluetooth terminal interface for managing users and settings.

## Bluetooth Commands

Connect at **9600 baud**. Default Admin Password: `123456`.

*   `LOGIN <pass>` - Login as admin to execute other commands.
*   `NEWPASS <pin>` - Change the user access PIN (Default: `1234`).
*   `ADDID <hex>` - Add a trusted RFID UID (e.g., `ADDID 526CA904`).
*   `DELID <hex>` - Remove a trust RFID UID.
*   `LISTIDS` - Print all authorized UIDs.
*   `ADMINPASS <pass>` - Change the admin password.

## Project Structure

```text
MKL25Z4_SecuritySystem/
├── source/          # Application Logic (FSM, Managers, Drivers)
├── board/           # Pin Mux & Clock Configuration
├── utilities/       # Debug Console & Assert
└── drivers/         # NXP Kinetis SDK Drivers
```

## Default Credentials

| Type | Default Value | Notes |
|------|---------------|-------|
| **Door PIN** | `1234` | Change with `NEWPASS` |
| **Admin Pass** | `123456` | For Bluetooth Login |
| **RFID** | (None) | Register via Admin `ADDID` |

## How to Run

1.  Import folder as **C/C++ Project** in MCUXpresso IDE.
2.  Build and Flash to **FRDM-MKL25Z4**.

