#ifndef STORAGE_MGR_H
#define STORAGE_MGR_H

#include <stdint.h>
#include <stdbool.h>

// Max number of stored RFIDs
#define MAX_STORED_IDS 50

// Magic Header to validate Flash Content
#define STORAGE_MAGIC 0xA5A5A5A7

// Persistent Configuration Structure
typedef struct {
    char door_pin[5];              // 4 chars + Null (e.g., "1234")
    char admin_password[10];        // Bluetooth Login Password (e.g., "123456")
    uint32_t authorized_uids[MAX_STORED_IDS]; // List of UIDs (0 = Empty)
    uint32_t magic_header;          // Integrity Check
} SecurityConfig_t;

// API
void Storage_Init(void);
void Storage_LoadConfig(SecurityConfig_t* outConfig);
bool Storage_SaveConfig(const SecurityConfig_t* inConfig);

// Helpers
bool Storage_UpdatePIN(const char* newPin);
bool Storage_UpdateAdminPass(const char* newPass);
bool Storage_AddRFID(uint32_t uid);
bool Storage_RemoveRFID(uint32_t uid);
void Storage_FactoryReset(void);
void Storage_ListRFIDs(void);
SecurityConfig_t* Storage_GetConfig(void);

#endif // STORAGE_MGR_H
