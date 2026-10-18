#include "bsp_can.h"
#include "main.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

void can_filter_init(void)
{
    CAN_FilterTypeDef can_filter_st = {0};

    can_filter_st.FilterActivation = ENABLE;
    can_filter_st.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter_st.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter_st.FilterIdHigh = 0x0000;
    can_filter_st.FilterIdLow = 0x0000;
    can_filter_st.FilterMaskIdHigh = 0x0000;
    can_filter_st.FilterMaskIdLow = 0x0000;
    can_filter_st.FilterBank = 0;
    can_filter_st.FilterFIFOAssignment = CAN_RX_FIFO0;

    (void)HAL_CAN_ConfigFilter(&hcan1, &can_filter_st);
    (void)HAL_CAN_Start(&hcan1);
    (void)HAL_CAN_ActivateNotification(&hcan1,
                                       CAN_IT_RX_FIFO0_MSG_PENDING);

    /* CAN2 is initialised by CubeMX as well; keep its filter valid even if
     * this demo only uses CAN1. */
    can_filter_st.SlaveStartFilterBank = 14;
    can_filter_st.FilterBank = 14;
    (void)HAL_CAN_ConfigFilter(&hcan2, &can_filter_st);
    (void)HAL_CAN_Start(&hcan2);
    (void)HAL_CAN_ActivateNotification(&hcan2,
                                       CAN_IT_RX_FIFO0_MSG_PENDING);
}

uint8_t BSP_CAN_IsBusOff(void)
{
    if ((hcan1.Instance->ESR & CAN_ESR_BOFF) != 0u)
    {
        return 1u;
    }

    return 0u;
}

uint8_t BSP_CAN_Recover(void)
{
    if (BSP_CAN_IsBusOff() == 0u)
    {
        return 1u;
    }

    if (HAL_CAN_Stop(&hcan1) != HAL_OK)
    {
        return 0u;
    }

    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        return 0u;
    }

    (void)HAL_CAN_ActivateNotification(&hcan1,
                                       CAN_IT_RX_FIFO0_MSG_PENDING);

    return (BSP_CAN_IsBusOff() == 0u) ? 1u : 0u;
}

uint32_t BSP_CAN_GetESR(void)
{
    return hcan1.Instance->ESR;
}
