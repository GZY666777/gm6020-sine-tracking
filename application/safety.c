/**
 * @file    safety.c
 * @brief   Protection logic and independent watchdog.
 */

#include "safety.h"
#include "app_config.h"
#include "bsp_can.h"

#include <math.h>
#include <string.h>

static volatile uint32_t s_faults = 0u;
static uint32_t s_tx_fail_streak = 0u;

static IWDG_HandleTypeDef s_iwdg;
static uint8_t s_iwdg_ready = 0u;

void Safety_Init(void)
{
    s_faults = 0u;
    s_tx_fail_streak = 0u;

    /* Freeze the IWDG while the core is halted by the debugger.
     * Without this a breakpoint longer than 250 ms resets the MCU. */
#ifdef DBGMCU_APB1_FZ_DBG_IWDG_STOP
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
#endif

    s_iwdg.Instance = IWDG;
    s_iwdg.Init.Prescaler = SAFETY_IWDG_PRESCALER;
    s_iwdg.Init.Reload = SAFETY_IWDG_RELOAD;

    if (HAL_IWDG_Init(&s_iwdg) == HAL_OK)
    {
        s_iwdg_ready = 1u;
    }
    else
    {
        s_iwdg_ready = 0u;
    }
}

void Safety_Feed(void)
{
    if (s_iwdg_ready != 0u)
    {
        HAL_IWDG_Refresh(&s_iwdg);
    }
}

void Safety_ReportTx(uint8_t ok)
{
    if (ok != 0u)
    {
        s_tx_fail_streak = 0u;
    }
    else if (s_tx_fail_streak < SAFETY_TX_FAIL_LIMIT)
    {
        s_tx_fail_streak++;
    }
}

void Safety_Update(uint32_t now_ms, const gm6020_feedback_t *fb,
                   float pos_rel_deg, uint8_t check_pos_limit,
                   uint8_t tx_ok)
{
    Safety_ReportTx(tx_ok);

    if (s_faults != 0u || fb == 0)
    {
        return;
    }

    /* 1) Communication watchdog: feedback must keep arriving. */
    if (fb->last_rx_ms != 0u &&
        (now_ms - fb->last_rx_ms) > SAFETY_FEEDBACK_TIMEOUT_MS)
    {
        s_faults |= SAFETY_FAULT_FEEDBACK_TIMEOUT;
    }

    /* 2) Overtemperature. */
    if (fb->temperature > SAFETY_TEMP_FAULT_C)
    {
        s_faults |= SAFETY_FAULT_OVERTEMP;
    }

    /* 3) Soft position limit, only meaningful in position mode. */
    if (check_pos_limit != 0u && fabsf(pos_rel_deg) > SAFETY_POS_LIMIT_DEG)
    {
        s_faults |= SAFETY_FAULT_POS_LIMIT;
    }

    /* 4) CAN controller bus-off. */
    if (BSP_CAN_IsBusOff() != 0u)
    {
        s_faults |= SAFETY_FAULT_CAN_BUSOFF;
    }

    /* 5) Persistent transmit failure (no ACK / mailbox stuck). */
    if (s_tx_fail_streak >= SAFETY_TX_FAIL_LIMIT)
    {
        s_faults |= SAFETY_FAULT_CAN_TX;
    }
}

uint8_t Safety_IsFaulted(void)
{
    return (s_faults != 0u) ? 1u : 0u;
}

uint32_t Safety_GetFaults(void)
{
    return s_faults;
}

uint8_t Safety_TryClear(uint32_t now_ms, const gm6020_feedback_t *fb,
                        float pos_rel_deg)
{
    if (fb == 0)
    {
        return 0u;
    }

    /* The motor must be talking to us again. */
    if (fb->last_rx_ms == 0u ||
        (now_ms - fb->last_rx_ms) > SAFETY_FEEDBACK_TIMEOUT_MS)
    {
        return 0u;
    }

    /* Do not re-enable a hot motor. */
    if (fb->temperature >= SAFETY_TEMP_WARN_C)
    {
        return 0u;
    }

    /* Rotor must be back inside the safe window. */
    if (fabsf(pos_rel_deg) > SAFETY_POS_CLEAR_DEG)
    {
        return 0u;
    }

    /* Bus-off needs a CAN controller restart. */
    if ((s_faults & SAFETY_FAULT_CAN_BUSOFF) != 0u)
    {
        if (BSP_CAN_Recover() == 0u)
        {
            return 0u;
        }
    }

    s_faults = 0u;
    s_tx_fail_streak = 0u;

    return 1u;
}

void Safety_ForceFault(uint32_t bit)
{
    s_faults |= bit;
}

const char *Safety_FaultName(uint32_t bit)
{
    switch (bit)
    {
        case SAFETY_FAULT_FEEDBACK_TIMEOUT:
            return "FEEDBACK_TIMEOUT";
        case SAFETY_FAULT_OVERTEMP:
            return "OVERTEMP";
        case SAFETY_FAULT_POS_LIMIT:
            return "POS_LIMIT";
        case SAFETY_FAULT_CAN_BUSOFF:
            return "CAN_BUSOFF";
        case SAFETY_FAULT_CAN_TX:
            return "CAN_TX";
        default:
            return "UNKNOWN";
    }
}
