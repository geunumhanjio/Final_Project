# 프로젝트 폴더 구조 한눈에 보기

```
ROS_CCTV_Robot/                          ← 프로젝트 루트
│
├── 📄 README.md                          ← 프로젝트 전체 개요
├── 📄 .gitignore                         ← Git 제외 파일
│
├── 📁 docs/                             ← 📚 모든 문서
│   ├── 📁 meeting_notes/                ← 멘토링 회의록
│   │   └── 260202_1차_멘토링.pdf
│   │
│   ├── 📁 hardware/                     ← 하드웨어 문서
│   │   ├── 시스템_다이어그램.png
│   │   ├── 핀_할당표.xlsx
│   │   ├── STM32_CubeMX_Setup.md
│   │   │
│   │   └── 📁 datasheets/               ← 데이터시트 모음
│   │       ├── L298N_datasheet.pdf
│   │       ├── MPU6050_register_map.pdf
│   │       ├── encoder_motor_spec.docx
│   │       └── STM32F401_reference.pdf
│   │
│   └── 📁 references/                   ← 참고 자료
│       └── ...
│
├── 📁 firmware/                         ← 🔧 STM32 펌웨어 (김지오)
│   ├── 📄 README.md                     ← 펌웨어 사용 설명서
│   │
│   ├── 📁 stm32_project/                ← CubeMX 프로젝트 (실제 개발)
│   │   ├── Core/
│   │   │   ├── Inc/
│   │   │   │   ├── main.h
│   │   │   │   ├── gpio.h
│   │   │   │   ├── tim.h
│   │   │   │   └── motor_control.h      ← 여기 추가!
│   │   │   │
│   │   │   └── Src/
│   │   │       ├── main.c
│   │   │       ├── gpio.c
│   │   │       ├── tim.c
│   │   │       └── motor_control.c      ← 여기 추가!
│   │   │
│   │   ├── Drivers/                     ← HAL 라이브러리
│   │   │   ├── CMSIS/
│   │   │   └── STM32F4xx_HAL_Driver/
│   │   │
│   │   ├── *.ioc                        ← CubeMX 설정 파일
│   │   └── Makefile
│   │
│   ├── 📁 libraries/                    ← 공통 라이브러리
│   │   ├── 📁 motor_control/            ← ✅ 완성!
│   │   │   ├── motor_control.h
│   │   │   └── motor_control.c
│   │   │
│   │   ├── 📁 encoder/                  ← 다음 작업
│   │   │   ├── encoder.h
│   │   │   └── encoder.c
│   │   │
│   │   └── 📁 imu/                      ← MPU6050
│   │       ├── mpu6050.h
│   │       └── mpu6050.c
│   │
│   ├── 📁 tests/                        ← 테스트 코드
│   │   ├── motor_test.c                 ← ✅ 완성!
│   │   ├── encoder_test.c
│   │   ├── imu_test.c
│   │   └── communication_test.c
│   │
│   └── 📁 scripts/                      ← 유틸리티 스크립트
│       ├── flash.sh                     ← 펌웨어 업로드
│       ├── serial_monitor.py            ← 시리얼 모니터
│       └── build.sh                     ← 빌드 자동화
│
├── 📁 ros_workspace/                    ← 🤖 ROS 패키지 (엄도윤)
│   ├── 📄 README.md
│   ├── 📁 src/
│   │   ├── robot_control/               ← 모터 제어 노드
│   │   │   ├── scripts/
│   │   │   │   └── cmd_vel_node.py
│   │   │   └── launch/
│   │   │
│   │   ├── camera_calibration/          ← PTZ 캘리브레이션
│   │   │
│   │   └── navigation/                  ← 네비게이션
│   │       ├── config/
│   │       └── launch/
│   │
│   ├── build/                           ← (Git 제외)
│   └── devel/                           ← (Git 제외)
│
├── 📁 server/                           ← 🌐 RTSP 서버 (이정근)
│   ├── 📄 README.md
│   ├── 📁 streaming/
│   │   ├── rtsp_server.py
│   │   └── gstreamer_pipeline.py
│   │
│   └── 📁 api/
│       └── rest_api.py
│
├── 📁 client/                           ← 💻 Qt UI (이한빈)
│   ├── 📄 README.md
│   ├── 📁 src/
│   │   ├── mainwindow.cpp
│   │   └── video_viewer.cpp
│   │
│   ├── 📁 ui/
│   │   └── mainwindow.ui
│   │
│   └── 📁 resources/
│       ├── icons/
│       └── images/
│
├── 📁 tools/                            ← 🔨 개발 도구
│   ├── 📁 serial_debug/                 ← 시리얼 디버깅 툴
│   │   └── uart_monitor.py
│   │
│   ├── 📁 calibration/                  ← 캘리브레이션
│   │   └── motor_calibration.py
│   │
│   └── 📁 performance_test/             ← 성능 측정
│       ├── odometry_test.py
│       └── speed_accuracy_test.py
│
└── 📁 portfolio/                        ← 📸 포트폴리오 자료
    ├── 📁 screenshots/                  ← 스크린샷
    ├── 📁 demo_videos/                  ← 데모 영상
    ├── 📁 performance_data/             ← 성능 측정 결과
    │   ├── odometry_error.csv
    │   └── speed_accuracy.xlsx
    └── 📁 presentation/                 ← 발표 자료
        └── final_presentation.pptx
```

---

## 📍 다운로드 파일 배치 위치

### ✅ 지금 받은 파일들

| 파일 | 배치 위치 |
|------|----------|
| `Motor_Control_Complete_Guide.md` | `firmware/README.md` |
| `STM32_CubeMX_Setup_Guide.md` | `docs/hardware/` |
| `motor_control.h` | `firmware/libraries/motor_control/` |
| `motor_control.c` | `firmware/libraries/motor_control/` |
| `main_test_example.c` | `firmware/tests/motor_test.c` |
| `Project_Management_Guide.md` | `docs/` (참고용) |
| `Quick_Start_Checklist.md` | 프로젝트 루트 (참고용) |
| `gitignore_template` | `.gitignore` (이름 변경) |
| `setup_project_structure.sh` | 프로젝트 루트 (실행 후 삭제 가능) |

### 📦 기존 프로젝트 파일들

| 파일 | 배치 위치 |
|------|----------|
| `260202_1차_멘토링_회의록.pdf` | `docs/meeting_notes/` |
| `시스템_다이어그램.png` | `docs/hardware/` |
| `엔코더_모터_스펙.docx` | `docs/hardware/datasheets/` |
| `MPU6050_register_map.pdf` | `docs/hardware/datasheets/` |
| `STM32F401_reference.pdf` | `docs/hardware/datasheets/` |

---

## 🎯 STM32 프로젝트에 라이브러리 추가하기

### 방법 1: 파일 직접 추가 (권장)

```
1. CubeMX로 프로젝트 생성
   → firmware/stm32_project/ 폴더에 생성

2. 라이브러리 파일 복사
   FROM: firmware/libraries/motor_control/motor_control.{h,c}
   TO:   firmware/stm32_project/Core/Inc/motor_control.h
         firmware/stm32_project/Core/Src/motor_control.c

3. STM32CubeIDE에서 프로젝트 Refresh (F5)

4. main.c 수정
   - #include "motor_control.h" 추가
   - Motor_Init() 호출
```

### 방법 2: 심볼릭 링크 (Linux/Mac)

```bash
cd firmware/stm32_project/Core/Inc/
ln -s ../../../libraries/motor_control/motor_control.h

cd firmware/stm32_project/Core/Src/
ln -s ../../../libraries/motor_control/motor_control.c
```

---

## 🔄 작업 흐름

```
1. 기능 개발
   └─ firmware/libraries/에서 라이브러리 작성
   └─ firmware/tests/에서 단위 테스트

2. 통합
   └─ firmware/stm32_project/에 통합
   └─ 전체 시스템 테스트

3. 문서화
   └─ README.md 업데이트
   └─ 성능 데이터 → portfolio/performance_data/

4. Git 커밋
   └─ git add → commit → push

5. 포트폴리오 정리
   └─ 스크린샷, 영상, 발표 자료
```

---

## 💡 팁

### Git에서 특정 폴더만 클론

```bash
# 자신의 담당 부분만 받기
git clone --depth 1 --filter=blob:none --sparse <repo-url>
cd ROS_CCTV_Robot
git sparse-checkout set firmware docs
```

### 용량 큰 파일 관리

```
docs/hardware/datasheets/  ← PDF 파일들 (Git LFS 사용 고려)
portfolio/demo_videos/     ← 영상 파일 (Git LFS 또는 별도 저장)

Git LFS 설정:
git lfs install
git lfs track "*.pdf"
git lfs track "*.mp4"
```

### IDE별 프로젝트 파일

```
STM32CubeIDE:
- .project, .cproject 등은 Git에 포함
- .settings/ 폴더는 개인 설정이므로 .gitignore

VSCode:
- .vscode/c_cpp_properties.json은 Git에 포함
- .vscode/settings.json은 개인 설정이므로 제외
```

---

**작성:** 김지오  
**날짜:** 2026-02-04
