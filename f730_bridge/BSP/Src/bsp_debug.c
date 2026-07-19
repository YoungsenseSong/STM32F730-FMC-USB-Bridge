#include "bsp_debug.h"

void BSP_Debug_Init(void)
{
  /* A transport will be attached after its CubeMX peripheral is configured. */
}

bool BSP_Debug_IsAvailable(void)
{
  return false;
}

bool BSP_Debug_Write(const uint8_t *data, size_t length)
{
  (void)data;
  (void)length;
  return false;
}
