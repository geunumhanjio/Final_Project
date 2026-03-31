#!/usr/bin/env python3
"""
ws_serial_test.py
=================
WebSocket 서버(ws://localhost:9090)에 클라이언트로 연결하여
방향키로 차량을 제어하는 테스트 스크립트.

조작 방법:
  ↑  : 전진
  ↓  : 후진
  ←  : 좌회전
  →  : 우회전
  스페이스 / s : 정지
  q  : 종료

의존성:
  pip install websockets
"""

import asyncio
import curses
import json
import time
import sys

try:
    import websockets
except ImportError:
    print("websockets 패키지가 필요합니다: pip install websockets")
    sys.exit(1)

# ── 설정 ─────────────────────────────────────────────────────────────────────
WS_URI        = "ws://172.20.26.8:9090"
LINEAR_STEP   = 0.3   # m/s  (전진/후진 속도)
ANGULAR_STEP  = 0.5    # rad/s (회전 속도)
SEND_RATE_HZ  = 10     # 초당 cmd_vel 전송 횟수
STOP_ON_IDLE  = True   # 키 입력이 없을 때 정지 명령 전송 여부


# ── 전역 속도 상태 ─────────────────────────────────────────────────────────
class VelState:
    linear_x: float = 0.0
    angular_z: float = 0.0
    quit: bool = False


state = VelState()


def build_cmd(linear_x: float, angular_z: float) -> str:
    return json.dumps({
        "type": "cmd_vel",
        "timestamp": time.time(),
        "data": {
            "linear_x": round(linear_x, 3),
            "angular_z": round(angular_z, 3),
        },
    })


# ── curses 키 입력 스레드 ──────────────────────────────────────────────────
def key_loop(stdscr):
    """curses 루프: 키 입력을 읽어 VelState를 갱신."""
    curses.curs_set(0)
    stdscr.nodelay(True)
    stdscr.timeout(50)  # 50ms 폴링

    stdscr.clear()
    stdscr.addstr(0, 0, "=== WebSocket 차량 제어 ===")
    stdscr.addstr(2, 0, "  ↑  : 전진")
    stdscr.addstr(3, 0, "  ↓  : 후진")
    stdscr.addstr(4, 0, "  ←  : 좌회전")
    stdscr.addstr(5, 0, "  →  : 우회전")
    stdscr.addstr(6, 0, "  SPACE / s : 정지")
    stdscr.addstr(7, 0, "  q  : 종료")
    stdscr.addstr(9, 0, "상태: 연결 중...")
    stdscr.refresh()

    while not state.quit:
        key = stdscr.getch()

        linear_x = 0.0
        angular_z = 0.0
        active = False

        if key == ord('q') or key == ord('Q'):
            state.quit = True
            state.linear_x = 0.0
            state.angular_z = 0.0
            break
        elif key == curses.KEY_UP:
            linear_x = LINEAR_STEP
            active = True
        elif key == curses.KEY_DOWN:
            linear_x = -LINEAR_STEP
            active = True
        elif key == curses.KEY_LEFT:
            angular_z = ANGULAR_STEP
            active = True
        elif key == curses.KEY_RIGHT:
            angular_z = -ANGULAR_STEP
            active = True
        elif key == ord(' ') or key == ord('s') or key == ord('S'):
            active = True  # 명시적 정지

        if active or STOP_ON_IDLE:
            state.linear_x = linear_x
            state.angular_z = angular_z

        # 상태 표시 업데이트
        try:
            arrow = ""
            if state.linear_x > 0:
                arrow = "↑ 전진"
            elif state.linear_x < 0:
                arrow = "↓ 후진"
            elif state.angular_z > 0:
                arrow = "← 좌회전"
            elif state.angular_z < 0:
                arrow = "→ 우회전"
            else:
                arrow = "■ 정지"

            stdscr.addstr(9, 0,
                f"상태: {arrow}  linear={state.linear_x:+.2f}  angular={state.angular_z:+.2f}  ")
            stdscr.refresh()
        except curses.error:
            pass


# ── asyncio WebSocket 루프 ─────────────────────────────────────────────────
async def ws_loop():
    interval = 1.0 / SEND_RATE_HZ
    reconnect_delay = 2.0

    while not state.quit:
        try:
            async with websockets.connect(WS_URI) as ws:
                print(f"[연결됨] {WS_URI}", flush=True)
                while not state.quit:
                    msg = build_cmd(state.linear_x, state.angular_z)
                    await ws.send(msg)
                    await asyncio.sleep(interval)

        except (websockets.exceptions.ConnectionClosed,
                ConnectionRefusedError,
                OSError) as e:
            if state.quit:
                break
            print(f"[연결 끊김] {e} — {reconnect_delay:.0f}초 후 재연결...", flush=True)
            await asyncio.sleep(reconnect_delay)

    # 종료 전 정지 명령 전송 시도
    try:
        async with websockets.connect(WS_URI) as ws:
            await ws.send(build_cmd(0.0, 0.0))
    except Exception:
        pass


# ── 메인 ──────────────────────────────────────────────────────────────────
def main():
    loop = asyncio.new_event_loop()

    # asyncio WebSocket 루프를 백그라운드 태스크로 실행
    async def runner():
        task = loop.create_task(ws_loop())
        # curses는 블로킹이므로 별도 스레드에서 실행
        await loop.run_in_executor(None, curses.wrapper, key_loop)
        state.quit = True
        await task

    try:
        loop.run_until_complete(runner())
    except KeyboardInterrupt:
        state.quit = True
    finally:
        loop.close()
        print("\n종료.")


if __name__ == "__main__":
    main()
