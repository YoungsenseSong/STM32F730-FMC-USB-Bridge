#include "bsp_led.h"

#define BSP_LED_D2_PORT GPIOC
#define BSP_LED_D2_PIN  GPIO_PIN_13

void BSP_LED_Init(void)
{
  BSP_LED_Off();
}

void BSP_LED_On(void)
{
  HAL_GPIO_WritePin(BSP_LED_D2_PORT, BSP_LED_D2_PIN, GPIO_PIN_RESET);
}

void BSP_LED_Off(void)
{
  HAL_GPIO_WritePin(BSP_LED_D2_PORT, BSP_LED_D2_PIN, GPIO_PIN_SET);
}

void BSP_LED_Toggle(void)
{
  HAL_GPIO_TogglePin(BSP_LED_D2_PORT, BSP_LED_D2_PIN);
}
