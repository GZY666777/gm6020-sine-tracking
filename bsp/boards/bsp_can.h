#ifndef BSP_CAN_H
#define BSP_CAN_H

#include <stdint.h>

/** Configure accept-all filters and start CAN1 + CAN2. */
extern void can_filter_init(void);

/** 1 if CAN1 is in bus-off. */
uint8_t BSP_CAN_IsBusOff(void);

/** Restart CAN1 after bus-off.  Returns 1 when the bus is usable again. */
uint8_t BSP_CAN_Recover(void);

/** Raw CAN1 error status register (diagnostics). */
uint32_t BSP_CAN_GetESR(void);

#endif /* BSP_CAN_H */
