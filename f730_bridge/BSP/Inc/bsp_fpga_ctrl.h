#ifndef BSP_FPGA_CTRL_H
#define BSP_FPGA_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  bool irq_asserted;
  bool event_pending;
  uint32_t assertion_count;
  uint32_t last_assertion_ms;
} BSP_FPGA_IRQ_State;

BSP_Status BSP_FPGA_Ctrl_Init(void);
void BSP_FPGA_Ctrl_Process(void);
BSP_FPGA_IRQ_State BSP_FPGA_IRQ_GetState(void);
bool BSP_FPGA_IRQ_TakeEvent(void);
bool BSP_FPGA_ResetIsSupported(void);
BSP_Status BSP_FPGA_SetReset(bool asserted);

#ifdef __cplusplus
}
#endif

#endif /* BSP_FPGA_CTRL_H */
