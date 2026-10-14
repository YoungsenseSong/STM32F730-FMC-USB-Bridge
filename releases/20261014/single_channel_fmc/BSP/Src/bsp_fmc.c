#include "bsp_fmc.h"

#include "fmc.h"

static bool BSP_FMC_IsAlignedRange(uint32_t byte_offset, size_t byte_length)
{
  return ((byte_offset & 1UL) == 0UL) &&
         (byte_offset < BSP_FMC_MAPPED_BYTES) &&
         (byte_length <= (size_t)(BSP_FMC_MAPPED_BYTES - byte_offset));
}

static volatile uint16_t *BSP_FMC_Address(uint32_t byte_offset)
{
  return (volatile uint16_t *)(uintptr_t)(BSP_FMC_BASE_ADDRESS + byte_offset);
}

static bool BSP_FMC_IsWritableRegister(uint32_t byte_offset)
{
  return (byte_offset == BSP_FMC_REG_GLOBAL_CTRL) ||
         (byte_offset == BSP_FMC_REG_IRQ_STATUS) ||
         (byte_offset == BSP_FMC_REG_IRQ_MASK) ||
         (byte_offset == BSP_FMC_REG_SYNC_CTRL) ||
         (byte_offset == BSP_FMC_REG_BLOCK_ACK);
}

static uint16_t BSP_FMC_LoadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t BSP_FMC_LoadLe32(const uint8_t *data)
{
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

static uint64_t BSP_FMC_LoadLe64(const uint8_t *data)
{
  return (uint64_t)BSP_FMC_LoadLe32(data) |
         ((uint64_t)BSP_FMC_LoadLe32(data + 4) << 32);
}

BSP_Status BSP_FMC_Init(void)
{
  if ((hsram1.Instance != FMC_NORSRAM_DEVICE) ||
      (HAL_SRAM_GetState(&hsram1) != HAL_SRAM_STATE_READY))
  {
    return BSP_STATUS_ERROR;
  }
  return BSP_STATUS_OK;
}

BSP_Status BSP_FMC_Read16(uint32_t byte_offset, uint16_t *value)
{
  if (value == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }
  if (!BSP_FMC_IsAlignedRange(byte_offset, sizeof(uint16_t)))
  {
    return BSP_STATUS_OUT_OF_RANGE;
  }

  *value = *BSP_FMC_Address(byte_offset);
  __DMB();
  return BSP_STATUS_OK;
}

BSP_Status BSP_FMC_WriteCommand16(uint32_t byte_offset, uint16_t value)
{
  if (!BSP_FMC_IsWritableRegister(byte_offset))
  {
    return BSP_STATUS_UNSUPPORTED;
  }

  *BSP_FMC_Address(byte_offset) = value;
  __DSB();
  return BSP_STATUS_OK;
}

BSP_Status BSP_FMC_ReadIdentity(BSP_FMC_Identity *identity)
{
  BSP_Status status;

  if (identity == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_FMC_Read16(BSP_FMC_REG_FPGA_ID, &identity->fpga_id);
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_PROTO_VERSION, &identity->protocol_version);
  }
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_BUILD_WORD, &identity->build_word);
  }
  return status;
}

BSP_Status BSP_FMC_ReadBlockSnapshot(BSP_FMC_BlockSnapshot *snapshot)
{
  uint16_t status_after;
  BSP_Status status;

  if (snapshot == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_FMC_Read16(BSP_FMC_REG_BLOCK_STATUS, &snapshot->status_raw);
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_BLOCK_LEN, &snapshot->length_raw);
  }
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_BLOCK_STATUS, &status_after);
  }
  if ((status == BSP_STATUS_OK) && (status_after != snapshot->status_raw))
  {
    status = BSP_STATUS_BUSY;
  }
  return status;
}

BSP_Status BSP_FMC_ReadDataWindow(uint32_t window_byte_offset, uint8_t *data, size_t length)
{
  uint32_t absolute_offset;
  size_t index;

  if ((data == NULL) && (length != 0U))
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }
  if ((window_byte_offset > BSP_FPGA_MAX_BLOCK_BYTES) ||
      (length > (size_t)(BSP_FPGA_MAX_BLOCK_BYTES - window_byte_offset)))
  {
    return BSP_STATUS_OUT_OF_RANGE;
  }

  absolute_offset = BSP_FMC_DATA_WINDOW_BASE + window_byte_offset;
  if (!BSP_FMC_IsAlignedRange(absolute_offset, length))
  {
    return BSP_STATUS_OUT_OF_RANGE;
  }

  for (index = 0U; index < length; index += 2U)
  {
    uint16_t word = *BSP_FMC_Address(absolute_offset + (uint32_t)index);
    data[index] = (uint8_t)(word & 0xFFU);
    if ((index + 1U) < length)
    {
      data[index + 1U] = (uint8_t)(word >> 8);
    }
  }
  __DMB();
  return BSP_STATUS_OK;
}

BSP_Status BSP_FMC_ReadBlockHeader(BSP_FPGA_BlockHeader *header)
{
  uint8_t raw[BSP_FPGA_BLOCK_HEADER_BYTES];
  BSP_Status status;
  uint32_t channel;

  if (header == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_FMC_ReadDataWindow(0U, raw, sizeof(raw));
  if (status != BSP_STATUS_OK)
  {
    return status;
  }

  header->magic = BSP_FMC_LoadLe32(&raw[0]);
  header->version = BSP_FMC_LoadLe16(&raw[4]);
  header->header_length = BSP_FMC_LoadLe16(&raw[6]);
  header->block_sequence = BSP_FMC_LoadLe32(&raw[8]);
  header->sync_epoch = BSP_FMC_LoadLe32(&raw[12]);
  header->payload_length = BSP_FMC_LoadLe32(&raw[16]);
  header->channel_mask = BSP_FMC_LoadLe32(&raw[20]);
  header->first_fpga_tick = BSP_FMC_LoadLe64(&raw[24]);
  header->last_fpga_tick = BSP_FMC_LoadLe64(&raw[32]);
  for (channel = 0U; channel < 4U; ++channel)
  {
    header->record_count[channel] = BSP_FMC_LoadLe32(&raw[40U + (channel * 4U)]);
  }
  header->flags = BSP_FMC_LoadLe32(&raw[56]);
  header->payload_crc32 = BSP_FMC_LoadLe32(&raw[60]);
  return BSP_STATUS_OK;
}

bool BSP_FMC_BlockHeaderIsSane(const BSP_FPGA_BlockHeader *header)
{
  return (header != NULL) &&
         (header->magic == BSP_FPGA_BLOCK_MAGIC) &&
         (header->version == BSP_FPGA_PROTOCOL_VERSION) &&
         (header->header_length == BSP_FPGA_BLOCK_HEADER_BYTES) &&
         (header->payload_length != 0U) &&
         (header->payload_length <= BSP_FPGA_MAX_PAYLOAD_BYTES) &&
         ((header->payload_length % BSP_FPGA_RECORD_BYTES) == 0U) &&
         ((header->channel_mask & ~0x0FUL) == 0UL) &&
         ((header->flags & 0x0FUL) == 0UL) &&
         (header->last_fpga_tick >= header->first_fpga_tick);
}

BSP_Status BSP_FMC_AcknowledgeBlock(uint32_t block_sequence)
{
  return BSP_FMC_WriteCommand16(BSP_FMC_REG_BLOCK_ACK,
                                (uint16_t)(block_sequence & 0xFFFFUL));
}
