/*
 * storage_mgr.c
 *
 * [PERSISTENT STORAGE MANAGER]
 * Uses Internal Flash (Last Sector) to save/load System Configuration.
 * Addresses: 0x1FC00 on MKL25Z128.
 */

#include "storage_mgr.h"
#include "fsl_flash.h"
#include "fsl_debug_console.h"
#include "MKL25Z4.h"
#include "output_mgr.h"
#include "uart_driver.h"
#include <string.h>

// FLASH Configuration
// KL25Z128 has 128KB Flash. Address range: 0x00000 - 0x1FFFF.
// We use the LAST SECTOR (1KB size) for storage.
#define STORAGE_SECTOR_ADDR   0x1FC00
#define STORAGE_SECTOR_SIZE   1024

static flash_config_t g_flashDriver;
static uint32_t pflashBlockBase = 0;
static uint32_t pflashTotalSize = 0;
static uint32_t pflashSectorSize = 0;

// Internal Cache of Config to avoid reading Flash constantly
static SecurityConfig_t g_cachedConfig;

// ============================================================================
// INTERNAL HELPERS
// ============================================================================
static void Print_Flash_Error(status_t status) {
    if (status != kStatus_FLASH_Success) {
        UART_Printf("[STORAGE] Flash Error Code: %d\r\n", status);
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void Storage_Init(void) {
    // 1. Initialize Flash Driver
    // 1. Initialize Flash Driver
    // For MKL25Z, standard init.
    status_t result = FLASH_Init(&g_flashDriver);
    if (result != kStatus_FLASH_Success) {
        UART_Printf("[STORAGE] Driver Init Failed!\r\n");
        return;
    }

    // 2. Get Flash Properties
    FLASH_GetProperty(&g_flashDriver, kFLASH_PropertyPflashBlockBaseAddr, &pflashBlockBase);
    FLASH_GetProperty(&g_flashDriver, kFLASH_PropertyPflashTotalSize, &pflashTotalSize);
    FLASH_GetProperty(&g_flashDriver, kFLASH_PropertyPflashSectorSize, &pflashSectorSize);

    UART_Printf("[STORAGE] Flash Initialized. Total: %d KB, Sector: %d B\r\n", 
            pflashTotalSize / 1024, pflashSectorSize);
            
    // 3. Load at Startup to populate Cache
    Storage_LoadConfig(&g_cachedConfig);
    UART_Printf("[STORAGE] Config Loaded. PIN: %s\r\n", g_cachedConfig.door_pin);
}

void Storage_LoadConfig(SecurityConfig_t* outConfig) {
    if (outConfig == NULL) return;

    // Direct Memory Read (Memory Mapped)
    SecurityConfig_t* stored = (SecurityConfig_t*)STORAGE_SECTOR_ADDR;
    
    // Check Integrity
    if (stored->magic_header == STORAGE_MAGIC) {
        // Valid Config Found
        memcpy(outConfig, stored, sizeof(SecurityConfig_t));
    } else {
        // Invalid or Fresh Chip -> Load Defaults
        UART_Printf("[STORAGE] No valid config found. Loading Defaults.\r\n");
        strcpy(outConfig->door_pin, "1234");
        strcpy(outConfig->admin_password, "123456");
        memset(outConfig->authorized_uids, 0, sizeof(outConfig->authorized_uids));
        outConfig->magic_header = STORAGE_MAGIC;
        
        // Auto-Save Defaults to initialize sector
        Storage_SaveConfig(outConfig);
    }
}

bool Storage_SaveConfig(const SecurityConfig_t* inConfig) {
    status_t result;
    
    // 1. Update Cache
    memcpy(&g_cachedConfig, inConfig, sizeof(SecurityConfig_t));

    // 2. Critical Section (Disable Interrupts)
    LED_Alarm_On(); // Visual Feedback: Start Write
    __disable_irq();

    // 3. Erase Sector
    // Erase full 1KB sector before writing
    result = FLASH_Erase(&g_flashDriver, STORAGE_SECTOR_ADDR, STORAGE_SECTOR_SIZE, kFLASH_ApiEraseKey);
    if (result != kStatus_FLASH_Success) {
        __enable_irq();
        LED_Alarm_Off(); // Error: Turn Off
        Print_Flash_Error(result);
        return false;
    }

    // 4. Program Data
    // SDK requires Source Array to be uint32_t aligned
    result = FLASH_Program(&g_flashDriver, STORAGE_SECTOR_ADDR, (uint32_t*)inConfig, sizeof(SecurityConfig_t));
    
    __enable_irq();
    LED_Alarm_Off(); // Visual Feedback: End Write

    if (result == kStatus_FLASH_Success) {
        UART_Printf("[STORAGE] Save Success.\r\n");
        return true;
    } else {
         Print_Flash_Error(result);
         return false;
    }
}
// ============================================================================
// HIGH LEVEL MANAGERS
// ============================================================================

bool Storage_UpdatePIN(const char* newPin) {
    if (strlen(newPin) > 4) return false;
    // Update field
    strncpy(g_cachedConfig.door_pin, newPin, 4);
    g_cachedConfig.door_pin[4] = 0; // Null Terminate
    return Storage_SaveConfig(&g_cachedConfig);
}

bool Storage_UpdateAdminPass(const char* newPass) {
    if (strlen(newPass) > 9) return false; // Limit to 9 chars + null
    // Update field
    strncpy(g_cachedConfig.admin_password, newPass, 9);
    g_cachedConfig.admin_password[9] = 0; // Null Terminate
    return Storage_SaveConfig(&g_cachedConfig);
}

bool Storage_AddRFID(uint32_t uid) {
    if (uid == 0) return false;

    // Check if already exists
    for (int i = 0; i < MAX_STORED_IDS; i++) {
        if (g_cachedConfig.authorized_uids[i] == uid) {
             UART_Printf("[STORAGE] UID %X already exists.\r\n", uid);
             return false; // Fail duplicate
        }
    }

    // Find Empty Slot
    for (int i = 0; i < MAX_STORED_IDS; i++) {
        if (g_cachedConfig.authorized_uids[i] == 0) {
            g_cachedConfig.authorized_uids[i] = uid;
            UART_Printf("[STORAGE] UID %x added at slot %d.\r\n", uid, i);
            return Storage_SaveConfig(&g_cachedConfig);
        }
    }

    UART_Printf("[STORAGE] Memory Full! Delete an old ID first.\r\n");
    return false;
}

bool Storage_RemoveRFID(uint32_t uid) {
    bool found = false;
    for (int i = 0; i < MAX_STORED_IDS; i++) {
        if (g_cachedConfig.authorized_uids[i] == uid) {
            g_cachedConfig.authorized_uids[i] = 0; // Clear
            found = true;
        }
    }
    
    if (found) {
        UART_Printf("[STORAGE] UID %x removed.\r\n", uid);
        return Storage_SaveConfig(&g_cachedConfig); 
    } else {
        UART_Printf("[STORAGE] UID %x not found.\r\n", uid);
        return false;
    }
}

void Storage_FactoryReset(void) {
    SecurityConfig_t def;
    strcpy(def.door_pin, "1234");
    strcpy(def.admin_password, "123456");
    memset(def.authorized_uids, 0, sizeof(def.authorized_uids));

    def.magic_header = STORAGE_MAGIC;
    Storage_SaveConfig(&def);
    UART_Printf("[STORAGE] Factory Reset Complete.\r\n");
}

void Storage_ListRFIDs(void) {
    UART_Printf("[STORAGE] Authorized UIDs:\r\n");
    int count = 0;
    for (int i = 0; i < MAX_STORED_IDS; i++) {
        if (g_cachedConfig.authorized_uids[i] != 0) {
            UART_Printf("  [%d]: %X\r\n", i + 1, g_cachedConfig.authorized_uids[i]);
            count++;
        }
    }
    if (count == 0) UART_Printf("  (None)\r\n");
}

SecurityConfig_t* Storage_GetConfig(void) {
    return &g_cachedConfig;
}
