#ifndef BRIDGE_PORT_H
#define BRIDGE_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifndef WCH_FMC_BRIDGE
#define WCH_FMC_BRIDGE 0
#endif

typedef enum
{
  BRIDGE_PORT_OK = 0,
  BRIDGE_PORT_BUSY = 1,
  BRIDGE_PORT_TIMEOUT = -1,
  BRIDGE_PORT_IO_ERROR = -2,
  BRIDGE_PORT_PROTOCOL_ERROR = -3
} BridgePortStatus;

typedef enum
{
  BRIDGE_USB_OFFLINE = 0,
  BRIDGE_USB_READY,
  BRIDGE_USB_BUSY,
  BRIDGE_USB_ERROR
} BridgeUsbState;

BridgePortStatus extbus_init(void);
uint16_t extbus_read16(uint32_t offset);
void extbus_write16(uint32_t offset, uint16_t value);
BridgePortStatus extbus_read_window(uint32_t offset, void *output, size_t bytes);

BridgePortStatus usb_init(void);
void usb_process(void);
#if WCH_USB_TRACE
void usb_trace_state(void);
#endif
BridgePortStatus usb_transmit(const uint8_t *data, size_t bytes);
BridgeUsbState usb_get_state(void);

void platform_init(void);
uint32_t platform_tick(void);
void platform_timebase_tick_isr(void);
void platform_log(const char *message);
void platform_gpio_heartbeat(void);
void platform_watchdog_kick(void);
uint32_t platform_reset_reason(void);

void fmc_bridge_init(void);
void fmc_bridge_process(void);

#endif
