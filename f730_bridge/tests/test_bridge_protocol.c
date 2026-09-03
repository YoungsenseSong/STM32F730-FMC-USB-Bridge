#include "bridge_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BUFFER_BYTES (BRIDGE_USB_HEADER_BYTES + BRIDGE_BLOCK_MAX_BYTES)

static uint8_t s_buffer[TEST_BUFFER_BYTES];

static void store_le32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
  data[2] = (uint8_t)(value >> 16);
  data[3] = (uint8_t)(value >> 24);
}

static int load_file(const char *directory,
                     const char *name,
                     uint8_t *buffer,
                     size_t capacity,
                     size_t *length)
{
  char path[512];
  FILE *file;
  size_t bytes;

  if (snprintf(path, sizeof(path), "%s/%s", directory, name) < 0)
  {
    return 0;
  }
  file = fopen(path, "rb");
  if (file == NULL)
  {
    return 0;
  }
  bytes = fread(buffer, 1U, capacity, file);
  if ((ferror(file) != 0) || ((bytes == capacity) && (fgetc(file) != EOF)))
  {
    (void)fclose(file);
    return 0;
  }
  (void)fclose(file);
  *length = bytes;
  return 1;
}

static int expect(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "FAIL: %s\n", message);
    return 0;
  }
  return 1;
}

int main(int argc, char **argv)
{
  const char *golden_dir;
  size_t length;
  Bridge_BlockInfo block;
  Bridge_UsbHeader usb;
  uint8_t built_header[BRIDGE_USB_HEADER_BYTES];

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s GOLDEN_DIR\n", argv[0]);
    return 2;
  }
  golden_dir = argv[1];

  if (!expect(Bridge_CRC16_CCITT_FALSE((const uint8_t *)"123456789", 9U) == 0x29B1U,
              "CRC16 check value") ||
      !expect(Bridge_CRC32_IEEE((const uint8_t *)"123456789", 9U) == 0xCBF43926UL,
              "CRC32 check value"))
  {
    return 1;
  }

  if (!load_file(golden_dir, "nrf_record_valid.bin", s_buffer,
                 sizeof(s_buffer), &length) ||
      !expect(Bridge_RecordValidate(s_buffer, length, NULL) == BRIDGE_PROTOCOL_OK,
              "valid record"))
  {
    return 1;
  }
  if (!load_file(golden_dir, "nrf_record_header_crc_bad.bin", s_buffer,
                 sizeof(s_buffer), &length) ||
      !expect(Bridge_RecordValidate(s_buffer, length, NULL) ==
                  BRIDGE_PROTOCOL_BAD_HEADER_CRC,
              "header CRC rejection"))
  {
    return 1;
  }
  if (!load_file(golden_dir, "nrf_record_payload_crc_bad.bin", s_buffer,
                 sizeof(s_buffer), &length) ||
      !expect(Bridge_RecordValidate(s_buffer, length, NULL) ==
                  BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC,
              "payload CRC rejection"))
  {
    return 1;
  }

  if (!load_file(golden_dir, "fpga_block_two_records.bin", s_buffer,
                 sizeof(s_buffer), &length) ||
      !expect(Bridge_BlockValidate(s_buffer, length, true, &block) == BRIDGE_PROTOCOL_OK,
              "valid block") ||
      !expect((block.block_sequence == 0x89ABCDEFUL) &&
              (block.payload_bytes == 2U * BRIDGE_RECORD_BYTES),
              "block metadata"))
  {
    return 1;
  }

  s_buffer[BRIDGE_BLOCK_HEADER_BYTES + 17U] ^= 0x80U;
  if (!expect(Bridge_BlockValidate(s_buffer, length, true, NULL) ==
                  BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC,
              "block payload CRC rejection"))
  {
    return 1;
  }

  if (!load_file(golden_dir, "fpga_block_two_records.bin", s_buffer,
                 sizeof(s_buffer), &length))
  {
    return 1;
  }
  s_buffer[BRIDGE_BLOCK_HEADER_BYTES] ^= 0x01U;
  store_le32(&s_buffer[60],
             Bridge_CRC32_IEEE(&s_buffer[BRIDGE_BLOCK_HEADER_BYTES],
                               length - BRIDGE_BLOCK_HEADER_BYTES));
  if (!expect(Bridge_BlockValidate(s_buffer, length, true, NULL) ==
                  BRIDGE_PROTOCOL_BAD_MAGIC,
              "nested record validation after outer CRC update"))
  {
    return 1;
  }

  if (!load_file(golden_dir, "fpga_block_two_records.bin", s_buffer,
                 sizeof(s_buffer), &length))
  {
    return 1;
  }
  store_le32(&s_buffer[20], 0x00000007UL);
  store_le32(&s_buffer[40], 0xFFFFFFFFUL);
  store_le32(&s_buffer[44], 0xFFFFFFFFUL);
  store_le32(&s_buffer[48], 4UL);
  store_le32(&s_buffer[52], 0UL);
  if (!expect(Bridge_BlockHeaderDecode(s_buffer, length, &block) ==
                  BRIDGE_PROTOCOL_BAD_FORMAT,
              "record-count sum cannot wrap to a valid total"))
  {
    return 1;
  }

  if (!load_file(golden_dir, "usb_data_frame.bin", s_buffer,
                 sizeof(s_buffer), &length) ||
      !expect(Bridge_UsbFrameValidate(s_buffer, length, &usb) == BRIDGE_PROTOCOL_OK,
              "valid USB DATA frame") ||
      !expect((usb.type == BRIDGE_USB_TYPE_DATA) &&
              (usb.message_id == 0x89ABCDEFUL),
              "USB DATA metadata") ||
      !expect(Bridge_UsbBuildHeader(built_header, usb.type, usb.packet_sequence,
                                   usb.message_id,
                                   &s_buffer[BRIDGE_USB_HEADER_BYTES],
                                   usb.payload_bytes) == BRIDGE_PROTOCOL_OK,
              "build USB header") ||
      !expect(memcmp(built_header, s_buffer, BRIDGE_USB_HEADER_BYTES) == 0,
              "rebuilt shared USB header"))
  {
    return 1;
  }

  s_buffer[BRIDGE_USB_HEADER_BYTES + 9U] ^= 0x40U;
  if (!expect(Bridge_UsbFrameValidate(s_buffer, length, NULL) ==
                  BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC,
              "USB payload CRC rejection"))
  {
    return 1;
  }

  puts("TEST_BRIDGE_PROTOCOL_OK");
  return 0;
}
