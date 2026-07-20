#include "bsp_usb.h"

#include "usb_otg.h"

static bool s_pcd_ready;

BSP_Status BSP_USB_Init(void)
{
  s_pcd_ready = (HAL_PCD_GetState(&hpcd_USB_OTG_FS) == HAL_PCD_STATE_READY);

  /* No VID/PID, descriptors, endpoints or Vendor Bulk class are available. */
  return s_pcd_ready ? BSP_STATUS_UNSUPPORTED : BSP_STATUS_ERROR;
}

BSP_USB_Capability BSP_USB_GetCapability(void)
{
  BSP_USB_Capability capability;

  capability.pcd_ready = s_pcd_ready;
  capability.vendor_bulk_available = false;
  capability.max_packet_bytes = BSP_USB_FS_MAX_PACKET_BYTES;
  return capability;
}
