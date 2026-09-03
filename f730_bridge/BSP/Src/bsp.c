#include "bsp.h"

#include "bsp_debug.h"
#include "bsp_bridge.h"
#include "bsp_fmc.h"
#include "bsp_fpga_ctrl.h"
#include "bsp_i2c.h"
#include "bsp_led.h"
#include "bsp_time.h"
#include "bsp_usb.h"

#include "iwdg.h"

#define BSP_DEBUG_HEARTBEAT_MS 1000U
#define BSP_USB_LED_ON_MS       60U
#define BSP_USB_LED_OFF_MS      60U
#define BSP_USB_LED_MAX_PENDING 255U

typedef enum
{
  BSP_USB_LED_IDLE = 0,
  BSP_USB_LED_ON,
  BSP_USB_LED_GAP
} BSP_USB_LedState;

static uint32_t s_usb_led_tick_ms;
static uint32_t s_usb_rx_packets_seen;
static uint32_t s_usb_led_pending;
static BSP_USB_LedState s_usb_led_state;
static uint32_t s_debug_tick_ms;

BSP_Status BSP_Init(void)
{
  BSP_Debug_Init();
  (void)BSP_Debug_WriteString("F730 bridge boot: USART1 115200 8N1\r\n");
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

  if (BSP_Bridge_Init() != BSP_STATUS_OK)
  {
    return BSP_STATUS_ERROR;
  }

  if (BSP_USB_Init() != BSP_STATUS_OK)
  {
    (void)BSP_Debug_WriteString("USB CDC FS init failed\r\n");
    return BSP_STATUS_ERROR;
  }
  s_usb_led_tick_ms = BSP_Time_GetMs();
  s_usb_rx_packets_seen = 0U;
  s_usb_led_pending = 0U;
  s_usb_led_state = BSP_USB_LED_IDLE;
  s_debug_tick_ms = s_usb_led_tick_ms;
  (void)BSP_Debug_WriteString(
      "USB CDC FS echo ready; PA5 flashes per OUT packet\r\n");
  return BSP_STATUS_OK;
}

void BSP_Process(void)
{
  uint32_t now_ms = BSP_Time_GetMs();
  BSP_USB_Stats usb_stats;
  uint32_t new_packets;

  BSP_Debug_Process();
  BSP_FPGA_Ctrl_Process();
  BSP_USB_Process();
  BSP_Bridge_Process();

  usb_stats = BSP_USB_GetStats();
  new_packets = usb_stats.rx_packets - s_usb_rx_packets_seen;
  s_usb_rx_packets_seen = usb_stats.rx_packets;
  if (new_packets != 0U)
  {
    if (new_packets > (BSP_USB_LED_MAX_PENDING - s_usb_led_pending))
    {
      s_usb_led_pending = BSP_USB_LED_MAX_PENDING;
    }
    else
    {
      s_usb_led_pending += new_packets;
    }
  }

  if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }

  if ((s_usb_led_state == BSP_USB_LED_IDLE) && (s_usb_led_pending != 0U))
  {
    BSP_LED_On();
    s_usb_led_tick_ms = now_ms;
    s_usb_led_state = BSP_USB_LED_ON;
  }
  else if ((s_usb_led_state == BSP_USB_LED_ON) &&
           BSP_Time_HasElapsed(s_usb_led_tick_ms, BSP_USB_LED_ON_MS))
  {
    BSP_LED_Off();
    s_usb_led_tick_ms = now_ms;
    s_usb_led_state = BSP_USB_LED_GAP;
  }
  else if ((s_usb_led_state == BSP_USB_LED_GAP) &&
           BSP_Time_HasElapsed(s_usb_led_tick_ms, BSP_USB_LED_OFF_MS))
  {
    --s_usb_led_pending;
    s_usb_led_state = BSP_USB_LED_IDLE;
  }

  if (BSP_Time_HasElapsed(s_debug_tick_ms, BSP_DEBUG_HEARTBEAT_MS))
  {
    s_debug_tick_ms = now_ms;
    (void)BSP_Debug_WriteString("F730 bridge heartbeat\r\n");
  }
}
