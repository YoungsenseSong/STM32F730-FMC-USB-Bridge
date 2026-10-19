#ifndef BRIDGE_USB_IDENTITY_H
#define BRIDGE_USB_IDENTITY_H

/*
 * Development-only CDC identity copied from the work1 CubeMX example so the
 * student project can be exercised on a local PC.  0x0483 belongs to ST; this
 * identity is not an allocation to this project and must be replaced before
 * distributing hardware or firmware outside the controlled lab setup.
 */
#define BRIDGE_USB_IDENTITY_CONFIGURED       1U
#define BRIDGE_USB_IDENTITY_TEST_ONLY        1U
#define BRIDGE_USB_VID                       0x0483U
#define BRIDGE_USB_PID                       0x5744U
#define BRIDGE_USB_DEVICE_RELEASE            0x0100U

#define BRIDGE_USB_MANUFACTURER_STRING       "STMicroelectronics"
#define BRIDGE_USB_PRODUCT_STRING            "STM32 Virtual ComPort"
#define BRIDGE_USB_CONFIGURATION_STRING      "CDC Config"
#define BRIDGE_USB_INTERFACE_STRING          "CDC Interface"

#include <stdbool.h>

bool Bridge_USB_IdentityIsConfigured(void);

#if (BRIDGE_USB_IDENTITY_CONFIGURED == 1U) && \
    ((BRIDGE_USB_VID == 0U) || (BRIDGE_USB_PID == 0U))
#error "Configured USB identity requires non-zero authorized VID and PID"
#endif

#if (BRIDGE_USB_IDENTITY_TEST_ONLY == 0U) && \
    (BRIDGE_USB_VID == 0x0483U) && (BRIDGE_USB_PID == 0x5744U)
#error "ST example VID/PID must remain explicitly marked test-only"
#endif

#endif /* BRIDGE_USB_IDENTITY_H */
