/*
 * pir_driver.h
 *
 * Driver for HC-SR501 PIR Motion Sensor.
 */

#ifndef PIR_DRIVER_H
#define PIR_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// Initialize PIR GPIO (PTA5)
void PIR_Init(void);

// Read PIR Status (True = Motion)
bool PIR_Read(void);

// Check Interrupt Flag (True = Motion Started)
bool PIR_CheckTriggered(void);

#endif // PIR_DRIVER_H


