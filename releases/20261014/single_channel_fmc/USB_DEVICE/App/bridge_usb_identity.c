#include "bridge_usb_identity.h"

bool Bridge_USB_IdentityIsConfigured(void)
{
  return BRIDGE_USB_IDENTITY_CONFIGURED == 1U;
}
