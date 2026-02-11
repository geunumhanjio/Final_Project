# ROS Robot Driver - BSP (Board Support Package)

산업 안전 재난 대응 자율주행 로봇의 STM32 펌웨어.
Hanwha Vision AI CCTV가 감지한 위험 좌표로 ROS 기반 로봇이 자율 이동하며, 이 저장소는 로봇의 **하드웨어 제어 계층**을 담당한다.

## 주요 기능

- **모터 제어** — L298N 드라이버를 통한 양쪽 바퀴 독립 속도 제어 (PWM)
- **UART 프로토콜** — Raspberry Pi 4와 바이너리 패킷 통신 (명령 수신 / 센서 데이터 송신)
- **엔코더** — 쿼드러처 엔코더로 좌우 바퀴 회전수 측정 (TIM3/TIM4)
- **IMU 센서** — MPU6050 6축 가속도/자이로 데이터 수집 (I2C1)
- **안전 기능** — 가속도 제한, 명령 타임아웃(500ms), 비상 정지

## 하드웨어

| 부품 | 역할 |
|------|------|
| STM32 NUCLEO-F401RE | MCU (Cortex-M4, 84 MHz) |
| L298N 모터 드라이버 | DC 모터 방향/속도 제어 |
| 엔코더 TT 모터 x2 | 바퀴 구동 + 회전수 측정 |
| MPU6050 (GY-521) | 6축 IMU (자세 측정) |
| Raspberry Pi 4 | ROS 노드, UART로 STM32와 통신 |

## Quick Start

### 빌드
```bash
make -j$(nproc)
```

### 플래시
```bash
openocd -f "board/st_nucleo_f4.cfg" -c "program build/ROS_Robot_Driver.elf verify reset"
```

### 디버그 시리얼 모니터 (USART2, PC USB 연결)
```bash
screen /dev/ttyACM0 115200
```

## 디렉토리 구조

```
BSP/
├── Core/
│   ├── Inc/              # 헤더 파일 (motor_control.h, encoder.h, ...)
│   └── Src/              # 소스 파일 (main.c, motor_control.c, ...)
├── Drivers/              # HAL 드라이버 + CMSIS (CubeMX 관리, 수정 금지)
├── docs/                 # 프로젝트 문서 (아래 참조)
├── tools/                # 테스트 유틸리티
├── Makefile              # 빌드 시스템
├── ROS_Robot_Driver.ioc  # CubeMX 프로젝트 파일
└── STM32F401XX_FLASH.ld  # 링커 스크립트
```

## 문서

| 문서 | 설명 |
|------|------|
| [docs/README.md](docs/README.md) | 문서 목차 및 읽기 순서 |
| [docs/architecture.md](docs/architecture.md) | 시스템 아키텍처 (데이터 흐름, 타이밍, ISR 구조) |
| [docs/guides/](docs/guides/) | 모듈별 셋업 가이드 (모터, 엔코더, IMU, CubeMX) |
| [docs/protocol/](docs/protocol/) | UART 통신 프로토콜 명세 |
| [docs/hardware/](docs/hardware/) | 데이터시트, 회로도 |

---

**작성:** 김지오
**MCU:** STM32F401RE (NUCLEO)
**Toolchain:** arm-none-eabi-gcc
