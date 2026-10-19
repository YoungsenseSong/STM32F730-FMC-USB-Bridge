#include "bridge_port.h"

#include "debug.h"

#define EXTBUS_BASE_ADDRESS 0x60000000UL

BridgePortStatus extbus_init(void)
{
#if WCH_ENABLE_EXTBUS
  FSMC_NORSRAMInitTypeDef config = {0};
  FSMC_NORSRAMTimingInitTypeDef timing = {0};
  GPIO_InitTypeDef gpio = {0};

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                         RCC_APB2Periph_GPIOD |
                         RCC_APB2Periph_GPIOE, ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  gpio.GPIO_Pin = GPIO_Pin_7; /* NADV; USBFS avoids USBHS PB7 conflict. */
  GPIO_Init(GPIOB, &gpio);
  gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
                  GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 |
                  GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_Init(GPIOD, &gpio);
  gpio.GPIO_Pin = GPIO_Pin_6; /* FSMC_NWAIT is driven by the FPGA. */
  gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOD, &gpio);
  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
  gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
                  GPIO_Pin_15;
  GPIO_Init(GPIOE, &gpio);

  timing.FSMC_AddressSetupTime = 15U;
  timing.FSMC_AddressHoldTime = 15U;
  timing.FSMC_DataSetupTime = 60U;
  timing.FSMC_BusTurnAroundDuration = 15U;
  timing.FSMC_AccessMode = FSMC_AccessMode_A;

  config.FSMC_Bank = FSMC_Bank1_NORSRAM1;
  config.FSMC_DataAddressMux = FSMC_DataAddressMux_Enable;
  config.FSMC_MemoryType = FSMC_MemoryType_PSRAM;
  config.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
  config.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
  config.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
  config.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Enable;
  config.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
  config.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
  config.FSMC_WaitSignal = FSMC_WaitSignal_Enable;
  config.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
  config.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
  config.FSMC_ReadWriteTimingStruct = &timing;
  config.FSMC_WriteTimingStruct = &timing;
  FSMC_NORSRAMInit(&config);
  FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
#endif
  return BRIDGE_PORT_OK;
}

uint16_t extbus_read16(uint32_t offset)
{
#if WCH_ENABLE_EXTBUS
  return *(volatile uint16_t *)(EXTBUS_BASE_ADDRESS + offset);
#else
  (void)offset;
  return 0U;
#endif
}

void extbus_write16(uint32_t offset, uint16_t value)
{
#if WCH_ENABLE_EXTBUS
  *(volatile uint16_t *)(EXTBUS_BASE_ADDRESS + offset) = value;
#else
  (void)offset;
  (void)value;
#endif
}

BridgePortStatus extbus_read_window(uint32_t offset, void *output, size_t bytes)
{
  if ((output == NULL) || ((offset | (uint32_t)bytes) & 1U) != 0U)
    return BRIDGE_PORT_IO_ERROR;
#if WCH_ENABLE_EXTBUS
  uint8_t *destination = (uint8_t *)output;
  volatile const uint16_t *source =
      (volatile const uint16_t *)(EXTBUS_BASE_ADDRESS + offset);
  for (size_t word_index = 0U; word_index < bytes / 2U; ++word_index)
  {
    uint16_t word = source[word_index];
    destination[2U * word_index] = (uint8_t)word;
    destination[2U * word_index + 1U] = (uint8_t)(word >> 8U);
  }
  return BRIDGE_PORT_OK;
#else
  (void)offset;
  (void)bytes;
  return BRIDGE_PORT_IO_ERROR;
#endif
}
