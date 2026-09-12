#ifndef BSP_USB_H
#define BSP_USB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_USB_FS_MAX_PACKET_BYTES 64U
#define BSP_USB_MAX_COMMAND_BYTES   256U

/*
 * The first board-level USB gate is a transparent CDC echo.  Set this to 0
 * after that gate to carry the existing UBR1 bridge frames over CDC instead.
 */
#define BSP_USB_CDC_ECHO_ENABLED    1U

typedef enum
{
  BSP_USB_TX_IDLE = 0,
  BSP_USB_TX_BUSY,
  BSP_USB_TX_COMPLETE,
  BSP_USB_TX_ERROR
} BSP_USB_TxState;

typedef struct
{
  bool pcd_ready;
  bool identity_configured;
  bool identity_test_only;
  bool cdc_fs_compiled;
  bool cdc_echo_enabled;
  bool device_started;
  bool host_configured;
  uint16_t max_packet_bytes;
} BSP_USB_Capability;

typedef struct
{
  uint32_t packet_sequence;
  uint32_t command_id;
  uint32_t payload_bytes;
  uint8_t payload[BSP_USB_MAX_COMMAND_BYTES];
} BSP_USB_Command;

typedef struct
{
  uint32_t rx_packets;
  uint32_t rx_bytes;
  uint32_t rx_framing_errors;
  uint32_t rx_crc_errors;
  uint32_t rx_backpressure_events;
  uint32_t commands_accepted;
  uint32_t tx_frames;
  uint32_t tx_bytes;
  uint32_t tx_errors;
  uint32_t disconnects;
  uint32_t echo_packets;
  uint32_t echo_bytes;
} BSP_USB_Stats;

BSP_Status BSP_USB_Init(void);
void BSP_USB_Process(void);
BSP_USB_Capability BSP_USB_GetCapability(void);
BSP_USB_Stats BSP_USB_GetStats(void);

BSP_Status BSP_USB_TransmitFrame(uint16_t type,
                                 uint32_t packet_sequence,
                                 uint32_t message_id,
                                 const uint8_t *payload,
                                 size_t payload_bytes);
BSP_USB_TxState BSP_USB_GetTxState(void);
void BSP_USB_ClearTxResult(void);
bool BSP_USB_TakeCommand(BSP_USB_Command *command);

#ifdef __cplusplus
}
#endif

#endif /* BSP_USB_H */
