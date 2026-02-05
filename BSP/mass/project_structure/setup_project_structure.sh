#!/bin/bash

# ROS CCTV Robot - 프로젝트 폴더 구조 생성 스크립트
# 작성: 김지오
# 날짜: 2026-02-04

echo "========================================"
echo "ROS CCTV Robot 프로젝트 폴더 생성"
echo "========================================"

# 프로젝트 루트 디렉토리 (필요시 수정)
PROJECT_ROOT="$HOME/ROS_CCTV_Robot"

# 프로젝트 루트 생성
mkdir -p "$PROJECT_ROOT"
cd "$PROJECT_ROOT"

echo "✓ 프로젝트 루트 생성: $PROJECT_ROOT"

# 문서 폴더
mkdir -p docs/meeting_notes
mkdir -p docs/hardware/datasheets
mkdir -p docs/references
echo "✓ docs/ 폴더 생성"

# 펌웨어 폴더 (김지오 담당)
mkdir -p firmware/libraries/motor_control
mkdir -p firmware/libraries/encoder
mkdir -p firmware/libraries/imu
mkdir -p firmware/tests
mkdir -p firmware/scripts
mkdir -p firmware/stm32_project
echo "✓ firmware/ 폴더 생성"

# ROS 워크스페이스 (엄도윤 담당)
mkdir -p ros_workspace/src
echo "✓ ros_workspace/ 폴더 생성"

# 서버 (이정근 담당)
mkdir -p server/streaming
mkdir -p server/api
echo "✓ server/ 폴더 생성"

# 클라이언트 (이한빈 담당)
mkdir -p client/src
mkdir -p client/ui
mkdir -p client/resources
echo "✓ client/ 폴더 생성"

# 개발 도구
mkdir -p tools/serial_debug
mkdir -p tools/calibration
mkdir -p tools/performance_test
echo "✓ tools/ 폴더 생성"

# 포트폴리오 자료
mkdir -p portfolio/screenshots
mkdir -p portfolio/demo_videos
mkdir -p portfolio/performance_data
mkdir -p portfolio/presentation
echo "✓ portfolio/ 폴더 생성"

# .gitignore 파일 생성
cat > .gitignore << 'EOF'
# STM32 / Embedded
*.o
*.elf
*.hex
*.bin
*.map
*.list
Debug/
Release/
.settings/

# IDE
.vscode/
.idea/
*.swp

# OS
.DS_Store
Thumbs.db

# ROS
ros_workspace/build/
ros_workspace/devel/
*.pyc
__pycache__/

# Build
*.a
*.so

# Logs
*.log
*.tmp
EOF
echo "✓ .gitignore 생성"

# README.md 템플릿 생성
cat > README.md << 'EOF'
# ROS 기반 CCTV 연동 자율주행 로봇

## 📖 프로젝트 개요
CCTV 영상을 활용한 실내 자율주행 로봇 시스템

## 👥 팀 구성
- **김지오**: STM32 펌웨어 개발
- **엄도윤**: ROS 기반 자율주행
- **이정근**: RTSP 스트리밍 서버
- **이한빈**: Qt 기반 UI

## 📂 폴더 구조
```
├── firmware/       # STM32 펌웨어
├── ros_workspace/  # ROS 노드
├── server/         # 스트리밍 서버
├── client/         # Qt UI
└── docs/          # 문서
```

## 🚀 빠른 시작
각 폴더의 README.md 참조

## 📊 진행 상황
- [x] 프로젝트 구조 설계
- [x] 모터 제어 라이브러리
- [ ] 엔코더 통합
- [ ] ROS 통신

EOF
echo "✓ README.md 템플릿 생성"

# firmware README 생성
cat > firmware/README.md << 'EOF'
# STM32 펌웨어

## 담당자
김지오

## 구현 기능
- [x] L298N 모터 제어
- [ ] 엔코더 오도메트리
- [ ] MPU6050 IMU
- [ ] UART 통신

## 빌드 방법
```bash
# STM32CubeIDE에서
1. stm32_project/ 프로젝트 열기
2. Build (Ctrl+B)
3. Run/Debug (F11)
```

## 사용 예제
```c
Motor_Init();
Motor_Forward(50);
```

EOF
echo "✓ firmware/README.md 생성"

# Git 초기화
git init
git add .
git commit -m "Initial commit: Project structure setup"
echo "✓ Git 초기화 완료"

echo ""
echo "========================================"
echo "✅ 프로젝트 폴더 생성 완료!"
echo "========================================"
echo ""
echo "📍 프로젝트 위치: $PROJECT_ROOT"
echo ""
echo "다음 단계:"
echo "1. cd $PROJECT_ROOT"
echo "2. 다운로드한 파일들을 적절한 폴더로 이동"
echo "3. GitHub 저장소 연결:"
echo "   git remote add origin <your-repo-url>"
echo "   git push -u origin main"
echo ""
