#ifndef KEYPAD_DRIVER_H
#define KEYPAD_DRIVER_H

#include <stdint.h>

// Initialize Keypad (Rows=PTB8-11 Out, Cols=PTE2-5 In PullUp)
void Keypad_Init(void);

// Scan for key press. Returns char ('0'-'9', 'A'-'D', '*', '#') or 0 if none.
char Keypad_Scan(void);

// Blocking wait for any key press
char Keypad_WaitKey(void);

// Non-blocking password check.
// Returns: 0 (Entering), 1 (Valid), -1 (Invalid)
int Keypad_CheckPassword(void);

// Tick function called from Timer ISR (e.g. 1ms)
void Keypad_Tick(void);

// Non-blocking Get Key. Returns key char or 0 if none.
char Keypad_GetKeyNonBlocking(void);

#endif // KEYPAD_DRIVER_H
