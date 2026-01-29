/*
 * servo_driver.c
 *
 * [SERVO DRIVER - SG90]
 * Uses TPM PWM (50Hz) to control locking mechanism.
 * Pin: PTB2 (TPM2_CH0).
 */

#include "servo_driver.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_tpm.h"

#define BOARD_TPM_BASEADDR TPM2
#define BOARD_TPM_CHANNEL  0U 

// Pin Mux: PTB2 (Alt3 for TPM2_CH0)
#define SERVO_PORT   PORTB
#define SERVO_PIN    2U
#define SERVO_ALT    kPORT_MuxAlt3

void Servo_Init(void) {
    tpm_config_t tpmInfo;
    tpm_chnl_pwm_signal_param_t tpmParam;

    // 1. Clocks
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_SetTpmClock(1U); // PLLFLLSEL

    // 2. Pin Mux (PTA2 = TPM2_CH1)
    PORT_SetPinMux(SERVO_PORT, SERVO_PIN, SERVO_ALT);

    // 3. TPM Init
    TPM_GetDefaultConfig(&tpmInfo);
    // Prescale 16 (from user code)
    tpmInfo.prescale = kTPM_Prescale_Divide_16; 
    TPM_Init(BOARD_TPM_BASEADDR, &tpmInfo);

    // 4. Setup PWM at 50Hz (20ms)
    // Initial Duty = 2% (approx 0.4ms - Closed)
    tpmParam.chnlNumber = (tpm_chnl_t)BOARD_TPM_CHANNEL;
    tpmParam.level = kTPM_HighTrue;
    tpmParam.dutyCyclePercent = 2; // Init Closed

    uint32_t tpmClock = CLOCK_GetFreq(kCLOCK_PllFllSelClk);
    
    // 50Hz is desired for SG90
    TPM_SetupPwm(BOARD_TPM_BASEADDR, &tpmParam, 1U, kTPM_CenterAlignedPwm, 50U, tpmClock);

    TPM_StartTimer(BOARD_TPM_BASEADDR, kTPM_SystemClock);
}

void Servo_SetDuty(uint8_t dutyCyclePercent) {
    // Limit safety
    if (dutyCyclePercent < 2) dutyCyclePercent = 2;
    if (dutyCyclePercent > 12) dutyCyclePercent = 12;

    /* Disable channel output before updating the dutycycle */
    TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, 0U);

    /* Update PWM duty cycle */
    TPM_UpdatePwmDutycycle(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, kTPM_CenterAlignedPwm, dutyCyclePercent);

    /* Start channel output with updated dutycycle */
    TPM_UpdateChnlEdgeLevelSelect(BOARD_TPM_BASEADDR, (tpm_chnl_t)BOARD_TPM_CHANNEL, kTPM_HighTrue);
}

// Servo Configuration
// Period: 20ms (50Hz)
// Duty: 2% (Close) to 12% (Max) -> Empirical for SG90
#define SERVO_OPEN_DUTY   4U  // Min
#define SERVO_CLOSE_DUTY  11U // Max

void Servo_Close(void) {
    Servo_SetDuty(SERVO_CLOSE_DUTY); 
}

void Servo_Open(void) {
    Servo_SetDuty(SERVO_OPEN_DUTY);
}
