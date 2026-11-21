/**
 ******************************************************************************
 * @file    uart.h
 * @brief   USART1 Driver & Simple Command Interface for Automated Testing
 ******************************************************************************
 *
 * Hardware Setup:
 *   - USART1_TX -> PA9  (connect to USB-TTL adapter RX)
 *   - USART1_RX -> PA10 (connect to USB-TTL adapter TX)
 *   - GND       -> GND
 *   - Settings  -> 115200 baud, 8N1, newline ('\n') terminated commands
 *
 * Supported Commands:
 *   - "PING" -> "PONG"
 *   - "ENC?" -> "ENC L=<left_count> R=<right_count>"
 *
 ******************************************************************************
 */

#ifndef UART_H
#define UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief  Initialize USART1 on PA9 (TX) / PA10 (RX) at 115200 baud (72 MHz APB2)
 */
void UART_Init(void);

/**
 * @brief  Send a null-terminated string over USART1
 */
void UART_SendString(const char *str);

/**
 * @brief  Send a signed 32-bit integer as ASCII text over USART1
 */
void UART_SendInt32(int32_t value);

/**
 * @brief  Poll USART1 for incoming commands (non-blocking, call in main loop)
 */
void UART_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
