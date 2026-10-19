#include "bridge_protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t next_value(uint32_t *state)
{
  *state = (*state * 1664525U) + 1013904223U;
  return *state;
}

int main(void)
{
  static const size_t lengths[] = {0U, 1U, 63U, 64U, 204U, 1024U, 16368U, 32736U};
  uint8_t *data = (uint8_t *)malloc(32736U + 64U);
  uint32_t seed = 0x730307U;
  size_t i;
  size_t j;

  if (data == NULL) return 2;
  for (i = 0U; i < 32736U + 64U; ++i) data[i] = (uint8_t)next_value(&seed);
  for (i = 0U; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
  {
    for (j = 0U; j < sizeof(lengths) / sizeof(lengths[0]); ++j)
    {
      size_t left = lengths[i] > 64U ? 64U : lengths[i];
      size_t right = lengths[j];
      uint32_t combined = Bridge_CRC32_Combine(
          Bridge_CRC32_IEEE(data, left),
          Bridge_CRC32_IEEE(data + left, right), right);
      uint32_t scanned = Bridge_CRC32_IEEE(data, left + right);
      if (combined != scanned)
      {
        fprintf(stderr, "FAIL left=%lu right=%lu\n",
                (unsigned long)left, (unsigned long)right);
        free(data);
        return 1;
      }
    }
  }
  free(data);
  puts("TEST_CRC_COMBINE_OK");
  return 0;
}
