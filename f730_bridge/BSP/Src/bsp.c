#include "bsp.h"

#include "bsp_debug.h"
#include "bsp_led.h"
#include "bsp_time.h"

#define BSP_LED_HEARTBEAT_MS 500U

static uint32_t s_led_tick_ms;

void BSP_Init(void)
{
  BSP_Debug_Init();
  BSP_LED_Init();
  s_led_tick_ms = BSP_Time_GetMs();
}

void BSP_Process(void)
{
  uint32_t now_ms = BSP_Time_GetMs();

  if (BSP_Time_HasElapsed(s_led_tick_ms, BSP_LED_HEARTBEAT_MS))
  {
    s_led_tick_ms = now_ms;
    BSP_LED_Toggle();
  }
}
