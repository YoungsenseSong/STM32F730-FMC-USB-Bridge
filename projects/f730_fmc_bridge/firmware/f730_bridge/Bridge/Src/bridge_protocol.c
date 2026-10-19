#include "bridge_protocol.h"

#define BRIDGE_RF_MAGIC 0xA55AU
#define BRIDGE_RF_MAX_SAMPLES 96U

static uint16_t Bridge_LoadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t Bridge_LoadLe32(const uint8_t *data)
{
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

static uint64_t Bridge_LoadLe64(const uint8_t *data)
{
  return (uint64_t)Bridge_LoadLe32(data) |
         ((uint64_t)Bridge_LoadLe32(data + 4) << 32);
}

static void Bridge_StoreLe16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
}

static void Bridge_StoreLe32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
  data[2] = (uint8_t)(value >> 16);
  data[3] = (uint8_t)(value >> 24);
}

uint16_t Bridge_CRC16_CCITT_FALSE(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFFU;
  size_t byte_index;

  if ((data == NULL) && (length != 0U))
  {
    return 0U;
  }

  for (byte_index = 0U; byte_index < length; ++byte_index)
  {
    uint32_t bit_index;
    crc ^= (uint16_t)((uint16_t)data[byte_index] << 8);
    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
      crc = ((crc & 0x8000U) != 0U) ?
              (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

uint32_t Bridge_CRC32_IEEE(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xFFFFFFFFUL;
  size_t byte_index;

  if ((data == NULL) && (length != 0U))
  {
    return 0U;
  }

  for (byte_index = 0U; byte_index < length; ++byte_index)
  {
    uint32_t bit_index;
    crc ^= data[byte_index];
    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
      crc = ((crc & 1UL) != 0UL) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

Bridge_ProtocolStatus Bridge_RecordValidate(const uint8_t *record,
                                            size_t length,
                                            Bridge_RecordInfo *info)
{
  uint16_t sample_count;

  if (record == NULL)
  {
    return BRIDGE_PROTOCOL_BAD_ARGUMENT;
  }
  if (length != BRIDGE_RECORD_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if (Bridge_LoadLe32(&record[0]) != BRIDGE_RECORD_MAGIC)
  {
    return BRIDGE_PROTOCOL_BAD_MAGIC;
  }
  if (record[4] != BRIDGE_RECORD_VERSION)
  {
    return BRIDGE_PROTOCOL_BAD_VERSION;
  }
  if (Bridge_LoadLe16(&record[6]) != BRIDGE_RECORD_TYPE_RF_V1)
  {
    return BRIDGE_PROTOCOL_BAD_TYPE;
  }
  if (Bridge_LoadLe16(&record[36]) != BRIDGE_RF_FRAME_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if (Bridge_CRC16_CCITT_FALSE(record, 38U) != Bridge_LoadLe16(&record[38]))
  {
    return BRIDGE_PROTOCOL_BAD_HEADER_CRC;
  }
  if (Bridge_CRC32_IEEE(&record[40], BRIDGE_RF_FRAME_BYTES) !=
      Bridge_LoadLe32(&record[244]))
  {
    return BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC;
  }
  if (Bridge_LoadLe16(&record[40]) != BRIDGE_RF_MAGIC)
  {
    return BRIDGE_PROTOCOL_BAD_FORMAT;
  }
  sample_count = Bridge_LoadLe16(&record[44]);
  if ((sample_count == 0U) || (sample_count > BRIDGE_RF_MAX_SAMPLES))
  {
    return BRIDGE_PROTOCOL_BAD_FORMAT;
  }

  if (info != NULL)
  {
    info->node_id = record[5];
    info->transport_sequence = Bridge_LoadLe32(&record[8]);
    info->sync_epoch = Bridge_LoadLe32(&record[12]);
    info->rx_tick = Bridge_LoadLe64(&record[16]);
    info->logical_sample_index = Bridge_LoadLe64(&record[24]);
    info->status_flags = Bridge_LoadLe32(&record[32]);
  }
  return BRIDGE_PROTOCOL_OK;
}

Bridge_ProtocolStatus Bridge_BlockHeaderDecode(const uint8_t *header,
                                               size_t available_bytes,
                                               Bridge_BlockInfo *info)
{
  Bridge_BlockInfo decoded;
  uint32_t channel;
  uint64_t count_total = 0U;
  uint32_t count_mask = 0U;

  if ((header == NULL) || (info == NULL))
  {
    return BRIDGE_PROTOCOL_BAD_ARGUMENT;
  }
  if (available_bytes < BRIDGE_BLOCK_HEADER_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if (Bridge_LoadLe32(&header[0]) != BRIDGE_BLOCK_MAGIC)
  {
    return BRIDGE_PROTOCOL_BAD_MAGIC;
  }
  if (Bridge_LoadLe16(&header[4]) != BRIDGE_BLOCK_VERSION)
  {
    return BRIDGE_PROTOCOL_BAD_VERSION;
  }
  if (Bridge_LoadLe16(&header[6]) != BRIDGE_BLOCK_HEADER_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }

  decoded.block_sequence = Bridge_LoadLe32(&header[8]);
  decoded.sync_epoch = Bridge_LoadLe32(&header[12]);
  decoded.payload_bytes = Bridge_LoadLe32(&header[16]);
  decoded.channel_mask = Bridge_LoadLe32(&header[20]);
  decoded.first_fpga_tick = Bridge_LoadLe64(&header[24]);
  decoded.last_fpga_tick = Bridge_LoadLe64(&header[32]);
  for (channel = 0U; channel < 4U; ++channel)
  {
    decoded.record_count[channel] = Bridge_LoadLe32(&header[40U + (channel * 4U)]);
    count_total += decoded.record_count[channel];
    if (decoded.record_count[channel] != 0U)
    {
      count_mask |= (1UL << channel);
    }
  }
  decoded.flags = Bridge_LoadLe32(&header[56]);
  decoded.payload_crc32 = Bridge_LoadLe32(&header[60]);

  if ((decoded.payload_bytes == 0U) ||
      (decoded.payload_bytes > BRIDGE_BLOCK_MAX_PAYLOAD_BYTES) ||
      ((decoded.payload_bytes % BRIDGE_RECORD_BYTES) != 0U))
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if ((decoded.channel_mask & ~0x0FUL) != 0U ||
      ((decoded.channel_mask & 0x0FUL) != count_mask) ||
      (count_total != (uint64_t)(decoded.payload_bytes / BRIDGE_RECORD_BYTES)) ||
      (decoded.last_fpga_tick < decoded.first_fpga_tick))
  {
    return BRIDGE_PROTOCOL_BAD_FORMAT;
  }
  if ((decoded.flags & 0x0FUL) != 0U)
  {
    return BRIDGE_PROTOCOL_BAD_FLAGS;
  }

  *info = decoded;
  return BRIDGE_PROTOCOL_OK;
}

Bridge_ProtocolStatus Bridge_BlockValidate(const uint8_t *block,
                                           size_t length,
                                           bool validate_records,
                                           Bridge_BlockInfo *info)
{
  Bridge_BlockInfo decoded;
  Bridge_ProtocolStatus status;
  size_t offset;

  if (block == NULL)
  {
    return BRIDGE_PROTOCOL_BAD_ARGUMENT;
  }
  status = Bridge_BlockHeaderDecode(block, length, &decoded);
  if (status != BRIDGE_PROTOCOL_OK)
  {
    return status;
  }
  if (length != (size_t)BRIDGE_BLOCK_HEADER_BYTES + decoded.payload_bytes)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if (Bridge_CRC32_IEEE(&block[BRIDGE_BLOCK_HEADER_BYTES], decoded.payload_bytes) !=
      decoded.payload_crc32)
  {
    return BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC;
  }
  if (validate_records)
  {
    for (offset = BRIDGE_BLOCK_HEADER_BYTES; offset < length;
         offset += BRIDGE_RECORD_BYTES)
    {
      status = Bridge_RecordValidate(&block[offset], BRIDGE_RECORD_BYTES, NULL);
      if (status != BRIDGE_PROTOCOL_OK)
      {
        return status;
      }
    }
  }
  if (info != NULL)
  {
    *info = decoded;
  }
  return BRIDGE_PROTOCOL_OK;
}

Bridge_ProtocolStatus Bridge_UsbHeaderDecode(const uint8_t *header,
                                             size_t available_bytes,
                                             Bridge_UsbHeader *info)
{
  Bridge_UsbHeader decoded;

  if ((header == NULL) || (info == NULL))
  {
    return BRIDGE_PROTOCOL_BAD_ARGUMENT;
  }
  if (available_bytes < BRIDGE_USB_HEADER_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if (Bridge_LoadLe32(&header[0]) != BRIDGE_USB_MAGIC)
  {
    return BRIDGE_PROTOCOL_BAD_MAGIC;
  }
  if (Bridge_LoadLe16(&header[4]) != BRIDGE_USB_VERSION)
  {
    return BRIDGE_PROTOCOL_BAD_VERSION;
  }
  decoded.type = Bridge_LoadLe16(&header[6]);
  if ((decoded.type < BRIDGE_USB_TYPE_DATA) || (decoded.type > BRIDGE_USB_TYPE_EVENT))
  {
    return BRIDGE_PROTOCOL_BAD_TYPE;
  }
  decoded.packet_sequence = Bridge_LoadLe32(&header[8]);
  decoded.message_id = Bridge_LoadLe32(&header[12]);
  decoded.payload_bytes = Bridge_LoadLe32(&header[16]);
  decoded.payload_crc32 = Bridge_LoadLe32(&header[20]);
  if (decoded.payload_bytes > BRIDGE_BLOCK_MAX_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  *info = decoded;
  return BRIDGE_PROTOCOL_OK;
}

Bridge_ProtocolStatus Bridge_UsbFrameValidate(const uint8_t *frame,
                                              size_t length,
                                              Bridge_UsbHeader *info)
{
  Bridge_UsbHeader decoded;
  Bridge_ProtocolStatus status;

  if (frame == NULL)
  {
    return BRIDGE_PROTOCOL_BAD_ARGUMENT;
  }
  status = Bridge_UsbHeaderDecode(frame, length, &decoded);
  if (status != BRIDGE_PROTOCOL_OK)
  {
    return status;
  }
  if (length != (size_t)BRIDGE_USB_HEADER_BYTES + decoded.payload_bytes)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }
  if (Bridge_CRC32_IEEE(&frame[BRIDGE_USB_HEADER_BYTES], decoded.payload_bytes) !=
      decoded.payload_crc32)
  {
    return BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC;
  }
  if (info != NULL)
  {
    *info = decoded;
  }
  return BRIDGE_PROTOCOL_OK;
}

Bridge_ProtocolStatus Bridge_UsbBuildHeader(uint8_t header[BRIDGE_USB_HEADER_BYTES],
                                           uint16_t type,
                                           uint32_t packet_sequence,
                                           uint32_t message_id,
                                           const uint8_t *payload,
                                           size_t payload_bytes)
{
  if (header == NULL || ((payload == NULL) && (payload_bytes != 0U)))
  {
    return BRIDGE_PROTOCOL_BAD_ARGUMENT;
  }
  if ((type < BRIDGE_USB_TYPE_DATA) || (type > BRIDGE_USB_TYPE_EVENT))
  {
    return BRIDGE_PROTOCOL_BAD_TYPE;
  }
  if (payload_bytes > BRIDGE_BLOCK_MAX_BYTES)
  {
    return BRIDGE_PROTOCOL_BAD_LENGTH;
  }

  Bridge_StoreLe32(&header[0], BRIDGE_USB_MAGIC);
  Bridge_StoreLe16(&header[4], BRIDGE_USB_VERSION);
  Bridge_StoreLe16(&header[6], type);
  Bridge_StoreLe32(&header[8], packet_sequence);
  Bridge_StoreLe32(&header[12], message_id);
  Bridge_StoreLe32(&header[16], (uint32_t)payload_bytes);
  Bridge_StoreLe32(&header[20], Bridge_CRC32_IEEE(payload, payload_bytes));
  return BRIDGE_PROTOCOL_OK;
}
