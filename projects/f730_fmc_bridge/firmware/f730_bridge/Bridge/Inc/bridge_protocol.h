#ifndef BRIDGE_PROTOCOL_H
#define BRIDGE_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BRIDGE_RECORD_MAGIC               0x3146524EUL
#define BRIDGE_RECORD_VERSION             1U
#define BRIDGE_RECORD_TYPE_RF_V1          1U
#define BRIDGE_RECORD_HEADER_BYTES        40U
#define BRIDGE_RF_FRAME_BYTES             204U
#define BRIDGE_RECORD_BYTES               248U

#define BRIDGE_BLOCK_MAGIC                0x31425046UL
#define BRIDGE_BLOCK_VERSION              1U
#define BRIDGE_BLOCK_HEADER_BYTES         64U
#define BRIDGE_BLOCK_TARGET_RECORDS       66U
#define BRIDGE_BLOCK_MAX_RECORDS          132U
#define BRIDGE_BLOCK_TARGET_PAYLOAD_BYTES (BRIDGE_BLOCK_TARGET_RECORDS * BRIDGE_RECORD_BYTES)
#define BRIDGE_BLOCK_MAX_PAYLOAD_BYTES    (BRIDGE_BLOCK_MAX_RECORDS * BRIDGE_RECORD_BYTES)
#define BRIDGE_BLOCK_MAX_BYTES            (BRIDGE_BLOCK_HEADER_BYTES + BRIDGE_BLOCK_MAX_PAYLOAD_BYTES)

#define BRIDGE_USB_MAGIC                  0x31524255UL
#define BRIDGE_USB_VERSION                1U
#define BRIDGE_USB_HEADER_BYTES           24U
#define BRIDGE_USB_TYPE_DATA              1U
#define BRIDGE_USB_TYPE_CMD               2U
#define BRIDGE_USB_TYPE_RSP               3U
#define BRIDGE_USB_TYPE_EVENT             4U

typedef enum
{
  BRIDGE_PROTOCOL_OK = 0,
  BRIDGE_PROTOCOL_BAD_ARGUMENT = -1,
  BRIDGE_PROTOCOL_BAD_LENGTH = -2,
  BRIDGE_PROTOCOL_BAD_MAGIC = -3,
  BRIDGE_PROTOCOL_BAD_VERSION = -4,
  BRIDGE_PROTOCOL_BAD_TYPE = -5,
  BRIDGE_PROTOCOL_BAD_HEADER_CRC = -6,
  BRIDGE_PROTOCOL_BAD_PAYLOAD_CRC = -7,
  BRIDGE_PROTOCOL_BAD_FORMAT = -8,
  BRIDGE_PROTOCOL_BAD_FLAGS = -9
} Bridge_ProtocolStatus;

typedef struct
{
  uint8_t node_id;
  uint32_t transport_sequence;
  uint32_t sync_epoch;
  uint64_t rx_tick;
  uint64_t logical_sample_index;
  uint32_t status_flags;
} Bridge_RecordInfo;

typedef struct
{
  uint32_t block_sequence;
  uint32_t sync_epoch;
  uint32_t payload_bytes;
  uint32_t channel_mask;
  uint64_t first_fpga_tick;
  uint64_t last_fpga_tick;
  uint32_t record_count[4];
  uint32_t flags;
  uint32_t payload_crc32;
} Bridge_BlockInfo;

typedef struct
{
  uint16_t type;
  uint32_t packet_sequence;
  uint32_t message_id;
  uint32_t payload_bytes;
  uint32_t payload_crc32;
} Bridge_UsbHeader;

uint16_t Bridge_CRC16_CCITT_FALSE(const uint8_t *data, size_t length);
uint32_t Bridge_CRC32_IEEE(const uint8_t *data, size_t length);

Bridge_ProtocolStatus Bridge_RecordValidate(const uint8_t *record,
                                            size_t length,
                                            Bridge_RecordInfo *info);
Bridge_ProtocolStatus Bridge_BlockHeaderDecode(const uint8_t *header,
                                               size_t available_bytes,
                                               Bridge_BlockInfo *info);
Bridge_ProtocolStatus Bridge_BlockValidate(const uint8_t *block,
                                           size_t length,
                                           bool validate_records,
                                           Bridge_BlockInfo *info);
Bridge_ProtocolStatus Bridge_UsbHeaderDecode(const uint8_t *header,
                                             size_t available_bytes,
                                             Bridge_UsbHeader *info);
Bridge_ProtocolStatus Bridge_UsbFrameValidate(const uint8_t *frame,
                                              size_t length,
                                              Bridge_UsbHeader *info);
Bridge_ProtocolStatus Bridge_UsbBuildHeader(uint8_t header[BRIDGE_USB_HEADER_BYTES],
                                           uint16_t type,
                                           uint32_t packet_sequence,
                                           uint32_t message_id,
                                           const uint8_t *payload,
                                           size_t payload_bytes);

#ifdef __cplusplus
}
#endif

#endif /* BRIDGE_PROTOCOL_H */
