#include "bridge_port.h"

#include "debug.h"

#include <stdio.h>

#include "bridge_storage.h"

int main(void)
{
  platform_init();
  printf("WCH bridge migration demo; SystemClk=%lu; reset=0x%08lx\r\n",
         (unsigned long)SystemCoreClock, (unsigned long)platform_reset_reason());
#if WCH_SIM_SOURCE
  platform_log("SIM source -> USBFS CDC; FPGA/FSMC disabled; USB identity TEST ONLY");
#elif WCH_FMC_BRIDGE
  platform_log("FPGA FSMC -> UBR1 USBFS bridge; WCH migration image");
#else
  platform_log("USBFS CDC echo; FPGA/FSMC disabled; USB identity TEST ONLY");
#endif

  bridge_storage_init();
  (void)extbus_init();
  (void)usb_init();
  fmc_bridge_init();

#if WCH_USB_TRACE
  printf("USBTRACE-v2 startup\r\n");
  usb_trace_state();
  uint32_t trace_loops = 0U;
#endif

  for (;;)
  {
    usb_process();
    fmc_bridge_process();
#if WCH_USB_TRACE
    if (++trace_loops >= 1000000U)
    {
      usb_trace_state();
      trace_loops = 0U;
    }
#endif
    platform_gpio_heartbeat();
    platform_watchdog_kick();
  }
}
