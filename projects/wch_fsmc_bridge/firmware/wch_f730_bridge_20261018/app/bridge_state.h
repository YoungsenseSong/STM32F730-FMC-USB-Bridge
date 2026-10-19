#ifndef BRIDGE_STATE_H
#define BRIDGE_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  BRIDGE_STATE_WAIT_USB = 0,
  BRIDGE_STATE_WAIT_BLOCK,
  BRIDGE_STATE_READ_BLOCK,
  BRIDGE_STATE_TX_DATA,
  BRIDGE_STATE_ACK_BLOCK,
  BRIDGE_STATE_FAULT
} BridgeState;

typedef struct
{
  BridgeState state;
  BridgeState resume_state;
  uint32_t retries;
  bool block_owned;
} BridgeStateMachine;

void BridgeState_Init(BridgeStateMachine *machine);
void BridgeState_SetUsb(BridgeStateMachine *machine, bool configured);
bool BridgeState_BlockReady(BridgeStateMachine *machine);
bool BridgeState_ReadComplete(BridgeStateMachine *machine, bool valid);
bool BridgeState_TxComplete(BridgeStateMachine *machine, bool sent);
bool BridgeState_AckComplete(BridgeStateMachine *machine, bool acked);

#endif
