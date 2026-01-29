#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Initialize Security Logic (Variables, State)
void Security_Init(void);

// Main State Machine Logic (Call Periodically, e.g., in Main Loop)
// Handles Sensor checks, FSM transitions, Alarms, Locks.
void Security_Update(void);

// Password Management API (Door)
bool Security_CheckPassword(char* inputPin);
void Security_SetPassword(const char* newPassword);

// Password Management API (Bluetooth Admin)
bool Security_CheckAdminPassword(char* inputPass);
void Security_SetAdminPassword(const char* newPassword);

#endif // SECURITY_MANAGER_H
