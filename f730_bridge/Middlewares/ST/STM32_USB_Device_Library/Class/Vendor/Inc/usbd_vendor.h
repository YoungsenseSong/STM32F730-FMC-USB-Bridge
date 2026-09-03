#ifndef USBD_VENDOR_H
#define USBD_VENDOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"

#define VENDOR_IN_EP                    0x81U
#define VENDOR_OUT_EP                   0x01U
#define VENDOR_FS_MAX_PACKET_SIZE       64U
#define VENDOR_CONFIG_DESC_SIZE         32U

typedef struct
{
  uint8_t (*Init)(void);
  uint8_t (*DeInit)(void);
  uint8_t (*Receive)(const uint8_t *data, uint32_t length);
  void (*TxComplete)(void);
} USBD_VENDOR_ItfTypeDef;

extern USBD_ClassTypeDef USBD_VENDOR;

uint8_t USBD_VENDOR_RegisterInterface(USBD_HandleTypeDef *pdev,
                                      USBD_VENDOR_ItfTypeDef *interface_fops);
uint8_t USBD_VENDOR_Transmit(USBD_HandleTypeDef *pdev,
                             uint8_t *data,
                             uint32_t length);
uint8_t USBD_VENDOR_ResumeReceive(USBD_HandleTypeDef *pdev);
uint8_t USBD_VENDOR_IsTxBusy(const USBD_HandleTypeDef *pdev);
uint8_t USBD_VENDOR_IsConfigured(const USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif /* USBD_VENDOR_H */
