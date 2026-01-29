/*
 * rfid_driver.c
 *
 * [RFID DRIVER - MFRC522]
 * Features: Non-Blocking FSM Architecture (Interrupt-Driven Logic).
 * Protocol: ISO 14443A (Mifare).
 */

#include "rfid_driver.h"
#include "fsl_spi.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "timer_driver.h"
#include "fsl_debug_console.h"
#include "uart_driver.h"
#include <string.h>

// ============================================================================
// FSM DEFINITIONS
// ============================================================================
typedef enum {
    RFID_IDLE,          // Waiting for cycle time
    RFID_REQ_SENT,      // Request Command sent, waiting for IRQ/Data
    RFID_ANTICOLL_SENT, // Anticoll Command sent, waiting for IRQ/Data
    RFID_HALT_SENT,     // Halt Command Sent (Optional wait) or just Done
} RFID_State_t;

static volatile RFID_State_t g_rfidState = RFID_IDLE;
static uint32_t g_rfid_timer = 0;       // For FSM timeouts
static uint32_t g_next_scan_time = 0;   // Interval control

// Result Holding
static int g_last_valid_result = 0; 

// Hardware Debounce
static uint8_t g_last_uid[5] = {0};
static uint32_t g_last_uid_time = 0;

// ============================================================================
// REGISTERS & CONSTANTS
// ============================================================================
// (Same definitions as before)
#define CommandReg     0x01
#define ComIEnReg      0x02
#define DivIEnReg      0x03
#define ComIrqReg      0x04
#define DivIrqReg      0x05
#define ErrorReg       0x06
#define Status1Reg     0x07
#define Status2Reg     0x08
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define WaterLevelReg  0x0B
#define ControlReg     0x0C
#define BitFramingReg  0x0D
#define CollReg        0x0E
#define ModeReg        0x11
#define TxModeReg      0x12
#define RxModeReg      0x13
#define TxControlReg   0x14
#define TxASKReg       0x15
#define RFCfgReg       0x26
#define TModeReg       0x2A
#define TPrescalerReg  0x2B
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D
#define PCD_IDLE       0x00
#define PCD_AUTHENT    0x0E
#define PCD_TRANSCEIVE 0x0C
#define PCD_RESETPHASE 0x0F
#define PCD_HALT       0x50
#define PCD_CALCCRC    0x03
#define CRCResultRegH  0x21
#define CRCResultRegL  0x22
#define PICC_ANTICOLL  0x93

#define RFID_CS_PIN   4U
#define RFID_RST_PIN  0U
#define RFID_IRQ_PIN  4U

// ============================================================================
// LOW LEVEL SPI (Synchronous but fast)
// ============================================================================
// SPI is blocking (fast MHz transfer); FSM handles timing.

void SPI0_Init_SDK(void) {
    spi_master_config_t userConfig;
    CLOCK_EnableClock(kCLOCK_PortC);
    PORT_SetPinMux(PORTC, 5U, kPORT_MuxAlt2); // SCK
    PORT_SetPinMux(PORTC, 6U, kPORT_MuxAlt2); // MOSI
    PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt2); // MISO
    PORT_SetPinMux(PORTC, RFID_CS_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTC, RFID_RST_PIN, kPORT_MuxAsGpio);
    GPIOC->PDDR |= (1U << RFID_CS_PIN) | (1U << RFID_RST_PIN);
    GPIOC->PSOR |= (1U << RFID_CS_PIN) | (1U << RFID_RST_PIN);

    SPI_MasterGetDefaultConfig(&userConfig);
    userConfig.baudRate_Bps = 1000000; 
    userConfig.outputMode = kSPI_SlaveSelectAsGpio; 
    userConfig.polarity = kSPI_ClockPolarityActiveHigh;
    userConfig.phase = kSPI_ClockPhaseFirstEdge;
    SPI_MasterInit(SPI0, &userConfig, CLOCK_GetFreq(kCLOCK_BusClk));
}

uint8_t SPI0_Transfer(uint8_t data) {
    uint8_t rxData = 0;
    spi_transfer_t xfer;
    xfer.txData = &data;
    xfer.rxData = &rxData;
    xfer.dataSize = 1;
    xfer.flags = kSPI_8BitMode;
    SPI_MasterTransferBlocking(SPI0, &xfer);
    return rxData;
}

void WriteReg(uint8_t addr, uint8_t val) {
    GPIOC->PCOR = (1U << RFID_CS_PIN);
    SPI0_Transfer((addr << 1) & 0x7E);
    SPI0_Transfer(val);
    GPIOC->PSOR = (1U << RFID_CS_PIN);
}

uint8_t ReadReg(uint8_t addr) {
    uint8_t val;
    GPIOC->PCOR = (1U << RFID_CS_PIN);
    SPI0_Transfer(((addr << 1) & 0x7E) | 0x80);
    val = SPI0_Transfer(0x00);
    GPIOC->PSOR = (1U << RFID_CS_PIN);
    return val;
}

// ============================================================================
// INIT
// ============================================================================
void RC522_Init(void) {
    SPI0_Init_SDK();
    
    // IRQ pin unused; FSM uses polled status registers.
    CLOCK_EnableClock(kCLOCK_PortD);
    PORT_SetPinMux(PORTD, RFID_IRQ_PIN, kPORT_MuxAsGpio);

    // Reset Hardware
    GPIOC->PCOR = (1U << RFID_RST_PIN); 
    // Hard delay for reset pulse 

    GPIOC->PSOR = (1U << RFID_RST_PIN); 
    for(volatile int i=0; i<100000; i++); 

    WriteReg(CommandReg, PCD_RESETPHASE); 
    for(volatile int i=0; i<100000; i++); 

    WriteReg(TModeReg, 0x8D);
    WriteReg(TPrescalerReg, 0x3E);
    WriteReg(TReloadRegH, 0);
    WriteReg(TReloadRegL, 30); 
    WriteReg(TxASKReg, 0x40);
    WriteReg(ModeReg, 0x3D);
    WriteReg(RFCfgReg, 0x70); 
    uint8_t temp = ReadReg(TxControlReg);
    if (!(temp & 0x03)) WriteReg(TxControlReg, temp | 0x03);
}

// ============================================================================
// FSM HELPERS
// ============================================================================
void Start_Transceive(uint8_t *data, uint8_t len) {
    WriteReg(CommandReg, PCD_IDLE); // Stop active command
    WriteReg(ComIrqReg, 0x7F);      // Clear IRQs
    WriteReg(FIFOLevelReg, 0x80);   // Flush FIFO
    
    for (int i = 0; i < len; i++) WriteReg(FIFODataReg, data[i]);
    
    WriteReg(CommandReg, PCD_TRANSCEIVE);
    uint8_t current = ReadReg(BitFramingReg);
    WriteReg(BitFramingReg, current | 0x80); // Start Send
}

// ============================================================================
// FSM TICK (Called from Main Loop)
// ============================================================================
void RFID_Tick(void) {
    uint32_t now = GetTick();

    switch(g_rfidState) {
        
        // --- 1. IDLE: Check if time to scan ---
        case RFID_IDLE:
             // Clear last UID after 500ms of inactivity to allow re-scan
             if (IsTimeout(g_last_uid_time, 500)) {
                 memset(g_last_uid, 0, 5); 
             }

            if (IsTimeout(g_next_scan_time, 100)) { // Scan every 100ms
                g_next_scan_time = now;
                
                // Start Request (REQA)
                WriteReg(BitFramingReg, 0x07); // 7 bits
                uint8_t buf = PICC_REQIDL;
                Start_Transceive(&buf, 1);
                
                g_rfidState = RFID_REQ_SENT;
                g_rfid_timer = now;
            }
            break;

        // --- 2. REQ SENT: Wait for completion ---
        case RFID_REQ_SENT:
            if (IsTimeout(g_rfid_timer, 25)) { // Timeout 25ms
                g_rfidState = RFID_IDLE; // Fail
                // Do NOT clear UID here instantly. Debounce handles it.
            } else {
                uint8_t n = ReadReg(ComIrqReg);
                if (n & 0x30) { // RXIRq or IdleIRq
                     // Check Error
                     if (!(ReadReg(ErrorReg) & 0x1B)) {
                         // REQ Success! Start Anticoll
                         WriteReg(BitFramingReg, 0x00);
                         uint8_t serNum[2] = { PICC_ANTICOLL, 0x20 };
                         Start_Transceive(serNum, 2);
                         
                         g_rfidState = RFID_ANTICOLL_SENT;
                         g_rfid_timer = GetTick();
                     } else {
                         g_rfidState = RFID_IDLE;
                     }
                }
            }
            break;

        // --- 3. ANTICOLL SENT: Wait for UID ---
        case RFID_ANTICOLL_SENT:
            if (IsTimeout(g_rfid_timer, 25)) {
                g_rfidState = RFID_IDLE;
            } else {
                 uint8_t n = ReadReg(ComIrqReg);
                 if (n & 0x30) {
                     if (!(ReadReg(ErrorReg) & 0x1B)) {
                         // UID Received!
                         uint8_t uid[5];
                         int i;
                         uint8_t nn = ReadReg(FIFOLevelReg);
                         if (nn > 5) nn = 5; 
                         for (i = 0; i < nn; i++) uid[i] = ReadReg(FIFODataReg);
                         
                         // Verify Checksum
                         uint8_t serNumCheck = 0;
                         for (i = 0; i < 4; i++) serNumCheck ^= uid[i];
                         
                         if (serNumCheck == uid[4]) {
                             // Valid UID Received
                             bool same = true;
                             for(i=0; i<4; i++) if (uid[i] != g_last_uid[i]) same = false;
                             
                             if (!same) {
                                 // NEW Card detected!
                                 memcpy(g_last_uid, uid, 5);
                                 g_last_uid_time = GetTick();
                                 
                                 UART_Printf("[ACCESS] Card Scanned: [%02X %02X %02X %02X]\r\n", uid[0], uid[1], uid[2], uid[3]);
                                 
                                 
                                 // We just report "Card Detected"
                                 g_last_valid_result = 1; // 1 = New Card Present
                             }
                             else {
                                 // Card still present; update timestamp 
                             }
                             
                             // Halt Card (Send Halt Command)
                             uint8_t buffer[4] = { PCD_HALT, 0, 0, 0 }; 
                             Start_Transceive(buffer, 2); // Send Halt
                             // No CRC needed for Halt, just best effort.
                         }
                     }
                     g_rfidState = RFID_IDLE; // Done
                 }
            }
            break;
            
        case RFID_HALT_SENT:
            g_rfidState = RFID_IDLE;
            break;
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================
int RFID_GetLastScanResult(void) {
    int res = g_last_valid_result;
    g_last_valid_result = 0; // Clear
    return res;
}

// Returns the 4-byte UID as a single integer for easy comparison
uint32_t RFID_GetLastUID(void) {
    return (uint32_t)((g_last_uid[0] << 24) | (g_last_uid[1] << 16) | (g_last_uid[2] << 8) | g_last_uid[3]);
}

int RFID_CheckScan(void) {
    return RFID_GetLastScanResult();
}
