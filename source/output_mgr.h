/*
 * output_mgr.h
 *
 * Manager for Audio/Visual Feedback (LED & Buzzer).
 */

#ifndef OUTPUT_MGR_H
#define OUTPUT_MGR_H

#include <stdint.h>

// Initialize LED & Buzzer
void Outputs_Init(void);

// LED Control
void LED_Alarm_On(void);
void LED_Alarm_Off(void);
void LED_Alarm_Toggle(void);

// Buzzer Control (Volume 0-100%)
void Buzzer_On(uint16_t pitch, uint8_t volume);
void Buzzer_Off(void);
void Buzzer_Beep(int duration_ms);

// Startup Animation
void Output_Startup_Sequence(void);

// ISR Hook (Called every 1ms)
void Outputs_Tick(void);

#endif // OUTPUT_MGR_H


