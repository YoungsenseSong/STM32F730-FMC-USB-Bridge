#ifndef BSP_LED_H
#define BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp.h"

/* The actual target board's status LED is connected to PA5. */
void BSP_LED_Init(void);
void BSP_LED_On(void);
void BSP_LED_Off(void);
void BSP_LED_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
