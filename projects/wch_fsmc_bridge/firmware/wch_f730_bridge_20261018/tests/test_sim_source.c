#include "bridge_protocol.h"
#include "bridge_sim.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "SIM_FAIL line %d\n", __LINE__); return 1; } } while (0)

static uint32_t load32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

int main(void)
{
  uint8_t block[BRIDGE_BLOCK_MAX_BYTES];
  uint8_t usb_header[BRIDGE_USB_HEADER_BYTES];
  const uint32_t counts[3] = {1U, 66U, 132U};

  CHECK(Bridge_SimBuild(NULL, sizeof(block), 0U) == 0U);
  CHECK(Bridge_SimBuild(block, 1U, 0U) == 0U);
  for (uint32_t sequence = 0U; sequence < 9U; ++sequence)
  {
    size_t bytes = Bridge_SimBuild(block, sizeof(block), sequence);
    size_t expected = BRIDGE_BLOCK_HEADER_BYTES +
                      counts[sequence % 3U] * BRIDGE_RECORD_BYTES;
    Bridge_BlockInfo info;
    CHECK(bytes == expected);
    CHECK(Bridge_BlockValidate(block, bytes, true, &info) == BRIDGE_PROTOCOL_OK);
    CHECK(info.block_sequence == sequence);
    CHECK(info.record_count[0] == counts[sequence % 3U]);
    CHECK(load32(block + 64U + 8U) == sequence * 132U);
    CHECK(block[64U + 40U] == 0x5AU && block[64U + 41U] == 0xA5U);
    CHECK(block[64U + 52U] == (uint8_t)(sequence + 12U));
    CHECK(Bridge_UsbBuildHeader(usb_header, BRIDGE_USB_TYPE_DATA,
                                sequence, sequence, block, bytes) == BRIDGE_PROTOCOL_OK);
    CHECK(load32(usb_header + 16U) == bytes);
    CHECK(load32(usb_header + 20U) == Bridge_CRC32_IEEE(block, bytes));
  }
  /* Corruption must still be rejected before the block is sent. */
  size_t bytes = Bridge_SimBuild(block, sizeof(block), 2U);
  block[BRIDGE_BLOCK_HEADER_BYTES + 40U] ^= 1U;
  CHECK(Bridge_BlockValidate(block, bytes, true, NULL) != BRIDGE_PROTOCOL_OK);
  puts("TEST_SIM_SOURCE_OK");
  return 0;
}
