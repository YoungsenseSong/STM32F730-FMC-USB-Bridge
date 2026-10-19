#include "bsp_bridge.h"

#include "bridge_protocol.h"
#include "bsp_fmc.h"
#include "bsp_usb.h"

#include <string.h>

#define BSP_BRIDGE_READ_CHUNK_BYTES 512U
#define BSP_BRIDGE_MAX_READ_RETRIES 3U

static void BSP_Bridge_SetState(BSP_BridgeState state);
static void BSP_Bridge_HandleWaitBlock(void);
static void BSP_Bridge_HandleReadBlock(void);
static void BSP_Bridge_HandleDataTx(void);
static void BSP_Bridge_HandleCommandTx(void);
static void BSP_Bridge_HandleAck(void);
static void BSP_Bridge_RecordReadFailure(Bridge_ProtocolStatus protocol_status);
static void BSP_Bridge_StoreLe32(uint8_t output[4], uint32_t value);

static BSP_BridgeStats s_stats;
static uint8_t s_block[BSP_FPGA_MAX_BLOCK_BYTES];
static uint32_t s_block_bytes;
static uint32_t s_read_offset;
static uint32_t s_read_retries;
static Bridge_BlockInfo s_block_info;
static bool s_have_last_acked_sequence;
static uint32_t s_last_acked_sequence;
static uint32_t s_usb_packet_sequence;
static BSP_USB_Command s_command;
static uint8_t s_command_response[4];
static BSP_BridgeState s_resume_state;

BSP_Status BSP_Bridge_Init(void)
{
  (void)memset(&s_stats, 0, sizeof(s_stats));
  s_stats.state = BSP_BRIDGE_WAIT_USB;
  s_stats.last_protocol_status = BRIDGE_PROTOCOL_OK;
  s_block_bytes = 0U;
  s_read_offset = 0U;
  s_read_retries = 0U;
  s_have_last_acked_sequence = false;
  s_last_acked_sequence = 0U;
  s_usb_packet_sequence = 0U;
  s_resume_state = BSP_BRIDGE_WAIT_BLOCK;
  return BSP_STATUS_OK;
}

void BSP_Bridge_Process(void)
{
  BSP_USB_Capability usb = BSP_USB_GetCapability();

  if (usb.cdc_echo_enabled)
  {
    BSP_Bridge_SetState(BSP_BRIDGE_WAIT_USB);
    return;
  }

  if (!usb.host_configured)
  {
    if (s_stats.state != BSP_BRIDGE_WAIT_USB)
    {
      s_resume_state = s_stats.state;
      if ((s_stats.state == BSP_BRIDGE_TX_DATA) ||
          (s_stats.state == BSP_BRIDGE_TX_COMMAND_RESPONSE))
      {
        ++s_stats.usb_errors;
        BSP_USB_ClearTxResult();
        if (s_stats.state == BSP_BRIDGE_TX_COMMAND_RESPONSE)
        {
          /* The host can retry the idempotent command after reconnect. */
          s_resume_state = BSP_BRIDGE_WAIT_BLOCK;
        }
      }
      BSP_Bridge_SetState(BSP_BRIDGE_WAIT_USB);
    }
    return;
  }

  if (s_stats.state == BSP_BRIDGE_WAIT_USB)
  {
    BSP_Bridge_SetState(s_resume_state);
  }

  switch (s_stats.state)
  {
    case BSP_BRIDGE_WAIT_BLOCK:
      BSP_Bridge_HandleWaitBlock();
      break;
    case BSP_BRIDGE_READ_BLOCK:
      BSP_Bridge_HandleReadBlock();
      break;
    case BSP_BRIDGE_TX_DATA:
      BSP_Bridge_HandleDataTx();
      break;
    case BSP_BRIDGE_TX_COMMAND_RESPONSE:
      BSP_Bridge_HandleCommandTx();
      break;
    case BSP_BRIDGE_ACK_BLOCK:
      BSP_Bridge_HandleAck();
      break;
    case BSP_BRIDGE_FAULT:
    case BSP_BRIDGE_WAIT_USB:
    default:
      break;
  }
}

BSP_BridgeStats BSP_Bridge_GetStats(void)
{
  return s_stats;
}

static void BSP_Bridge_SetState(BSP_BridgeState state)
{
  s_stats.state = state;
}

static void BSP_Bridge_HandleWaitBlock(void)
{
  BSP_FMC_BlockSnapshot snapshot;
  Bridge_ProtocolStatus protocol_status;

  if (BSP_USB_TakeCommand(&s_command))
  {
    BSP_Bridge_StoreLe32(s_command_response,
                        (uint32_t)(int32_t)BSP_STATUS_UNSUPPORTED);
    if (BSP_USB_TransmitFrame(BRIDGE_USB_TYPE_RSP, s_usb_packet_sequence,
                              s_command.command_id, s_command_response,
                              sizeof(s_command_response)) == BSP_STATUS_OK)
    {
      ++s_usb_packet_sequence;
      ++s_stats.unsupported_commands;
      BSP_Bridge_SetState(BSP_BRIDGE_TX_COMMAND_RESPONSE);
    }
    return;
  }

  if (BSP_FMC_ReadBlockSnapshot(&snapshot) != BSP_STATUS_OK)
  {
    ++s_stats.fmc_errors;
    return;
  }
  if ((snapshot.status_raw & BSP_FMC_BLOCK_STATUS_READY) == 0U)
  {
    return;
  }
  if ((snapshot.length_raw < BSP_FPGA_BLOCK_HEADER_BYTES) ||
      (snapshot.length_raw > BSP_FPGA_MAX_BLOCK_BYTES))
  {
    BSP_Bridge_RecordReadFailure(BRIDGE_PROTOCOL_BAD_LENGTH);
    return;
  }

  s_block_bytes = snapshot.length_raw;
  s_stats.block_owned = true;
  if (BSP_FMC_ReadDataWindow(0U, s_block,
                             BSP_FPGA_BLOCK_HEADER_BYTES) != BSP_STATUS_OK)
  {
    BSP_Bridge_RecordReadFailure(BRIDGE_PROTOCOL_BAD_ARGUMENT);
    return;
  }
  protocol_status = Bridge_BlockHeaderDecode(s_block,
                                             BSP_FPGA_BLOCK_HEADER_BYTES,
                                             &s_block_info);
  if ((protocol_status != BRIDGE_PROTOCOL_OK) ||
      (s_block_bytes != (BRIDGE_BLOCK_HEADER_BYTES + s_block_info.payload_bytes)))
  {
    BSP_Bridge_RecordReadFailure((protocol_status != BRIDGE_PROTOCOL_OK) ?
                                 protocol_status : BRIDGE_PROTOCOL_BAD_LENGTH);
    return;
  }

  s_stats.last_block_sequence = s_block_info.block_sequence;
  if (s_have_last_acked_sequence &&
      (s_block_info.block_sequence == s_last_acked_sequence))
  {
    ++s_stats.duplicate_blocks;
    BSP_Bridge_SetState(BSP_BRIDGE_ACK_BLOCK);
    return;
  }

  s_read_offset = BSP_FPGA_BLOCK_HEADER_BYTES;
  BSP_Bridge_SetState(BSP_BRIDGE_READ_BLOCK);
}

static void BSP_Bridge_HandleReadBlock(void)
{
  uint32_t remaining = s_block_bytes - s_read_offset;
  uint32_t chunk = (remaining > BSP_BRIDGE_READ_CHUNK_BYTES) ?
                   BSP_BRIDGE_READ_CHUNK_BYTES : remaining;

  if (remaining != 0U)
  {
    if (BSP_FMC_ReadDataWindow(s_read_offset, &s_block[s_read_offset],
                               chunk) != BSP_STATUS_OK)
    {
      BSP_Bridge_RecordReadFailure(BRIDGE_PROTOCOL_BAD_ARGUMENT);
      return;
    }
    s_read_offset += chunk;
    return;
  }

  s_stats.last_protocol_status = Bridge_BlockValidate(s_block, s_block_bytes,
                                                       true, &s_block_info);
  if (s_stats.last_protocol_status != BRIDGE_PROTOCOL_OK)
  {
    BSP_Bridge_RecordReadFailure((Bridge_ProtocolStatus)s_stats.last_protocol_status);
    return;
  }
  ++s_stats.blocks_validated;
  s_read_retries = 0U;
  BSP_Bridge_SetState(BSP_BRIDGE_TX_DATA);
}

static void BSP_Bridge_HandleDataTx(void)
{
  BSP_USB_TxState tx_state = BSP_USB_GetTxState();

  if (tx_state == BSP_USB_TX_IDLE)
  {
    BSP_Status status = BSP_USB_TransmitFrame(BRIDGE_USB_TYPE_DATA,
                                              s_usb_packet_sequence,
                                              s_block_info.block_sequence,
                                              s_block, s_block_bytes);
    if (status == BSP_STATUS_OK)
    {
      ++s_usb_packet_sequence;
      return;
    }
    if (status != BSP_STATUS_BUSY)
    {
      ++s_stats.usb_errors;
      s_resume_state = BSP_BRIDGE_TX_DATA;
      BSP_Bridge_SetState(BSP_BRIDGE_WAIT_USB);
    }
    return;
  }
  if (tx_state == BSP_USB_TX_COMPLETE)
  {
    BSP_USB_ClearTxResult();
    ++s_stats.blocks_sent;
    BSP_Bridge_SetState(BSP_BRIDGE_ACK_BLOCK);
  }
  else if (tx_state == BSP_USB_TX_ERROR)
  {
    BSP_USB_ClearTxResult();
    ++s_stats.usb_errors;
    s_resume_state = BSP_BRIDGE_TX_DATA;
    BSP_Bridge_SetState(BSP_BRIDGE_WAIT_USB);
  }
}

static void BSP_Bridge_HandleCommandTx(void)
{
  BSP_USB_TxState tx_state = BSP_USB_GetTxState();

  if (tx_state == BSP_USB_TX_COMPLETE)
  {
    BSP_USB_ClearTxResult();
    BSP_Bridge_SetState(BSP_BRIDGE_WAIT_BLOCK);
  }
  else if (tx_state == BSP_USB_TX_ERROR)
  {
    BSP_USB_ClearTxResult();
    ++s_stats.usb_errors;
    s_resume_state = BSP_BRIDGE_WAIT_BLOCK;
    BSP_Bridge_SetState(BSP_BRIDGE_WAIT_USB);
  }
}

static void BSP_Bridge_HandleAck(void)
{
  if (BSP_FMC_AcknowledgeBlock(s_block_info.block_sequence) != BSP_STATUS_OK)
  {
    ++s_stats.fmc_errors;
    BSP_Bridge_SetState(BSP_BRIDGE_FAULT);
    return;
  }

  s_last_acked_sequence = s_block_info.block_sequence;
  s_have_last_acked_sequence = true;
  ++s_stats.blocks_acked;
  s_stats.block_owned = false;
  s_block_bytes = 0U;
  BSP_Bridge_SetState(BSP_BRIDGE_WAIT_BLOCK);
}

static void BSP_Bridge_RecordReadFailure(Bridge_ProtocolStatus protocol_status)
{
  s_stats.last_protocol_status = protocol_status;
  ++s_stats.protocol_errors;
  ++s_read_retries;
  if (s_read_retries >= BSP_BRIDGE_MAX_READ_RETRIES)
  {
    /* Do not ACK a block that failed validation.  Board recovery needs a
       reviewed FPGA reset path, which remains unsupported. */
    BSP_Bridge_SetState(BSP_BRIDGE_FAULT);
  }
  else
  {
    BSP_Bridge_SetState(BSP_BRIDGE_WAIT_BLOCK);
  }
}

static void BSP_Bridge_StoreLe32(uint8_t output[4], uint32_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
  output[2] = (uint8_t)(value >> 16);
  output[3] = (uint8_t)(value >> 24);
}
