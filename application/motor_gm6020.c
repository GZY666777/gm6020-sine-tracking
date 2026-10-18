/**
 * @file    motor_gm6020.c
 * @brief   DJI GM6020 CAN driver.
 */

#include "motor_gm6020.h"

#include <string.h>

#include "app_config.h"

static CAN_HandleTypeDef *s_hcan = 0;
static uint8_t s_motor_id = 1u;

static volatile gm6020_feedback_t s_feedback;
static CAN_TxHeaderTypeDef s_tx_header;
static uint8_t s_tx_data[8];

#define GM6020_FEEDBACK_BASE_ID 0x204u
#define GM6020_CMD_ID_1_TO_4    0x1FFu
#define GM6020_CMD_ID_5_TO_7    0x2FFu

static void GM6020_OnRx(uint32_t std_id, const uint8_t *data)
{
    uint16_t ecd;
    int16_t speed;
    int16_t current;

    if (std_id != (uint32_t)(GM6020_FEEDBACK_BASE_ID + s_motor_id))
    {
        return;
    }

    ecd = (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    speed = (int16_t)(((uint16_t)data[2] << 8) | (uint16_t)data[3]);
    current = (int16_t)(((uint16_t)data[4] << 8) | (uint16_t)data[5]);

    s_feedback.last_ecd = s_feedback.ecd;
    s_feedback.ecd = ecd;
    s_feedback.speed_rpm = speed;
    s_feedback.current_raw = current;
    s_feedback.temperature = data[6];
    s_feedback.last_rx_ms = HAL_GetTick();
    s_feedback.rx_count++;
}

void GM6020_Init(CAN_HandleTypeDef *hcan, uint8_t motor_id)
{
    s_hcan = hcan;
    s_motor_id = motor_id;

    memset((void *)&s_feedback, 0, sizeof(s_feedback));

    s_tx_header.StdId = (motor_id <= 4u) ? GM6020_CMD_ID_1_TO_4
                                         : GM6020_CMD_ID_5_TO_7;
    s_tx_header.IDE = CAN_ID_STD;
    s_tx_header.RTR = CAN_RTR_DATA;
    s_tx_header.DLC = 8u;
    s_tx_header.TransmitGlobalTime = DISABLE;
}

uint8_t GM6020_SendVoltageCmd(int16_t voltage_cmd)
{
    uint32_t mailbox;
    uint8_t slot;

    if (s_hcan == 0 || s_motor_id < 1u || s_motor_id > 7u)
    {
        return 0u;
    }

    slot = (uint8_t)((s_motor_id - 1u) & 0x03u);

    if (MOTOR_SIGN < 0)
    {
        voltage_cmd = (int16_t)(-voltage_cmd);
    }

    memset(s_tx_data, 0, sizeof(s_tx_data));
    s_tx_data[slot * 2u] = (uint8_t)(((uint16_t)voltage_cmd) >> 8);
    s_tx_data[slot * 2u + 1u] = (uint8_t)(((uint16_t)voltage_cmd) & 0xFFu);

    if (HAL_CAN_AddTxMessage(s_hcan, &s_tx_header, s_tx_data, &mailbox) != HAL_OK)
    {
        return 0u;
    }

    return 1u;
}

const gm6020_feedback_t *GM6020_GetFeedback(void)
{
    return (const gm6020_feedback_t *)&s_feedback;
}

float GM6020_GetAngleDeg(void)
{
    /* 8192 encoder counts per mechanical revolution. */
    float angle = ((float)s_feedback.ecd) * (360.0f / 8192.0f);

    if (MOTOR_SIGN < 0)
    {
        angle = 360.0f - angle;
        if (angle >= 360.0f)
        {
            angle -= 360.0f;
        }
    }

    return angle;
}

float GM6020_GetSpeedRpm(void)
{
    float speed = (float)s_feedback.speed_rpm;

    if (MOTOR_SIGN < 0)
    {
        speed = -speed;
    }

    return speed;
}

uint8_t GM6020_IsOnline(uint32_t now_ms, uint32_t timeout_ms)
{
    if (s_feedback.last_rx_ms == 0u)
    {
        return 0u;
    }

    return ((now_ms - s_feedback.last_rx_ms) <= timeout_ms) ? 1u : 0u;
}

uint8_t GM6020_GetMotorId(void)
{
    return s_motor_id;
}

/**
 * @brief  HAL CAN RX FIFO 0 callback.
 *
 * The callback is shared by all CAN peripherals.  This project uses CAN1
 * for the GM6020; CAN2 frames are ignored.
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if (hcan->Instance != CAN1)
    {
        /* CAN2 is started by bsp_can.c but unused here.  Drain its FIFO so
         * the RX interrupt does not stay pending forever. */
        while (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header,
                                    rx_data) == HAL_OK)
        {
        }
        return;
    }

    while (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK)
    {
        if (rx_header.IDE == CAN_ID_STD)
        {
            GM6020_OnRx(rx_header.StdId, rx_data);
        }
    }
}
