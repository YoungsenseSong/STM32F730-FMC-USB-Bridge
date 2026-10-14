#include "bsp_fmc.h"

#include "fmc.h"

static bool BSP_FMC_IsAlignedRange(uint32_t byte_offset, size_t byte_length)
{
  return ((byte_offset & 1UL) == 0UL) &&
         (byte_offset < BSP_FMC_MAPPED_BYTES) &&
         (byte_length <= (size_t)(BSP_FMC_MAPPED_BYTES - byte_offset));
}

/* Physical bus: PD1=bidirectional data, PE10=clock, PD7=active-low select.
 * This retains the complete CSR/block/ACK contract, using BYTE addresses.
 * Only this independent candidate uses a serial transport. */
volatile struct { uint32_t transactions, errors, last_status, last_value; } g_serial_diag;
#pragma GCC push_options
#pragma GCC optimize ("O2")
static inline __attribute__((always_inline)) void Serial_Delay(void)
{
  uint32_t start = DWT->CYCCNT;
  /* Keep the GPIO transport well below the FPGA synchronizer margin while
   * reading the long data window.  The earlier 250 ns half-period was
   * sufficient for CSR identity but produced sporadic response framing on
   * sustained block reads. */
  while ((uint32_t)(DWT->CYCCNT - start) < 500U) { __NOP(); }
}
static void Serial_Send(uint32_t value, unsigned bits)
{
  while (bits != 0U) {
    --bits;
    GPIOD->BSRR = (value & (1UL << bits)) ? (1UL << 1) : (1UL << 17);
    Serial_Delay(); GPIOE->BSRR = 1UL << 10;
    Serial_Delay(); GPIOE->BSRR = 1UL << 26;
  }
}
static uint32_t Serial_Receive(unsigned bits)
{
  uint32_t value = 0;
  while (bits-- != 0U) {
    Serial_Delay(); GPIOE->BSRR = 1UL << 10;
    Serial_Delay(); value = (value << 1) | ((GPIOD->IDR >> 1) & 1U);
    GPIOE->BSRR = 1UL << 26;
  }
  Serial_Delay();
  return value;
}
static BSP_Status Serial_ExchangeOnce(bool write, uint16_t address, uint16_t *word)
{
  GPIOD->BSRR = 1UL << 23; Serial_Delay();
  Serial_Send(((write ? 0x5aUL : 0xa5UL) << 16) | address, 24);
  if (write) { Serial_Send(*word, 16); }
  GPIOD->MODER &= ~(3UL << 2); /* Release data before all turnaround clocks. */
  (void)Serial_Receive(8);
  uint32_t response = Serial_Receive(24);
  GPIOD->BSRR = 1UL << 7; Serial_Delay();
  GPIOD->BSRR = 1UL << 1;
  GPIOD->MODER = (GPIOD->MODER & ~(3UL << 2)) | (1UL << 2);
  ++g_serial_diag.transactions;
  g_serial_diag.last_status = response >> 16;
  g_serial_diag.last_value = response & 0xffffU;
  if ((response >> 16) != 0xc3U) { ++g_serial_diag.errors; return BSP_STATUS_ERROR; }
  *word = (uint16_t)response;
  return BSP_STATUS_OK;
}
static BSP_Status Serial_Exchange(bool write, uint16_t address, uint16_t *word)
{
  /* Failed framing leaves CS high. Read retries cannot release a block.
   * Writes remain single-shot because a lost reply may follow a real command. */
  unsigned attempts = write ? 1U : 4U;
  while (attempts-- != 0U) {
    BSP_Status status = Serial_ExchangeOnce(write, address, word);
    if (status == BSP_STATUS_OK) { return status; }
  }
  return BSP_STATUS_ERROR;
}
#pragma GCC pop_options

static bool BSP_FMC_IsWritableRegister(uint32_t byte_offset)
{
  return (byte_offset == BSP_FMC_REG_GLOBAL_CTRL) ||
         (byte_offset == BSP_FMC_REG_IRQ_STATUS) ||
         (byte_offset == BSP_FMC_REG_IRQ_MASK) ||
         (byte_offset == BSP_FMC_REG_SYNC_CTRL) ||
         (byte_offset == BSP_FMC_REG_BLOCK_ACK);
}

static uint16_t BSP_FMC_LoadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t BSP_FMC_LoadLe32(const uint8_t *data)
{
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

static uint64_t BSP_FMC_LoadLe64(const uint8_t *data)
{
  return (uint64_t)BSP_FMC_LoadLe32(data) |
         ((uint64_t)BSP_FMC_LoadLe32(data + 4) << 32);
}

BSP_Status BSP_FMC_Init(void)
{
  __HAL_RCC_GPIOD_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  GPIOD->BSRR = (1UL << 7) | (1UL << 1) | (1UL << 4) | (1UL << 5);
  GPIOE->BSRR = 1UL << 26;
  GPIO_InitTypeDef cfg = {0};
  cfg.Mode=GPIO_MODE_OUTPUT_PP; cfg.Pull=GPIO_NOPULL; cfg.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
  cfg.Pin=GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_4 | GPIO_PIN_5; HAL_GPIO_Init(GPIOD,&cfg);
  cfg.Pin=GPIO_PIN_10; HAL_GPIO_Init(GPIOE,&cfg);
  return BSP_STATUS_OK;
}

BSP_Status BSP_FMC_Read16(uint32_t byte_offset, uint16_t *value)
{
  if (value == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }
  if (!BSP_FMC_IsAlignedRange(byte_offset, sizeof(uint16_t)))
  {
    return BSP_STATUS_OUT_OF_RANGE;
  }

  return Serial_Exchange(false, (uint16_t)byte_offset, value);
}

BSP_Status BSP_FMC_WriteCommand16(uint32_t byte_offset, uint16_t value)
{
  if (!BSP_FMC_IsWritableRegister(byte_offset))
  {
    return BSP_STATUS_UNSUPPORTED;
  }

  return Serial_Exchange(true, (uint16_t)byte_offset, &value);
}

BSP_Status BSP_FMC_ReadIdentity(BSP_FMC_Identity *identity)
{
  BSP_Status status;

  if (identity == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_FMC_Read16(BSP_FMC_REG_FPGA_ID, &identity->fpga_id);
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_PROTO_VERSION, &identity->protocol_version);
  }
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_BUILD_WORD, &identity->build_word);
  }
  return status;
}

BSP_Status BSP_FMC_ReadBlockSnapshot(BSP_FMC_BlockSnapshot *snapshot)
{
  uint16_t status_after;
  BSP_Status status;

  if (snapshot == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_FMC_Read16(BSP_FMC_REG_BLOCK_STATUS, &snapshot->status_raw);
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_BLOCK_LEN, &snapshot->length_raw);
  }
  if (status == BSP_STATUS_OK)
  {
    status = BSP_FMC_Read16(BSP_FMC_REG_BLOCK_STATUS, &status_after);
  }
  if ((status == BSP_STATUS_OK) && (status_after != snapshot->status_raw))
  {
    status = BSP_STATUS_BUSY;
  }
  return status;
}

BSP_Status BSP_FMC_ReadDataWindow(uint32_t window_byte_offset, uint8_t *data, size_t length)
{
  uint32_t absolute_offset;
  size_t index;

  if ((data == NULL) && (length != 0U))
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }
  if ((window_byte_offset > BSP_FPGA_MAX_BLOCK_BYTES) ||
      (length > (size_t)(BSP_FPGA_MAX_BLOCK_BYTES - window_byte_offset)))
  {
    return BSP_STATUS_OUT_OF_RANGE;
  }

  absolute_offset = BSP_FMC_DATA_WINDOW_BASE + window_byte_offset;
  if (!BSP_FMC_IsAlignedRange(absolute_offset, length))
  {
    return BSP_STATUS_OUT_OF_RANGE;
  }

  for (index = 0U; index < length; index += 2U)
  {
    uint16_t word;
    BSP_Status status = BSP_FMC_Read16(absolute_offset + (uint32_t)index, &word);
    if (status != BSP_STATUS_OK) { return status; }
    data[index] = (uint8_t)(word & 0xFFU);
    if ((index + 1U) < length)
    {
      data[index + 1U] = (uint8_t)(word >> 8);
    }
  }
  __DMB();
  return BSP_STATUS_OK;
}

BSP_Status BSP_FMC_ReadBlockHeader(BSP_FPGA_BlockHeader *header)
{
  uint8_t raw[BSP_FPGA_BLOCK_HEADER_BYTES];
  BSP_Status status;
  uint32_t channel;

  if (header == NULL)
  {
    return BSP_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_FMC_ReadDataWindow(0U, raw, sizeof(raw));
  if (status != BSP_STATUS_OK)
  {
    return status;
  }

  header->magic = BSP_FMC_LoadLe32(&raw[0]);
  header->version = BSP_FMC_LoadLe16(&raw[4]);
  header->header_length = BSP_FMC_LoadLe16(&raw[6]);
  header->block_sequence = BSP_FMC_LoadLe32(&raw[8]);
  header->sync_epoch = BSP_FMC_LoadLe32(&raw[12]);
  header->payload_length = BSP_FMC_LoadLe32(&raw[16]);
  header->channel_mask = BSP_FMC_LoadLe32(&raw[20]);
  header->first_fpga_tick = BSP_FMC_LoadLe64(&raw[24]);
  header->last_fpga_tick = BSP_FMC_LoadLe64(&raw[32]);
  for (channel = 0U; channel < 4U; ++channel)
  {
    header->record_count[channel] = BSP_FMC_LoadLe32(&raw[40U + (channel * 4U)]);
  }
  header->flags = BSP_FMC_LoadLe32(&raw[56]);
  header->payload_crc32 = BSP_FMC_LoadLe32(&raw[60]);
  return BSP_STATUS_OK;
}

bool BSP_FMC_BlockHeaderIsSane(const BSP_FPGA_BlockHeader *header)
{
  return (header != NULL) &&
         (header->magic == BSP_FPGA_BLOCK_MAGIC) &&
         (header->version == BSP_FPGA_PROTOCOL_VERSION) &&
         (header->header_length == BSP_FPGA_BLOCK_HEADER_BYTES) &&
         (header->payload_length != 0U) &&
         (header->payload_length <= BSP_FPGA_MAX_PAYLOAD_BYTES) &&
         ((header->payload_length % BSP_FPGA_RECORD_BYTES) == 0U) &&
         ((header->channel_mask & ~0x0FUL) == 0UL) &&
         ((header->flags & ~0x8UL) == 0UL) &&
         (header->last_fpga_tick >= header->first_fpga_tick);
}

BSP_Status BSP_FMC_AcknowledgeBlock(uint32_t block_sequence)
{
  return BSP_FMC_WriteCommand16(BSP_FMC_REG_BLOCK_ACK,
                                (uint16_t)(block_sequence & 0xFFFFUL));
}
