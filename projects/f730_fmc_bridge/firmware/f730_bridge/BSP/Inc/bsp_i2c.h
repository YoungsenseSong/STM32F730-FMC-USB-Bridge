#ifndef BSP_I2C_H
#define BSP_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stdint.h>

BSP_Status BSP_I2C_Init(void);
bool BSP_I2C_IsReady(uint8_t address_7bit, uint32_t trials, uint32_t timeout_ms);
BSP_Status BSP_I2C_Transmit(uint8_t address_7bit, const uint8_t *data, uint16_t length, uint32_t timeout_ms);
BSP_Status BSP_I2C_Receive(uint8_t address_7bit, uint8_t *data, uint16_t length, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_H */
