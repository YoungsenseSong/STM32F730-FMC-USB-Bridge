#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_core.h"
#include "usbd_cdc.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

USBD_StatusTypeDef MX_USB_DEVICE_Init(void);
USBD_StatusTypeDef MX_USB_DEVICE_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_DEVICE_H */
