#ifndef BSP_DEBUG_H
#define BSP_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* No debug transport is configured in the stage-1 CubeMX baseline. */
void BSP_Debug_Init(void);
bool BSP_Debug_IsAvailable(void);
bool BSP_Debug_Write(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DEBUG_H */
