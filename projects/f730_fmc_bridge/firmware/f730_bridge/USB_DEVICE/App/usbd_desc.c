#include "usbd_desc.h"

#include "bridge_usb_identity.h"
#include "usbd_ctlreq.h"

#define BRIDGE_USB_LANG_ID             0x0409U
#define BRIDGE_USB_SERIAL_DESC_BYTES   26U

static uint8_t *Bridge_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *Bridge_LangIdDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *Bridge_ManufacturerDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *Bridge_ProductDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *Bridge_SerialDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *Bridge_ConfigurationDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *Bridge_InterfaceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static void Bridge_UpdateSerial(void);
static void Bridge_IntToUnicode(uint32_t value, uint8_t *buffer, uint8_t digits);

USBD_DescriptorsTypeDef FS_Desc =
{
  Bridge_DeviceDescriptor,
  Bridge_LangIdDescriptor,
  Bridge_ManufacturerDescriptor,
  Bridge_ProductDescriptor,
  Bridge_SerialDescriptor,
  Bridge_ConfigurationDescriptor,
  Bridge_InterfaceDescriptor
};

__ALIGN_BEGIN static uint8_t s_device_descriptor[USB_LEN_DEV_DESC] __ALIGN_END =
{
  USB_LEN_DEV_DESC,
  USB_DESC_TYPE_DEVICE,
  0x00U, 0x02U,
  0x02U,
  0x02U,
  0x00U,
  USB_MAX_EP0_SIZE,
  LOBYTE(BRIDGE_USB_VID), HIBYTE(BRIDGE_USB_VID),
  LOBYTE(BRIDGE_USB_PID), HIBYTE(BRIDGE_USB_PID),
  LOBYTE(BRIDGE_USB_DEVICE_RELEASE), HIBYTE(BRIDGE_USB_DEVICE_RELEASE),
  USBD_IDX_MFC_STR,
  USBD_IDX_PRODUCT_STR,
  USBD_IDX_SERIAL_STR,
  USBD_MAX_NUM_CONFIGURATION
};

__ALIGN_BEGIN static uint8_t s_lang_id_descriptor[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(BRIDGE_USB_LANG_ID), HIBYTE(BRIDGE_USB_LANG_ID)
};

__ALIGN_BEGIN static uint8_t s_string_descriptor[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;
__ALIGN_BEGIN static uint8_t s_serial_descriptor[BRIDGE_USB_SERIAL_DESC_BYTES] __ALIGN_END =
{
  BRIDGE_USB_SERIAL_DESC_BYTES,
  USB_DESC_TYPE_STRING
};

static uint8_t *Bridge_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(s_device_descriptor);
  return s_device_descriptor;
}

static uint8_t *Bridge_LangIdDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(s_lang_id_descriptor);
  return s_lang_id_descriptor;
}

static uint8_t *Bridge_ManufacturerDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)BRIDGE_USB_MANUFACTURER_STRING,
                 s_string_descriptor, length);
  return s_string_descriptor;
}

static uint8_t *Bridge_ProductDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)BRIDGE_USB_PRODUCT_STRING,
                 s_string_descriptor, length);
  return s_string_descriptor;
}

static uint8_t *Bridge_SerialDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  Bridge_UpdateSerial();
  *length = sizeof(s_serial_descriptor);
  return s_serial_descriptor;
}

static uint8_t *Bridge_ConfigurationDescriptor(USBD_SpeedTypeDef speed,
                                               uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)BRIDGE_USB_CONFIGURATION_STRING,
                 s_string_descriptor, length);
  return s_string_descriptor;
}

static uint8_t *Bridge_InterfaceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)BRIDGE_USB_INTERFACE_STRING,
                 s_string_descriptor, length);
  return s_string_descriptor;
}

static void Bridge_UpdateSerial(void)
{
  uint32_t serial0 = *(const uint32_t *)UID_BASE;
  uint32_t serial1 = *(const uint32_t *)(UID_BASE + 4U);
  uint32_t serial2 = *(const uint32_t *)(UID_BASE + 8U);

  serial0 += serial2;
  Bridge_IntToUnicode(serial0, &s_serial_descriptor[2], 8U);
  Bridge_IntToUnicode(serial1, &s_serial_descriptor[18], 4U);
}

static void Bridge_IntToUnicode(uint32_t value, uint8_t *buffer, uint8_t digits)
{
  uint8_t index;
  for (index = 0U; index < digits; ++index)
  {
    uint8_t nibble = (uint8_t)(value >> 28);
    buffer[index * 2U] = (uint8_t)((nibble < 10U) ?
                          (uint8_t)('0' + nibble) :
                          (uint8_t)('A' + nibble - 10U));
    buffer[index * 2U + 1U] = 0U;
    value <<= 4;
  }
}
