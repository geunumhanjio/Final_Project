# BSP 문서 목차

## 읽기 순서 (추천)

처음 이 프로젝트를 접하는 경우, 아래 순서로 읽는 것을 권장합니다:

1. [시스템 아키텍처](architecture.md) — 전체 구조와 데이터 흐름 파악
2. [UART 프로토콜](protocol/uart_protocol.md) — RPi↔STM32 통신 방식 이해
3. [모터 제어 가이드](guides/Motor_Control_Complete_Guide.md) — 모터 드라이버 사용법
4. [엔코더 가이드](guides/Encoder_Setup_Guide.md) — 엔코더 설정 및 데이터 해석
5. [MPU6050 가이드](guides/MPU6050_Setup_Guide.md) — IMU 센서 설정

## 문서 목록

| 문서 | 경로 | 설명 | 대상 |
|------|------|------|------|
| 시스템 아키텍처 | [architecture.md](architecture.md) | 블록 다이어그램, SW 레이어, 타이밍, ISR 구조 | 전체 개발자 |
| UART 프로토콜 | [protocol/uart_protocol.md](protocol/uart_protocol.md) | 패킷 구조, 명령 정의, ROS 연동 | RPi/STM32 개발자 |
| 모터 제어 가이드 | [guides/Motor_Control_Complete_Guide.md](guides/Motor_Control_Complete_Guide.md) | 하드웨어 연결, CubeMX 설정, API, 안전 기능 | BSP 개발자 |
| 엔코더 가이드 | [guides/Encoder_Setup_Guide.md](guides/Encoder_Setup_Guide.md) | 쿼드러처 엔코더 설정 및 데이터 해석 | BSP 개발자 |
| MPU6050 가이드 | [guides/MPU6050_Setup_Guide.md](guides/MPU6050_Setup_Guide.md) | IMU 센서 I2C 통신 설정 | BSP 개발자 |
| CubeMX 설정 | [guides/STM32_CubeMX_Setup_Guide.md](guides/STM32_CubeMX_Setup_Guide.md) | 모터 제어 핀/타이머 CubeMX 설정 | BSP 개발자 |

## 하드웨어 참고 자료

| 파일 | 경로 | 설명 |
|------|------|------|
| MPU6050 레지스터 맵 | [hardware/RM-MPU-6000A.pdf](hardware/RM-MPU-6000A.pdf) | MPU6050 데이터시트 |
| 엔코더 모터 스펙 | [hardware/](hardware/) | DFROBOT 엔코더 TT 모터 사양서 |
| L298N 회로도 | [hardware/L298N.png](hardware/L298N.png) | 모터 드라이버 회로 |
| 시스템 다이어그램 | [hardware/](hardware/) | 전체 시스템 배선도 |
