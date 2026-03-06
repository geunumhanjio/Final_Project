/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "drv_motor.h"
#include "app_packet_parser.h"
#include "drv_encoder.h"
#include "drv_mpu6050.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  if (Motor_Init() == MOTOR_OK) {
      printf("Motor driver initialized successfully!\r\n");
  } else {
      printf("Motor driver initialization failed!\r\n");
      Error_Handler();
  }

  Protocol_Init(&huart1);
  printf("UART protocol ready. Waiting for RPi commands...\r\n");

  Encoder_Init();
  printf("Encoder started (TIM3=Left, TIM4=Right)\r\n");

  if (MPU6050_Init() == HAL_OK) {
      printf("MPU6050 initialized!\r\n");
  } else {
      printf("MPU6050 init failed! Check wiring.\r\n");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Protocol_Process();
    Motor_CheckTimeout();

    uint32_t now = HAL_GetTick();

    // /* PID 속도 제어 루프 — 10ms 주기
    //  * Encoder_GetSpeed()는 호출할 때마다 이전 카운트를 갱신하므로
    //  * 여기서만 호출하고 결과를 변수에 저장한다. */
    // static uint32_t last_pid  = 0;
    // static float    last_spd_L = 0.0f;
    // static float    last_spd_R = 0.0f;
    // if (now - last_pid >= 10) {
    //     last_pid   = now;
    //     last_spd_L = Encoder_GetSpeed(ENCODER_LEFT);
    //     last_spd_R = Encoder_GetSpeed(ENCODER_RIGHT);
    //     Motor_PID_Update(last_spd_L, last_spd_R);
    // }
    
    // /* Send sensor data to RPi every 20ms via USART1 */
    // static uint32_t last_send   = 0;
    // static int16_t  prev_odom_L = 0;
    // static int16_t  prev_odom_R = 0;
    // if (now - last_send >= 20) {
    //     last_send = now;

    //     int16_t cur_L = Encoder_GetCount(ENCODER_LEFT);
    //     int16_t cur_R = Encoder_GetCount(ENCODER_RIGHT);
    //     Protocol_SendOdom(cur_L - prev_odom_L,
    //                       cur_R - prev_odom_R);
    //     prev_odom_L = cur_L;
    //     prev_odom_R = cur_R;

    //     MPU6050_Data_t imu;
    //     if (MPU6050_ReadAll(&imu) == HAL_OK) {
    //         Protocol_SendIMU(&imu);
    //     }
    // }

    // /* Debug print to PC (USART2) every 500ms */
    // static uint32_t last_print = 0;
    // if (now - last_print >= 500) {
    //     last_print = now;

    //     const ProtoStats_t *st = Protocol_GetStats();
    //     printf("[DIAG] rx_pkt:%lu err:%lu  spd L:%.1f R:%.1f mm/s  enc L:%d R:%d\r\n",
    //            st->rx_packets, Protocol_GetErrorCount(),
    //            last_spd_L, last_spd_R,
    //            Encoder_GetCount(ENCODER_LEFT),
    //            Encoder_GetCount(ENCODER_RIGHT));
    // }

    /* 모터 속도 테스트 *************************************************************/
    // /* 1. 엔코더 현재 속도값 읽기 */
    // static uint32_t last_time  = 0;
    // static float    spd_L = 0.0f;
    // static float    spd_R = 0.0f;
    // if (now - last_time >= 10) {
    //     last_time   = now;
    //     spd_L = Encoder_GetSpeed(ENCODER_LEFT);
    //     spd_R = Encoder_GetSpeed(ENCODER_RIGHT);
    // }

    // /* 2. 블루 버튼으로 모터 동작 토글 */    
    // static uint8_t motor_running = 0;
    // static uint8_t blue_button_pressed = 0; 
    // if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
    //     if (!blue_button_pressed) {
    //         motor_running = !motor_running;
    //         blue_button_pressed = 1; // 버튼이 눌렸음을 기록
    //         printf("Button Pressed!\r\n");
    //     }
    // } 
    // else {
    //     blue_button_pressed = 0; // 버튼이 떼어졌음을 기록
    // }

    // /* 3. 실제 모터 제어 */
    // static int16_t test_speed = 500;  // 테스트용 속도
    // static uint32_t last_send   = 0;
    // if (motor_running) {
    //     Motor_SetRaw(MOTOR_LEFT, test_speed);
    //     Motor_SetRaw(MOTOR_RIGHT, test_speed);

    //     /* 3-1. RPi로 로봇 속력 [mm/s] 전송 - 100ms 주기 */
    //     if (now - last_send >= 100) {
    //       last_send = now;
    //       Protocol_SendOdom((int16_t) spd_L, (int16_t) spd_R);
    //     }
    // }
    // else {
    //     Motor_SetRaw(MOTOR_LEFT, 0);
    //     Motor_SetRaw(MOTOR_RIGHT, 0);
    // }

    

  }
  /* USER CODE END 3 */
}
    
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
