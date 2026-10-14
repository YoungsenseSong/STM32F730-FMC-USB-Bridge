#ifndef BSP_BRIDGE_H
#define BSP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  BSP_BRIDGE_WAIT_USB = 0,
  BSP_BRIDGE_WAIT_BLOCK,
  BSP_BRIDGE_READ_BLOCK,
  BSP_BRIDGE_TX_DATA,
  BSP_BRIDGE_TX_COMMAND_RESPONSE,
  BSP_BRIDGE_ACK_BLOCK,
  BSP_BRIDGE_FAULT
} BSP_BridgeState;

typedef struct
{
  BSP_BridgeState state;
  uint32_t blocks_validated;
  uint32_t blocks_sent;
  uint32_t blocks_acked;
  uint32_t duplicate_blocks;
  uint32_t fmc_errors;
  uint32_t protocol_errors;
  uint32_t usb_errors;
  uint32_t unsupported_commands;
  uint32_t last_block_sequence;
  int32_t last_protocol_status;
  bool block_owned;
} BSP_BridgeStats;

BSP_Status BSP_Bridge_Init(void);
void BSP_Bridge_Process(void);
BSP_BridgeStats BSP_Bridge_GetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BRIDGE_H */
