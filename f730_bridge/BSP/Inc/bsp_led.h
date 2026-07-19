#ifndef BSP_LED_H
#define BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

/* The board D2 LED is connected to PC13 and is active low. */
void BSP_LED_Init(void);
void BSP_LED_On(void);
void BSP_LED_Off(void);
void BSP_LED_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
