#include "bsp_fpga_ctrl.h"

#include "bsp_time.h"

static bool s_irq_asserted;
static bool s_irq_event_pending;
static uint32_t s_irq_assertion_count;
static uint32_t s_irq_last_assertion_ms;

static bool BSP_FPGA_ReadIRQ(void)
{
  return HAL_GPIO_ReadPin(FPGA_IRQ_RESERVED_GPIO_Port, FPGA_IRQ_RESERVED_Pin) == GPIO_PIN_SET;
}

BSP_Status BSP_FPGA_Ctrl_Init(void)
{
  s_irq_asserted = BSP_FPGA_ReadIRQ();
  s_irq_event_pending = s_irq_asserted;
  s_irq_assertion_count = s_irq_asserted ? 1U : 0U;
  s_irq_last_assertion_ms = s_irq_asserted ? BSP_Time_GetMs() : 0U;
  return BSP_STATUS_OK;
}

void BSP_FPGA_Ctrl_Process(void)
{
  bool asserted = BSP_FPGA_ReadIRQ();

  if (asserted && !s_irq_asserted)
  {
    ++s_irq_assertion_count;
    s_irq_last_assertion_ms = BSP_Time_GetMs();
    s_irq_event_pending = true;
  }
  s_irq_asserted = asserted;
}

BSP_FPGA_IRQ_State BSP_FPGA_IRQ_GetState(void)
{
  BSP_FPGA_IRQ_State state;

  state.irq_asserted = s_irq_asserted;
  state.event_pending = s_irq_event_pending;
  state.assertion_count = s_irq_assertion_count;
  state.last_assertion_ms = s_irq_last_assertion_ms;
  return state;
}

bool BSP_FPGA_IRQ_TakeEvent(void)
{
  bool pending = s_irq_event_pending;
  s_irq_event_pending = false;
  return pending;
}

bool BSP_FPGA_ResetIsSupported(void)
{
  return false;
}

BSP_Status BSP_FPGA_SetReset(bool asserted)
{
  (void)asserted;
  return BSP_STATUS_UNSUPPORTED;
}
