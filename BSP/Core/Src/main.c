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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motor_control.h"
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
static uint8_t test_phase = 0;
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
  /* USER CODE BEGIN 2 */
  if (Motor_Init() == MOTOR_OK) {
      printf("Motor driver initialized successfully!\r\n");
  } else {
      printf("Motor driver initialization failed!\r\n");
      Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    switch (test_phase) {
    case 0:
        // 전진: 양쪽 300 mm/s
        printf("[Test 1] Forward 300 mm/s\r\n");
        Motor_SetVelocity(300, 300);
        HAL_Delay(2000);
        test_phase++;
        break;

    case 1:
        // 부드러운 정지 (Coast)
        printf("[Test 2] SoftStop\r\n");
        Motor_SoftStop();
        HAL_Delay(1000);
        test_phase++;
        break;

    case 2:
        // 후진: 양쪽 -200 mm/s
        printf("[Test 3] Backward 200 mm/s\r\n");
        Motor_SetVelocity(-200, -200);
        HAL_Delay(2000);
        test_phase++;
        break;

    case 3:
        // 비상 정지 (Brake)
        printf("[Test 4] EmergencyStop\r\n");
        Motor_EmergencyStop();
        HAL_Delay(1000);
        test_phase++;
        break;

    case 4:
        // 비상 정지 해제
        printf("[Test 5] ReleaseEmergency\r\n");
        Motor_ReleaseEmergency();
        HAL_Delay(500);
        test_phase++;
        break;

    case 5:
        // 제자리 좌회전: 왼쪽 후진, 오른쪽 전진
        printf("[Test 6] TurnLeft (L=-250, R=+250)\r\n");
        Motor_SetVelocity(-250, 250);
        HAL_Delay(1500);
        test_phase++;
        break;

    case 6:
        printf("[Test 6-1] SoftStop\r\n");
        Motor_SoftStop();
        HAL_Delay(1000);
        test_phase++;
        break;

    case 7:
        // 제자리 우회전: 왼쪽 전진, 오른쪽 후진
        printf("[Test 7] TurnRight (L=+250, R=-250)\r\n");
        Motor_SetVelocity(250, -250);
        HAL_Delay(1500);
        test_phase++;
        break;

    case 8:
        printf("[Test 7-1] SoftStop\r\n");
        Motor_SoftStop();
        HAL_Delay(1000);
        test_phase++;
        break;

    case 9:
        // 좌 완만 커브: 왼쪽 느리게, 오른쪽 빠르게
        printf("[Test 8] Curve Left (L=150, R=400)\r\n");
        Motor_SetVelocity(150, 400);
        HAL_Delay(2000);
        test_phase++;
        break;

    case 10:
        printf("[Test 8-1] SoftStop\r\n");
        Motor_SoftStop();
        HAL_Delay(1000);
        test_phase++;
        break;

    case 11:
        // 최대 속도 테스트
        printf("[Test 9] Max speed 600 mm/s\r\n");
        Motor_SetVelocity(600, 600);
        HAL_Delay(2000);
        test_phase++;
        break;

    case 12:
        // 비상 정지로 최대 속도에서 즉시 정지
        printf("[Test 10] EmergencyStop from max speed\r\n");
        Motor_EmergencyStop();
        HAL_Delay(1000);
        Motor_ReleaseEmergency();
        HAL_Delay(500);
        test_phase++;
        break;

    case 13:
        // 타임아웃 테스트: 명령 후 500ms 이상 대기
        printf("[Test 11] Timeout test - driving then waiting 700ms\r\n");
        Motor_SetVelocity(300, 300);
        HAL_Delay(700);
        if (Motor_CheckTimeout()) {
            printf("  -> Timeout triggered, auto-stopped!\r\n");
        } else {
            printf("  -> No timeout (unexpected)\r\n");
        }
        HAL_Delay(1000);
        test_phase++;
        break;

    default:
        // 전체 테스트 완료 → 처음부터 반복
        printf("=== All tests done. Restarting... ===\r\n\r\n");
        HAL_Delay(3000);
        test_phase = 0;
        break;
    }
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
