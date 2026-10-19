#include "bridge_state.h"

#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void)
{
  BridgeStateMachine machine;
  BridgeState_Init(&machine);
  CHECK(machine.state == BRIDGE_STATE_WAIT_USB);
  BridgeState_SetUsb(&machine, true);
  CHECK(machine.state == BRIDGE_STATE_WAIT_BLOCK);
  CHECK(BridgeState_BlockReady(&machine));
  CHECK(machine.block_owned && machine.state == BRIDGE_STATE_READ_BLOCK);
  CHECK(BridgeState_ReadComplete(&machine, true));
  CHECK(machine.state == BRIDGE_STATE_TX_DATA);
  BridgeState_SetUsb(&machine, false);
  CHECK(machine.state == BRIDGE_STATE_WAIT_USB);
  BridgeState_SetUsb(&machine, true);
  CHECK(machine.state == BRIDGE_STATE_TX_DATA);
  CHECK(BridgeState_TxComplete(&machine, true));
  CHECK(machine.state == BRIDGE_STATE_ACK_BLOCK && machine.block_owned);
  CHECK(BridgeState_AckComplete(&machine, true));
  CHECK(machine.state == BRIDGE_STATE_WAIT_BLOCK && !machine.block_owned);

  CHECK(BridgeState_BlockReady(&machine));
  CHECK(BridgeState_ReadComplete(&machine, false));
  CHECK(BridgeState_BlockReady(&machine));
  CHECK(BridgeState_ReadComplete(&machine, false));
  CHECK(BridgeState_BlockReady(&machine));
  CHECK(BridgeState_ReadComplete(&machine, false));
  CHECK(machine.state == BRIDGE_STATE_FAULT);
  puts("TEST_BRIDGE_STATE_OK");
  return 0;
}
