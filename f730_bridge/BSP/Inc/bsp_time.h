#ifndef BSP_TIME_H
#define BSP_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stdint.h>

uint32_t BSP_Time_GetMs(void);
void BSP_Time_DelayMs(uint32_t delay_ms);
bool BSP_Time_HasElapsed(uint32_t start_ms, uint32_t interval_ms);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TIME_H */
