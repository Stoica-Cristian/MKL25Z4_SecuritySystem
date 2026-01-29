/*
 * timer_driver.h
 *
 * Low-level PIT Timer Driver for 1ms System Tick.
 */

#ifndef TIMER_DRIVER_H_
#define TIMER_DRIVER_H_

#include <stdint.h>

// Initialize PIT for 1ms
void PIT_Init(void);

// Get System Time (ms)
uint32_t GetTick(void);

// Check if time elapsed (True if current - start >= duration)
uint8_t IsTimeout(uint32_t startTick, uint32_t durationMs);

#endif /* TIMER_DRIVER_H_ */
