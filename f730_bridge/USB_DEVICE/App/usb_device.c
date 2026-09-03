#include "usb_device.h"

#include "usbd_desc.h"

USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

USBD_StatusTypeDef MX_USB_DEVICE_Init(void)
{
  USBD_StatusTypeDef status;

  status = USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
  if (status == USBD_OK)
  {
    status = USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
  }
  if (status == USBD_OK)
  {
    status = (USBD_StatusTypeDef)USBD_CDC_RegisterInterface(&hUsbDeviceFS,
                                                            &USBD_Interface_fops_FS);
  }
  if (status == USBD_OK)
  {
    status = USBD_Start(&hUsbDeviceFS);
  }
  if (status != USBD_OK)
  {
    (void)USBD_DeInit(&hUsbDeviceFS);
  }
  return status;
}

USBD_StatusTypeDef MX_USB_DEVICE_DeInit(void)
{
  return USBD_DeInit(&hUsbDeviceFS);
}
