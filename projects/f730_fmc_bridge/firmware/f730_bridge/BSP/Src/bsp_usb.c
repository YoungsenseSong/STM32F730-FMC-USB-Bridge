#include "bsp_usb.h"

#include "bridge_protocol.h"
#include "bridge_usb_identity.h"
#include "usb_device.h"
#include "usb_otg.h"
#include "usbd_cdc.h"

#include <string.h>

#define BSP_USB_RX_RING_BYTES       1024U
#define BSP_USB_RX_RING_MASK        (BSP_USB_RX_RING_BYTES - 1U)
#define BSP_USB_COMMAND_QUEUE_DEPTH 4U

typedef enum
{
  BSP_USB_WIRE_SEEK_MAGIC = 0,
  BSP_USB_WIRE_HEADER,
  BSP_USB_WIRE_PAYLOAD
} BSP_USB_WireState;

typedef enum
{
  BSP_USB_TX_OWNER_NONE = 0,
  BSP_USB_TX_OWNER_ECHO,
  BSP_USB_TX_OWNER_FRAME_HEADER,
  BSP_USB_TX_OWNER_FRAME_PAYLOAD
} BSP_USB_TxOwner;

static int8_t BSP_USB_InterfaceInit(void);
static int8_t BSP_USB_InterfaceDeInit(void);
static int8_t BSP_USB_InterfaceControl(uint8_t command, uint8_t *data,
                                       uint16_t length);
static int8_t BSP_USB_InterfaceReceive(uint8_t *data, uint32_t *length);
static int8_t BSP_USB_InterfaceTxComplete(uint8_t *data, uint32_t *length,
                                          uint8_t endpoint);
static bool BSP_USB_ArmReceive(void);
static bool BSP_USB_StartTransmit(uint8_t *data, uint32_t length);
static void BSP_USB_ProcessEcho(void);
static void BSP_USB_ProcessWireProtocol(void);
static uint16_t BSP_USB_RingUsed(void);
static uint16_t BSP_USB_RingFree(void);
static bool BSP_USB_RingPop(uint8_t *value);
static void BSP_USB_ResetWireParser(void);
static bool BSP_USB_EnqueueCommand(const Bridge_UsbHeader *header,
                                   const uint8_t *payload);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  BSP_USB_InterfaceInit,
  BSP_USB_InterfaceDeInit,
  BSP_USB_InterfaceControl,
  BSP_USB_InterfaceReceive,
  BSP_USB_InterfaceTxComplete
};

static bool s_pcd_ready;
static bool s_device_started;
static volatile bool s_class_active;
static volatile bool s_rx_paused;

static uint8_t s_rx_packet[BSP_USB_FS_MAX_PACKET_BYTES];
static uint8_t s_rx_ring[BSP_USB_RX_RING_BYTES];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

static BSP_USB_WireState s_wire_state;
static uint8_t s_wire_header[BRIDGE_USB_HEADER_BYTES];
static uint32_t s_wire_header_used;
static Bridge_UsbHeader s_wire_decoded;
static uint8_t s_wire_payload[BSP_USB_MAX_COMMAND_BYTES];
static uint32_t s_wire_payload_used;

static BSP_USB_Command s_command_queue[BSP_USB_COMMAND_QUEUE_DEPTH];
static uint32_t s_command_read;
static uint32_t s_command_write;
static uint32_t s_command_count;

static volatile BSP_USB_TxState s_tx_state;
static volatile BSP_USB_TxOwner s_tx_owner;
static uint8_t s_echo_tx_packet[BSP_USB_FS_MAX_PACKET_BYTES];
static uint16_t s_echo_tx_bytes;
static uint8_t s_tx_header[BRIDGE_USB_HEADER_BYTES];
static uint8_t *s_tx_payload;
static uint32_t s_tx_payload_bytes;
static uint8_t s_line_coding[7] =
{
  0x00U, 0xC2U, 0x01U, 0x00U,
  0x00U,
  0x00U,
  0x08U
};
static BSP_USB_Stats s_stats;

BSP_Status BSP_USB_Init(void)
{
  s_pcd_ready = (HAL_PCD_GetState(&hpcd_USB_OTG_FS) == HAL_PCD_STATE_READY);
  s_device_started = false;
  s_class_active = false;
  s_rx_paused = true;
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_command_read = 0U;
  s_command_write = 0U;
  s_command_count = 0U;
  s_tx_state = BSP_USB_TX_IDLE;
  s_tx_owner = BSP_USB_TX_OWNER_NONE;
  s_echo_tx_bytes = 0U;
  s_tx_payload = NULL;
  s_tx_payload_bytes = 0U;
  (void)memset(&s_stats, 0, sizeof(s_stats));
  BSP_USB_ResetWireParser();

  if (!s_pcd_ready)
  {
    return BSP_STATUS_ERROR;
  }
  if (!Bridge_USB_IdentityIsConfigured())
  {
    return BSP_STATUS_UNSUPPORTED;
  }
  if (MX_USB_DEVICE_Init() != USBD_OK)
  {
    return BSP_STATUS_ERROR;
  }
  s_device_started = true;
  return BSP_STATUS_OK;
}

void BSP_USB_Process(void)
{
  if (BSP_USB_CDC_ECHO_ENABLED != 0U)
  {
    BSP_USB_ProcessEcho();
  }
  else
  {
    BSP_USB_ProcessWireProtocol();
  }

  if (s_class_active && s_rx_paused &&
      (BSP_USB_RingFree() >= BSP_USB_FS_MAX_PACKET_BYTES))
  {
    if (BSP_USB_ArmReceive())
    {
      s_rx_paused = false;
    }
  }
}

BSP_USB_Capability BSP_USB_GetCapability(void)
{
  BSP_USB_Capability capability;

  capability.pcd_ready = s_pcd_ready;
  capability.identity_configured = Bridge_USB_IdentityIsConfigured();
  capability.identity_test_only = BRIDGE_USB_IDENTITY_TEST_ONLY != 0U;
  capability.cdc_fs_compiled = true;
  capability.cdc_echo_enabled = BSP_USB_CDC_ECHO_ENABLED != 0U;
  capability.device_started = s_device_started;
  capability.host_configured = s_class_active &&
      (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
  capability.max_packet_bytes = BSP_USB_FS_MAX_PACKET_BYTES;
  return capability;
}

BSP_USB_Stats BSP_USB_GetStats(void)
{
  return s_stats;
}

BSP_Status BSP_USB_TransmitFrame(uint16_t type,
                                 uint32_t packet_sequence,
                                 uint32_t message_id,
                                 const uint8_t *payload,
                                 size_t payload_bytes)
{
  Bridge_ProtocolStatus protocol_status;

  if (BSP_USB_CDC_ECHO_ENABLED != 0U)
  {
    return BSP_STATUS_UNSUPPORTED;
  }
  if (!s_class_active || (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
  {
    return BSP_STATUS_UNSUPPORTED;
  }
  if ((s_tx_state != BSP_USB_TX_IDLE) ||
      (s_tx_owner != BSP_USB_TX_OWNER_NONE))
  {
    return BSP_STATUS_BUSY;
  }
  protocol_status = Bridge_UsbBuildHeader(s_tx_header, type, packet_sequence,
                                          message_id, payload, payload_bytes);
  if (protocol_status != BRIDGE_PROTOCOL_OK)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  s_tx_payload = (uint8_t *)(uintptr_t)payload;
  s_tx_payload_bytes = (uint32_t)payload_bytes;
  s_tx_state = BSP_USB_TX_BUSY;
  s_tx_owner = BSP_USB_TX_OWNER_FRAME_HEADER;
  if (!BSP_USB_StartTransmit(s_tx_header, sizeof(s_tx_header)))
  {
    s_tx_owner = BSP_USB_TX_OWNER_NONE;
    s_tx_state = BSP_USB_TX_ERROR;
    ++s_stats.tx_errors;
    return BSP_STATUS_ERROR;
  }
  ++s_stats.tx_frames;
  s_stats.tx_bytes += (uint32_t)(BRIDGE_USB_HEADER_BYTES + payload_bytes);
  return BSP_STATUS_OK;
}

BSP_USB_TxState BSP_USB_GetTxState(void)
{
  return s_tx_state;
}

void BSP_USB_ClearTxResult(void)
{
  if ((s_tx_state == BSP_USB_TX_COMPLETE) || (s_tx_state == BSP_USB_TX_ERROR))
  {
    s_tx_state = BSP_USB_TX_IDLE;
    s_tx_owner = BSP_USB_TX_OWNER_NONE;
    s_tx_payload = NULL;
    s_tx_payload_bytes = 0U;
  }
}

bool BSP_USB_TakeCommand(BSP_USB_Command *command)
{
  if ((BSP_USB_CDC_ECHO_ENABLED != 0U) ||
      (command == NULL) || (s_command_count == 0U))
  {
    return false;
  }
  *command = s_command_queue[s_command_read];
  s_command_read = (s_command_read + 1U) % BSP_USB_COMMAND_QUEUE_DEPTH;
  --s_command_count;
  return true;
}

static int8_t BSP_USB_InterfaceInit(void)
{
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_rx_paused = false;
  s_tx_state = BSP_USB_TX_IDLE;
  s_tx_owner = BSP_USB_TX_OWNER_NONE;
  s_class_active = true;
  if (!BSP_USB_ArmReceive())
  {
    s_rx_paused = true;
    return (int8_t)USBD_FAIL;
  }
  return (int8_t)USBD_OK;
}

static int8_t BSP_USB_InterfaceDeInit(void)
{
  s_class_active = false;
  s_rx_paused = true;
  s_rx_head = 0U;
  s_rx_tail = 0U;
  ++s_stats.disconnects;
  if ((s_tx_state == BSP_USB_TX_BUSY) &&
      ((s_tx_owner == BSP_USB_TX_OWNER_FRAME_HEADER) ||
       (s_tx_owner == BSP_USB_TX_OWNER_FRAME_PAYLOAD)))
  {
    s_tx_state = BSP_USB_TX_ERROR;
    ++s_stats.tx_errors;
  }
  else
  {
    s_tx_state = BSP_USB_TX_IDLE;
  }
  s_tx_owner = BSP_USB_TX_OWNER_NONE;
  return (int8_t)USBD_OK;
}

static int8_t BSP_USB_InterfaceControl(uint8_t command, uint8_t *data,
                                       uint16_t length)
{
  if (data == NULL)
  {
    return (int8_t)USBD_FAIL;
  }
  if ((command == CDC_SET_LINE_CODING) && (length == sizeof(s_line_coding)))
  {
    (void)memcpy(s_line_coding, data, sizeof(s_line_coding));
  }
  else if ((command == CDC_GET_LINE_CODING) &&
           (length == sizeof(s_line_coding)))
  {
    (void)memcpy(data, s_line_coding, sizeof(s_line_coding));
  }
  return (int8_t)USBD_OK;
}

static int8_t BSP_USB_InterfaceReceive(uint8_t *data, uint32_t *length)
{
  uint32_t index;

  if ((data == NULL) || (length == NULL) ||
      (*length > BSP_USB_FS_MAX_PACKET_BYTES) ||
      (BSP_USB_RingFree() < *length))
  {
    s_rx_paused = true;
    ++s_stats.rx_backpressure_events;
    return (int8_t)USBD_BUSY;
  }
  for (index = 0U; index < *length; ++index)
  {
    s_rx_ring[s_rx_head] = data[index];
    s_rx_head = (uint16_t)((s_rx_head + 1U) & BSP_USB_RX_RING_MASK);
  }
  ++s_stats.rx_packets;
  s_stats.rx_bytes += *length;

  if (BSP_USB_RingFree() >= BSP_USB_FS_MAX_PACKET_BYTES)
  {
    if (!BSP_USB_ArmReceive())
    {
      s_rx_paused = true;
      ++s_stats.rx_backpressure_events;
    }
  }
  else
  {
    s_rx_paused = true;
    ++s_stats.rx_backpressure_events;
  }
  return (int8_t)USBD_OK;
}

static int8_t BSP_USB_InterfaceTxComplete(uint8_t *data, uint32_t *length,
                                          uint8_t endpoint)
{
  UNUSED(data);
  UNUSED(length);
  UNUSED(endpoint);

  if (s_tx_owner == BSP_USB_TX_OWNER_ECHO)
  {
    s_rx_tail = (uint16_t)((s_rx_tail + s_echo_tx_bytes) & BSP_USB_RX_RING_MASK);
    ++s_stats.echo_packets;
    s_stats.echo_bytes += s_echo_tx_bytes;
    ++s_stats.tx_frames;
    s_stats.tx_bytes += s_echo_tx_bytes;
    s_echo_tx_bytes = 0U;
    s_tx_owner = BSP_USB_TX_OWNER_NONE;
    return (int8_t)USBD_OK;
  }

  if (s_tx_owner == BSP_USB_TX_OWNER_FRAME_HEADER)
  {
    if (s_tx_payload_bytes == 0U)
    {
      s_tx_owner = BSP_USB_TX_OWNER_NONE;
      s_tx_state = BSP_USB_TX_COMPLETE;
    }
    else
    {
      s_tx_owner = BSP_USB_TX_OWNER_FRAME_PAYLOAD;
      if (!BSP_USB_StartTransmit(s_tx_payload, s_tx_payload_bytes))
      {
        s_tx_owner = BSP_USB_TX_OWNER_NONE;
        s_tx_state = BSP_USB_TX_ERROR;
        ++s_stats.tx_errors;
      }
    }
  }
  else if (s_tx_owner == BSP_USB_TX_OWNER_FRAME_PAYLOAD)
  {
    s_tx_owner = BSP_USB_TX_OWNER_NONE;
    s_tx_state = BSP_USB_TX_COMPLETE;
  }
  return (int8_t)USBD_OK;
}

static bool BSP_USB_ArmReceive(void)
{
  if (!s_class_active)
  {
    return false;
  }
  if (USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_packet) != (uint8_t)USBD_OK)
  {
    return false;
  }
  return USBD_CDC_ReceivePacket(&hUsbDeviceFS) == (uint8_t)USBD_OK;
}

static bool BSP_USB_StartTransmit(uint8_t *data, uint32_t length)
{
  if ((data == NULL) || !s_class_active ||
      (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
  {
    return false;
  }
  if (USBD_CDC_SetTxBuffer(&hUsbDeviceFS, data, length) != (uint8_t)USBD_OK)
  {
    return false;
  }
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS) == (uint8_t)USBD_OK;
}

static void BSP_USB_ProcessEcho(void)
{
  uint16_t used;
  uint16_t index;

  if (!s_class_active || (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) ||
      (s_tx_owner != BSP_USB_TX_OWNER_NONE))
  {
    return;
  }
  used = BSP_USB_RingUsed();
  if (used == 0U)
  {
    return;
  }
  s_echo_tx_bytes = (used > BSP_USB_FS_MAX_PACKET_BYTES) ?
                    BSP_USB_FS_MAX_PACKET_BYTES : used;
  for (index = 0U; index < s_echo_tx_bytes; ++index)
  {
    s_echo_tx_packet[index] =
        s_rx_ring[(s_rx_tail + index) & BSP_USB_RX_RING_MASK];
  }
  s_tx_owner = BSP_USB_TX_OWNER_ECHO;
  if (!BSP_USB_StartTransmit(s_echo_tx_packet, s_echo_tx_bytes))
  {
    s_tx_owner = BSP_USB_TX_OWNER_NONE;
    s_echo_tx_bytes = 0U;
    ++s_stats.tx_errors;
  }
}

static void BSP_USB_ProcessWireProtocol(void)
{
  static const uint8_t magic_bytes[4] = {'U', 'B', 'R', '1'};
  uint8_t value;

  while (s_command_count < BSP_USB_COMMAND_QUEUE_DEPTH)
  {
    if (!BSP_USB_RingPop(&value))
    {
      break;
    }

    if (s_wire_state == BSP_USB_WIRE_SEEK_MAGIC)
    {
      if (value == magic_bytes[s_wire_header_used])
      {
        s_wire_header[s_wire_header_used++] = value;
        if (s_wire_header_used == 4U)
        {
          s_wire_state = BSP_USB_WIRE_HEADER;
        }
      }
      else
      {
        s_wire_header_used = (value == magic_bytes[0]) ? 1U : 0U;
        if (s_wire_header_used != 0U)
        {
          s_wire_header[0] = value;
        }
        ++s_stats.rx_framing_errors;
      }
      continue;
    }

    if (s_wire_state == BSP_USB_WIRE_HEADER)
    {
      s_wire_header[s_wire_header_used++] = value;
      if (s_wire_header_used == BRIDGE_USB_HEADER_BYTES)
      {
        Bridge_ProtocolStatus protocol_status =
            Bridge_UsbHeaderDecode(s_wire_header, sizeof(s_wire_header),
                                   &s_wire_decoded);
        if ((protocol_status != BRIDGE_PROTOCOL_OK) ||
            (s_wire_decoded.type != BRIDGE_USB_TYPE_CMD) ||
            (s_wire_decoded.payload_bytes > BSP_USB_MAX_COMMAND_BYTES))
        {
          ++s_stats.rx_framing_errors;
          BSP_USB_ResetWireParser();
        }
        else if (s_wire_decoded.payload_bytes == 0U)
        {
          if (s_wire_decoded.payload_crc32 != Bridge_CRC32_IEEE(NULL, 0U))
          {
            ++s_stats.rx_crc_errors;
          }
          else
          {
            (void)BSP_USB_EnqueueCommand(&s_wire_decoded, NULL);
          }
          BSP_USB_ResetWireParser();
        }
        else
        {
          s_wire_payload_used = 0U;
          s_wire_state = BSP_USB_WIRE_PAYLOAD;
        }
      }
      continue;
    }

    s_wire_payload[s_wire_payload_used++] = value;
    if (s_wire_payload_used == s_wire_decoded.payload_bytes)
    {
      if (Bridge_CRC32_IEEE(s_wire_payload, s_wire_payload_used) !=
          s_wire_decoded.payload_crc32)
      {
        ++s_stats.rx_crc_errors;
      }
      else
      {
        (void)BSP_USB_EnqueueCommand(&s_wire_decoded, s_wire_payload);
      }
      BSP_USB_ResetWireParser();
    }
  }
}

static uint16_t BSP_USB_RingUsed(void)
{
  return (uint16_t)((s_rx_head - s_rx_tail) & BSP_USB_RX_RING_MASK);
}

static uint16_t BSP_USB_RingFree(void)
{
  return (uint16_t)(BSP_USB_RX_RING_MASK - BSP_USB_RingUsed());
}

static bool BSP_USB_RingPop(uint8_t *value)
{
  if ((value == NULL) || (s_rx_tail == s_rx_head))
  {
    return false;
  }
  *value = s_rx_ring[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1U) & BSP_USB_RX_RING_MASK);
  return true;
}

static void BSP_USB_ResetWireParser(void)
{
  s_wire_state = BSP_USB_WIRE_SEEK_MAGIC;
  s_wire_header_used = 0U;
  s_wire_payload_used = 0U;
  (void)memset(&s_wire_decoded, 0, sizeof(s_wire_decoded));
}

static bool BSP_USB_EnqueueCommand(const Bridge_UsbHeader *header,
                                   const uint8_t *payload)
{
  BSP_USB_Command *command;

  if ((header == NULL) || (s_command_count >= BSP_USB_COMMAND_QUEUE_DEPTH))
  {
    return false;
  }
  command = &s_command_queue[s_command_write];
  command->packet_sequence = header->packet_sequence;
  command->command_id = header->message_id;
  command->payload_bytes = header->payload_bytes;
  if (header->payload_bytes != 0U)
  {
    (void)memcpy(command->payload, payload, header->payload_bytes);
  }
  s_command_write = (s_command_write + 1U) % BSP_USB_COMMAND_QUEUE_DEPTH;
  ++s_command_count;
  ++s_stats.commands_accepted;
  return true;
}
