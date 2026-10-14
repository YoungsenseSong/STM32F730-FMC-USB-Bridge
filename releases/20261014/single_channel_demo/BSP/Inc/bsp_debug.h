#ifndef BSP_DEBUG_H
#define BSP_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* USART1: PA9 TX, PA10 RX, 115200 bit/s, 8 data bits, no parity, 1 stop bit. */
void BSP_Debug_Init(void);
void BSP_Debug_Process(void);
bool BSP_Debug_IsAvailable(void);
bool BSP_Debug_Write(const uint8_t *data, size_t length);
bool BSP_Debug_WriteString(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DEBUG_H */
