#ifndef __UART_PROTOCOL_H
#define __UART_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "drv_mpu6050.h"
#include <stdbool.h>

/* Protocol constants --------------------------------------------------------*/
#define PROTO_HEADER1           0xFF
#define PROTO_HEADER2           0xFE
#define PROTO_MAX_DATA_LEN      12
#define PROTO_MIN_PACKET_SIZE   5   /* H1 + H2 + CMD + LEN + CHKSUM */
#define PROTO_MAX_PACKET_SIZE   17  /* H1 + H2 + CMD + LEN + 12 DATA + CHKSUM */

/* Command definitions: RPi -> STM32 (0x01 ~ 0x7F) --------------------------*/
#define CMD_VELOCITY            0x01
#define CMD_STOP                0x02
#define CMD_ESTOP               0x03
#define CMD_RELEASE             0x04

/* Response definitions: STM32 -> RPi (0x80 ~ 0xFF) -------------------------*/
#define RSP_ODOM                0x81
#define RSP_IMU                 0x82

/* Parser state machine ------------------------------------------------------*/
typedef enum {
    PARSE_WAIT_HEADER1 = 0,
    PARSE_WAIT_HEADER2,
    PARSE_WAIT_CMD,
    PARSE_WAIT_LEN,
    PARSE_WAIT_DATA,
    PARSE_WAIT_CHECKSUM
} ParseState_t;

/* Parsed packet structure ---------------------------------------------------*/
typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[PROTO_MAX_DATA_LEN];
    uint8_t checksum;
} Packet_t;

/* Protocol statistics -------------------------------------------------------*/
typedef struct {
    uint32_t rx_packets;
    uint32_t rx_checksum_errors;
    uint32_t rx_invalid_len;
    uint32_t rx_unknown_cmd;
} ProtoStats_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  프로토콜 모듈 초기화 및 UART RX 인터럽트 시작
 * @param  huart: RPi 통신용 UART 핸들 (huart1)
 */
void Protocol_Init(UART_HandleTypeDef *huart);

/**
 * @brief  수신 바이트를 상태머신에 투입 (ISR 콜백에서 호출)
 * @param  byte: 수신된 1바이트
 */
void Protocol_FeedByte(uint8_t byte);

/**
 * @brief  완성된 패킷이 있는지 확인하고 명령 실행 (메인 루프에서 호출)
 * @retval true: 패킷 처리됨, false: 처리할 패킷 없음
 */
bool Protocol_Process(void);

/**
 * @brief  프로토콜 통계 조회
 * @retval ProtoStats_t 포인터
 */
const ProtoStats_t* Protocol_GetStats(void);

/**
 * @brief  UART 에러 발생 횟수 조회 (디버깅용)
 * @retval 에러 카운트
 */
uint32_t Protocol_GetErrorCount(void);

/**
 * @brief  오도메트리(엔코더 카운트) 응답 패킷 송신
 * @param  left_ticks:  왼쪽 엔코더 카운트 (int16)
 * @param  right_ticks: 오른쪽 엔코더 카운트 (int16)
 */
void Protocol_SendOdom(int16_t left_ticks, int16_t right_ticks);

/**
 * @brief  IMU 센서 데이터 응답 패킷 송신
 * @param  imu: MPU6050 6축 raw 데이터 포인터
 */
void Protocol_SendIMU(const MPU6050_Data_t *imu);

#ifdef __cplusplus
}
#endif

#endif /* __UART_PROTOCOL_H */
