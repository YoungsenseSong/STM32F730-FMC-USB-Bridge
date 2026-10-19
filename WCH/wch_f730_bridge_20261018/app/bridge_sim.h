#ifndef BRIDGE_SIM_H
#define BRIDGE_SIM_H

#include <stddef.h>
#include <stdint.h>

/* Matches the frozen F730 simulator: 1, 66, 132 records in rotation. */
size_t Bridge_SimBuild(uint8_t *output, size_t capacity, uint32_t sequence);

#endif
