#include "usb_diag.h"
#include "main.h"
#include "usb_device.h"

#define USB_DIAG_MAGIC 0x55374431U
volatile UsbDiag_Trace g_usb_diag __attribute__((section(".noinit.usb_diag")));

void UsbDiag_BootCapture(void)
{
  uint32_t csr = RCC->CSR;
  if ((g_usb_diag.magic != USB_DIAG_MAGIC) || (g_usb_diag.version != 1U) ||
      ((csr & (RCC_CSR_PORRSTF | RCC_CSR_BORRSTF)) != 0U))
  {
    UsbDiag_Trace zero = {0};
    g_usb_diag = zero;
    g_usb_diag.magic = USB_DIAG_MAGIC;
    g_usb_diag.version = 1U;
  }
  ++g_usb_diag.boots;
  g_usb_diag.reset_csr = csr;
  g_usb_diag.previous_stage = g_usb_diag.stage;
  g_usb_diag.stage = 1U;
  g_usb_diag.tick_ms = 0U;
  g_usb_diag.heartbeats = 0U;
  g_usb_diag.usb_resets = 0U;
  g_usb_diag.usb_suspends = 0U;
  g_usb_diag.usb_resumes = 0U;
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

void UsbDiag_Tick(void)
{
  uint32_t now = HAL_GetTick();
  g_usb_diag.tick_ms = now;
  g_usb_diag.usb_state = hUsbDeviceFS.dev_state;
  if ((now / 500U) != g_usb_diag.heartbeats)
  {
    g_usb_diag.heartbeats = now / 500U;
    HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin);
  }
}

void UsbDiag_Fault(uint32_t kind)
{
  __disable_irq();
  g_usb_diag.fault_kind = kind;
  g_usb_diag.fault_cfsr = SCB->CFSR;
  g_usb_diag.fault_hfsr = SCB->HFSR;
  g_usb_diag.fault_bfar = SCB->BFAR;
  g_usb_diag.fault_mmfar = SCB->MMFAR;
  g_usb_diag.fault_ipsr = __get_IPSR();
  g_usb_diag.stage = 0xF000U | kind;
  __DSB();
  while (1) { /* Keep the baseline watchdog behavior; retain fault evidence. */ }
}
