#ifndef BSP_H
#define BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  BSP_STATUS_OK = 0,
  BSP_STATUS_ERROR = -1,
  BSP_STATUS_INVALID_ARGUMENT = -2,
  BSP_STATUS_OUT_OF_RANGE = -3,
  BSP_STATUS_BUSY = -4,
  BSP_STATUS_UNSUPPORTED = -5
} BSP_Status;

BSP_Status BSP_Init(void);
void BSP_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
