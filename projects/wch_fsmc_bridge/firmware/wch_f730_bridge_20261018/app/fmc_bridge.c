#include "bridge_port.h"

#include "bridge_protocol.h"
#include "bridge_storage.h"
#include "debug.h"

#include <stdbool.h>
#include <string.h>

#define FMC_REG_BLOCK_STATUS 0x0040UL
#define FMC_REG_BLOCK_LEN    0x0042UL
#define FMC_REG_BLOCK_ACK    0x0044UL
#define FMC_REG_FPGA_ID      0x0000UL
#define FMC_REG_PROTO_VER    0x0002UL
#define FMC_REG_GLOBAL_CTRL  0x0010UL
#define FMC_DATA_WINDOW      0x1000UL
#define FMC_BLOCK_READY      0x0001U
#define FMC_READ_CHUNK       512U
#define FMC_READ_MAX_RETRIES 2U

typedef enum
{
  FMC_BRIDGE_WAIT_BLOCK = 0,
  FMC_BRIDGE_READ_BLOCK,
  FMC_BRIDGE_TX_FRAME,
  FMC_BRIDGE_ACK_BLOCK,
  FMC_BRIDGE_FAULT
} FmcBridgeState;

static FmcBridgeState s_state;
static uint32_t s_block_bytes;
static uint32_t s_read_offset;
static uint32_t s_read_retries;
static uint32_t s_tx_offset;
static uint32_t s_packet_sequence;
static uint32_t s_last_acked_sequence;
static bool s_have_last_acked;
static bool s_usb_in_flight;
static bool s_fpga_acquisition_started;
static Bridge_BlockInfo s_block_info;

static bool fmc_read_snapshot(uint16_t *status, uint16_t *length)
{
  uint16_t status_after;

  if ((status == NULL) || (length == NULL)) return false;
  *status = extbus_read16(FMC_REG_BLOCK_STATUS);
  *length = extbus_read16(FMC_REG_BLOCK_LEN);
  status_after = extbus_read16(FMC_REG_BLOCK_STATUS);
  return *status == status_after;
}

void fmc_bridge_init(void)
{
  s_state = FMC_BRIDGE_WAIT_BLOCK;
  s_block_bytes = 0U;
  s_read_offset = 0U;
  s_read_retries = 0U;
  s_tx_offset = 0U;
  s_packet_sequence = 0U;
  s_last_acked_sequence = 0U;
  s_have_last_acked = false;
  s_usb_in_flight = false;
  s_fpga_acquisition_started = false;
  (void)memset(&s_block_info, 0, sizeof(s_block_info));
  (void)memset(g_bridge_usb_header, 0, sizeof(g_bridge_usb_header));
}

void fmc_bridge_process(void)
{
  uint16_t status;
  uint16_t length;
  size_t chunk;

  if (s_state == FMC_BRIDGE_FAULT) return;
  if (usb_get_state() == BRIDGE_USB_OFFLINE)
  {
    s_usb_in_flight = false;
    if (s_state == FMC_BRIDGE_TX_FRAME) s_state = FMC_BRIDGE_WAIT_BLOCK;
    return;
  }

  if (!s_fpga_acquisition_started)
  {
    uint16_t fpga_id = extbus_read16(FMC_REG_FPGA_ID);
    uint16_t protocol_version = extbus_read16(FMC_REG_PROTO_VER);
    if ((fpga_id == 0x5343U) && (protocol_version == 1U))
    {
      extbus_write16(FMC_REG_GLOBAL_CTRL, 0x0005U);
      s_fpga_acquisition_started = true;
    }
    else
    {
      return;
    }
  }

  switch (s_state)
  {
    case FMC_BRIDGE_WAIT_BLOCK:
      if (!fmc_read_snapshot(&status, &length)) return;
      if ((status & FMC_BLOCK_READY) == 0U) return;
      if ((length < BRIDGE_BLOCK_HEADER_BYTES) ||
          (length > BRIDGE_BLOCK_MAX_BYTES))
      {
        s_state = FMC_BRIDGE_FAULT;
        platform_log("WCH FMC bridge: invalid block length");
        return;
      }
      s_block_bytes = length;
      if (extbus_read_window(FMC_DATA_WINDOW, g_bridge_block,
                             BRIDGE_BLOCK_HEADER_BYTES) != BRIDGE_PORT_OK)
      {
        s_state = FMC_BRIDGE_FAULT;
        return;
      }
      if (Bridge_BlockHeaderDecode(g_bridge_block, BRIDGE_BLOCK_HEADER_BYTES,
                                   &s_block_info) != BRIDGE_PROTOCOL_OK ||
          s_block_bytes != BRIDGE_BLOCK_HEADER_BYTES + s_block_info.payload_bytes)
      {
        s_state = FMC_BRIDGE_FAULT;
        platform_log("WCH FMC bridge: invalid block header");
        return;
      }
      if (s_have_last_acked &&
          (s_block_info.block_sequence == s_last_acked_sequence))
      {
        s_state = FMC_BRIDGE_ACK_BLOCK;
        return;
      }
      s_read_offset = BRIDGE_BLOCK_HEADER_BYTES;
      s_read_retries = 0U;
      s_state = FMC_BRIDGE_READ_BLOCK;
      break;

    case FMC_BRIDGE_READ_BLOCK:
      if (s_read_offset < s_block_bytes)
      {
        chunk = s_block_bytes - s_read_offset;
        if (chunk > FMC_READ_CHUNK) chunk = FMC_READ_CHUNK;
        if (extbus_read_window(FMC_DATA_WINDOW + s_read_offset,
                               &g_bridge_block[s_read_offset], chunk) != BRIDGE_PORT_OK)
        {
          s_state = FMC_BRIDGE_FAULT;
          platform_log("WCH FMC bridge: FMC read failed");
          return;
        }
        s_read_offset += (uint32_t)chunk;
        break;
      }
      Bridge_ProtocolStatus validation =
          Bridge_BlockValidate(g_bridge_block, s_block_bytes, true, &s_block_info);
      if (validation != BRIDGE_PROTOCOL_OK)
      {
        if (s_read_retries < FMC_READ_MAX_RETRIES)
        {
          ++s_read_retries;
          s_read_offset = BRIDGE_BLOCK_HEADER_BYTES;
          printf("WCH FMC bridge: retry block=%lu attempt=%lu status=%d\r\n",
                 (unsigned long)s_block_info.block_sequence,
                 (unsigned long)s_read_retries, (int)validation);
          break;
        }
        s_state = FMC_BRIDGE_FAULT;
        printf("WCH FMC bridge: validation failed block=%lu status=%d\r\n",
               (unsigned long)s_block_info.block_sequence, (int)validation);
        return;
      }
      if (Bridge_UsbBuildHeader(g_bridge_usb_header, BRIDGE_USB_TYPE_DATA,
                                s_packet_sequence, s_block_info.block_sequence,
                                g_bridge_block, s_block_bytes) != BRIDGE_PROTOCOL_OK)
      {
        s_state = FMC_BRIDGE_FAULT;
        return;
      }
      s_tx_offset = 0U;
      s_usb_in_flight = false;
      s_state = FMC_BRIDGE_TX_FRAME;
      break;

    case FMC_BRIDGE_TX_FRAME:
      if (s_usb_in_flight)
      {
        if (usb_get_state() == BRIDGE_USB_BUSY) return;
        s_usb_in_flight = false;
        if (s_tx_offset >= BRIDGE_USB_HEADER_BYTES + s_block_bytes)
        {
          ++s_packet_sequence;
          s_state = FMC_BRIDGE_ACK_BLOCK;
          break;
        }
      }
      if (s_tx_offset < BRIDGE_USB_HEADER_BYTES)
      {
        chunk = BRIDGE_USB_HEADER_BYTES - s_tx_offset;
        if (chunk > 64U) chunk = 64U;
        if (usb_transmit(&g_bridge_usb_header[s_tx_offset], chunk) != BRIDGE_PORT_OK)
          return;
      }
      else
      {
        uint32_t block_offset = s_tx_offset - BRIDGE_USB_HEADER_BYTES;
        chunk = s_block_bytes - block_offset;
        if (chunk > 64U) chunk = 64U;
        if (usb_transmit(&g_bridge_block[block_offset], chunk) != BRIDGE_PORT_OK)
          return;
      }
      s_tx_offset += (uint32_t)chunk;
      s_usb_in_flight = true;
      break;

    case FMC_BRIDGE_ACK_BLOCK:
      extbus_write16(FMC_REG_BLOCK_ACK,
                     (uint16_t)(s_block_info.block_sequence & 0xFFFFU));
      s_last_acked_sequence = s_block_info.block_sequence;
      s_have_last_acked = true;
      s_block_bytes = 0U;
      s_state = FMC_BRIDGE_WAIT_BLOCK;
      break;

    case FMC_BRIDGE_FAULT:
    default:
      break;
  }
}
