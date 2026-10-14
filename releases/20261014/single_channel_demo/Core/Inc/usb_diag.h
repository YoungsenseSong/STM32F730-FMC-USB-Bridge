#ifndef USB_DIAG_H
#define USB_DIAG_H
#include <stdint.h>
typedef struct {
  uint32_t magic, version, boots, reset_csr, previous_stage, stage;
  uint32_t tick_ms, heartbeats, usb_state, usb_resets, usb_suspends, usb_resumes;
  uint32_t fault_kind, fault_cfsr, fault_hfsr, fault_bfar, fault_mmfar, fault_ipsr;
} UsbDiag_Trace;
extern volatile UsbDiag_Trace g_usb_diag;
void UsbDiag_BootCapture(void);
void UsbDiag_Tick(void);
void UsbDiag_Fault(uint32_t kind) __attribute__((noreturn));
#endif
