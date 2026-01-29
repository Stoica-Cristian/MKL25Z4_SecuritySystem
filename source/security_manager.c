/*
 * security_manager.c
 *
 * [CORE BUSINESS LOGIC]
 * Handles State Machine (Armed, Disarmed, Triggered), Auth Validation,
 * and Sensor Monitoring.
 */

#include "security_manager.h"
#include "fsl_debug_console.h"
#include "uart_driver.h" // For Bluetooth Logs
#include "MKL25Z4.h"
#include <ctype.h>

// Drivers
#include "pir_driver.h"
#include "rfid_driver.h"
#include "servo_driver.h"
#include "keypad_driver.h"
#include "output_mgr.h"
#include "timer_driver.h"
#include "storage_mgr.h"

// ============================================================================
// DEFINITIONS & CONSTANTS
// ============================================================================
typedef enum {
    STATE_ARMED,        // System Active, Monitoring Sensors
    STATE_ENTRY_DELAY,  // Grace Period (5s) for Auth
    STATE_EXIT_DELAY,   // Grace Period (10s) to leave
    STATE_TRIGGERED,    // Alarm On!
    STATE_DISARMED,     // System Off, Door Access Allowed
    STATE_LOCKED,       // Lockout (Brute Force Protection)
} SystemState_t;

#define ENTRY_DELAY_MS      5000U   // Time allowed to enter PIN after motion detected
#define DISARM_WINDOW_MS    5000U   // Duration the door remains unlocked
#define ALARM_BLINK_MS      500U    // LED toggle speed when alarm is active (0.5s)
#define STARTUP_DELAY_MS    2000U   // Sensor stabilization time at boot
#define AUTO_LOCK_DELAY_MS  1000U   // Wait time before re-locking the door
#define EXIT_DELAY_MS       10000U  // Grace period to leave the house after arming
#define BRUTE_FORCE_LIMIT   3       // Max invalid attempts before system lockout
#define LOCKOUT_TIME_MS     10000U  // Duration of lockout penalty (10s)
#define INITIAL_VOLUME      10      // Starting buzzer PWM duty cycle (%)
#define MAX_VOLUME          50      // Max buzzer volume during panic (%)

#define AUTH_VALID          1       // Credential accepted
#define AUTH_INVALID       -1       // Credential rejected
#define AUTH_NONE           0       // No credential presented

// ============================================================================
// STATE VARIABLES
// ============================================================================
static SystemState_t currentState = STATE_TRIGGERED;
static uint32_t stateEntryTime = 0;   
static uint32_t lastAlarmToggle = 0; 
static int alarmVolume = INITIAL_VOLUME;
static uint8_t failedAttempts = 0;

static bool doorUnlockedMsg = false;
static bool waitingForAutoLock = false;

// Persistent Configuration Copy
// Persistent Configuration Pointer
// static SecurityConfig_t g_sysConfig;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

/* Checks both Keypad and RFID for valid credentials */
static int Check_Auth(void) {
    // 1. Keypad Check
    int kp = Keypad_CheckPassword();
    
    // 2. RFID Check (Dynamic from Flash)
    int rf_status = RFID_CheckScan(); // 1=Card, 2=Tag (Legacy), -1=Invalid (Not used), 0=None
    int rf_auth = AUTH_NONE;
    
    if (rf_status > 0) { // Card Detected
        uint32_t scannedUid = RFID_GetLastUID();
        
        // Iterate through authorized list
        bool found = false;
        SecurityConfig_t* liveConfig = Storage_GetConfig();
        for (int i = 0; i < MAX_STORED_IDS; i++) {
             if (liveConfig->authorized_uids[i] != 0 && liveConfig->authorized_uids[i] == scannedUid) {
                 found = true;
                 break;
             }
        }
        
        if (found) {
             UART_Printf("[ACCESS] RFID Authorized (UID: %x)\r\n", scannedUid);
             rf_auth = AUTH_VALID;
        } else {
             UART_Printf("[ACCESS] RFID DENIED (UID: %x)\r\n", scannedUid);
             rf_auth = AUTH_INVALID;
        }
    }
    
    if (kp == 1 || rf_auth == AUTH_VALID) return AUTH_VALID;
    if (kp == -1 || rf_auth == AUTH_INVALID) return AUTH_INVALID;
    return AUTH_NONE;
}

/* Manages Brute Force logic */
static void Check_Brute_Force(void) {
    failedAttempts++;
    UART_Printf("\r\n[SECURITY] Invalid Auth! Attempts: %d/%d\r\n", failedAttempts, BRUTE_FORCE_LIMIT);
    
    if (failedAttempts >= BRUTE_FORCE_LIMIT) {
        UART_Printf("\r\n[SECURITY] BRUTE FORCE DETECTED! SYSTEM LOCKED.\r\n");
        currentState = STATE_LOCKED;
        stateEntryTime = GetTick();
        
        // Panic Mode (Siren)
        alarmVolume = MAX_VOLUME;
        Buzzer_On(2000, alarmVolume); // Start High Pitch
        LED_Alarm_On(); 
    }
}

// ============================================================================

// ======================================
// PUBLIC API
// ======================================
void Security_Init(void) {
    // Clear Sensors before Arming
    PIR_CheckTriggered();       // Clear stale motion
    RFID_GetLastScanResult();   // Clear stale cards
    Keypad_GetKeyNonBlocking(); // Clear stale keys
    
    currentState = STATE_ARMED; 
    Servo_Close(); 
    stateEntryTime = GetTick(); 
    UART_Printf("Security Manager Initialized. State: ARMED\r\n");
}

bool Security_CheckPassword(char* inputPin) {
    if (strcmp(inputPin, Storage_GetConfig()->door_pin) == 0) {
        UART_Printf("[ACCESS] Keypad PIN Accepted.\r\n");
        return true;
    }
    UART_Printf("[ACCESS] Keypad PIN Rejected.\r\n");
    return false;
}

void Security_SetPassword(const char* newPassword) {
    if (newPassword == NULL) return;
    
    // 1. Length Check
    size_t len = strlen(newPassword);
    if (len != 4) {
        UART_Printf("\r\n[ADMIN ] ERR: PIN must be EXACTLY 4 characters.\r\n");
        return;
    }

    // 2. Alphanumeric Check (4x4 Keypad: 0-9, A, B, C, D, *, #)
    for (size_t i = 0; i < len; i++) {
        char c = newPassword[i];
        bool isDigit = isdigit((unsigned char)c);
        bool isAlpha = (c >= 'A' && c <= 'D');
        bool isSpecial = (c == '*' || c == '#');
        
        if (!isDigit && !isAlpha && !isSpecial) {
            UART_Printf("\r\n[ADMIN ] ERR: PIN Invalid. Use 0-9, A-D, *, #\r\n");
            return;
        }
    }

    // 3. Save
    if (Storage_UpdatePIN(newPassword)) {
         UART_Printf("\r\n[ADMIN ] Password Updated & Saved to Flash.\r\n");
    } else {
         UART_Printf("\r\n[ADMIN ] ERR: Flash Save Failed.\r\n");
    }
}

bool Security_CheckAdminPassword(char* inputPass) {
    return (strcmp(inputPass, Storage_GetConfig()->admin_password) == 0);
}

void Security_SetAdminPassword(const char* newPassword) {
    if (newPassword == NULL) return;
    
    // 1. Length Check (Max 9 chars for bluetooth pass)
    size_t len = strlen(newPassword);
    if (len < 1 || len > 9) {
        UART_Printf("\r\n[ADMIN ] ERR: Pass must be 1-9 chars.\r\n");
        return;
    }

    // 2. Save
    if (Storage_UpdateAdminPass(newPassword)) {
         UART_Printf("\r\n[ADMIN ] Admin Password Updated & Saved.\r\n");
    } else {
         UART_Printf("\r\n[ADMIN ] ERR: Flash Save Failed.\r\n");
    }
}

/* Main State Machine Loop - Called periodically from Main */
void Security_Update(void) {
    if (GetTick() < STARTUP_DELAY_MS) return;

    switch(currentState) {
        
        // --- ARMED STATE: Monitor Sensors ---
        case STATE_ARMED:
            {
                int kp = Keypad_CheckPassword();
                int rf = RFID_CheckScan();
                int rf_auth = AUTH_NONE;

                // Validate RFID immediately if present
                if (rf > 0) {
                    uint32_t uid = RFID_GetLastUID();
                    bool found = false;
                    SecurityConfig_t* liveConfig = Storage_GetConfig();
                    
                    for (int i = 0; i < MAX_STORED_IDS; i++) {
                         if (liveConfig->authorized_uids[i] != 0 && liveConfig->authorized_uids[i] == uid) {
                             found = true;
                             break;
                         }
                    }
                    if (found) {
                        UART_Printf("[ACCESS] RFID Authorized (UID: %x)\r\n", uid);
                        rf_auth = AUTH_VALID;
                    } else {
                        UART_Printf("[ACCESS] RFID DENIED (UID: %x)\r\n", uid);
                        rf_auth = AUTH_INVALID;
                    }
                }

                // 1. Check Explicit Auth (User Action)
                if (kp == 1 || rf_auth == AUTH_VALID) {
                    UART_Printf("\r\n[ACCESS] AUTHORIZED! Unlocking Door directly...\r\n");
                    Buzzer_Beep(200); 
                    currentState = STATE_DISARMED;
                    stateEntryTime = GetTick();
                    doorUnlockedMsg = false;
                    waitingForAutoLock = false;
                    failedAttempts = 0;
                }
                // 2. Check Invalid Auth
                else if (kp == -1 || rf_auth == AUTH_INVALID) {
                     Check_Brute_Force();
                }
                // 3. Check Passive Intrusion (PIR) or Wakeup
                else if (PIR_CheckTriggered() || kp == 2) {
                    UART_Printf("\r\n[ALARM ] MOTION DETECTED! Entry Delay Started (5s)...\r\n");
                    Servo_Close();
                    currentState = STATE_ENTRY_DELAY;
                    stateEntryTime = GetTick();
                }
            }
            break;

        // --- ENTRY DELAY: 5s Grace Period ---
        case STATE_ENTRY_DELAY:
            if (IsTimeout(stateEntryTime, ENTRY_DELAY_MS)) {
                UART_Printf("\r\n[ALARM ] ENTRY TIMEOUT! ALARM TRIGGERED!\r\n");
                alarmVolume = INITIAL_VOLUME; 
                currentState = STATE_TRIGGERED;
                lastAlarmToggle = GetTick();
            }
            
            int authStatus = Check_Auth();
            if (authStatus == AUTH_VALID) {
                UART_Printf("\r\n[ACCESS] AUTHORIZED.\r\n");
                Buzzer_Beep(200); // Success Chime
                currentState = STATE_DISARMED;
                stateEntryTime = GetTick();
                doorUnlockedMsg = false; 
                waitingForAutoLock = false; 
                failedAttempts = 0; 
            } 
            else if (authStatus == AUTH_INVALID) {
                 UART_Printf("\r\n[ACCESS] DENIED! Retry...\r\n");
                 Buzzer_Beep(800); // Error Buzz
                 Check_Brute_Force();
            }
            break;

        // --- ALARM TRIGGERED: Siren Active ---
        case STATE_TRIGGERED:
            if (IsTimeout(lastAlarmToggle, ALARM_BLINK_MS)) {
                lastAlarmToggle = GetTick(); 
                static int toggle = 0;
                toggle = !toggle;
                if (toggle) { Buzzer_On(1000, alarmVolume); LED_Alarm_On(); } 
                else        { Buzzer_On(500, alarmVolume);  LED_Alarm_Off(); }
            }
            
            authStatus = Check_Auth();
            if (authStatus == AUTH_VALID) {
                UART_Printf("\r\n[ACCESS] AUTHORIZED! Silencing Alarm...\r\n");
                Buzzer_Off(); LED_Alarm_Off();
                Buzzer_Beep(200); // Success Chime (overrides Off briefly)
                
                currentState = STATE_DISARMED;
                stateEntryTime = GetTick();
                doorUnlockedMsg = false;
                waitingForAutoLock = false;
                failedAttempts = 0; 
            } 
            else if (authStatus == AUTH_INVALID) {
                UART_Printf("[ACCESS] DENIED! Volume UP.\r\n");
                alarmVolume += 10;
                if (alarmVolume > MAX_VOLUME) alarmVolume = MAX_VOLUME;
                Check_Brute_Force();
            }
            break;
            
        // --- LOCKED: Brute Force Penalty ---
        case STATE_LOCKED:
            {
                if (IsTimeout(lastAlarmToggle, 100)) { 
                     lastAlarmToggle = GetTick();
                     static int toggle = 0;
                     toggle = !toggle;
                     if (toggle) {
                         LED_Alarm_On();
                         Buzzer_On(2500, alarmVolume); // High Pitch
                     } else {
                         LED_Alarm_Off();
                         Buzzer_On(1500, alarmVolume); // Low Pitch (Siren Effect)
                     }
                }
                
                // Flush Inputs during lock
                RFID_GetLastScanResult(); 
                Keypad_GetKeyNonBlocking();

                if (IsTimeout(stateEntryTime, LOCKOUT_TIME_MS)) {
                     UART_Printf("\r\n[ALARM ] LOCKOUT EXPIRED. ALARM ACTIVE! Auth Required.\r\n");
                     
                     // Transition to Triggered to ensure alarm sounds
                     currentState = STATE_TRIGGERED; 
                     lastAlarmToggle = GetTick();
                     failedAttempts = 0; 

                     RFID_GetLastScanResult();
                     Keypad_GetKeyNonBlocking();
                }
            }
            break;
            
        // --- EXIT DELAY: 10s to leave ---
        case STATE_EXIT_DELAY:
            {
                if (IsTimeout(lastAlarmToggle, 1000)) { 
                     lastAlarmToggle = GetTick();
                     static int toggle = 0;
                     toggle = !toggle;
                     if (toggle) LED_Alarm_On(); else LED_Alarm_Off();
                }

                if (IsTimeout(stateEntryTime, EXIT_DELAY_MS)) {
                     UART_Printf("[SYSTEM] System ARMED. Monitoring Active.\r\n");
                     currentState = STATE_ARMED;
                     LED_Alarm_Off();
                     
                     // Clear PIR buffer
                     PIR_CheckTriggered();
                     RFID_GetLastScanResult(); 
                     Keypad_GetKeyNonBlocking();
                }
            }
            break;

        // --- DISARMED: Door Access ---
        case STATE_DISARMED:
            {
                uint32_t elapsed = GetTick() - stateEntryTime;
                
                // 1. Unlock Phase
                if (elapsed < DISARM_WINDOW_MS) {
                    if (!doorUnlockedMsg) {
                        Servo_Open();
                        UART_Printf("[SYSTEM] Door UNLOCKED. Closing in 5s...\r\n");
                        doorUnlockedMsg = true;
                    }
                } 
                // 2. Auto-Lock Phase
                else {
                    if (!waitingForAutoLock) {
                        UART_Printf("[SYSTEM] Auto-Locking...\r\n");
                        Servo_Close();
                        stateEntryTime = GetTick();
                        waitingForAutoLock = true;
                    }
                    
                    if (waitingForAutoLock) {
                         // Start Exit Delay after lock
                         if (IsTimeout(stateEntryTime, AUTO_LOCK_DELAY_MS)) {
                             UART_Printf("[SYSTEM] Exit Delay Started (10s). Leaving...\r\n");
                             currentState = STATE_EXIT_DELAY;
                             stateEntryTime = GetTick(); 
                             waitingForAutoLock = false;
                         }
                    }
                }
            }
            break;
    }
}
