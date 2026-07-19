#include "bsp_time.h"

uint32_t BSP_Time_GetMs(void)
{
  return HAL_GetTick();
}

void BSP_Time_DelayMs(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}

bool BSP_Time_HasElapsed(uint32_t start_ms, uint32_t interval_ms)
{
  return ((uint32_t)(BSP_Time_GetMs() - start_ms) >= interval_ms);
}
