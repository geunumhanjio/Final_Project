"""
pid_analyzer.py — STM32 PID 튜닝 데이터 실시간 분석기

STM32가 UART2(USB)로 전송하는 데이터를 수신하여
실시간 그래프와 성능 지표를 출력한다.

데이터 포맷: DATA,<tick_ms>,<target>,<actual_L>,<actual_R>

사용법:
    python3 pid_analyzer.py                  # 포트 자동 감지
    python3 pid_analyzer.py --port /dev/ttyACM0
    python3 pid_analyzer.py --port /dev/ttyACM0 --save
"""

import argparse
import csv
import sys
import time
from collections import deque
from datetime import datetime
from pathlib import Path

import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# ── 설정 ──────────────────────────────────────────────────────────────
BAUD_RATE    = 115200
WINDOW_SEC   = 10       # 그래프에 표시할 시간 범위 (초)
DATA_PREFIX  = "DATA,"  # STM32가 전송하는 데이터 라인 접두사


# ── 시리얼 포트 자동 감지 ───────────────────────────────────────────
def find_stm32_port() -> str:
    """STM32 VCP 포트를 자동으로 찾는다."""
    candidates = []
    for port in serial.tools.list_ports.comports():
        desc = (port.description or "").lower()
        if any(kw in desc for kw in ["stm", "nucleo", "stlink", "usb serial", "ttyacm", "usbmodem"]):
            candidates.append(port.device)

    if not candidates:
        # 일반 USB 시리얼 포트 후보
        for port in serial.tools.list_ports.comports():
            if "ttyACM" in port.device or "ttyUSB" in port.device or "COM" in port.device:
                candidates.append(port.device)

    if not candidates:
        print("[ERROR] STM32 포트를 찾을 수 없습니다.")
        print("연결된 포트 목록:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device}: {p.description}")
        sys.exit(1)

    print(f"[INFO] 포트 감지: {candidates[0]}")
    return candidates[0]


# ── 성능 지표 계산 ─────────────────────────────────────────────────
class PIDMetrics:
    def __init__(self, target: float):
        self.target      = target
        self.start_tick  = None
        self.rise_time   = None    # 목표의 90%에 처음 도달한 시간 (ms)
        self.overshoot   = 0.0     # 최대 오버슈트 (mm/s)
        self.steady_err  = 0.0     # 정상 상태 오차 (최근 평균)
        self._peak       = 0.0
        self._settled    = deque(maxlen=20)   # 최근 20개 샘플 (2초)

    def update(self, tick: float, actual: float):
        if self.start_tick is None:
            self.start_tick = tick

        elapsed = tick - self.start_tick

        # 상승 시간: 목표의 90% 최초 도달
        if self.rise_time is None and actual >= self.target * 0.9:
            self.rise_time = elapsed

        # 오버슈트
        if actual > self._peak:
            self._peak    = actual
            self.overshoot = max(0.0, self._peak - self.target)

        # 정상 상태 오차 (2초 이상 경과 후)
        if elapsed > 2000:
            self._settled.append(actual)
            if self._settled:
                self.steady_err = self.target - sum(self._settled) / len(self._settled)

    def print_summary(self):
        print("\n── PID 성능 지표 ──────────────────────")
        print(f"  목표 속도    : {self.target:.1f} mm/s")
        if self.rise_time is not None:
            print(f"  상승 시간    : {self.rise_time:.0f} ms  (목표의 90% 도달)")
        else:
            print(f"  상승 시간    : 미도달")
        print(f"  최대 오버슈트: {self.overshoot:.1f} mm/s  ({self.overshoot/self.target*100:.1f}%)")
        print(f"  정상상태 오차: {self.steady_err:.1f} mm/s")
        print("────────────────────────────────────────")


# ── 메인 ──────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="STM32 PID 실시간 분석기")
    parser.add_argument("--port", help="시리얼 포트 (미지정 시 자동 감지)")
    parser.add_argument("--baud", type=int, default=BAUD_RATE)
    parser.add_argument("--save", action="store_true", help="CSV로 저장")
    args = parser.parse_args()

    port = args.port or find_stm32_port()

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"[ERROR] 포트 열기 실패: {e}")
        sys.exit(1)

    print(f"[INFO] 연결됨: {port} @ {args.baud} baud")
    print("[INFO] 블루 버튼으로 모터를 켜면 데이터 수집 시작")
    print("[INFO] 창을 닫으면 종료\n")

    # CSV 저장 설정
    csv_file = csv_writer = None
    if args.save:
        fname = f"pid_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        csv_file   = open(fname, "w", newline="")
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(["tick_ms", "target", "actual_L", "actual_R"])
        print(f"[INFO] CSV 저장: {fname}")

    # 데이터 버퍼 (슬라이딩 윈도우)
    max_pts  = WINDOW_SEC * 20   # 50ms 주기 → 초당 20개
    ticks    = deque(maxlen=max_pts)
    actuals_L = deque(maxlen=max_pts)
    actuals_R = deque(maxlen=max_pts)
    targets   = deque(maxlen=max_pts)

    metrics_L = metrics_R = None
    t0 = None

    # ── 그래프 설정 ────────────────────────────────────────────────
    fig, (ax_l, ax_r) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)
    fig.suptitle("STM32 PID 속도 제어 — 실시간 모니터", fontsize=13)

    line_tgt_l, = ax_l.plot([], [], "k--", linewidth=1.2, label="Target")
    line_act_l, = ax_l.plot([], [], "b-",  linewidth=1.5, label="Actual L")
    ax_l.set_ylabel("속도 (mm/s)")
    ax_l.set_title("Left Motor")
    ax_l.legend(loc="upper right")
    ax_l.grid(True, alpha=0.3)
    ax_l.set_ylim(-50, 650)

    line_tgt_r, = ax_r.plot([], [], "k--", linewidth=1.2, label="Target")
    line_act_r, = ax_r.plot([], [], "r-",  linewidth=1.5, label="Actual R")
    ax_r.set_xlabel("시간 (s)")
    ax_r.set_ylabel("속도 (mm/s)")
    ax_r.set_title("Right Motor")
    ax_r.legend(loc="upper right")
    ax_r.grid(True, alpha=0.3)
    ax_r.set_ylim(-50, 650)

    plt.tight_layout()

    # ── 애니메이션 업데이트 ────────────────────────────────────────
    def update(_frame):
        nonlocal t0, metrics_L, metrics_R

        # 버퍼에 쌓인 라인 전부 처리
        while ser.in_waiting:
            try:
                raw = ser.readline().decode("utf-8", errors="ignore").strip()
            except Exception:
                continue

            if not raw.startswith(DATA_PREFIX):
                if raw:
                    print(f"[STM32] {raw}")
                continue

            parts = raw[len(DATA_PREFIX):].split(",")
            if len(parts) < 4:
                continue

            try:
                tick     = float(parts[0])
                target   = float(parts[1])
                actual_L = float(parts[2])
                actual_R = float(parts[3])
            except ValueError:
                continue

            if t0 is None:
                t0 = tick
                metrics_L = PIDMetrics(target)
                metrics_R = PIDMetrics(target)
                print(f"[INFO] 수집 시작 (target={target:.0f} mm/s)")

            t_sec = (tick - t0) / 1000.0
            ticks.append(t_sec)
            targets.append(target)
            actuals_L.append(actual_L)
            actuals_R.append(actual_R)

            metrics_L.update(tick - t0, actual_L)
            metrics_R.update(tick - t0, actual_R)

            if csv_writer:
                csv_writer.writerow([tick, target, actual_L, actual_R])

        if not ticks:
            return line_tgt_l, line_act_l, line_tgt_r, line_act_r

        t_list = list(ticks)
        line_tgt_l.set_data(t_list, list(targets))
        line_act_l.set_data(t_list, list(actuals_L))
        line_tgt_r.set_data(t_list, list(targets))
        line_act_r.set_data(t_list, list(actuals_R))

        # x축 슬라이딩 윈도우
        x_max = max(t_list)
        x_min = max(0.0, x_max - WINDOW_SEC)
        ax_l.set_xlim(x_min, x_max + 0.5)
        ax_r.set_xlim(x_min, x_max + 0.5)

        return line_tgt_l, line_act_l, line_tgt_r, line_act_r

    ani = animation.FuncAnimation(fig, update, interval=100, blit=False, cache_frame_data=False)

    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        if metrics_L:
            metrics_L.print_summary()
        if metrics_R:
            metrics_R.print_summary()
        ser.close()
        if csv_file:
            csv_file.close()
            print(f"[INFO] CSV 저장 완료")


if __name__ == "__main__":
    main()
