/*
 * servo_driver.h
 *
 * Driver for SG90 Servo (PWM Control).
 */

#ifndef SERVO_DRIVER_H
#define SERVO_DRIVER_H

#include <stdint.h>

// Initialize Servo PWM (TPM2 CH1, 50Hz)
void Servo_Init(void);

// Set Duty Cycle % (2-12%)
void Servo_SetDuty(uint8_t dutyCyclePercent);

// Convenience Helpers
void Servo_Open(void);
void Servo_Close(void);

#endif // SERVO_DRIVER_H


