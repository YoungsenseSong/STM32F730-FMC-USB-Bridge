#include "bridge_state.h"

#define BRIDGE_MAX_READ_RETRIES 3U

void BridgeState_Init(BridgeStateMachine *machine)
{
  machine->state = BRIDGE_STATE_WAIT_USB;
  machine->resume_state = BRIDGE_STATE_WAIT_BLOCK;
  machine->retries = 0U;
  machine->block_owned = false;
}

void BridgeState_SetUsb(BridgeStateMachine *machine, bool configured)
{
  if (!configured)
  {
    if (machine->state != BRIDGE_STATE_WAIT_USB)
    {
      machine->resume_state = machine->state;
      machine->state = BRIDGE_STATE_WAIT_USB;
    }
  }
  else if (machine->state == BRIDGE_STATE_WAIT_USB)
  {
    machine->state = machine->resume_state;
  }
}

bool BridgeState_BlockReady(BridgeStateMachine *machine)
{
  if (machine->state != BRIDGE_STATE_WAIT_BLOCK) return false;
  machine->block_owned = true;
  machine->state = BRIDGE_STATE_READ_BLOCK;
  return true;
}

bool BridgeState_ReadComplete(BridgeStateMachine *machine, bool valid)
{
  if (machine->state != BRIDGE_STATE_READ_BLOCK) return false;
  if (valid)
  {
    machine->retries = 0U;
    machine->state = BRIDGE_STATE_TX_DATA;
  }
  else if (++machine->retries >= BRIDGE_MAX_READ_RETRIES)
  {
    machine->state = BRIDGE_STATE_FAULT;
  }
  else
  {
    machine->block_owned = false;
    machine->state = BRIDGE_STATE_WAIT_BLOCK;
  }
  return true;
}

bool BridgeState_TxComplete(BridgeStateMachine *machine, bool sent)
{
  if (machine->state != BRIDGE_STATE_TX_DATA) return false;
  if (sent) machine->state = BRIDGE_STATE_ACK_BLOCK;
  return true;
}

bool BridgeState_AckComplete(BridgeStateMachine *machine, bool acked)
{
  if (machine->state != BRIDGE_STATE_ACK_BLOCK) return false;
  machine->state = acked ? BRIDGE_STATE_WAIT_BLOCK : BRIDGE_STATE_FAULT;
  if (acked) machine->block_owned = false;
  return true;
}
