#include "bridge_port.h"

#include "UART.h"
#include "ch32v30x_usbfs_device.h"

#if WCH_SIM_SOURCE
#include "bridge_protocol.h"
#include "bridge_sim.h"
#include "bridge_storage.h"
#endif

#if WCH_USB_TRACE
#include <stdio.h>
#endif

#if WCH_SIM_SOURCE
#define SIM_FRAME_GAP_MS 100U
static uint8_t s_sim_in_flight;
static uint8_t s_sim_fault;
static uint32_t s_sim_sequence;
static uint32_t s_sim_last_frame_tick;
static size_t s_sim_frame_bytes;
static size_t s_sim_offset;
#elif WCH_FMC_BRIDGE
/* The FSMC bridge owns USB packet scheduling; USB IRQs handle CDC state. */
#else
static uint8_t s_echo_in_flight;
static uint8_t s_echo_zlp_in_flight;
static uint16_t s_echo_bytes;
#endif

BridgePortStatus usb_init(void)
{
  (void)RCC_Configuration();
  TIM2_Init();
  UART2_Init(1U, DEF_UARTx_BAUDRATE, DEF_UARTx_STOPBIT, DEF_UARTx_PARITY);
  USBFS_RCC_Init();
  USBFS_Device_Init(ENABLE);
#if WCH_SIM_SOURCE
  s_sim_in_flight = 0U;
  s_sim_fault = 0U;
  s_sim_sequence = 0U;
  s_sim_last_frame_tick = platform_tick() - SIM_FRAME_GAP_MS;
  s_sim_frame_bytes = 0U;
  s_sim_offset = 0U;
#elif WCH_FMC_BRIDGE
  /* No USB-side queue is needed; fmc_bridge.c schedules endpoint packets. */
#else
  s_echo_in_flight = 0U;
  s_echo_zlp_in_flight = 0U;
  s_echo_bytes = 0U;
#endif
  return BRIDGE_PORT_OK;
}

void usb_process(void)
{
#if WCH_SIM_SOURCE
  if (USBFS_DevEnumStatus == 0U)
  {
    /* Start the deterministic stream as soon as USB is configured. */
    s_sim_in_flight = 0U;
    s_sim_sequence = 0U;
    s_sim_frame_bytes = 0U;
    s_sim_offset = 0U;
    s_sim_last_frame_tick = platform_tick() - SIM_FRAME_GAP_MS;
    return;
  }
  if (s_sim_fault != 0U) return;

  if (s_sim_in_flight != 0U)
  {
    if (USBFS_Endp_Busy[DEF_UEP3] != 0U) return;
    s_sim_in_flight = 0U;
    if (s_sim_offset == s_sim_frame_bytes)
    {
      s_sim_frame_bytes = 0U;
      ++s_sim_sequence;
      s_sim_last_frame_tick = platform_tick();
    }
  }

  if (s_sim_frame_bytes == 0U)
  {
    if ((uint32_t)(platform_tick() - s_sim_last_frame_tick) <
        SIM_FRAME_GAP_MS) return;
    size_t block_bytes = Bridge_SimBuild(g_bridge_block,
                                        sizeof(g_bridge_block), s_sim_sequence);
    if ((block_bytes == 0U) ||
        (Bridge_BlockValidate(g_bridge_block, block_bytes, true, NULL) !=
         BRIDGE_PROTOCOL_OK) ||
        (Bridge_UsbBuildHeader(g_bridge_usb_header, BRIDGE_USB_TYPE_DATA,
                               s_sim_sequence, s_sim_sequence, g_bridge_block,
                               block_bytes) != BRIDGE_PROTOCOL_OK))
    {
      s_sim_fault = 1U;
      platform_log("SIM source validation failed; stream stopped");
      return;
    }
    s_sim_frame_bytes = BRIDGE_USB_HEADER_BYTES + block_bytes;
    s_sim_offset = 0U;
  }

  if (USBFS_Endp_Busy[DEF_UEP3] != 0U) return;
  const uint8_t *source;
  size_t remaining;
  if (s_sim_offset < BRIDGE_USB_HEADER_BYTES)
  {
    source = g_bridge_usb_header + s_sim_offset;
    remaining = BRIDGE_USB_HEADER_BYTES - s_sim_offset;
  }
  else
  {
    size_t block_offset = s_sim_offset - BRIDGE_USB_HEADER_BYTES;
    source = g_bridge_block + block_offset;
    remaining = s_sim_frame_bytes - s_sim_offset;
  }
  size_t chunk_bytes = (remaining > DEF_USBD_ENDP3_SIZE) ?
                       DEF_USBD_ENDP3_SIZE : remaining;
  if (usb_transmit(source, chunk_bytes) == BRIDGE_PORT_OK)
  {
    s_sim_offset += chunk_bytes;
    s_sim_in_flight = 1U;
  }
#elif WCH_FMC_BRIDGE
  /* The FSMC bridge owns USB packet scheduling; USB IRQs handle CDC state. */
  return;
#else
  if (s_echo_in_flight != 0U)
  {
    if (USBFS_Endp_Busy[DEF_UEP3] == 0U)
    {
      /* A short packet ends a CDC transfer. A full final packet needs a ZLP. */
      if ((s_echo_zlp_in_flight == 0U) &&
          (s_echo_bytes == DEF_USBD_ENDP3_SIZE) &&
          (Uart.Tx_RemainNum == 1U))
      {
        if (USBFS_Endp_DataUp(DEF_UEP3, USBFS_EP3_Buf, 0U,
                              DEF_UEP_CPY_LOAD) == 0U)
          s_echo_zlp_in_flight = 1U;
        return;
      }
      NVIC_DisableIRQ(USBFS_IRQn);
      if (Uart.Tx_RemainNum != 0U)
      {
        uint16_t index = Uart.Tx_DealNum;
        Uart.Tx_PackLen[index] = 0U;
        Uart.Tx_DealNum = (uint16_t)((index + 1U) % DEF_UARTx_TX_BUF_NUM_MAX);
        Uart.Tx_RemainNum--;
      }
      s_echo_in_flight = 0U;
      s_echo_zlp_in_flight = 0U;
      if ((Uart.USB_Down_StopFlag != 0U) && (Uart.Tx_RemainNum < 2U))
      {
        USBFSD->UEP2_RX_CTRL = (USBFSD->UEP2_RX_CTRL & ~USBFS_UEP_R_RES_MASK) |
                               USBFS_UEP_R_RES_ACK;
        Uart.USB_Down_StopFlag = 0U;
      }
      NVIC_EnableIRQ(USBFS_IRQn);
    }
    return;
  }

  if (Uart.Tx_RemainNum != 0U)
  {
    uint16_t index = Uart.Tx_DealNum;
    uint16_t bytes = Uart.Tx_PackLen[index];
    uint8_t *packet = &UART2_Tx_Buf[index * DEF_USB_FS_PACK_LEN];
    if (USBFS_Endp_DataUp(DEF_UEP3, packet, bytes, DEF_UEP_CPY_LOAD) == 0U)
    {
      s_echo_bytes = bytes;
      s_echo_in_flight = 1U;
    }
  }
#endif
}

BridgePortStatus usb_transmit(const uint8_t *data, size_t bytes)
{
  if ((data == NULL) || (bytes > DEF_USBD_ENDP3_SIZE)) return BRIDGE_PORT_IO_ERROR;
  if (USBFS_DevEnumStatus == 0U) return BRIDGE_PORT_IO_ERROR;
  if (USBFS_Endp_Busy[DEF_UEP3] != 0U) return BRIDGE_PORT_BUSY;
  return (USBFS_Endp_DataUp(DEF_UEP3, (uint8_t *)data, (uint16_t)bytes,
                           DEF_UEP_CPY_LOAD) == 0U) ?
         BRIDGE_PORT_OK : BRIDGE_PORT_BUSY;
}

BridgeUsbState usb_get_state(void)
{
  if (USBFS_DevEnumStatus == 0U) return BRIDGE_USB_OFFLINE;
  return (USBFS_Endp_Busy[DEF_UEP3] != 0U) ? BRIDGE_USB_BUSY : BRIDGE_USB_READY;
}

#if WCH_USB_TRACE
void usb_trace_state(void)
{
#if WCH_SIM_SOURCE
  printf("sim tick=%lu enum=%u seq=%lu offset=%lu total=%lu busy=%u fault=%u\r\n",
         (unsigned long)platform_tick(), USBFS_DevEnumStatus,
         (unsigned long)s_sim_sequence, (unsigned long)s_sim_offset,
         (unsigned long)s_sim_frame_bytes, USBFS_Endp_Busy[DEF_UEP3],
         s_sim_fault);
#elif WCH_FMC_BRIDGE
  printf("fmc-bridge tick=%lu enum=%u busy=%u\r\n",
         (unsigned long)platform_tick(), USBFS_DevEnumStatus,
         USBFS_Endp_Busy[DEF_UEP3]);
#else
  printf("usb tick=%lu enum=%u load=%u remain=%u deal=%u inbusy=%u inflight=%u rx=0x%04x tx=0x%04x\r\n",
         (unsigned long)platform_tick(),
         USBFS_DevEnumStatus, Uart.Tx_LoadNum, Uart.Tx_RemainNum,
         Uart.Tx_DealNum, USBFS_Endp_Busy[DEF_UEP3], s_echo_in_flight,
         USBFSD->UEP2_RX_CTRL, USBFSD->UEP3_TX_CTRL);
#endif
}
#endif
