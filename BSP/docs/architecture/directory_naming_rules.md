# Directory & Naming Conventions
(프로젝트 폴더 구조 및 파일 네이밍 규칙)

본 문서는 프로젝트의 일관성을 유지하기 위해 디렉터리 구성 방식과 파일 이름 부여 규칙을 규정합니다.

---

## 1. 하드웨어 문서 (`docs/hardware/`) 규칙

이 폴더에는 센서, 모터, 보드 등 하드웨어 부품의 사양(Spec)과 매뉴얼, 진리표 등 '변경되지 않는 물리적 스펙'을 보관합니다. 소프트웨어 연산이나 수학 공식은 `notes/`에 보관하여 물리적 스펙과 논리적 구현을 분리합니다.

### 1-1. 폴더 구조 (역할별 10단위)
확장성을 고려해 10단위로 역할을 묶습니다. 새로운 카테고리가 생기면 중간 번호(예: `15_display`)를 삽입합니다.
```text
docs/hardware/
├── 10_mcu      (메인 프로세서 / 보드 스펙)
├── 20_sensor   (자세를 읽어들이는 센서부)
├── 30_driver   (모터를 구동시키는 IC, 기판)
├── 40_actuator (실제로 움직이는 모터 동력부)
└── 50_power    (시스템 전력 공급원)
```

### 1-2. 파일 규칙 (5-Part Naming)
**`[카테고리]_[용도]_[기판명/칩고유명]_[모듈형태]_[문서종류].확장자`**

- **카테고리**: mcu, sensor, driver, actuator, power (폴더와 매핑)
- **용도**: core, imu, motor, battery 등 프로젝트 내에서의 쓸모
- **기판명/칩명**: stm32f401re, fit0450, l298n 등 고유 식별부품명
- **모듈형태**: 
  - `module` (여러 칩이 납땜되어 조립된 PCB 보드)
  - `chip` (검은색 단일 IC)
  - `device` (보조배터리 등 외함이 덮인 완제품)
  - `pack` (보호회로가 결합된 부품군)
- **문서종류**: `datasheet` (공식 스펙), `quickref` (실무 개발에 필요한 엑기스 요약본), `spec` (물리 한계표)

> **예시:** `actuator_motor_fit0450_module_spec.md` (모터 액추에이터용, DFRobot FIT0450 기어모터가 결합된 모듈 모음의 기계 스펙 문서)

> 예시: RM0090_STM32F405_RefManual.pdf

### 문서 종류 
| **분류** | **약어** | **설명** |
| --- | --- | --- |
| 회로도 | SCH | Schematic. 회로 설계 도면. 펌웨어/BSP 개발 시 MCU 핀 맵, 풀업/풀다운 저항, 인터럽트 라인 연결 상태를 확인할 때 가장 많이 참조한다. |
| 부품표 | BOM | Bill of Materials. 실장되는 부품 목록. I2C 주소나 특정 부품의 정확한 파트 넘버를 확인할 때 사용한다. |
| 거버 파일 | GER | Gerber. 실제 PCB 제조를 위한 CAM 데이터. |
| 배치도 | ASM | Assembly drawing. 실물 PCB 위 부품 실장 위치도. 보드 디버깅 시 오실로스코프나 멀티미터로 프로빙할 테스트 포인트(TP)를 찾을 때 유용하다. |
| 기구도면 | MECH | Mechanical. 외관 및 기구 설계 도면. 모터나 라이다 같은 외부 장치와의 물리적 체결 구조를 파악할 때 본다. |
| 데이터시트 | DS | Datasheet. 특정 부품(센서, 모터 드라이버 등)의 스펙 및 레지스터 맵 








Ran command
~/…/BSP $ git commit -m "refactor: 파일 의미 전달 향상을 위한 소스코드 리팩토링 및 네이밍 변경" -m "- \`drv_\`, \`algo_\`, \`app_\` 등의 접두사를 사용하여 각 소스코드의 역할(드라이버, 알고리즘, 어플리케이션 등)을 명확하게 드러내도록 파일명 분리 및 변경" -m "- \`docs/architecture/directory_naming_rules.md\` 구조에 맞추어 \`Components/\` 하위 폴더(Comms, Motor, Sensor)로 분류 및 이동" -m "- 코드 분석 가이드라인 문서(\`docs/guides/code_analysis_guideline.md\`) 추가 및 불필요한 기존 마크다운 파일 제거"
Exit code 0
2
Executing git commit





Analyzed
Generating문서. |
| 블록도 | BLK | Block Diagram. 시스템 전체 전원 트리나 주요 칩 간의 인터페이스(UART, SPI 등) 구조도. |
| **문서 약어** | **풀네임** | **설명** |
| --- | --- | --- |
| DS | Datasheet | 칩의 핀 맵, 패키지 물리적 규격, 전압 및 전류 등 전기적 특성(Electrical characteristics)을 명시한다. |
| RM | Reference Manual | 하드웨어 레지스터 맵, 메모리 맵, 각 주변장치(Peripheral)의 상세한 하드웨어 동작 원리를 설명한다. 펌웨어 제어 시 가장 많이 본다. |
| PM | Programming Manual | Cortex-M 코어 아키텍처, 어셈블리어 명령어 셋, 시스템 타이머(SysTick) 등 코어 레벨의 정보를 제공한다. |
| UM | User Manual | ST가 제공하는 개발 보드(평가 보드)의 하드웨어 회로도, 핀 헤더 배치, 점퍼 설정 방법 등을 설명한다. |
| AN | Application Note | 특정 센서 인터페이스, 모터 제어, 부트로더 작성 등 특정 주제에 대한 하드웨어 및 소프트웨어 설계 가이드라인을 제공한다. |
| ES | Errata Sheet | 칩 설계상 발생한 하드웨어 버그(Silicon Limitation)와 이를 소프트웨어적으로 회피하는 방법(Workaround)을 명시한다. |

---

## 2. 도구 및 스크립트 (`tools/`) 규칙

이 폴더에는 보드를 제어하기 위해 외부 기기(라즈베리파이 등)에서 활용하는 파이썬 스크립트나, 완성되어 배포 대기 중인 펌웨어 바이너리 파일들을 통합 관리합니다.

### 2-1. 폴더 구조
```text
tools/
├── 10_rpi_scripts       (라즈베리파이 환경에서 구동하는 테스트/모니터링 코드)
└── 20_firmware_releases (버전별로 릴리즈된 펌웨어 결과물 백업)
```

### 2-2. 파일 규칙 (3-Part Naming)
**`[플랫폼]_[타입]_[핵심동작_버전].확장자`**

- **플랫폼 (실행 환경):** `rpi` (라즈베리파이), `stm32` (MCU 플래싱), `pc` (윈도우 유틸리티)
- **타입 (종류):** `script` (.py, .sh 소스코드), `fw` (.bin, .hex 컴파일 완료본), `guide` (설명서)
- **핵심동작/버전:** `motor_test`, `serial_monitor`, `FinalProject_BaseCtrl_260301_v1` 등 식별자

> **예시:** 
> - `rpi_script_serial_monitor.py` (RPi에서 시리얼 통신을 관찰하는 파이썬 코드)
> - `stm32_fw_FinalProject_BaseCtrl_260301_v1.bin` (STM32에 구워 넣을 26년 3월 1일 자 베이스컨트롤러 펌웨어 v1)

---

## 3. C/C++ 소스코드 아키텍처 (`Core/`, `Components/`) 규칙

STM32CubeMX가 생성한 자동화 코드와 사용자가 직접 작성한 어플리케이션 코드가 섞여 손실되는 것을 방지하기 위해 **컴포넌트 중심 아키텍처(Component-based Architecture)** 를 채택합니다.

### 3-1. 폴더 구조
```text
BSP/
├── Core/       (접근 금지 구역: STM32CubeMX 자동생성 전용 코드 - main.c, gpio.c 등)
├── Drivers/    (ST 배포용 기초 HAL 라이브러리들)
└── Components/ (사용자 전용 구역: 도메인 기능별로 분리된 어플리케이션 코드)
    ├── Motor/  (모터 제어, 엔코더 펄스 리딩, PID 제어 등)
    ├── Sensor/ (IMU 등 센서 통신 및 파싱)
    └── Comms/  (UART/CAN 등을 통한 외부 통신 및 프로토콜 로직)
```

### 3-2. 소스코드 접두사 규칙 
해당 파일이 하드웨어를 직접 제어하는지, 아니면 순수 수학/논리 부분인지 식별하기 위해 파일명 앞에 다음 접두사를 의무적으로 붙입니다.

- **`drv_` (Driver)**: 센서 통신(I2C/SPI), 모터 핀 제어(PWM) 등 실제 **하드웨어를 직접 건드리는** 최하단 코드 (예: `drv_motor.c`, `drv_mpu6050.c`, `drv_encoder.c`)
- **`algo_` (Algorithm)**: PID 공식, 역운동학 계산 등 하드웨어 핀 번호와 무관한 **순수 수학 연산** 코드 (예: `algo_pid.c`)
- **`app_` (Application)**: 하단 Driver와 알고리즘을 가져다 결합하여 **상위 로봇 동작(명령 해석 등)을 결정**하는 뇌 역할의 코드 (예: `app_packet_parser.c`)
