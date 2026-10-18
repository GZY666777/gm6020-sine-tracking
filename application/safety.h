/**
 * @file    safety.h
 * @brief   Layered protection: communication watchdog + hardware IWDG.
 */

#ifndef SAFETY_H
#define SAFETY_H

#include <stdint.h>
#include "motor_gm6020.h"

#define SAFETY_FAULT_FEEDBACK_TIMEOUT  (1u << 0)
#define SAFETY_FAULT_OVERTEMP          (1u << 1)
#define SAFETY_FAULT_POS_LIMIT         (1u << 2)
#define SAFETY_FAULT_CAN_BUSOFF        (1u << 3)
#define SAFETY_FAULT_CAN_TX            (1u << 4)

/** Initialise the independent watchdog (250 ms) and clear fault state. */
void Safety_Init(void);

/** Refresh the IWDG.  Called only from the 1 kHz control step. */
void Safety_Feed(void);

/** Record the result of the latest CAN transmission. */
void Safety_ReportTx(uint8_t ok);

/**
 * @brief  Evaluate all run-time protection conditions.
 * @param  now_ms          HAL tick
 * @param  fb              latest motor feedback
 * @param  pos_rel_deg     position relative to the captured zero
 * @param  check_pos_limit 1 while the position loop is active
 * @param  tx_ok           result of the latest GM6020 send
 *
 * Faults are latched until Safety_TryClear() succeeds.
 */
void Safety_Update(uint32_t now_ms, const gm6020_feedback_t *fb,
                   float pos_rel_deg, uint8_t check_pos_limit,
                   uint8_t tx_ok);

uint8_t Safety_IsFaulted(void);
uint32_t Safety_GetFaults(void);

/**
 * @brief  Try to clear latched faults.
 * @return 1 if faults were cleared, 0 if the motor is not healthy yet.
 *
 * Refuses to clear while feedback is missing, the motor is still hot, or
 * the rotor is still outside the safe position window.  A bus-off fault
 * triggers a CAN controller restart.
 */
uint8_t Safety_TryClear(uint32_t now_ms, const gm6020_feedback_t *fb,
                        float pos_rel_deg);

void Safety_ForceFault(uint32_t bit);

/** Human-readable name for one fault bit. */
const char *Safety_FaultName(uint32_t bit);

#endif /* SAFETY_H */
