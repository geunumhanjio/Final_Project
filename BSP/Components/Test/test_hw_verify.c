#if defined(MODE_HW_VERIFY)

#include "test_hw_verify.h"
#include "drv_motor.h"
#include "drv_encoder.h"
#include "drv_mpu6050.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>

/* ── 설정 상수 ──────────────────────────────────────────────────────────── */
#define TEST_MOTOR_SPEED_MMPS    200    /* 전진 검증 속도 (mm/s) */
#define TEST_MOTOR_REV_MMPS     -200    /* 후진 검증 속도 (mm/s) */
#define TEST_ENC_DURATION_MS    2000    /* 전진 엔코더 검증 구동 시간 */
#define TEST_DIR_DURATION_MS    1500    /* 후진 방향 검증 구동 시간 */
#define TEST_COOLDOWN_MS         200    /* 모터 정지 후 관성 제거 대기 */
#define TEST_ENC_MIN_COUNT       100    /* PASS 최소 엔코더 카운트 절댓값 */
#define TEST_IMU_ACCEL_Z_MIN    8000    /* PASS 최소 accel_z 절댓값 (≈0.5g at ±2g) */

/* ── LED 핀 ──────────────────────────────────────────────────────────────── */
#define TEST_LED_PORT   GPIOC
#define TEST_LED_PIN    GPIO_PIN_7
#define TEST_LED_BLINK_MS  1000   /* ON / OFF 각 유지 시간 */

/* ── 상태 열거형 ─────────────────────────────────────────────────────────── */
typedef enum {
    HW_STATE_INIT = 0,
    HW_STATE_IMU,
    HW_STATE_ENC_L_START,
    HW_STATE_ENC_L_WAIT,
    HW_STATE_ENC_L_READ,
    HW_STATE_ENC_R_START,
    HW_STATE_ENC_R_WAIT,
    HW_STATE_ENC_R_READ,
    HW_STATE_MOTOR_L_REV,
    HW_STATE_MOTOR_L_WAIT,
    HW_STATE_MOTOR_L_READ,
    HW_STATE_MOTOR_R_REV,
    HW_STATE_MOTOR_R_WAIT,
    HW_STATE_MOTOR_R_READ,
    HW_STATE_LED_ON,
    HW_STATE_LED_OFF,
    HW_STATE_SUMMARY,
    HW_STATE_DONE
} HwVerify_State_t;

typedef enum { RESULT_PENDING, RESULT_PASS, RESULT_FAIL } TestResult_t;

/* ── 결과 저장 ───────────────────────────────────────────────────────────── */
static struct {
    TestResult_t imu;
    TestResult_t enc_l;
    TestResult_t enc_r;
    TestResult_t motor_l;
    TestResult_t motor_r;
    TestResult_t led;
} s_results;

/* ── 상태 머신 변수 ──────────────────────────────────────────────────────── */
static HwVerify_State_t s_state    = HW_STATE_INIT;
static uint32_t         s_state_ts = 0;

static void set_state(HwVerify_State_t next)
{
    s_state    = next;
    s_state_ts = HAL_GetTick();
}

static uint32_t elapsed(void)
{
    return HAL_GetTick() - s_state_ts;
}

static const char *rstr(TestResult_t r)
{
    if (r == RESULT_PASS) return "PASS";
    if (r == RESULT_FAIL) return "FAIL";
    return "N/A";
}

/* ════════════════════════════════════════════════════════════════════════════
   Init
   ════════════════════════════════════════════════════════════════════════════ */
void Test_HwVerify_Init(void)
{
    s_results.imu     = RESULT_PENDING;
    s_results.enc_l   = RESULT_PENDING;
    s_results.enc_r   = RESULT_PENDING;
    s_results.motor_l = RESULT_PENDING;
    s_results.motor_r = RESULT_PENDING;
    s_results.led     = RESULT_PENDING;

    /* PC7 GPIO Output 초기화 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = TEST_LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TEST_LED_PORT, &gpio);
    HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_RESET);

    set_state(HW_STATE_INIT);

    printf("\r\n=== HW VERIFY START ===\r\n");
    printf("자동 진행 (~11초) — 버튼 불필요\r\n");
    printf("주의: 모터가 회전합니다. 바닥에 내려놓으세요.\r\n");
    printf("========================\r\n\r\n");
}

/* ════════════════════════════════════════════════════════════════════════════
   Loop — main() while(1) 에서 매 루프 호출
   ════════════════════════════════════════════════════════════════════════════ */
void Test_HwVerify_Loop(void)
{
    switch (s_state)
    {

    /* ── INIT: 500ms 안정화 대기 ─────────────────────────────────────────── */
    case HW_STATE_INIT:
        if (elapsed() >= 500) {
            set_state(HW_STATE_IMU);
        }
        break;

    /* ── [1/5] IMU 검증 (동기 처리) ─────────────────────────────────────── */
    case HW_STATE_IMU:
    {
        printf("[TEST 1/5] IMU MPU6050...\r\n");

        HAL_StatusTypeDef init_ok = MPU6050_Init();

        MPU6050_Data_t imu = {0};
        HAL_StatusTypeDef read_ok = MPU6050_ReadAll(&imu);

        printf("  MPU6050_Init : %s\r\n", (init_ok == HAL_OK) ? "OK" : "ERROR");
        printf("  MPU6050_Read : %s\r\n", (read_ok == HAL_OK) ? "OK" : "ERROR");

        if (read_ok == HAL_OK) {
            printf("  accel: X=%+d Y=%+d Z=%+d\r\n",
                   imu.accel_x, imu.accel_y, imu.accel_z);
            printf("  gyro:  X=%+d Y=%+d Z=%+d\r\n",
                   imu.gyro_x, imu.gyro_y, imu.gyro_z);
        }

        if (init_ok == HAL_OK &&
            read_ok == HAL_OK &&
            abs(imu.accel_z) > TEST_IMU_ACCEL_Z_MIN)
        {
            s_results.imu = RESULT_PASS;
            printf("[IMU] PASS\r\n\r\n");
        } else {
            s_results.imu = RESULT_FAIL;
            printf("[IMU] FAIL");
            if (init_ok != HAL_OK) printf(" — Init 실패 (I2C 배선 확인)");
            if (read_ok != HAL_OK) printf(" — Read 실패");
            if (init_ok == HAL_OK && read_ok == HAL_OK)
                printf(" — accel_z=%+d (기대: |>%d|)", imu.accel_z, TEST_IMU_ACCEL_Z_MIN);
            printf("\r\n\r\n");
        }

        set_state(HW_STATE_ENC_L_START);
        break;
    }

    /* ── [2/5] 엔코더 왼쪽 전진 ─────────────────────────────────────────── */
    case HW_STATE_ENC_L_START:
        printf("[TEST 2/5] Motor L + Encoder L (Forward)...\r\n");
        printf("  running +%d mm/s for %dms...\r\n",
               TEST_MOTOR_SPEED_MMPS, TEST_ENC_DURATION_MS);
        Encoder_ResetCount(ENCODER_LEFT);
        Motor_SetRaw(MOTOR_LEFT, TEST_MOTOR_SPEED_MMPS);
        set_state(HW_STATE_ENC_L_WAIT);
        break;

    case HW_STATE_ENC_L_WAIT:
        if (elapsed() >= TEST_ENC_DURATION_MS) {
            set_state(HW_STATE_ENC_L_READ);
        }
        break;

    case HW_STATE_ENC_L_READ:
    {
        Motor_SetRaw(MOTOR_LEFT, 0);
        int16_t cnt = Encoder_GetCount(ENCODER_LEFT);
        printf("  enc_count = %+d\r\n", cnt);

        if (cnt > TEST_ENC_MIN_COUNT) {
            s_results.enc_l = RESULT_PASS;
            printf("[ENC_L FWD] PASS\r\n\r\n");
        } else {
            s_results.enc_l = RESULT_FAIL;
            printf("[ENC_L FWD] FAIL — count=%+d (기대: >%d)\r\n\r\n",
                   cnt, TEST_ENC_MIN_COUNT);
        }
        HAL_Delay(TEST_COOLDOWN_MS);
        set_state(HW_STATE_ENC_R_START);
        break;
    }

    /* ── [3/5] 엔코더 오른쪽 전진 ───────────────────────────────────────── */
    case HW_STATE_ENC_R_START:
        printf("[TEST 3/5] Motor R + Encoder R (Forward)...\r\n");
        printf("  running +%d mm/s for %dms...\r\n",
               TEST_MOTOR_SPEED_MMPS, TEST_ENC_DURATION_MS);
        Encoder_ResetCount(ENCODER_RIGHT);
        Motor_SetRaw(MOTOR_RIGHT, TEST_MOTOR_SPEED_MMPS);
        set_state(HW_STATE_ENC_R_WAIT);
        break;

    case HW_STATE_ENC_R_WAIT:
        if (elapsed() >= TEST_ENC_DURATION_MS) {
            set_state(HW_STATE_ENC_R_READ);
        }
        break;

    case HW_STATE_ENC_R_READ:
    {
        Motor_SetRaw(MOTOR_RIGHT, 0);
        int16_t cnt = Encoder_GetCount(ENCODER_RIGHT);
        printf("  enc_count = %+d\r\n", cnt);

        if (cnt > TEST_ENC_MIN_COUNT) {
            s_results.enc_r = RESULT_PASS;
            printf("[ENC_R FWD] PASS\r\n\r\n");
        } else {
            s_results.enc_r = RESULT_FAIL;
            printf("[ENC_R FWD] FAIL — count=%+d (기대: >%d)\r\n\r\n",
                   cnt, TEST_ENC_MIN_COUNT);
        }
        HAL_Delay(TEST_COOLDOWN_MS);
        set_state(HW_STATE_MOTOR_L_REV);
        break;
    }

    /* ── [4/5] 모터 왼쪽 후진 방향 ──────────────────────────────────────── */
    case HW_STATE_MOTOR_L_REV:
        printf("[TEST 4/5] Motor L Direction (Reverse)...\r\n");
        printf("  running %d mm/s for %dms...\r\n",
               TEST_MOTOR_REV_MMPS, TEST_DIR_DURATION_MS);
        Encoder_ResetCount(ENCODER_LEFT);
        Motor_SetRaw(MOTOR_LEFT, TEST_MOTOR_REV_MMPS);
        set_state(HW_STATE_MOTOR_L_WAIT);
        break;

    case HW_STATE_MOTOR_L_WAIT:
        if (elapsed() >= TEST_DIR_DURATION_MS) {
            set_state(HW_STATE_MOTOR_L_READ);
        }
        break;

    case HW_STATE_MOTOR_L_READ:
    {
        Motor_SetRaw(MOTOR_LEFT, 0);
        int16_t cnt = Encoder_GetCount(ENCODER_LEFT);
        printf("  enc_count = %+d\r\n", cnt);

        if (cnt < -TEST_ENC_MIN_COUNT) {
            s_results.motor_l = RESULT_PASS;
            printf("[MOTOR_L REV] PASS\r\n\r\n");
        } else {
            s_results.motor_l = RESULT_FAIL;
            printf("[MOTOR_L REV] FAIL — count=%+d (기대: <%d)\r\n\r\n",
                   cnt, -TEST_ENC_MIN_COUNT);
        }
        HAL_Delay(TEST_COOLDOWN_MS);
        set_state(HW_STATE_MOTOR_R_REV);
        break;
    }

    /* ── [5/5] 모터 오른쪽 후진 방향 ────────────────────────────────────── */
    case HW_STATE_MOTOR_R_REV:
        printf("[TEST 5/5] Motor R Direction (Reverse)...\r\n");
        printf("  running %d mm/s for %dms...\r\n",
               TEST_MOTOR_REV_MMPS, TEST_DIR_DURATION_MS);
        Encoder_ResetCount(ENCODER_RIGHT);
        Motor_SetRaw(MOTOR_RIGHT, TEST_MOTOR_REV_MMPS);
        set_state(HW_STATE_MOTOR_R_WAIT);
        break;

    case HW_STATE_MOTOR_R_WAIT:
        if (elapsed() >= TEST_DIR_DURATION_MS) {
            set_state(HW_STATE_MOTOR_R_READ);
        }
        break;

    case HW_STATE_MOTOR_R_READ:
    {
        Motor_SetRaw(MOTOR_RIGHT, 0);
        int16_t cnt = Encoder_GetCount(ENCODER_RIGHT);
        printf("  enc_count = %+d\r\n", cnt);

        if (cnt < -TEST_ENC_MIN_COUNT) {
            s_results.motor_r = RESULT_PASS;
            printf("[MOTOR_R REV] PASS\r\n\r\n");
        } else {
            s_results.motor_r = RESULT_FAIL;
            printf("[MOTOR_R REV] FAIL — count=%+d (기대: <%d)\r\n\r\n",
                   cnt, -TEST_ENC_MIN_COUNT);
        }
        HAL_Delay(TEST_COOLDOWN_MS);
        set_state(HW_STATE_LED_ON);
        break;
    }

    /* ── [6/6] LED 검증 ──────────────────────────────────────────────────── */
    case HW_STATE_LED_ON:
        if (elapsed() == 0) {
            printf("[TEST 6/6] External LED (PC7)...\r\n");
            printf("  LED ON — 지금 켜져 있어야 합니다 (%dms)...\r\n", TEST_LED_BLINK_MS);
            HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_SET);
        }
        if (elapsed() >= TEST_LED_BLINK_MS) {
            set_state(HW_STATE_LED_OFF);
        }
        break;

    case HW_STATE_LED_OFF:
        if (elapsed() == 0) {
            printf("  LED OFF — 지금 꺼져 있어야 합니다 (%dms)...\r\n", TEST_LED_BLINK_MS);
            HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_RESET);
        }
        if (elapsed() >= TEST_LED_BLINK_MS) {
            /* 눈으로 확인하는 테스트 — GPIO 제어 자체는 성공으로 기록 */
            s_results.led = RESULT_PASS;
            printf("[LED] PASS (육안 확인 필요)\r\n\r\n");
            set_state(HW_STATE_SUMMARY);
        }
        break;

    /* ── SUMMARY ─────────────────────────────────────────────────────────── */
    case HW_STATE_SUMMARY:
    {
        Motor_SoftStop();

        uint8_t all_pass = (s_results.imu     == RESULT_PASS) &&
                           (s_results.enc_l   == RESULT_PASS) &&
                           (s_results.enc_r   == RESULT_PASS) &&
                           (s_results.motor_l == RESULT_PASS) &&
                           (s_results.motor_r == RESULT_PASS) &&
                           (s_results.led     == RESULT_PASS);

        printf("=== SUMMARY ===\r\n");
        printf("IMU      : %s\r\n", rstr(s_results.imu));
        printf("ENC_L    : %s\r\n", rstr(s_results.enc_l));
        printf("ENC_R    : %s\r\n", rstr(s_results.enc_r));
        printf("MOTOR_L  : %s\r\n", rstr(s_results.motor_l));
        printf("MOTOR_R  : %s\r\n", rstr(s_results.motor_r));
        printf("LED(PC7) : %s\r\n", rstr(s_results.led));
        printf("===============\r\n");

        if (all_pass) {
            printf("=== ALL PASS ===\r\n");
        } else {
            printf("=== SOME FAILED — 배선/연결 확인 필요 ===\r\n");
        }

        set_state(HW_STATE_DONE);
        break;
    }

    /* ── DONE ────────────────────────────────────────────────────────────── */
    case HW_STATE_DONE:
    default:
        break;
    }
}

#endif /* MODE_HW_VERIFY */
