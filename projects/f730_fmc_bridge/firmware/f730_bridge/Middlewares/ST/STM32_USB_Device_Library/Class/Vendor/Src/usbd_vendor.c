#include "usbd_vendor.h"

#include "usbd_ctlreq.h"

typedef struct
{
  __IO uint32_t tx_busy;
  __IO uint32_t rx_armed;
  __IO uint32_t configured;
  uint8_t alternate_setting;
  __ALIGN_BEGIN uint8_t rx_buffer[VENDOR_FS_MAX_PACKET_SIZE] __ALIGN_END;
} USBD_VENDOR_HandleTypeDef;

static uint8_t USBD_VENDOR_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_VENDOR_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_VENDOR_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_VENDOR_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_VENDOR_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_VENDOR_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_VENDOR_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_VENDOR_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_VENDOR_GetDeviceQualifierDesc(uint16_t *length);
static USBD_VENDOR_HandleTypeDef *USBD_VENDOR_GetHandle(const USBD_HandleTypeDef *pdev);

static USBD_VENDOR_HandleTypeDef s_vendor_handle;
static uint16_t s_status_info;

USBD_ClassTypeDef USBD_VENDOR =
{
  USBD_VENDOR_Init,
  USBD_VENDOR_DeInit,
  USBD_VENDOR_Setup,
  NULL,
  NULL,
  USBD_VENDOR_DataIn,
  USBD_VENDOR_DataOut,
  NULL,
  NULL,
  NULL,
  USBD_VENDOR_GetHSCfgDesc,
  USBD_VENDOR_GetFSCfgDesc,
  USBD_VENDOR_GetOtherSpeedCfgDesc,
  USBD_VENDOR_GetDeviceQualifierDesc
};

__ALIGN_BEGIN static uint8_t s_config_descriptor[VENDOR_CONFIG_DESC_SIZE] __ALIGN_END =
{
  0x09U,
  USB_DESC_TYPE_CONFIGURATION,
  LOBYTE(VENDOR_CONFIG_DESC_SIZE), HIBYTE(VENDOR_CONFIG_DESC_SIZE),
  0x01U,
  0x01U,
  USBD_IDX_CONFIG_STR,
  0x80U,
  50U,

  0x09U,
  USB_DESC_TYPE_INTERFACE,
  0x00U,
  0x00U,
  0x02U,
  0xFFU,
  0x00U,
  0x00U,
  USBD_IDX_INTERFACE_STR,

  0x07U,
  USB_DESC_TYPE_ENDPOINT,
  VENDOR_OUT_EP,
  USBD_EP_TYPE_BULK,
  LOBYTE(VENDOR_FS_MAX_PACKET_SIZE), HIBYTE(VENDOR_FS_MAX_PACKET_SIZE),
  0x00U,

  0x07U,
  USB_DESC_TYPE_ENDPOINT,
  VENDOR_IN_EP,
  USBD_EP_TYPE_BULK,
  LOBYTE(VENDOR_FS_MAX_PACKET_SIZE), HIBYTE(VENDOR_FS_MAX_PACKET_SIZE),
  0x00U
};

__ALIGN_BEGIN static uint8_t s_other_speed_descriptor[VENDOR_CONFIG_DESC_SIZE] __ALIGN_END =
{
  0x09U,
  USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION,
  LOBYTE(VENDOR_CONFIG_DESC_SIZE), HIBYTE(VENDOR_CONFIG_DESC_SIZE),
  0x01U, 0x01U, USBD_IDX_CONFIG_STR, 0x80U, 50U,
  0x09U, USB_DESC_TYPE_INTERFACE, 0x00U, 0x00U, 0x02U,
  0xFFU, 0x00U, 0x00U, USBD_IDX_INTERFACE_STR,
  0x07U, USB_DESC_TYPE_ENDPOINT, VENDOR_OUT_EP, USBD_EP_TYPE_BULK,
  LOBYTE(VENDOR_FS_MAX_PACKET_SIZE), HIBYTE(VENDOR_FS_MAX_PACKET_SIZE), 0x00U,
  0x07U, USB_DESC_TYPE_ENDPOINT, VENDOR_IN_EP, USBD_EP_TYPE_BULK,
  LOBYTE(VENDOR_FS_MAX_PACKET_SIZE), HIBYTE(VENDOR_FS_MAX_PACKET_SIZE), 0x00U
};

__ALIGN_BEGIN static uint8_t s_device_qualifier[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00U, 0x02U,
  0x00U, 0x00U, 0x00U,
  USB_MAX_EP0_SIZE,
  0x01U,
  0x00U
};

static USBD_VENDOR_HandleTypeDef *USBD_VENDOR_GetHandle(const USBD_HandleTypeDef *pdev)
{
  if (pdev == NULL)
  {
    return NULL;
  }
  return (USBD_VENDOR_HandleTypeDef *)pdev->pClassDataCmsit[0];
}

static uint8_t USBD_VENDOR_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_VENDOR_ItfTypeDef *fops;
  UNUSED(cfgidx);

  USBD_memset(&s_vendor_handle, 0, sizeof(s_vendor_handle));
  pdev->pClassDataCmsit[pdev->classId] = &s_vendor_handle;
  pdev->pClassData = &s_vendor_handle;

  if (USBD_LL_OpenEP(pdev, VENDOR_IN_EP, USBD_EP_TYPE_BULK,
                     VENDOR_FS_MAX_PACKET_SIZE) != USBD_OK)
  {
    return (uint8_t)USBD_FAIL;
  }
  pdev->ep_in[VENDOR_IN_EP & 0x0FU].is_used = 1U;
  pdev->ep_in[VENDOR_IN_EP & 0x0FU].maxpacket = VENDOR_FS_MAX_PACKET_SIZE;

  if (USBD_LL_OpenEP(pdev, VENDOR_OUT_EP, USBD_EP_TYPE_BULK,
                     VENDOR_FS_MAX_PACKET_SIZE) != USBD_OK)
  {
    (void)USBD_LL_CloseEP(pdev, VENDOR_IN_EP);
    pdev->ep_in[VENDOR_IN_EP & 0x0FU].is_used = 0U;
    return (uint8_t)USBD_FAIL;
  }
  pdev->ep_out[VENDOR_OUT_EP & 0x0FU].is_used = 1U;
  pdev->ep_out[VENDOR_OUT_EP & 0x0FU].maxpacket = VENDOR_FS_MAX_PACKET_SIZE;

  fops = (USBD_VENDOR_ItfTypeDef *)pdev->pUserData[pdev->classId];
  if ((fops == NULL) || (fops->Init == NULL) || (fops->Init() != (uint8_t)USBD_OK))
  {
    (void)USBD_LL_CloseEP(pdev, VENDOR_OUT_EP);
    (void)USBD_LL_CloseEP(pdev, VENDOR_IN_EP);
    pdev->ep_out[VENDOR_OUT_EP & 0x0FU].is_used = 0U;
    pdev->ep_in[VENDOR_IN_EP & 0x0FU].is_used = 0U;
    return (uint8_t)USBD_FAIL;
  }

  s_vendor_handle.configured = 1U;
  return USBD_VENDOR_ResumeReceive(pdev);
}

static uint8_t USBD_VENDOR_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_VENDOR_ItfTypeDef *fops;
  UNUSED(cfgidx);

  (void)USBD_LL_CloseEP(pdev, VENDOR_IN_EP);
  (void)USBD_LL_CloseEP(pdev, VENDOR_OUT_EP);
  pdev->ep_in[VENDOR_IN_EP & 0x0FU].is_used = 0U;
  pdev->ep_out[VENDOR_OUT_EP & 0x0FU].is_used = 0U;

  fops = (USBD_VENDOR_ItfTypeDef *)pdev->pUserData[pdev->classId];
  if ((fops != NULL) && (fops->DeInit != NULL))
  {
    (void)fops->DeInit();
  }

  USBD_memset(&s_vendor_handle, 0, sizeof(s_vendor_handle));
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  pdev->pClassData = NULL;
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_VENDOR_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);

  if (handle == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if ((req->bmRequest & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_STANDARD)
  {
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bRequest)
  {
    case USB_REQ_GET_STATUS:
      if ((pdev->dev_state != USBD_STATE_CONFIGURED) || (req->wLength != 2U))
      {
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      s_status_info = 0U;
      (void)USBD_CtlSendData(pdev, (uint8_t *)&s_status_info, 2U);
      break;

    case USB_REQ_GET_INTERFACE:
      if ((pdev->dev_state != USBD_STATE_CONFIGURED) || (req->wLength != 1U))
      {
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      (void)USBD_CtlSendData(pdev, &handle->alternate_setting, 1U);
      break;

    case USB_REQ_SET_INTERFACE:
      if ((pdev->dev_state != USBD_STATE_CONFIGURED) || (req->wValue != 0U))
      {
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      handle->alternate_setting = 0U;
      break;

    case USB_REQ_CLEAR_FEATURE:
      break;

    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
  }
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_VENDOR_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);
  USBD_VENDOR_ItfTypeDef *fops;

  if ((handle == NULL) || (epnum != (VENDOR_IN_EP & 0x7FU)))
  {
    return (uint8_t)USBD_FAIL;
  }
  handle->tx_busy = 0U;
  fops = (USBD_VENDOR_ItfTypeDef *)pdev->pUserData[pdev->classId];
  if ((fops != NULL) && (fops->TxComplete != NULL))
  {
    fops->TxComplete();
  }
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_VENDOR_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);
  USBD_VENDOR_ItfTypeDef *fops;
  uint32_t length;
  uint8_t receive_status = (uint8_t)USBD_FAIL;

  if ((handle == NULL) || (epnum != (VENDOR_OUT_EP & 0x7FU)))
  {
    return (uint8_t)USBD_FAIL;
  }

  handle->rx_armed = 0U;
  length = USBD_LL_GetRxDataSize(pdev, VENDOR_OUT_EP);
  if (length > VENDOR_FS_MAX_PACKET_SIZE)
  {
    return (uint8_t)USBD_FAIL;
  }

  fops = (USBD_VENDOR_ItfTypeDef *)pdev->pUserData[pdev->classId];
  if ((fops != NULL) && (fops->Receive != NULL))
  {
    receive_status = fops->Receive(handle->rx_buffer, length);
  }
  if (receive_status == (uint8_t)USBD_OK)
  {
    return USBD_VENDOR_ResumeReceive(pdev);
  }
  return receive_status;
}

static uint8_t *USBD_VENDOR_GetFSCfgDesc(uint16_t *length)
{
  *length = sizeof(s_config_descriptor);
  return s_config_descriptor;
}

static uint8_t *USBD_VENDOR_GetHSCfgDesc(uint16_t *length)
{
  *length = sizeof(s_config_descriptor);
  return s_config_descriptor;
}

static uint8_t *USBD_VENDOR_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = sizeof(s_other_speed_descriptor);
  return s_other_speed_descriptor;
}

static uint8_t *USBD_VENDOR_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = sizeof(s_device_qualifier);
  return s_device_qualifier;
}

uint8_t USBD_VENDOR_RegisterInterface(USBD_HandleTypeDef *pdev,
                                      USBD_VENDOR_ItfTypeDef *interface_fops)
{
  if ((pdev == NULL) || (interface_fops == NULL))
  {
    return (uint8_t)USBD_FAIL;
  }
  pdev->pUserData[0] = interface_fops;
  return (uint8_t)USBD_OK;
}

uint8_t USBD_VENDOR_Transmit(USBD_HandleTypeDef *pdev,
                             uint8_t *data,
                             uint32_t length)
{
  USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);

  if ((handle == NULL) || ((data == NULL) && (length != 0U)) ||
      (pdev->dev_state != USBD_STATE_CONFIGURED) ||
      (handle->configured == 0U))
  {
    return (uint8_t)USBD_FAIL;
  }
  if (handle->tx_busy != 0U)
  {
    return (uint8_t)USBD_BUSY;
  }
  handle->tx_busy = 1U;
  if (USBD_LL_Transmit(pdev, VENDOR_IN_EP, data, length) != USBD_OK)
  {
    handle->tx_busy = 0U;
    return (uint8_t)USBD_FAIL;
  }
  return (uint8_t)USBD_OK;
}

uint8_t USBD_VENDOR_ResumeReceive(USBD_HandleTypeDef *pdev)
{
  USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);

  if ((handle == NULL) || (handle->configured == 0U))
  {
    return (uint8_t)USBD_FAIL;
  }
  if (handle->rx_armed != 0U)
  {
    return (uint8_t)USBD_OK;
  }
  if (USBD_LL_PrepareReceive(pdev, VENDOR_OUT_EP, handle->rx_buffer,
                             VENDOR_FS_MAX_PACKET_SIZE) != USBD_OK)
  {
    return (uint8_t)USBD_FAIL;
  }
  handle->rx_armed = 1U;
  return (uint8_t)USBD_OK;
}

uint8_t USBD_VENDOR_IsTxBusy(const USBD_HandleTypeDef *pdev)
{
  const USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);
  return (handle != NULL) ? (uint8_t)(handle->tx_busy != 0U) : 0U;
}

uint8_t USBD_VENDOR_IsConfigured(const USBD_HandleTypeDef *pdev)
{
  const USBD_VENDOR_HandleTypeDef *handle = USBD_VENDOR_GetHandle(pdev);
  return (uint8_t)((pdev != NULL) && (handle != NULL) &&
                   (pdev->dev_state == USBD_STATE_CONFIGURED) &&
                   (handle->configured != 0U));
}
