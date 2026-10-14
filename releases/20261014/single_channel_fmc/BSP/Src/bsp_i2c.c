#include "bsp_i2c.h"

#include "i2c.h"

static bool BSP_I2C_AddressIsValid(uint8_t address_7bit)
{
  return (address_7bit > 0U) && (address_7bit < 0x78U);
}

BSP_Status BSP_I2C_Init(void)
{
  return (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_READY) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

bool BSP_I2C_IsReady(uint8_t address_7bit, uint32_t trials, uint32_t timeout_ms)
{
  if (!BSP_I2C_AddressIsValid(address_7bit) || (trials == 0U))
  {
    return false;
  }
  return HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)address_7bit << 1, trials, timeout_ms) == HAL_OK;
}

BSP_Status BSP_I2C_Transmit(uint8_t address_7bit, const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
  if (!BSP_I2C_AddressIsValid(address_7bit) || (data == NULL) || (length == 0U))
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }
  return (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)address_7bit << 1,
                                  (uint8_t *)data, length, timeout_ms) == HAL_OK)
           ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

BSP_Status BSP_I2C_Receive(uint8_t address_7bit, uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
  if (!BSP_I2C_AddressIsValid(address_7bit) || (data == NULL) || (length == 0U))
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }
  return (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)address_7bit << 1,
                                 data, length, timeout_ms) == HAL_OK)
           ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}
