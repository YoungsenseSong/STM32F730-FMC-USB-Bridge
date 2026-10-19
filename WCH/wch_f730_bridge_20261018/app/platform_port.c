#include "bridge_port.h"

#include "debug.h"

#include <stdio.h>

static uint32_t s_last_heartbeat;
static uint32_t s_reset_reason;
static volatile uint32_t s_elapsed_100us;

void platform_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  SystemCoreClockUpdate();
  Delay_Init();
  USART_Printf_Init(115200U);

  s_reset_reason = RCC->RSTSCKR;
  RCC_ClearFlag();

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  gpio.GPIO_Pin = GPIO_Pin_8;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  gpio.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &gpio);
  GPIO_ResetBits(GPIOA, GPIO_Pin_8);
  s_last_heartbeat = platform_tick();
}

uint32_t platform_tick(void)
{
  return s_elapsed_100us / 10U;
}

void platform_timebase_tick_isr(void)
{
  s_elapsed_100us++;
}

void platform_log(const char *message)
{
  if (message != NULL) printf("%s\r\n", message);
}

void platform_gpio_heartbeat(void)
{
  uint32_t now = platform_tick();
  if ((uint32_t)(now - s_last_heartbeat) >= 500U)
  {
    GPIOA->OUTDR ^= GPIO_Pin_8;
    s_last_heartbeat = now;
  }
}

void platform_watchdog_kick(void)
{
  /* Intentionally disabled until board reset policy is reviewed. */
}

uint32_t platform_reset_reason(void)
{
  return s_reset_reason;
}
