#include "bridge_protocol.h"

/* Linear reflected CRC shift over GF(2). Columns describe the effect on
 * each of the 32 input bits. Squaring advances twice as many zero bits. */
static uint32_t Apply(const uint32_t columns[32], uint32_t value)
{
  uint32_t result = 0U;
  for (size_t bit = 0U; value != 0U; ++bit, value >>= 1U)
    if (value & 1U) result ^= columns[bit];
  return result;
}

uint32_t Bridge_CRC32_Combine(uint32_t crc_a, uint32_t crc_b, size_t bytes_b)
{
  uint32_t columns[32], squared[32];
  if (bytes_b == 0U) return crc_a;
  /* Construct the operator for one zero byte directly. */
  for (size_t bit = 0U; bit < 32U; ++bit)
  {
    uint32_t value = (uint32_t)1U << bit;
    for (size_t step = 0U; step < 8U; ++step)
      value = (value >> 1U) ^ ((value & 1U) ? 0xEDB88320U : 0U);
    columns[bit] = value;
  }
  while (bytes_b != 0U)
  {
    if (bytes_b & 1U) crc_a = Apply(columns, crc_a);
    bytes_b >>= 1U;
    if (bytes_b == 0U) break;
    for (size_t bit = 0U; bit < 32U; ++bit)
      squared[bit] = Apply(columns, columns[bit]);
    for (size_t bit = 0U; bit < 32U; ++bit) columns[bit] = squared[bit];
  }
  return crc_a ^ crc_b;
}
