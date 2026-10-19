#include "bsp_led.h"

#define BSP_LED_PORT          STATUS_LED_GPIO_Port
#define BSP_LED_PIN           STATUS_LED_Pin
/* Provisional until the corrected board schematic confirms PA5 polarity. */
#define BSP_LED_ACTIVE_STATE  GPIO_PIN_SET
#define BSP_LED_INACTIVE_STATE GPIO_PIN_RESET

void BSP_LED_Init(void)
{
  BSP_LED_Off();
}

void BSP_LED_On(void)
{
  HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, BSP_LED_ACTIVE_STATE);
}

void BSP_LED_Off(void)
{
  HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, BSP_LED_INACTIVE_STATE);
}

void BSP_LED_Toggle(void)
{
  HAL_GPIO_TogglePin(BSP_LED_PORT, BSP_LED_PIN);
}
