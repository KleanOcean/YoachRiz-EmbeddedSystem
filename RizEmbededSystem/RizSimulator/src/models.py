"""
RizSimulator Data Models
数据模型定义
"""

from dataclasses import dataclass, field
from typing import List, Tuple
import time

from constants import (
    LED_COUNT, INNER_RING_COUNT, OUTER_RING_COUNT,
    TERMINATE_MODE, STATE_DISCONNECTED,
    DEFAULT_BLINKBREAK, DEFAULT_TIMEDBREAK, DEFAULT_BUFFER,
    DEFAULT_BUZZER, DEFAULT_BUZZERTIME
)


@dataclass
class LEDState:
    """LED灯光状态"""
    inner_ring: List[Tuple[int, int, int]] = field(default_factory=lambda: [(0, 0, 0)] * INNER_RING_COUNT)
    outer_ring: List[Tuple[int, int, int]] = field(default_factory=lambda: [(0, 0, 0)] * OUTER_RING_COUNT)
    brightness: float = 1.0  # 0.0 - 1.0
    is_on: bool = False

    def set_all(self, color: Tuple[int, int, int]):
        """设置所有LED为相同颜色"""
        self.inner_ring = [color] * INNER_RING_COUNT
        self.outer_ring = [color] * OUTER_RING_COUNT

    def set_inner(self, color: Tuple[int, int, int]):
        """设置内圈颜色"""
        self.inner_ring = [color] * INNER_RING_COUNT

    def set_outer(self, color: Tuple[int, int, int]):
        """设置外圈颜色"""
        self.outer_ring = [color] * OUTER_RING_COUNT

    def clear(self):
        """清除所有LED"""
        self.inner_ring = [(0, 0, 0)] * INNER_RING_COUNT
        self.outer_ring = [(0, 0, 0)] * OUTER_RING_COUNT
        self.is_on = False


@dataclass
class TOFSensorState:
    """TOF传感器状态"""
    distance: int = 1000  # mm
    amplitude: int = 100
    baseline: int = 0
    baseline_history: List[int] = field(default_factory=list)
    consecutive_detections: int = 0
    last_detection_time: float = 0.0
    is_cooldown: bool = False
    cooldown_start: float = 0.0
    detection_active: bool = False

    def reset(self):
        """重置传感器状态"""
        self.consecutive_detections = 0
        self.is_cooldown = False
        self.detection_active = False

    def add_baseline_sample(self, amplitude: int):
        """添加基线样本"""
        self.baseline_history.append(amplitude)
        if len(self.baseline_history) > 30:  # BASELINE_HISTORY_SIZE
            self.baseline_history.pop(0)
        if self.baseline_history:
            self.baseline = sum(self.baseline_history) // len(self.baseline_history)


@dataclass
class DeviceConfig:
    """设备配置参数"""
    game_mode: int = TERMINATE_MODE
    prev_game_mode: int = -1
    blink_break: int = DEFAULT_BLINKBREAK
    timed_break: int = DEFAULT_TIMEDBREAK
    buffer_time: int = DEFAULT_BUFFER
    buzzer_enabled: bool = (DEFAULT_BUZZER == 1)
    buzzer_time: int = DEFAULT_BUZZERTIME
    double_mode_index: int = 0
    sensor_mode: int = 1  # 1=LiDAR, 2=MMWave, 3=Both
    config_blink_count: int = 1


@dataclass
class DeviceStats:
    """设备统计数据"""
    trigger_count: int = 0
    total_reaction_time: float = 0.0
    fastest_reaction: float = float('inf')
    average_reaction: float = 0.0
    last_trigger_time: float = 0.0

    def record_trigger(self):
        """记录触发"""
        current_time = time.time()
        if self.last_trigger_time > 0:
            reaction_time = (current_time - self.last_trigger_time) * 1000  # ms
            self.total_reaction_time += reaction_time
            self.fastest_reaction = min(self.fastest_reaction, reaction_time)

        self.trigger_count += 1
        self.last_trigger_time = current_time

        if self.trigger_count > 0:
            self.average_reaction = self.total_reaction_time / self.trigger_count

    def reset(self):
        """重置统计"""
        self.trigger_count = 0
        self.total_reaction_time = 0.0
        self.fastest_reaction = float('inf')
        self.average_reaction = 0.0
        self.last_trigger_time = 0.0


@dataclass
class RizDevice:
    """Riz设备模型"""
    device_id: int
    name: str = ""
    mac_address: str = ""
    firmware_version: str = "v1.0.0"

    # 状态
    connection_state: int = STATE_DISCONNECTED
    led_state: LEDState = field(default_factory=LEDState)
    tof_state: TOFSensorState = field(default_factory=TOFSensorState)
    config: DeviceConfig = field(default_factory=DeviceConfig)
    stats: DeviceStats = field(default_factory=DeviceStats)

    # 控制标志
    able_to_turn_on: bool = True
    buzzer_active: bool = False
    buzzer_start_time: float = 0.0

    # BLE相关
    ble_server: any = None  # BLEGATTServer实例（延迟导入避免循环）

    def __post_init__(self):
        """初始化后处理"""
        if not self.name:
            self.name = f"RIZ-{self.device_id:04d}"
        if not self.mac_address:
            self.mac_address = self._generate_mac()

    def _generate_mac(self) -> str:
        """生成MAC地址"""
        mac_bytes = [
            0xAA, 0xBB, 0xCC, 0xDD,
            (self.device_id >> 8) & 0xFF,
            self.device_id & 0xFF
        ]
        return ':'.join(f'{b:02X}' for b in mac_bytes)

    def to_dict(self) -> dict:
        """转换为字典"""
        return {
            'device_id': self.device_id,
            'name': self.name,
            'mac_address': self.mac_address,
            'firmware_version': self.firmware_version,
            'connection_state': self.connection_state,
            'game_mode': self.config.game_mode,
            'led_on': self.led_state.is_on,
            'tof_distance': self.tof_state.distance,
            'stats': {
                'trigger_count': self.stats.trigger_count,
                'average_reaction': self.stats.average_reaction,
                'fastest_reaction': self.stats.fastest_reaction if self.stats.fastest_reaction != float('inf') else 0
            }
        }
