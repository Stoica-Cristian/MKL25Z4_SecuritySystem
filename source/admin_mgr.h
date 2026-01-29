#ifndef ADMIN_MGR_H
#define ADMIN_MGR_H

#include <stdint.h>
#include <stdbool.h>

// Called when a full line is received via UART
void Admin_ProcessCommand(char* cmd);

#endif // ADMIN_MGR_H
