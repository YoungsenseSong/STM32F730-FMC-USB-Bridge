#include "bsp_time.h"

#include "tim.h"

static bool s_us_counter_available;

bool BSP_Time_Init(void)
{
  s_us_counter_available = (HAL_TIM_Base_Start(&htim6) == HAL_OK);
  return s_us_counter_available;
}

uint32_t BSP_Time_GetMs(void)
{
  return HAL_GetTick();
}

bool BSP_Time_IsUsCounterAvailable(void)
{
  return s_us_counter_available;
}

uint16_t BSP_Time_GetUsCounter(void)
{
  return s_us_counter_available ? (uint16_t)__HAL_TIM_GET_COUNTER(&htim6) : 0U;
}

void BSP_Time_DelayMs(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}

bool BSP_Time_HasElapsed(uint32_t start_ms, uint32_t interval_ms)
{
  return ((uint32_t)(BSP_Time_GetMs() - start_ms) >= interval_ms);
}
