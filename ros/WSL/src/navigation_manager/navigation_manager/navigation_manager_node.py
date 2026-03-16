#!/usr/bin/env python3
"""
navigation_manager_node.py
==========================
자율 주행 미션 컨트롤러.

Nav2 NavigateToPose action client를 wrapping하여
WebSocket bridge로 전달되는 UI 명령을 처리합니다.

구독:
  /nav/command  (std_msgs/String, JSON)
    {"cmd": "goto",    "x": 1.0, "y": 0.5, "yaw": 0.0}
    {"cmd": "goto_wp", "name": "wp1"}
    {"cmd": "patrol",  "route": "default"}
    {"cmd": "cancel"}

발행:
  /nav/status   (std_msgs/String, JSON, 1Hz)
    {"state": "NAVIGATING", "target": "wp1", "dist_remaining": 0.45,
     "patrol_active": true, "patrol_index": 0, "patrol_total": 3}
  /nav/feedback (std_msgs/String, JSON, Nav2 feedback 주기)
    {"dist_remaining": 0.45, "eta_sec": 3.2,
     "current_wp": 1, "total_wp": 3}
"""

import json
import math
import time
import yaml
import os
from enum import Enum, auto

import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor

from std_msgs.msg import String
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import OccupancyGrid
from nav2_msgs.action import NavigateToPose
from action_msgs.msg import GoalStatus
from tf2_ros import Buffer, TransformListener
import tf2_ros


class State(Enum):
    IDLE       = auto()
    NAVIGATING = auto()
    RECOVERING = auto()
    FAILED     = auto()


class NavigationManagerNode(Node):

    def __init__(self):
        super().__init__('navigation_manager')

        # ── Parameters ──────────────────────────────────────────────────────
        self.declare_parameter('waypoints_file', '')
        self.declare_parameter('max_retries', 2)
        self.declare_parameter('retry_delay_sec', 2.0)
        self.declare_parameter('linear_speed', 0.12)        # m/s (ETA 추정용)
        self.declare_parameter('replan_on_map_update', True)
        self.declare_parameter('replan_min_interval', 5.0)  # 최소 재계획 간격 (초)

        wp_file                  = self.get_parameter('waypoints_file').value
        self._max_retries        = self.get_parameter('max_retries').value
        self._retry_delay        = self.get_parameter('retry_delay_sec').value
        self._linear_speed       = self.get_parameter('linear_speed').value
        self._replan_on_map_upd  = self.get_parameter('replan_on_map_update').value
        self._replan_min_interval= self.get_parameter('replan_min_interval').value

        # ── Waypoint store ───────────────────────────────────────────────────
        self._waypoints: dict      = {}
        self._patrol_routes: dict  = {}
        if wp_file and os.path.isfile(wp_file):
            self._load_waypoints(wp_file)
        else:
            self.get_logger().warn(
                f'waypoints_file not found or empty: {wp_file!r}. '
                'Only coordinate-based goto commands will work.'
            )

        # ── State ────────────────────────────────────────────────────────────
        self._state                     = State.IDLE
        self._current_target_name: str | None = None
        self._current_pose: tuple | None = None   # (x, y, yaw)
        self._retry_count               = 0
        self._patrol_route: list[str]   = []
        self._patrol_index              = 0
        self._patrol_active             = False
        self._dist_remaining            = 0.0
        self._goal_handle               = None
        self._retry_timer               = None

        # ── Current robot pose (map→base_footprint TF, RViz robot model과 동일) ─
        self._robot_x:   float = 0.0
        self._robot_y:   float = 0.0
        self._robot_yaw: float = 0.0

        # TF2: map→base_footprint 조회용
        self._tf_buffer   = Buffer()
        self._tf_listener = TransformListener(self._tf_buffer, self)

        # ── Map replan ───────────────────────────────────────────────────────
        self._last_map_stamp            = None   # 마지막으로 수신한 맵 타임스탬프
        self._replan_pending            = False  # 재계획 대기 중
        self._last_replan_time          = 0.0    # time.monotonic() 기준

        # ── Action client ────────────────────────────────────────────────────
        cb_group = ReentrantCallbackGroup()
        self._nav_client = ActionClient(
            self,
            NavigateToPose,
            'navigate_to_pose',
            callback_group=cb_group,
        )

        # ── Pub / Sub ────────────────────────────────────────────────────────
        self._cmd_sub = self.create_subscription(
            String, '/nav/command', self._on_command, 10)

        # RViz2 2D Goal Pose → navigate_to
        self._goal_pose_sub = self.create_subscription(
            PoseStamped, '/goal_pose', self._on_goal_pose, 10,
            callback_group=cb_group)

        if self._replan_on_map_upd:
            self._map_sub = self.create_subscription(
                OccupancyGrid, '/map', self._on_map, 1,
                callback_group=cb_group)

        self._status_pub   = self.create_publisher(String, '/nav/status',   10)
        self._feedback_pub = self.create_publisher(String, '/nav/feedback', 10)

        # 1 Hz 상태 발행
        self.create_timer(1.0, self._publish_status)

        self.get_logger().info(
            f'NavigationManager ready. '
            f'Waypoints: {list(self._waypoints.keys())}, '
            f'Routes: {list(self._patrol_routes.keys())}'
        )

    # ── Waypoint Loading ─────────────────────────────────────────────────────

    def _load_waypoints(self, filepath: str) -> None:
        with open(filepath, 'r') as f:
            data = yaml.safe_load(f)
        self._waypoints      = data.get('waypoints', {})
        self._patrol_routes  = data.get('patrol_routes', {})
        self.get_logger().info(
            f'Loaded {len(self._waypoints)} waypoints, '
            f'{len(self._patrol_routes)} routes from {filepath}'
        )

    # ── Robot pose tracking (map→base_footprint TF) ─────────────────────────

    def _update_robot_pose(self) -> None:
        """map→base_footprint TF 조회 → RViz robot model 위치와 동일."""
        try:
            t = self._tf_buffer.lookup_transform(
                'map', 'base_footprint', rclpy.time.Time())
            self._robot_x = t.transform.translation.x
            self._robot_y = t.transform.translation.y
            q = t.transform.rotation
            self._robot_yaw = math.atan2(
                2.0 * (q.w * q.z + q.x * q.y),
                1.0 - 2.0 * (q.y * q.y + q.z * q.z),
            )
        except tf2_ros.LookupException:
            pass
        except tf2_ros.ExtrapolationException:
            pass

    # ── RViz2 Goal Pose ──────────────────────────────────────────────────────

    def _on_goal_pose(self, msg: PoseStamped) -> None:
        """RViz2 '2D Goal Pose' 툴 → navigation_manager를 통해 Nav2로 전달."""
        x = msg.pose.position.x
        y = msg.pose.position.y
        q = msg.pose.orientation
        yaw = math.atan2(
            2.0 * (q.w * q.z + q.x * q.y),
            1.0 - 2.0 * (q.y * q.y + q.z * q.z),
        )
        self._patrol_active = False
        self.get_logger().info(
            f'RViz2 goal_pose → ({x:.2f}, {y:.2f}, {math.degrees(yaw):.1f}°)')
        self._navigate_to(x, y, yaw, label=f'rviz({x:.2f},{y:.2f})')

    # ── Command Handler ──────────────────────────────────────────────────────

    def _on_command(self, msg: String) -> None:
        try:
            cmd = json.loads(msg.data)
        except json.JSONDecodeError as e:
            self.get_logger().error(f'Invalid JSON command: {e}')
            return

        action = cmd.get('cmd', '')
        self.get_logger().info(f'Command received: {cmd}')

        if action == 'cancel':
            self._cancel_navigation()

        elif action == 'goto':
            x   = float(cmd.get('x',   0.0))
            y   = float(cmd.get('y',   0.0))
            yaw = float(cmd.get('yaw', 0.0))
            self._patrol_active = False
            self._navigate_to(x, y, yaw, label=f'({x:.2f}, {y:.2f})')

        elif action == 'goto_wp':
            name = cmd.get('name', '')
            if name not in self._waypoints:
                self.get_logger().error(f'Unknown waypoint: "{name}"')
                return
            self._patrol_active = False
            self._navigate_to_waypoint(name)

        elif action == 'patrol':
            route_name = cmd.get('route', 'default')
            if route_name not in self._patrol_routes:
                self.get_logger().error(f'Unknown patrol route: "{route_name}"')
                return
            self._start_patrol(route_name)

        else:
            self.get_logger().warn(f'Unknown command: "{action}"')

    # ── Navigation ───────────────────────────────────────────────────────────

    def _navigate_to_waypoint(self, name: str) -> None:
        wp = self._waypoints[name]
        self._navigate_to(
            float(wp['x']),
            float(wp['y']),
            float(wp.get('yaw', 0.0)),
            label=name,
        )

    def _navigate_to(self, x: float, y: float, yaw: float, label: str) -> None:
        if not self._nav_client.wait_for_server(timeout_sec=3.0):
            self.get_logger().error('navigate_to_pose action server not available!')
            self._set_state(State.FAILED)
            return

        # 이전 재시도 타이머 정리
        if self._retry_timer is not None:
            self._retry_timer.cancel()
            self._retry_timer.destroy()
            self._retry_timer = None

        goal = NavigateToPose.Goal()
        goal.pose                         = PoseStamped()
        goal.pose.header.frame_id         = 'map'
        goal.pose.header.stamp            = self.get_clock().now().to_msg()
        goal.pose.pose.position.x         = x
        goal.pose.pose.position.y         = y
        goal.pose.pose.orientation.z      = math.sin(yaw / 2.0)
        goal.pose.pose.orientation.w      = math.cos(yaw / 2.0)

        self._current_target_name = label
        self._current_pose        = (x, y, yaw)
        self._set_state(State.NAVIGATING)

        send_future = self._nav_client.send_goal_async(
            goal,
            feedback_callback=self._on_feedback,
        )
        send_future.add_done_callback(self._on_goal_accepted)

        self.get_logger().info(
            f'→ Navigating to {label}  '
            f'(x={x:.2f}, y={y:.2f}, yaw={math.degrees(yaw):.1f}°)'
        )

    def _on_goal_accepted(self, future) -> None:
        self._goal_handle = future.result()
        if not self._goal_handle.accepted:
            self.get_logger().error('Goal rejected by Nav2!')
            self._set_state(State.FAILED)
            return
        result_future = self._goal_handle.get_result_async()
        result_future.add_done_callback(self._on_result)

    def _on_feedback(self, feedback_msg) -> None:
        dist = feedback_msg.feedback.distance_remaining
        self._dist_remaining = dist

        eta = dist / self._linear_speed if self._linear_speed > 0 else 0.0
        payload = json.dumps({
            'dist_remaining': round(dist, 3),
            'eta_sec':        round(eta,  1),
            'current_wp':     self._patrol_index + 1 if self._patrol_active else None,
            'total_wp':       len(self._patrol_route) if self._patrol_active else None,
        })
        self._feedback_pub.publish(String(data=payload))

    def _on_result(self, future) -> None:
        result = future.result()
        status = result.status

        if status == GoalStatus.STATUS_SUCCEEDED:
            self.get_logger().info(f'✓ Reached: {self._current_target_name}')
            self._retry_count    = 0
            self._dist_remaining = 0.0
            if self._patrol_active:
                self._advance_patrol()
            else:
                self._set_state(State.IDLE)

        elif status == GoalStatus.STATUS_CANCELED:
            if self._replan_pending:
                # 맵 업데이트로 인한 cancel → _on_replan_cancel_done 에서 처리
                return
            self.get_logger().info('Navigation canceled.')
            self._patrol_active = False
            self._set_state(State.IDLE)

        else:
            self.get_logger().warn(
                f'✗ Navigation failed (status={status})  '
                f'target={self._current_target_name}'
            )
            self._handle_failure()

    # ── Failure & Retry ──────────────────────────────────────────────────────

    def _handle_failure(self) -> None:
        if self._retry_count < self._max_retries:
            self._retry_count += 1
            self.get_logger().info(
                f'Retrying in {self._retry_delay:.0f}s  '
                f'({self._retry_count}/{self._max_retries}): {self._current_target_name}'
            )
            self._set_state(State.RECOVERING)
            self._retry_timer = self.create_timer(
                self._retry_delay, self._on_retry_timer)
        else:
            self.get_logger().error(
                f'Max retries exceeded for: {self._current_target_name}')
            self._patrol_active  = False
            self._retry_count    = 0
            self._set_state(State.FAILED)

    def _on_retry_timer(self) -> None:
        # one-shot: 타이머 즉시 해제
        self._retry_timer.cancel()
        self._retry_timer.destroy()
        self._retry_timer = None

        if self._state != State.RECOVERING:
            return

        if self._current_pose is not None:
            x, y, yaw = self._current_pose
            self._navigate_to(x, y, yaw, label=self._current_target_name or '')
        else:
            self._set_state(State.IDLE)

    # ── Map Replan ───────────────────────────────────────────────────────────

    def _on_map(self, msg: OccupancyGrid) -> None:
        """맵 업데이트 감지 → 주행 중이면 재계획."""
        stamp = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9

        # 첫 수신은 기준값만 저장
        if self._last_map_stamp is None:
            self._last_map_stamp = stamp
            return

        # 타임스탬프가 같거나 이전이면 무시
        if stamp <= self._last_map_stamp:
            return
        self._last_map_stamp = stamp

        # NAVIGATING 상태이고 재계획 대기 중이 아닐 때만
        if self._state != State.NAVIGATING or self._replan_pending:
            return

        # 최소 재계획 간격 체크
        now = time.monotonic()
        if now - self._last_replan_time < self._replan_min_interval:
            return

        self.get_logger().info(
            f'Map updated → replanning to {self._current_target_name}')
        self._replan_pending   = True
        self._last_replan_time = now

        if self._goal_handle is not None:
            cancel_future = self._goal_handle.cancel_goal_async()
            cancel_future.add_done_callback(self._on_replan_cancel_done)
        else:
            self._replan_pending = False

    def _on_replan_cancel_done(self, future) -> None:
        """재계획용 cancel 완료 → 동일 goal 재전송."""
        self._replan_pending = False
        if self._current_pose is None:
            return
        x, y, yaw = self._current_pose
        self._navigate_to(x, y, yaw, label=self._current_target_name or '')

    # ── Patrol ───────────────────────────────────────────────────────────────

    def _start_patrol(self, route_name: str) -> None:
        self._patrol_route  = list(self._patrol_routes[route_name])
        self._patrol_index  = 0
        self._patrol_active = True
        self._retry_count   = 0
        self.get_logger().info(
            f'Patrol started: "{route_name}" → {self._patrol_route}')
        self._navigate_to_waypoint(self._patrol_route[0])

    def _advance_patrol(self) -> None:
        self._patrol_index = (self._patrol_index + 1) % len(self._patrol_route)
        next_wp = self._patrol_route[self._patrol_index]
        self.get_logger().info(
            f'Patrol next: [{self._patrol_index + 1}/{len(self._patrol_route)}] {next_wp}')
        self._navigate_to_waypoint(next_wp)

    # ── Cancel ───────────────────────────────────────────────────────────────

    def _cancel_navigation(self) -> None:
        self._patrol_active = False
        if self._retry_timer is not None:
            self._retry_timer.cancel()
            self._retry_timer.destroy()
            self._retry_timer = None
        if self._goal_handle is not None:
            self.get_logger().info('Canceling current goal...')
            self._goal_handle.cancel_goal_async()
        else:
            self._set_state(State.IDLE)

    # ── Status ───────────────────────────────────────────────────────────────

    def _set_state(self, new_state: State) -> None:
        if self._state != new_state:
            self.get_logger().info(
                f'State: {self._state.name} → {new_state.name}')
        self._state = new_state
        self._publish_status()

    def _publish_status(self) -> None:
        self._update_robot_pose()
        goal_x   = self._current_pose[0] if self._current_pose else None
        goal_y   = self._current_pose[1] if self._current_pose else None
        goal_yaw = self._current_pose[2] if self._current_pose else None
        payload = json.dumps({
            'state':          self._state.name,
            'target':         self._current_target_name,
            'robot_x':        round(self._robot_x,   3),
            'robot_y':        round(self._robot_y,   3),
            'robot_yaw':      round(self._robot_yaw, 3),
            'goal_x':         goal_x,
            'goal_y':         goal_y,
            'goal_yaw':       goal_yaw,
            'dist_remaining': round(self._dist_remaining, 3),
            'patrol_active':  self._patrol_active,
            'patrol_index':   self._patrol_index if self._patrol_active else None,
            'patrol_total':   len(self._patrol_route) if self._patrol_active else None,
        })
        self._status_pub.publish(String(data=payload))


def main(args=None):
    rclpy.init(args=args)
    node = NavigationManagerNode()
    executor = MultiThreadedExecutor()
    executor.add_node(node)
    try:
        executor.spin()
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
