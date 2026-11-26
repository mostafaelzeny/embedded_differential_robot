/**
 ******************************************************************************
 * @file    uart.c
 * @brief   USART1 Driver & Command Parser for Automated HIL Testing
 ******************************************************************************
 *
 * How automated tests interact with this file:
 *   - tests/test_ping.py    sends "PING\n" -> receives "PONG\n"
 *   - tests/test_encoder.py sends "ENC?\n" -> receives "ENC L=<int> R=<int>\n"
 *
 ******************************************************************************
 */

#include "uart.h"
#include "encoder.h"
#include "main.h"
#include <string.h>

/* ================ Private Defines ================ */

#define UART_APB2_CLOCK_HZ 72000000U
#define UART_BAUDRATE      115200U
#define UART_RX_BUF_SIZE   32U

/* ================ Private Variables ================ */

static char rx_buf[UART_RX_BUF_SIZE];
static uint8_t rx_index = 0;

/* ================ Private Functions ================ */

static void UART_SendChar(char c) {
  while (!READ_BIT(USART1->SR, USART_SR_TXE)) {
  }
  WRITE_REG(USART1->DR, (uint8_t)c);
}

static void UART_SendString(const char *str) {
  while (*str != '\0') {
    UART_SendChar(*str++);
  }
}

static void UART_SendInt32(int32_t value) {
  char digits[12];
  uint8_t i = 0;
  uint32_t mag;

  if (value < 0) {
    UART_SendChar('-');
    mag = (uint32_t)(-(value + 1)) + 1U;
  } else {
    mag = (uint32_t)value;
  }

  if (mag == 0U) {
    UART_SendChar('0');
    return;
  }

  while (mag > 0U) {
    digits[i++] = (char)('0' + (mag % 10U));
    mag /= 10U;
  }

  while (i > 0U) {
    UART_SendChar(digits[--i]);
  }
}

static void ProcessCommand(const char *cmd) {
  if (strcmp(cmd, "PING") == 0) {
    UART_SendString("PONG\n");
  } else if (strcmp(cmd, "ENC?") == 0) {
    UART_SendString("ENC L=");
    UART_SendInt32(Encoder_GetCountLeft());
    UART_SendString(" R=");
    UART_SendInt32(Encoder_GetCountRight());
    UART_SendString("\n");
  }
}

/* ================ Public Functions ================ */

void UART_Init(void) {
  /* Enable GPIOA and USART1 clocks on APB2 */
  SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN);

  /* PA9 (TX): AF Push-Pull 50 MHz (0xB), PA10 (RX): Input Pull-Up (0x8) */
  MODIFY_REG(GPIOA->CRH, 0x00000FF0, (0xB << 4) | (0x8 << 8));
  SET_BIT(GPIOA->ODR, (1U << 10));

  /* 72 MHz / 115200 = 625 (0x0271) */
  WRITE_REG(USART1->BRR, UART_APB2_CLOCK_HZ / UART_BAUDRATE);

  /* Enable USART1, TX, and RX (8N1 default) */
  WRITE_REG(USART1->CR1, USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);

  rx_index = 0;
}

void UART_Poll(void) {
  uint32_t sr = USART1->SR;

  /* Clear overrun error if one occurred */
  if (sr & USART_SR_ORE) {
    (void)USART1->DR;
    rx_index = 0;
    return;
  }

  if (!(sr & USART_SR_RXNE)) {
    return;
  }

  char c = (char)(USART1->DR & 0xFFU);
  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    rx_buf[rx_index] = '\0';
    ProcessCommand(rx_buf);
    rx_index = 0;
  } else if (rx_index < (UART_RX_BUF_SIZE - 1U)) {
    rx_buf[rx_index++] = c;
  } else {
    rx_index = 0;
  }
}
