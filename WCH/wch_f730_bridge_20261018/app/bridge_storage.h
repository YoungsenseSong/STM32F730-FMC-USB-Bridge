#ifndef BRIDGE_STORAGE_H
#define BRIDGE_STORAGE_H

#include "bridge_protocol.h"

extern uint8_t g_bridge_block[BRIDGE_BLOCK_MAX_BYTES];
extern uint8_t g_bridge_usb_header[BRIDGE_USB_HEADER_BYTES];

void bridge_storage_init(void);

#endif
