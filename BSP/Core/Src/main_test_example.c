/**
  ******************************************************************************
  * @file           : main.c 테스트 예제
  * @brief          : 모터 제어 라이브러리 사용 예제
  ******************************************************************************
  * CubeMX로 생성된 main.c의 USER CODE 섹션에 삽입
  ******************************************************************************
  */

/* USER CODE BEGIN Includes */
#include "motor_control.h"
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
// main() 함수의 초기화 부분에 추가

// 모터 드라이버 초기화
if (Motor_Init() == MOTOR_OK) {
    printf("Motor driver initialized successfully!\r\n");
} else {
    printf("Motor driver initialization failed!\r\n");
    Error_Handler();
}

/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
// while(1) 루프 안에 추가

/**
 * 테스트 시퀀스 1: 기본 동작 테스트
 */
void Test_BasicMovement(void)
{
    printf("=== Motor Test Sequence ===\r\n");
    
    // 1. 전진 (50% 속도)
    printf("Forward 50%%...\r\n");
    Motor_Forward(50);
    HAL_Delay(2000);
    
    // 2. 정지
    printf("Stop...\r\n");
    Motor_Stop();
    HAL_Delay(1000);
    
    // 3. 후진 (30% 속도)
    printf("Backward 30%%...\r\n");
    Motor_Backward(30);
    HAL_Delay(2000);
    
    // 4. 정지
    printf("Stop...\r\n");
    Motor_Stop();
    HAL_Delay(1000);
    
    // 5. 좌회전
    printf("Turn Left 40%%...\r\n");
    Motor_TurnLeft(40);
    HAL_Delay(1000);
    
    // 6. 정지
    printf("Stop...\r\n");
    Motor_Stop();
    HAL_Delay(1000);
    
    // 7. 우회전
    printf("Turn Right 40%%...\r\n");
    Motor_TurnRight(40);
    HAL_Delay(1000);
    
    // 8. 관성 정지
    printf("Coast stop...\r\n");
    Motor_Coast();
    HAL_Delay(2000);
    
    printf("=== Test Complete ===\r\n\r\n");
}

/**
 * 테스트 시퀀스 2: 속도 변화 테스트
 */
void Test_SpeedRamping(void)
{
    printf("=== Speed Ramping Test ===\r\n");
    
    // 0%에서 100%까지 점진적 가속
    for (uint8_t speed = 0; speed <= 100; speed += 10) {
        printf("Speed: %d%%\r\n", speed);
        Motor_Forward(speed);
        HAL_Delay(500);
    }
    
    // 100%에서 0%까지 점진적 감속
    for (int8_t speed = 100; speed >= 0; speed -= 10) {
        printf("Speed: %d%%\r\n", speed);
        Motor_Forward(speed);
        HAL_Delay(500);
    }
    
    Motor_Stop();
    printf("=== Speed Test Complete ===\r\n\r\n");
}

/**
 * 테스트 시퀀스 3: 개별 모터 제어
 */
void Test_IndividualControl(void)
{
    printf("=== Individual Motor Control Test ===\r\n");
    
    // 왼쪽 모터만 동작
    printf("Left motor only (forward 50%%)...\r\n");
    Motor_SetDirection(MOTOR_LEFT, MOTOR_DIR_FORWARD);
    Motor_SetSpeed(MOTOR_LEFT, 50);
    Motor_SetSpeed(MOTOR_RIGHT, 0);
    HAL_Delay(2000);
    
    Motor_Stop();
    HAL_Delay(1000);
    
    // 오른쪽 모터만 동작
    printf("Right motor only (forward 50%%)...\r\n");
    Motor_SetDirection(MOTOR_RIGHT, MOTOR_DIR_FORWARD);
    Motor_SetSpeed(MOTOR_LEFT, 0);
    Motor_SetSpeed(MOTOR_RIGHT, 50);
    HAL_Delay(2000);
    
    Motor_Stop();
    HAL_Delay(1000);
    
    // 좌우 다른 속도 (부드러운 곡선 주행)
    printf("Differential speed (L:30%%, R:60%%)...\r\n");
    Motor_SetBoth(30, 60, MOTOR_DIR_FORWARD, MOTOR_DIR_FORWARD);
    HAL_Delay(3000);
    
    Motor_Stop();
    printf("=== Individual Test Complete ===\r\n\r\n");
}

// while(1) 루프에서 호출
while (1)
{
    Test_BasicMovement();
    HAL_Delay(3000);
    
    Test_SpeedRamping();
    HAL_Delay(3000);
    
    Test_IndividualControl();
    HAL_Delay(3000);
}

/* USER CODE END 3 */

/**
  ******************************************************************************
  * @note  USART2 printf 리다이렉션 (선택 사항)
  *        디버깅 메시지를 시리얼 모니터로 출력하려면 추가
  ******************************************************************************
  */

/* USER CODE BEGIN 4 */

// printf를 USART2로 리다이렉션
#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/* USER CODE END 4 */
