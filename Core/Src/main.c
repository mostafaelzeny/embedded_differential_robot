/**
 ******************************************************************************
 * @file    main.c
 * @brief   Main program for Differential Drive Robot
 ******************************************************************************
 */

#include "main.h"
#include "differential_drive.h"
#include "encoder.h"
#include "motor.h"
#include "uart.h"

/*
 * Set to 0 for safe bench / automated UART testing (motors stay stopped,
 * encoders and USART1 remain active).
 * Set to 1 to run the full IMU + PID straight-line driving loop.
 */
#define DRIVE_ENABLED 0

#define CONTROL_LOOP_PERIOD_MS 10U    /* 100 Hz control loop (10 ms) */
#define CONTROL_LOOP_DT        0.01f  /* 10 ms in seconds */
#define TARGET_FORWARD_SPEED   500.0f /* Target speed in encoder counts/sec */

void SystemClock_Config(void);

int main(void) {
  HAL_Init();
  SystemClock_Config();

  /* Initialize USART1 (115200 baud) for PC communication / automated tests */
  UART_Init();

#if DRIVE_ENABLED
  /* Initialize motors, encoders, IMU, and PID controllers */
  DifferentialDrive_Init();

  /* Calibrate IMU gyro bias (keep robot stationary for ~1 second) */
  DifferentialDrive_Calibrate();

  /* Start moving forward in a straight line */
  DifferentialDrive_SetSpeed(TARGET_FORWARD_SPEED);

  uint32_t last_tick = HAL_GetTick();
#else
  /* Bench test mode: keep motors stopped, enable encoders for ENC? queries */
  Motor_Init();
  Encoder_Init();
#endif

  while (1) {
    /* Handle incoming PC test commands (PING, ENC?) without blocking */
    UART_Poll();

#if DRIVE_ENABLED
    /* Run control loop every 10 ms (100 Hz) */
    uint32_t now = HAL_GetTick();
    if ((now - last_tick) >= CONTROL_LOOP_PERIOD_MS) {
      last_tick = now;
      DifferentialDrive_Update(CONTROL_LOOP_DT);
    }
#endif
  }
}

/**
 * @brief  HSE (8 MHz) -> PLL x9 -> SYSCLK = 72 MHz
 *         AHB = 72 MHz, APB1 = 36 MHz (TIM3 = 72 MHz), APB2 = 72 MHz
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {
  }
}
