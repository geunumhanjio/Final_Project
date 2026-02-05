# 🚀 빠른 시작 체크리스트

## ✅ 초기 설정 (최초 1회)

### 1. 프로젝트 폴더 생성
```bash
# 방법 1: 자동 스크립트 사용 (권장)
bash setup_project_structure.sh

# 방법 2: 수동 생성
mkdir -p ~/ROS_CCTV_Robot/firmware/libraries/motor_control
mkdir -p ~/ROS_CCTV_Robot/docs/hardware
# ... (나머지 폴더)
```

**완료 확인:**
- [ ] 폴더 구조 생성 완료
- [ ] .gitignore 파일 생성
- [ ] README.md 파일 생성

---

### 2. 다운로드 파일 정리

**이동할 파일들:**
```
✓ Motor_Control_Complete_Guide.md
  → firmware/README.md

✓ STM32_CubeMX_Setup_Guide.md
  → docs/hardware/

✓ motor_control.h, motor_control.c
  → firmware/libraries/motor_control/

✓ main_test_example.c
  → firmware/tests/motor_test.c

✓ 260202_1차_멘토링_회의록.pdf
  → docs/meeting_notes/

✓ 시스템_다이어그램.png
  → docs/hardware/

✓ 엔코더_모터_스펙.docx
  → docs/hardware/datasheets/
```

**완료 확인:**
- [ ] 모든 파일이 적절한 위치에 배치됨

---

### 3. Git 저장소 설정

```bash
cd ~/ROS_CCTV_Robot

# Git 초기화 (스크립트로 이미 했다면 생략)
git init

# GitHub 저장소 생성 후
git remote add origin https://github.com/your-team/ROS_CCTV_Robot.git

# 첫 푸시
git add .
git commit -m "Initial commit: Motor control library and docs"
git push -u origin main
```

**완료 확인:**
- [ ] GitHub 저장소 생성
- [ ] 원격 저장소 연결
- [ ] 첫 커밋 & 푸시 완료

---

### 4. STM32 프로젝트 생성

```bash
cd ~/ROS_CCTV_Robot/firmware/stm32_project
```

**STM32CubeMX 작업:**
- [ ] `docs/hardware/STM32_CubeMX_Setup_Guide.md` 참조
- [ ] 핀 설정 (TIM2, GPIO, USART2 등)
- [ ] 클럭 설정 (84 MHz)
- [ ] 코드 생성
- [ ] `motor_control.h/c` 파일 추가
- [ ] `main.c` 수정 (테스트 코드 추가)

**완료 확인:**
- [ ] CubeMX 프로젝트 생성
- [ ] 라이브러리 통합
- [ ] 빌드 성공

---

## 📝 일일 작업 루틴

### 작업 시작

```bash
# 1. 최신 코드 받기
cd ~/ROS_CCTV_Robot
git pull origin main

# 2. 브랜치 생성 (새 기능 작업 시)
git checkout -b feature/encoder-integration

# 3. 작업 시작!
# STM32CubeIDE 열기 또는 코드 편집
```

**체크:**
- [ ] 최신 코드 동기화
- [ ] 브랜치 생성 (feature/xxx)

---

### 작업 중

**코딩 규칙:**
- [ ] 의미있는 함수/변수명 사용
- [ ] 주석 작성 (특히 어려운 부분)
- [ ] 테스트 코드 작성

**커밋 주기:**
- 작은 단위로 자주 커밋 (30분~1시간 단위)
- 한 커밋 = 하나의 기능/버그 수정

---

### 작업 종료

```bash
# 1. 변경사항 확인
git status
git diff

# 2. 스테이징
git add firmware/libraries/encoder/

# 3. 커밋
git commit -m "feat: Add encoder pulse counting

- Implement TIM3/TIM4 encoder mode
- Calculate distance from pulse count
- Test with 1920 PPR motor"

# 4. 푸시
git push origin feature/encoder-integration

# 5. GitHub에서 Pull Request 생성 (필요시)
```

**체크:**
- [ ] 변경사항 커밋
- [ ] 원격 저장소 푸시
- [ ] (주요 기능) Pull Request 생성

---

## 🎯 주간 목표 체크리스트

### Week 1: 모터 제어 (✅ 완료!)
- [x] 하드웨어 연결
- [x] CubeMX 설정
- [x] 모터 제어 라이브러리 작성
- [x] 기본 동작 테스트
- [ ] 라즈베리파이 UART 통신 

### Week 2: 엔코더 통합
- [ ] 엔코더 수신 확인
- [ ] TIM3/TIM4 Encoder Mode 설정
- [ ] 펄스 카운트 → 거리 변환
- [ ] 오도메트리 계산 구현
- [ ] ROS /odom 토픽 퍼블리시

### Week 3: IMU 통합
- [ ] MPU6050 I2C 통신 구현
- [ ] 가속도/각속도 데이터 수집
- [ ] 상보필터/칼만필터 적용
- [ ] ROS /imu 토픽 퍼블리시
- [ ] 엔코더+IMU 센서 퓨전

### Week 4: 성능 최적화
- [ ] PID 속도 제어
- [ ] 직진성 개선 (양쪽 바퀴 동기화)
- [ ] 성능 측정 (오차율 계산)
- [ ] 문서화 (Before/After 수치)

---

## 📊 포트폴리오 준비 체크리스트

### 문서화
- [ ] 각 기능별 README 작성
- [ ] 문제 해결 과정 기록 (Issue/PR)
- [ ] 성능 측정 결과 정리 (표/그래프)
- [ ] 멘토 피드백 반영 내역

### 코드 품질
- [ ] 일관된 코딩 스타일
- [ ] 주석 및 Doxygen 주석
- [ ] 테스트 코드 작성
- [ ] 에러 처리

### Git 히스토리
- [ ] 명확한 커밋 메시지
- [ ] 브랜치 전략 준수
- [ ] PR 리뷰 참여
- [ ] Issue 트래킹

### 시연 자료
- [ ] 동작 영상 촬영
- [ ] 스크린샷 수집
- [ ] 발표 자료 (PPT)
- [ ] 데모 시나리오 작성

---

## 🔥 자주 사용하는 명령어 모음

### Git 명령어
```bash
# 상태 확인
git status

# 변경사항 확인
git diff

# 커밋 히스토리
git log --oneline --graph

# 브랜치 목록
git branch -a

# 브랜치 전환
git checkout <branch-name>

# 변경사항 임시 저장
git stash
git stash pop

# 특정 파일만 커밋
git add <file>
git commit -m "message"
```

### STM32 개발
```bash
# 시리얼 모니터 (Linux)
screen /dev/ttyUSB0 115200

# 또는 minicom
minicom -D /dev/ttyUSB0 -b 115200

# 빌드 (명령줄, 선택사항)
make -C <project-dir>
```

### 프로젝트 탐색
```bash
# 폴더 구조 보기
tree -L 2

# 파일 찾기
find . -name "*.h"

# 코드 검색
grep -r "Motor_Init" firmware/
```

---

## 📞 도움 요청 시

### 팀원에게 질문하기 전 체크
- [ ] 에러 메시지 복사
- [ ] 관련 코드 스니펫 준비
- [ ] 시도한 해결 방법 정리
- [ ] 환경 정보 (STM32 모델, IDE 버전 등)

### GitHub Issue 템플릿
```markdown
## 문제 설명
[간단한 설명]

## 재현 단계
1. ...
2. ...

## 예상 결과
[원하는 동작]

## 실제 결과
[현재 발생하는 문제]

## 환경
- STM32: F401
- IDE: STM32CubeIDE 1.x.x
- 빌드 설정: Debug

## 추가 정보
[스크린샷, 로그 등]
```

---

## 🎓 학습 자료 링크

- [ ] STM32 HAL Documentation
- [ ] L298N Datasheet
- [ ] MPU6050 Register Map
- [ ] ROS Navigation Tutorials
- [ ] Git Workflow Guide

---

**마지막 업데이트:** 2026-02-04
**작성자:** 김지오
