#include "bsp.h"

#include "bsp_debug.h"
#include "bsp_fmc.h"
#include "bsp_fpga_ctrl.h"
#include "bsp_i2c.h"
#include "bsp_led.h"
#include "bsp_time.h"
#include "bsp_usb.h"

#include "iwdg.h"

#define BSP_LED_HEARTBEAT_MS 500U

static uint32_t s_led_tick_ms;

BSP_Status BSP_Init(void)
{
  BSP_Debug_Init();
  BSP_LED_Init();

  if (!BSP_Time_Init())
  {
    return BSP_STATUS_ERROR;
  }
  if (BSP_FMC_Init() != BSP_STATUS_OK)
  {
    return BSP_STATUS_ERROR;
  }
  if (BSP_I2C_Init() != BSP_STATUS_OK)
  {
    return BSP_STATUS_ERROR;
  }
  if (BSP_FPGA_Ctrl_Init() != BSP_STATUS_OK)
  {
    return BSP_STATUS_ERROR;
  }

  /* The PCD is configured, but Vendor Bulk descriptors/class code are absent. */
  (void)BSP_USB_Init();
  s_led_tick_ms = BSP_Time_GetMs();
  return BSP_STATUS_OK;
}

void BSP_Process(void)
{
  uint32_t now_ms = BSP_Time_GetMs();

  BSP_FPGA_Ctrl_Process();

  if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }

  if (BSP_Time_HasElapsed(s_led_tick_ms, BSP_LED_HEARTBEAT_MS))
  {
    s_led_tick_ms = now_ms;
    BSP_LED_Toggle();
  }
}
