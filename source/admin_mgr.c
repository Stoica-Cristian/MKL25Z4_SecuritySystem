/*
 * admin_mgr.c
 *
 * [ADMINISTRATION MANAGER]
 * Handles Bluetooth Command Parsing (CLI).
 * Commands: LOGIN, NEWPASS, ADDID, etc.
 */

#include "admin_mgr.h"
#include "security_manager.h"
#include "storage_mgr.h"
#include "fsl_debug_console.h"
#include "uart_driver.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Command Buffer definitions
#define CMD_LOGIN    "LOGIN"
#define CMD_NEWPASS  "NEWPASS"
#define CMD_UNLOCK   "UNLOCK"
#define CMD_STATUS   "STATUS"
#define CMD_ADDID    "ADDID"
#define CMD_DELID    "DELID"
#define CMD_ADMINPASS "ADMINPASS"
#define CMD_LISTIDS   "LISTIDS"

// Temporary Admin Session
static bool g_admin_logged_in = false;

void Admin_ProcessCommand(char* cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;

    UART_Printf("\r\n[ADMIN_RX] Cmd: %s\r\n", cmd);

    // 1. Process LOGIN
    if (strncmp(cmd, CMD_LOGIN, 5) == 0) {
        // Format: LOGIN <PASS>
        char* token = strtok(cmd, " ");
        token = strtok(NULL, " "); 
        
        if (token != NULL) {
            // Dynamic Password Check
            if (Security_CheckAdminPassword(token)) {
                g_admin_logged_in = true;
                UART_Printf("[ADMIN ] LOGIN SUCCESS. Session Active.\r\n");
            } else {
                UART_Printf("[ADMIN ] LOGIN FAILED. Invalid Credentials.\r\n");
            }
        } else {
             UART_Printf("[ADMIN ] ERR: Missing Password.\r\n");
        }
        return;
    }

    // 2. Security Check (Session Required)
    if (!g_admin_logged_in) {
        UART_Printf("[ADMIN ] ACCESS DENIED. Please LOGIN first.\r\n");
        return;
    }

    // 3. Command Dispatch
    if (strncmp(cmd, CMD_NEWPASS, 7) == 0) {
        char* token = strtok(cmd, " ");
        token = strtok(NULL, " ");
        
        if (token != NULL) {
            Security_SetPassword(token);
            UART_Printf("[ADMIN ] User PIN updated remotely.\r\n");
        } else {
            UART_Printf("[ADMIN ] ERR: Missing PIN.\r\n");
        }
    }
    else if (strncmp(cmd, CMD_UNLOCK, 6) == 0) {
        UART_Printf("[ADMIN ] Feature not implemented. Use RFID/Keypad.\r\n");
    }
    else if (strncmp(cmd, CMD_STATUS, 6) == 0) {
        UART_Printf("[ADMIN ] System Active. Logged In.\r\n");
    }
    // 6. ADDID <HEX>
    else if (strncmp(cmd, CMD_ADDID, 5) == 0) {
        char* token = strtok(cmd, " ");
        token = strtok(NULL, ""); // Get Remainder
        if (token != NULL) {
             // Clean Spaces
             char cleanHex[32];
             int idx = 0;
             for(int i=0; token[i] != 0 && idx < 16; i++) {
                 char c = token[i];
                 if(c != ' ' && c != '\n' && c != '\r') cleanHex[idx++] = c;
             }
             cleanHex[idx] = 0;
            uint32_t uid = (uint32_t)strtoul(cleanHex, NULL, 16);
            if (uid != 0) {
                if (Storage_AddRFID(uid)) UART_Printf("[ADMIN ] ID Added: %X\r\n", uid);
                else UART_Printf("[ADMIN ] ERR: Storage Full or Save Failed.\r\n");
            } else UART_Printf("[ADMIN ] ERR: Invalid Hex ID.\r\n");
        } else UART_Printf("[ADMIN ] ERR: Missing ID.\r\n");
    }
    // 7. DELID <HEX>
    else if (strncmp(cmd, CMD_DELID, 5) == 0) {
        char* token = strtok(cmd, " ");
        token = strtok(NULL, ""); // Get Remainder
        if (token != NULL) {
             // Clean Spaces
             char cleanHex[32];
             int idx = 0;
             for(int i=0; token[i] != 0 && idx < 16; i++) {
                 char c = token[i];
                 if(c != ' ' && c != '\n' && c != '\r') cleanHex[idx++] = c;
             }
             cleanHex[idx] = 0;
            uint32_t uid = (uint32_t)strtoul(cleanHex, NULL, 16);
            if (Storage_RemoveRFID(uid)) UART_Printf("[ADMIN ] ID Removed: %X\r\n", uid);
            else UART_Printf("[ADMIN ] ERR: ID Not Found.\r\n");
        } else UART_Printf("[ADMIN ] ERR: Missing ID.\r\n");
    }
    // 8. ADMINPASS <PASS>
    else if (strncmp(cmd, CMD_ADMINPASS, 9) == 0) {
        char* token = strtok(cmd, " ");
        token = strtok(NULL, " ");
        
        if (token != NULL) {
            Security_SetAdminPassword(token);
        } else {
            UART_Printf("[ADMIN ] ERR: Missing new password.\r\n");
        }
    }
    // 9. LISTIDS
    else if (strncmp(cmd, CMD_LISTIDS, 7) == 0) {
        Storage_ListRFIDs();
    }
    
    else {
        UART_Printf("[ADMIN ] Unknown Command.\r\n");
    }
}
