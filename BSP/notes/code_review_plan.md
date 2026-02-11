# 코드 파악 계획

## 학습 순서: 쉬운 것 → 어려운 것

```
1. 엔코더 (26줄) → 2. MPU6050 (72줄) → 3. UART 통신 (223줄)
```

---

## 1. 엔코더 (encoder.c / encoder.h)

### 파일
- `Core/Inc/encoder.h` (15줄)
- `Core/Src/encoder.c` (26줄)
- 관련: `Core/Src/tim.c` → `MX_TIM3_Init()`, `MX_TIM4_Init()`

### 핵심 개념
- TIM3(왼쪽 바퀴), TIM4(오른쪽 바퀴)가 하드웨어 엔코더 모드로 동작
- CPU 개입 없이 타이머가 자동으로 펄스를 카운팅
- `__HAL_TIM_GET_COUNTER()`로 현재 카운트 값만 읽으면 끝

### 파악 순서
- [ ] `encoder.h` 읽기 — 외부 인터페이스 (3개 함수, enum)
- [ ] `main.c`에서 호출 흐름 확인 — `Encoder_Init()` → `Encoder_GetCount()`
- [ ] `encoder.c` 구현 읽기
- [ ] `tim.c`의 `MX_TIM3_Init()` 엔코더 모드 설정 확인

### 중점 질문
- [ ] 카운터가 16비트(0~65535)인데 오버플로우 처리가 있는가?
- [ ] 오른쪽 엔코더에 `-` 부호가 붙은 이유는? (물리적 장착 방향)
- [ ] 카운트 값 → 실제 거리(mm) 변환이 구현되어 있는가?
- [ ] 엔코더 필터값(IC1Filter=10)의 의미는?

---

## 2. MPU6050 IMU (mpu6050.c / mpu6050.h)

### 파일
- `Core/Inc/mpu6050.h` (28줄)
- `Core/Src/mpu6050.c` (72줄)
- 관련: `Core/Src/i2c.c` → `MX_I2C1_Init()`
- 참고: `docs/mpu6050/RM-MPU-6000A.pdf` (데이터시트)

### 핵심 개념
- I2C 통신으로 레지스터 읽기/쓰기
- Init: WHO_AM_I 확인 → 슬립 해제 → 샘플레이트/감도 설정
- ReadAll: 14바이트 연속 읽기 → 가속도 3축 + 자이로 3축 파싱

### 파악 순서
- [ ] `mpu6050.h` 읽기 — 레지스터 맵, 구조체, 함수 선언
- [ ] `main.c`에서 호출 흐름 확인 — `MPU6050_Init()` → `MPU6050_ReadAll()`
- [ ] `mpu6050.c` 구현 읽기 — `WriteReg`/`ReadReg` 헬퍼, Init 순서, ReadAll 파싱
- [ ] 데이터시트에서 레지스터 값의 의미 확인

### 중점 질문
- [ ] raw 값 → 실제 물리량(g, °/s) 변환이 구현되어 있는가?
- [ ] 감도 설정 (가속도 ±2g, 자이로 ±250°/s)이 용도에 적절한가?
- [ ] Polling 방식인데 메인루프 성능에 영향이 있는가?
- [ ] DLPF(디지털 저역통과 필터) 비활성화 상태가 적절한가?
- [ ] 온도 데이터(buf[6..7])를 버리고 있는데, 필요한가?

---

## 3. UART 통신 (uart_protocol.c / uart_protocol.h)

### 파일
- `Core/Inc/uart_protocol.h` (99줄)
- `Core/Src/uart_protocol.c` (223줄)
- 관련: `Core/Src/stm32f4xx_it.c` → `USART1_IRQHandler()`
- 관련: `Core/Src/usart.c` → `MX_USART1_UART_Init()`

### 핵심 개념

#### 수신 흐름 (RPi → STM32)
```
하드웨어 인터럽트(USART1_IRQHandler)
  → HAL_UART_IRQHandler()
  → HAL_UART_RxCpltCallback()
  → Protocol_FeedByte()        ← ISR에서 상태머신 파싱
  → ready_packet에 복사         ← packet_ready = true
  → 메인루프 Protocol_Process() ← 꺼내서 Dispatch → 모터 제어
```

#### 송신 흐름 (STM32 → RPi)
```
메인루프 50ms 주기
  → Protocol_SendOdom()  ← 엔코더 카운트 전송
  → Protocol_SendIMU()   ← IMU 6축 데이터 전송
  → Protocol_SendPacket() → HAL_UART_Transmit() (블로킹)
```

#### 패킷 구조
```
[0xFF][0xFE][CMD][LEN][DATA 0~12바이트][CHECKSUM]
  헤더1 헤더2  명령  길이     데이터        XOR검증
```

### 파악 순서
- [ ] `uart_protocol.h` 읽기 — 명령 코드, 상태머신 enum, 패킷 구조체
- [ ] `stm32f4xx_it.c` 읽기 — 인터럽트 진입점
- [ ] `uart_protocol.c` 수신 파트 — `Protocol_Init()` → `FeedByte()` → `Process()`
- [ ] `uart_protocol.c` 송신 파트 — `SendPacket()` → `SendOdom()` / `SendIMU()`
- [ ] `main.c` 메인루프에서 전체 흐름 추적

### 중점 질문
- [ ] 상태머신 6단계 흐름을 이해했는가?
- [ ] ISR과 메인루프 사이 `volatile`, `packet_ready` 더블버퍼링의 역할은?
- [ ] 패킷이 빠르게 연속 수신되면 새 패킷이 버려지는데, 괜찮은가?
- [ ] 송신이 블로킹(HAL_UART_Transmit)인데, 성능 병목이 되지 않는가?
- [ ] 체크섬 검증만으로 통신 신뢰성이 충분한가?
- [ ] 에러 발생 시 복구 메커니즘이 있는가?

---

## 각 모듈 파악 방법 (공통)

1. **헤더 파일 먼저** — 외부 인터페이스(함수, 구조체, 상수) 파악
2. **main.c 호출 순서** — Init → 메인루프에서 어떻게 사용되는지
3. **구현 파일** — 내부 동작 이해
4. **데이터 흐름 추적** — 센서 → 변수 → 패킷 → RPi까지

## 파악 후 개선 포인트 (예상)

- 엔코더 오버플로우 처리 + 거리 변환
- MPU6050 물리량 변환 + 필터링
- UART 수신 버퍼 큐 도입 (패킷 드롭 방지)
- 송신 DMA 전환 (블로킹 제거)
- 모터 데드존 보정
