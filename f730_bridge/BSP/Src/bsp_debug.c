#include "bsp_debug.h"

#include "usart.h"

#include <stdio.h>
#include <string.h>

#define BSP_DEBUG_TX_CHUNK_BYTES 64U
#define BSP_DEBUG_TX_TIMEOUT_MS  100U
#define BSP_DEBUG_RX_LINE_BYTES  32U
#define BSP_DEBUG_RX_BUDGET      16U

static bool s_debug_available;
static uint8_t s_rx_line[BSP_DEBUG_RX_LINE_BYTES];
static size_t s_rx_length;
static bool s_rx_overflow;

void BSP_Debug_Init(void)
{
  s_debug_available = (huart1.Instance == USART1) &&
                      (HAL_UART_GetState(&huart1) != HAL_UART_STATE_RESET);
  s_rx_length = 0U;
  s_rx_overflow = false;
}

void BSP_Debug_Process(void)
{
  uint32_t received = 0U;

  if (!s_debug_available)
  {
    return;
  }

  while (received < BSP_DEBUG_RX_BUDGET)
  {
    uint8_t byte;

    if (HAL_UART_Receive(&huart1, &byte, 1U, 0U) != HAL_OK)
    {
      break;
    }
    ++received;

    if ((byte == '\r') || (byte == '\n'))
    {
      if (s_rx_overflow)
      {
        (void)BSP_Debug_WriteString("ERR line too long\r\n");
      }
      else if ((s_rx_length == 4U) &&
               (memcmp(s_rx_line, "ping", 4U) == 0))
      {
        (void)BSP_Debug_WriteString("pong\r\n");
      }
      else if (s_rx_length != 0U)
      {
        (void)BSP_Debug_WriteString("ERR unknown command\r\n");
      }

      s_rx_length = 0U;
      s_rx_overflow = false;
    }
    else if (s_rx_length < sizeof(s_rx_line))
    {
      s_rx_line[s_rx_length++] = byte;
    }
    else
    {
      s_rx_overflow = true;
    }
  }
}

bool BSP_Debug_IsAvailable(void)
{
  return s_debug_available;
}

bool BSP_Debug_Write(const uint8_t *data, size_t length)
{
  if ((!s_debug_available) || ((data == NULL) && (length != 0U)))
  {
    return false;
  }

  while (length != 0U)
  {
    uint16_t chunk = (length > BSP_DEBUG_TX_CHUNK_BYTES) ?
                     BSP_DEBUG_TX_CHUNK_BYTES : (uint16_t)length;

    if (HAL_UART_Transmit(&huart1, (uint8_t *)(uintptr_t)data, chunk,
                          BSP_DEBUG_TX_TIMEOUT_MS) != HAL_OK)
    {
      return false;
    }
    data += chunk;
    length -= chunk;
  }

  return true;
}

bool BSP_Debug_WriteString(const char *text)
{
  if (text == NULL)
  {
    return false;
  }
  return BSP_Debug_Write((const uint8_t *)text, strlen(text));
}

int __io_putchar(int ch)
{
  uint8_t byte = (uint8_t)ch;

  return BSP_Debug_Write(&byte, 1U) ? ch : EOF;
}

int fputc(int ch, FILE *stream)
{
  (void)stream;
  return __io_putchar(ch);
}
