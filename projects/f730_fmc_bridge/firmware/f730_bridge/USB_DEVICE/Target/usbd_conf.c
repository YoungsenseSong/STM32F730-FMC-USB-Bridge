#include "usbd_core.h"
#include "usb_otg.h"

static USBD_StatusTypeDef USBD_FromHalStatus(HAL_StatusTypeDef status)
{
  switch (status)
  {
    case HAL_OK:
      return USBD_OK;
    case HAL_BUSY:
      return USBD_BUSY;
    case HAL_ERROR:
    case HAL_TIMEOUT:
    default:
      return USBD_FAIL;
  }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData,
                       epnum,
                       hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData,
                      epnum,
                      hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

  if (hpcd->Init.speed != PCD_SPEED_FULL)
  {
    Error_Handler();
  }
  USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, speed);
  USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
  __HAL_PCD_GATE_PHYCLOCK(hpcd);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
  __HAL_PCD_UNGATE_PHYCLOCK(hpcd);
  USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  if ((pdev == NULL) || (pdev->id != DEVICE_FS) ||
      (HAL_PCD_GetState(&hpcd_USB_OTG_FS) != HAL_PCD_STATE_READY))
  {
    return USBD_FAIL;
  }

  hpcd_USB_OTG_FS.pData = pdev;
  pdev->pData = &hpcd_USB_OTG_FS;

  HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x60U);
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x20U);
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x40U);
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2U, 0x10U);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  return USBD_FromHalStatus(HAL_PCD_DeInit((PCD_HandleTypeDef *)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  return USBD_FromHalStatus(HAL_PCD_Start((PCD_HandleTypeDef *)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  return USBD_FromHalStatus(HAL_PCD_Stop((PCD_HandleTypeDef *)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev,
                                  uint8_t ep_addr,
                                  uint8_t ep_type,
                                  uint16_t ep_mps)
{
  return USBD_FromHalStatus(HAL_PCD_EP_Open((PCD_HandleTypeDef *)pdev->pData,
                                            ep_addr, ep_mps, ep_type));
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return USBD_FromHalStatus(HAL_PCD_EP_Close((PCD_HandleTypeDef *)pdev->pData,
                                             ep_addr));
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return USBD_FromHalStatus(HAL_PCD_EP_Flush((PCD_HandleTypeDef *)pdev->pData,
                                             ep_addr));
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return USBD_FromHalStatus(HAL_PCD_EP_SetStall((PCD_HandleTypeDef *)pdev->pData,
                                                ep_addr));
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return USBD_FromHalStatus(HAL_PCD_EP_ClrStall((PCD_HandleTypeDef *)pdev->pData,
                                                ep_addr));
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;
  if ((ep_addr & 0x80U) != 0U)
  {
    return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
  }
  return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev,
                                        uint8_t dev_addr)
{
  return USBD_FromHalStatus(HAL_PCD_SetAddress((PCD_HandleTypeDef *)pdev->pData,
                                               dev_addr));
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev,
                                    uint8_t ep_addr,
                                    uint8_t *pbuf,
                                    uint32_t size)
{
  return USBD_FromHalStatus(HAL_PCD_EP_Transmit((PCD_HandleTypeDef *)pdev->pData,
                                                ep_addr, pbuf, size));
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,
                                          uint8_t ep_addr,
                                          uint8_t *pbuf,
                                          uint32_t size)
{
  return USBD_FromHalStatus(HAL_PCD_EP_Receive((PCD_HandleTypeDef *)pdev->pData,
                                               ep_addr, pbuf, size));
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}
