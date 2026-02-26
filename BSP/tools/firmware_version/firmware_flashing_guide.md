# STM32 펌웨어 플래시 가이드

> **배포 주체:** BSP 팀
> **대상 독자:** ROS 팀 — STM32 보드에 펌웨어를 올려야 하는 팀원
> **문의:** BSP 팀 (새 펌웨어 배포 또는 프로토콜 변경 시 BSP 팀이 공지합니다)

---

## 1. 개요

BSP 팀은 STM32 펌웨어를 빌드하여 `.bin` 파일 형태로 배포합니다.
RPi 팀은 이 가이드에 따라 라즈베리파이에서 직접 STM32 보드에 펌웨어를 업로드할 수 있습니다.

```
[ BSP 팀 ] -- 빌드 → ROS_Robot_Driver.bin --> [ RPi 팀 ] -- 플래시 → [ STM32 NUCLEO ]
```

---

## 2. 현재 펌웨어 정보

| 항목 | 내용 |
|------|------|
| 타겟 MCU | STM32F401RE (NUCLEO-F401RE) |
| 바이너리 크기 | ~22 KB / 512 KB Flash |
| UART 통신 | USART1, 115200 baud (RPi ↔ STM32) |
| 응답 주기 | 50 ms (오도메트리 + IMU 데이터 송신) |

### 구현된 기능

- [x] RPi로부터 속도 명령 수신 (`CMD_VELOCITY`)
- [x] 부드러운 정지 / 비상 정지 (`CMD_STOP`, `CMD_ESTOP`, `CMD_RELEASE`)
- [x] 엔코더 카운트 데이터 송신 (`RSP_ODOM`, 50ms 주기)
- [x] IMU raw 데이터 송신 (`RSP_IMU`, 50ms 주기)
- [x] 명령 타임아웃 안전장치 (500ms 무응답 시 자동 정지)

---

## 3. 준비물

### 하드웨어

| 항목 | 비고 |
|------|------|
| STM32 NUCLEO-F401RE 보드 | — |
| USB Micro-B 케이블 | STM 보드와 RPI 연결용 |
| 라즈베리파이 4 | — |

### 소프트웨어

라즈베리파이에 `stlink-tools` 패키지가 필요합니다.

```bash
sudo apt update && sudo apt install stlink-tools
```

---

## 4. 하드웨어 연결

```
라즈베리파이 4                NUCLEO-F401RE
┌─────────────┐              ┌──────────────────────┐
│             │              │  ┌────────────────┐   │
│   USB-A ────┼──Micro-B────►│  │  ST-LINK/V2-1  │   │
│             │              │  │  (CN1 커넥터)   │   │
└─────────────┘              │  └────────────────┘   │
                             │                        │
                             │  ← 이 USB로 전원도 공급됨 │
                             └──────────────────────┘
```

---

## 5. 펌웨어 파일 업데이트 
 
BSP 팀이 새 펌웨어를 배포할 때는 로봇의 라즈베리파이의 ~/Final_Project_firmware 디렉터리에  저장 후 공지합니다. 

```bash
# 예시: 홈 디렉터리에 저장
~/Final_Project_firmware/V1/ROS_Robot_Driver.bin
```

---

## 6. STM 보드 Load 방법

### 방법 A — 스크립트 사용 (권장)

라즈베리파이의 `~/Final_Project_firmware/flash_stm32.sh` 스크립트를 사용합니다.

```bash
# 실행 (스크립트와 .bin 파일이 같은 디렉터리에 있을 때)
cd ~/Final_Project_firmware
sudo ./flash_stm32.sh ~/<버전>/ROS_Robot_Driver.bin
```

**정상 출력 예시:**

```
========================================
  STM32 Firmware Flash Utility
  Target: STM32F401RE (NUCLEO-F401RE)
========================================

[INFO]  Firmware : ROS_Robot_Driver.bin (22280 bytes)
[INFO]  Address  : 0x08000000

[STEP 1/2] ST-Link 장치 확인...
Found 1 stlink programmers
  ...serial: ...

[STEP 2/2] 펌웨어 업로드 중...
st-flash 1.x.x
2024-xx-xx ... INFO  ...
Flash written and verified! jolly good!

========================================
  [OK] 플래시 완료!
  STM32가 새 펌웨어로 부팅 중입니다.
========================================
```

---

### 방법 B — 수동 명령어

```bash
# ST-Link 연결 확인
st-info --probe

# 플래시
st-flash --reset write ROS_Robot_Driver.bin 0x08000000
```

---

## 7. 동작 확인

플래시 완료 후 STM 보드의 검은색 버튼을 눌러 리셋합니다. 


## 8. 펌웨어 버전 이력

| 버전 | 날짜 | 주요 변경사항 |
|------|------|--------------|
| 0.1.0 | 2026-02-22 | 모터 제어 시, PID 없이 PWM 값을 그대로 사용  |
| 0.2.0 | 2026-02-24 | 모터 PID 제어 코드 기본 구현 

---

*BSP 팀에서 새 펌웨어 배포 시 이 문서도 함께 업데이트됩니다.*
