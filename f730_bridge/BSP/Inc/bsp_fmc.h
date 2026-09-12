#ifndef BSP_FMC_H
#define BSP_FMC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_FMC_BASE_ADDRESS             0x60000000UL
#define BSP_FMC_MAPPED_BYTES             0x00010000UL

#define BSP_FMC_REG_FPGA_ID              0x0000UL
#define BSP_FMC_REG_PROTO_VERSION        0x0002UL
#define BSP_FMC_REG_BUILD_WORD           0x0004UL
#define BSP_FMC_REG_GLOBAL_CTRL          0x0010UL
#define BSP_FMC_REG_GLOBAL_STATUS        0x0012UL
#define BSP_FMC_REG_IRQ_STATUS           0x0020UL
#define BSP_FMC_REG_IRQ_MASK             0x0022UL
#define BSP_FMC_REG_SYNC_CTRL            0x0030UL
#define BSP_FMC_REG_SYNC_STATUS          0x0032UL
#define BSP_FMC_REG_BLOCK_STATUS         0x0040UL
#define BSP_FMC_REG_BLOCK_LEN            0x0042UL
#define BSP_FMC_REG_BLOCK_ACK            0x0044UL
#define BSP_FMC_CHANNEL_STATUS_BASE      0x0100UL
#define BSP_FMC_COMMAND_FIFO_BASE        0x0400UL
#define BSP_FMC_DATA_WINDOW_BASE         0x1000UL

#define BSP_FMC_BLOCK_STATUS_READY       0x0001U
#define BSP_FMC_BLOCK_STATUS_BANK        0x0002U

#define BSP_FPGA_BLOCK_MAGIC             0x31425046UL /* bytes: F P B 1 */
#define BSP_FPGA_PROTOCOL_VERSION        1U
#define BSP_FPGA_BLOCK_HEADER_BYTES      64UL
#define BSP_FPGA_RECORD_BYTES            248UL
#define BSP_FPGA_MAX_PAYLOAD_BYTES       (132UL * BSP_FPGA_RECORD_BYTES)
#define BSP_FPGA_MAX_BLOCK_BYTES         (BSP_FPGA_BLOCK_HEADER_BYTES + BSP_FPGA_MAX_PAYLOAD_BYTES)

typedef struct
{
  uint16_t fpga_id;
  uint16_t protocol_version;
  uint16_t build_word;
} BSP_FMC_Identity;

typedef struct
{
  uint16_t status_raw;
  uint16_t length_raw;
} BSP_FMC_BlockSnapshot;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_length;
  uint32_t block_sequence;
  uint32_t sync_epoch;
  uint32_t payload_length;
  uint32_t channel_mask;
  uint64_t first_fpga_tick;
  uint64_t last_fpga_tick;
  uint32_t record_count[4];
  uint32_t flags;
  uint32_t payload_crc32;
} BSP_FPGA_BlockHeader;

BSP_Status BSP_FMC_Init(void);
BSP_Status BSP_FMC_Read16(uint32_t byte_offset, uint16_t *value);
BSP_Status BSP_FMC_WriteCommand16(uint32_t byte_offset, uint16_t value);
BSP_Status BSP_FMC_ReadIdentity(BSP_FMC_Identity *identity);
BSP_Status BSP_FMC_ReadBlockSnapshot(BSP_FMC_BlockSnapshot *snapshot);
BSP_Status BSP_FMC_ReadDataWindow(uint32_t window_byte_offset, uint8_t *data, size_t length);
BSP_Status BSP_FMC_ReadBlockHeader(BSP_FPGA_BlockHeader *header);
bool BSP_FMC_BlockHeaderIsSane(const BSP_FPGA_BlockHeader *header);
BSP_Status BSP_FMC_AcknowledgeBlock(uint32_t block_sequence);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FMC_H */
