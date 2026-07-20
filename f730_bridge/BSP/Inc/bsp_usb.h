#ifndef BSP_USB_H
#define BSP_USB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stdint.h>

#define BSP_USB_FS_MAX_PACKET_BYTES 64U

typedef struct
{
  bool pcd_ready;
  bool vendor_bulk_available;
  uint16_t max_packet_bytes;
} BSP_USB_Capability;

BSP_Status BSP_USB_Init(void);
BSP_USB_Capability BSP_USB_GetCapability(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_USB_H */
