# 프로젝트 폴더 구조 및 관리 가이드

## 📁 권장 폴더 구조

```
ROS_CCTV_Robot/
│
├── README.md                          # 프로젝트 전체 설명
├── .gitignore                         # Git 제외 파일 목록
│
├── docs/                              # 📚 문서 모음
│   ├── meeting_notes/                 # 멘토링 회의록
│   │   └── 260202_1차_멘토링.pdf
│   ├── hardware/                      # 하드웨어 관련 문서
│   │   ├── 시스템_다이어그램.png
│   │   ├── 핀_할당표.xlsx
│   │   └── datasheets/                # 데이터시트 모음
│   │       ├── L298N_datasheet.pdf
│   │       ├── MPU6050_datasheet.pdf
│   │       └── encoder_motor_spec.docx
│   └── references/                    # 참고 자료
│       └── STM32_HAL_documentation.pdf
│
├── firmware/                          # 🔧 STM32 펌웨어 (당신 담당)
│   ├── README.md                      # 펌웨어 설명 및 빌드 방법
│   ├── stm32_project/                 # CubeMX 생성 프로젝트
│   │   ├── Core/
│   │   │   ├── Inc/
│   │   │   │   ├── main.h
│   │   │   │   └── motor_control.h
│   │   │   └── Src/
│   │   │       ├── main.c
│   │   │       └── motor_control.c
│   │   ├── Drivers/
│   │   └── *.ioc                      # CubeMX 프로젝트 파일
│   │
│   ├── libraries/                     # 공통 라이브러리
│   │   ├── motor_control/
│   │   │   ├── motor_control.h
│   │   │   └── motor_control.c
│   │   ├── encoder/                   # (엔코더 배송 후 추가)
│   │   │   ├── encoder.h
│   │   │   └── encoder.c
│   │   └── imu/                       # MPU6050 라이브러리
│   │       ├── mpu6050.h
│   │       └── mpu6050.c
│   │
│   ├── tests/                         # 테스트 코드
│   │   ├── motor_test.c
│   │   ├── encoder_test.c
│   │   └── communication_test.c
│   │
│   └── scripts/                       # 빌드/플래시 스크립트
│       ├── flash.sh
│       └── serial_monitor.py
│
├── ros_workspace/                     # 🤖 ROS 패키지 (엄도윤 담당)
│   ├── src/
│   │   ├── robot_control/
│   │   ├── camera_calibration/
│   │   └── navigation/
│   └── README.md
│
├── server/                            # 🌐 서버 코드 (이정근 담당)
│   ├── streaming/
│   ├── api/
│   └── README.md
│
├── client/                            # 💻 Qt UI (이한빈 담당)
│   ├── src/
│   ├── ui/
│   └── README.md
│
└── tools/                             # 🔨 개발 도구
    ├── serial_debug/                  # 시리얼 통신 디버깅 툴
    ├── calibration/                   # 캘리브레이션 스크립트
    └── performance_test/              # 성능 측정 도구
```

---

## 📥 지금 다운로드한 파일 정리 방법

### 1단계: 로컬에 프로젝트 폴더 생성

```bash
# 프로젝트 루트 폴더 생성
mkdir ~/ROS_CCTV_Robot
cd ~/ROS_CCTV_Robot

# 기본 폴더 구조 생성
mkdir -p docs/{meeting_notes,hardware/datasheets,references}
mkdir -p firmware/{libraries/motor_control,tests,scripts}
mkdir -p ros_workspace/src
mkdir -p server client tools
```

### 2단계: 다운로드한 파일 배치

```
다운로드 폴더의 파일들을 다음과 같이 이동:

Motor_Control_Complete_Guide.md
→ firmware/README.md (이름 변경 후 복사)

STM32_CubeMX_Setup_Guide.md
→ docs/hardware/STM32_CubeMX_Setup.md

motor_control.h
motor_control.c
→ firmware/libraries/motor_control/

main_test_example.c
→ firmware/tests/motor_test.c (이름 변경)
```

### 3단계: 기존 프로젝트 파일 정리

```
기존에 받은 문서들:

260202_1차_멘토링_회의록.pdf
→ docs/meeting_notes/

시스템_다이어그램.png
→ docs/hardware/

엔코더_모터_스펙.docx
MPU6050_datasheet.pdf
STM32F401_Reference_Manual.pdf
→ docs/hardware/datasheets/
```

---

## 🔄 Git 저장소 초기화 (필수!)

### Git 설정

```bash
cd ~/ROS_CCTV_Robot

# Git 초기화
git init

# .gitignore 생성 (아래 내용 참고)
# ... (다음 섹션에서)

# 초기 커밋
git add .
git commit -m "Initial commit: Project structure setup"
```

### .gitignore 파일 내용

```gitignore
# STM32 관련
*.o
*.elf
*.hex
*.bin
*.map
*.list
Debug/
Release/
.settings/
*.launch

# CubeMX 생성 파일 중 제외
firmware/stm32_project/Drivers/      # HAL 라이브러리 (용량 큼)
firmware/stm32_project/.mxproject

# IDE 관련
.vscode/
.idea/
*.swp
*.swo
*~

# OS 관련
.DS_Store
Thumbs.db
desktop.ini

# ROS 관련
ros_workspace/build/
ros_workspace/devel/
ros_workspace/install/
*.pyc
__pycache__/

# 빌드 산출물
*.a
*.so

# 임시 파일
*.tmp
*.log
```

---

## 📝 README.md 작성 (포트폴리오용)

### 프로젝트 루트 README.md

```markdown
# ROS 기반 CCTV 연동 자율주행 로봇

## 📖 프로젝트 개요
CCTV 영상을 활용한 실내 자율주행 로봇 시스템

## 👥 팀 구성
- **김지오**: STM32 펌웨어 개발 (모터 제어, 센서 통합)
- **엄도윤**: ROS 기반 자율주행 (SLAM, Navigation)
- **이정근**: RTSP 스트리밍 서버
- **이한빈**: Qt 기반 UI 및 영상 처리

## 🏗️ 시스템 아키텍처
[시스템 다이어그램 이미지]

## 📂 폴더 구조
- `firmware/`: STM32 펌웨어 (C/HAL)
- `ros_workspace/`: ROS 노드 (Python/C++)
- `server/`: RTSP 스트리밍 서버
- `client/`: Qt UI
- `docs/`: 문서 및 회의록

## 🚀 빠른 시작
각 폴더의 README.md 참조

## 📊 진행 상황
- [x] 프로젝트 구조 설계
- [x] 하드웨어 핀 할당
- [x] 모터 제어 라이브러리 구현
- [ ] 엔코더 통합
- [ ] ROS 통신 구현
- [ ] ...

## 🎓 멘토링
- 멘토: 한화비전 김명준 책임님
- 회의록: `docs/meeting_notes/`
```

### firmware/README.md (당신 담당 부분)

```markdown
# STM32 펌웨어 - 모터 제어 및 센서 통합

## 🎯 담당자
김지오

## 📌 구현 기능
- [x] L298N 모터 드라이버 제어
- [ ] 엔코더 기반 오도메트리
- [ ] MPU6050 IMU 통합
- [ ] 라즈베리파이 UART 통신
- [ ] ROS 메시지 퍼블리싱

## 🔧 하드웨어
- MCU: STM32F401
- 모터 드라이버: L298N
- 센서: MPU6050 (I2C), 엔코더 모터 4개

## 📋 핀 할당
| 기능 | STM32 핀 | 용도 |
|------|----------|------|
| 모터 PWM | PA0, PA1 | TIM2_CH1, CH2 |
| 모터 방향 | PC0-PC3 | GPIO Output |
| ... | ... | ... |

## 🚀 빌드 방법
```bash
# STM32CubeIDE에서
1. 프로젝트 열기: stm32_project/
2. Build Project (Ctrl+B)
3. Run/Debug (F11)
```

## 📖 사용 예제
```c
Motor_Init();
Motor_Forward(50);  // 50% 속도로 전진
HAL_Delay(2000);
Motor_Stop();
```

## 🧪 테스트
`tests/motor_test.c` 참조

## 📚 참고 문서
- `../docs/hardware/STM32_CubeMX_Setup.md`
- L298N 데이터시트
```

---

## 💾 작업 흐름 (Git Workflow)

### 일일 작업 루틴

```bash
# 1. 작업 시작 전 - 최신 코드 받기
git pull origin main

# 2. 기능별 브랜치 생성
git checkout -b feature/encoder-integration

# 3. 코드 작성 및 테스트
# ... (개발 작업)

# 4. 커밋 (의미있는 단위로 자주)
git add firmware/libraries/encoder/
git commit -m "feat: Add encoder library for odometry calculation

- Implement TIM3/TIM4 encoder mode
- Add pulse to distance conversion
- Test with 1920 PPR encoder"

# 5. 원격 저장소에 푸시
git push origin feature/encoder-integration

# 6. GitHub에서 Pull Request 생성
# (팀원 리뷰 후 main 브랜치에 병합)
```

### 커밋 메시지 규칙 (Conventional Commits)

```
feat: 새로운 기능 추가
fix: 버그 수정
docs: 문서 수정
test: 테스트 코드 추가
refactor: 코드 리팩토링
perf: 성능 개선
style: 코드 포맷팅

예시:
feat: Implement PWM motor control with L298N
fix: Correct motor direction inversion issue
docs: Add CubeMX setup guide
perf: Optimize encoder interrupt handler
```

---

## 🤝 협업 가이드

### 브랜치 전략

```
main (또는 master)
  ├── dev (개발 통합 브�ch)
  │   ├── feature/motor-control (김지오)
  │   ├── feature/encoder (김지오)
  │   ├── feature/ros-navigation (엄도윤)
  │   ├── feature/rtsp-server (이정근)
  │   └── feature/qt-ui (이한빈)
  └── release/v1.0 (릴리스 브랜치)
```

### 코드 리뷰 절차

1. 기능 개발 완료 후 Pull Request 생성
2. 팀원 1명 이상의 리뷰 & Approve
3. CI 테스트 통과 확인
4. dev 브랜치에 병합

---

## 📊 진행 상황 추적

### GitHub Projects 활용

**보드 구성:**
```
TODO → In Progress → Testing → Done

카드 예시:
- [feat] 모터 제어 라이브러리 구현 (김지오)
- [feat] 엔코더 통합 (김지오)
- [feat] ROS 오도메트리 노드 (엄도윤)
```

### Milestone 설정

```
Milestone 1: 기본 구동 검증 (2주)
- 모터 제어 구현
- 라즈베리파이 통신

Milestone 2: 센서 통합 (2주)
- 엔코더 오도메트리
- IMU 데이터 수집

Milestone 3: 성능 최적화 (2주)
- PID 제어
- 성능 측정
```

---

## 🎯 백업 전략

### 1. Git 원격 저장소 (필수)
- GitHub/GitLab 사용
- 매일 push

### 2. 클라우드 백업 (선택)
```
Google Drive / OneDrive
  └── ROS_CCTV_Robot_Backup/
      ├── weekly_backup_2026-02-04.zip
      └── datasheets/ (용량 큰 PDF)
```

### 3. 로컬 백업 (권장)
- 외장 HDD/SSD에 주간 백업
- CubeMX 프로젝트 전체 백업 (Drivers 포함)

---

## 📸 포트폴리오용 자료 수집

### 정리할 내용

```
portfolio/
├── screenshots/            # 동작 영상 캡처
├── demo_videos/           # 데모 영상
├── performance_data/      # 성능 측정 결과
│   ├── odometry_error.csv
│   └── speed_accuracy.xlsx
└── presentation/          # 발표 자료
    └── final_presentation.pptx
```

### 문서화할 내용
- 문제 상황과 해결 과정
- 성능 측정 결과 (Before/After)
- 코드 리뷰 내용
- 멘토 피드백 반영 내역

---

## ⚡ 요약: 지금 바로 할 일

1. **폴더 생성**
   ```bash
   mkdir -p ~/ROS_CCTV_Robot/firmware/libraries/motor_control
   ```

2. **파일 이동**
   - 다운로드한 5개 파일을 위 구조대로 배치

3. **Git 초기화**
   ```bash
   git init
   git add .
   git commit -m "Initial commit: Motor control library"
   ```

4. **GitHub 저장소 생성 & 푸시**
   ```bash
   git remote add origin https://github.com/your-team/ROS_CCTV_Robot.git
   git push -u origin main
   ```

5. **README 작성**
   - 프로젝트 개요
   - 자신의 담당 부분 설명

---

**작성:** 김지오  
**날짜:** 2026-02-04  
**버전:** 1.0
