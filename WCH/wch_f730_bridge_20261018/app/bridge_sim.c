#include "bridge_sim.h"
#include "bridge_protocol.h"

#include <string.h>

static void store16(uint8_t *out, uint16_t value)
{
  out[0] = (uint8_t)value;
  out[1] = (uint8_t)(value >> 8U);
}

static void store32(uint8_t *out, uint32_t value)
{
  store16(out, (uint16_t)value);
  store16(out + 2U, (uint16_t)(value >> 16U));
}

size_t Bridge_SimBuild(uint8_t *output, size_t capacity, uint32_t sequence)
{
  static const uint32_t counts[3] = {1U, 66U, 132U};
  uint32_t count = counts[sequence % 3U];
  size_t length = BRIDGE_BLOCK_HEADER_BYTES + count * BRIDGE_RECORD_BYTES;

  if ((output == NULL) || (capacity < length)) return 0U;
  memset(output, 0, length);
  store32(output, BRIDGE_BLOCK_MAGIC);
  store16(output + 4U, BRIDGE_BLOCK_VERSION);
  store16(output + 6U, BRIDGE_BLOCK_HEADER_BYTES);
  store32(output + 8U, sequence);
  store32(output + 12U, 0x53494D31UL);
  store32(output + 16U, count * BRIDGE_RECORD_BYTES);
  store32(output + 20U, 1U);
  store32(output + 40U, count);

  for (uint32_t record_index = 0U; record_index < count; ++record_index)
  {
    uint8_t *record = output + BRIDGE_BLOCK_HEADER_BYTES +
                      record_index * BRIDGE_RECORD_BYTES;
    store32(record, BRIDGE_RECORD_MAGIC);
    record[4] = BRIDGE_RECORD_VERSION;
    store16(record + 6U, BRIDGE_RECORD_TYPE_RF_V1);
    store32(record + 8U, sequence * BRIDGE_BLOCK_MAX_RECORDS + record_index);
    store32(record + 12U, 0x53494D31UL);
    store16(record + 36U, BRIDGE_RF_FRAME_BYTES);
    store16(record + 38U, Bridge_CRC16_CCITT_FALSE(record, 38U));
    for (uint32_t byte_index = 0U; byte_index < BRIDGE_RF_FRAME_BYTES; ++byte_index)
      record[40U + byte_index] = (uint8_t)(sequence + record_index + byte_index);
    store16(record + 40U, 0xA55AU);
    store16(record + 44U, 96U);
    store32(record + 244U,
            Bridge_CRC32_IEEE(record + 40U, BRIDGE_RF_FRAME_BYTES));
  }

  store32(output + 60U,
          Bridge_CRC32_IEEE(output + BRIDGE_BLOCK_HEADER_BYTES,
                            count * BRIDGE_RECORD_BYTES));
  return length;
}
