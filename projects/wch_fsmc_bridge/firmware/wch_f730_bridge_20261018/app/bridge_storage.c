#include "bridge_storage.h"

#include <stdint.h>

/* Preserves the F730 maximum block boundary in the WCH RAM budget. */
uint8_t g_bridge_block[BRIDGE_BLOCK_MAX_BYTES];

/* Protocol header remains separate so payload ownership can span USB retries. */
uint8_t g_bridge_usb_header[BRIDGE_USB_HEADER_BYTES];

void bridge_storage_init(void)
{
  /* Keep the production-size ownership buffers in the linked RAM budget. */
  g_bridge_block[0] = 0U;
  g_bridge_block[BRIDGE_BLOCK_MAX_BYTES - 1U] = 0U;
  g_bridge_usb_header[0] = 0U;
}
