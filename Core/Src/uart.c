/**
 ******************************************************************************
 * @file    uart.c
 * @brief   USART1 Driver & Command Parser for Automated HIL Testing
 ******************************************************************************
 *
 * How automated tests interact with this file:
 *   1. The PC test script (tests/test_uart.py) sends an ASCII command ending
 *      with '\n' over USB-UART at 115200 baud.
 *   2. UART_Poll() reads bytes one by one without blocking the main loop.
 *   3. When '\n' is received, ProcessCommand() matches the command and sends
 *      back a newline-terminated ASCII response for pytest to assert on.
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

/**
 * @brief  Send a single byte over USART1
 */
static void UART_SendChar(char c) {
  while (!READ_BIT(USART1->SR, USART_SR_TXE)) {
    /* Wait until transmit data register is empty */
  }
  WRITE_REG(USART1->DR, (uint8_t)c);
}

/**
 * @brief  Parse and respond to a complete command line
 */
static void ProcessCommand(const char *cmd) {
  if (strcmp(cmd, "PING") == 0) {
    /* Connectivity & MCU responsiveness check */
    UART_SendString("PONG\n");
  } else if (strcmp(cmd, "ENC?") == 0) {
    /* Read live quadrature encoder counts from encoder.c */
    int32_t left = Encoder_GetCountLeft();
    int32_t right = Encoder_GetCountRight();

    UART_SendString("ENC L=");
    UART_SendInt32(left);
    UART_SendString(" R=");
    UART_SendInt32(right);
    UART_SendString("\n");
  } else if (cmd[0] != '\0') {
    /* Unknown command */
    UART_SendString("ERR\n");
  }
}

/* ================ Public Functions ================ */

/**
 * @brief  Initialize USART1 (PA9 TX, PA10 RX, 115200 8N1)
 */
void UART_Init(void) {
  /* Enable GPIOA and USART1 clocks on APB2 */
  SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN);

  /* PA9 (TX): Alternate Function Push-Pull, 50 MHz (MODE=11, CNF=10 -> 0xB) */
  /* PA10 (RX): Input with Pull-Up (MODE=00, CNF=10 -> 0x8, ODR10=1) */
  MODIFY_REG(GPIOA->CRH, 0x00000FF0, (0xB << 4) | (0x8 << 8));
  SET_BIT(GPIOA->ODR, (1U << 10));

  /* Configure baud rate: 72 MHz / 115200 = 625 (0x0271) */
  WRITE_REG(USART1->BRR, UART_APB2_CLOCK_HZ / UART_BAUDRATE);

  /* Enable USART1, Transmitter, and Receiver (default 8N1) */
  WRITE_REG(USART1->CR1, USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);

  rx_index = 0;
}

/**
 * @brief  Send a null-terminated string
 */
void UART_SendString(const char *str) {
  while (*str != '\0') {
    UART_SendChar(*str++);
  }
}

/**
 * @brief  Send a signed 32-bit integer as ASCII decimal
 */
void UART_SendInt32(int32_t value) {
  char digits[12];
  uint8_t i = 0;
  uint32_t mag;

  if (value < 0) {
    UART_SendChar('-');
    mag = (uint32_t)(-(value + 1)) + 1U; /* Safe even for INT32_MIN */
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

/**
 * @brief  Non-blocking poll for incoming UART commands
 */
void UART_Poll(void) {
  uint32_t sr = USART1->SR;

  /* Recover cleanly if an overrun error occurred */
  if (sr & USART_SR_ORE) {
    (void)USART1->DR;
    rx_index = 0;
    return;
  }

  /* Nothing received yet */
  if (!(sr & USART_SR_RXNE)) {
    return;
  }

  char c = (char)(USART1->DR & 0xFFU);

  /* Ignore carriage return so both "\n" and "\r\n" work */
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
    /* Line too long: discard and wait for next newline */
    rx_index = 0;
  }
}
