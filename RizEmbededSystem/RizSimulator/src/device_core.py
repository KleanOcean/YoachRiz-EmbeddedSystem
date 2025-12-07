"""
RizSimulator Device Core
设备核心逻辑，基于ESP32固件
"""

import time
import random
from typing import Optional, Callable

from models import RizDevice, LEDState, TOFSensorState
from constants import *
from logger import get_logger

logger = get_logger("DeviceCore")


class DeviceController:
    """设备控制器 - 处理设备逻辑"""

    def __init__(self, device: RizDevice):
        self.device = device
        self.light_change_callback: Optional[Callable] = None
        self.state_change_callback: Optional[Callable] = None
        self.notification_callback: Optional[Callable] = None

        # Animation state
        self.animation_running = False
        self.animation_start_time = 0
        self.animation_type = None

    def turn_light_on(self, color: tuple, dual_led: bool = False):
        """点亮灯光"""
        if dual_led:
            # 双LED模式：内圈外圈颜色不同
            r, g, b = color
            self.device.led_state.set_inner(color)
            self.device.led_state.set_outer((b, r, g))  # 颜色轮换
        else:
            self.device.led_state.set_all(color)

        self.device.led_state.is_on = True

        # 启动蜂鸣器
        if self.device.config.buzzer_enabled:
            self.device.buzzer_active = True
            self.device.buzzer_start_time = time.time()

        logger.info(f"[{self.device.name}] 点亮灯光 RGB{color}, 模式: {self.device.config.game_mode}, 双LED: {dual_led}")

        if self.light_change_callback:
            self.light_change_callback(self.device.led_state)

    def turn_light_off(self):
        """关闭灯光"""
        self.device.led_state.clear()
        self.device.buzzer_active = False
        self.device.able_to_turn_on = False
        self.animation_running = False

        logger.info(f"[{self.device.name}] 关闭灯光")

        if self.light_change_callback:
            self.light_change_callback(self.device.led_state)

    def handle_game_mode(self, mode: int):
        """处理游戏模式"""
        logger.info(f"[{self.device.name}] 处理游戏模式: {mode}")

        if mode == MANUAL_MODE:
            self._handle_manual_mode()
        elif mode == RANDOM_MODE:
            self._handle_random_mode()
        elif mode == RHYTHM_MODE:
            self._handle_rhythm_mode()
        elif mode == DOUBLE_MODE:
            self._handle_double_mode()
        elif mode == OPENING_MODE:
            self._handle_opening_mode()
        elif mode == CLOSING_MODE:
            self._handle_closing_mode()
        elif mode == TERMINATE_MODE:
            self._handle_terminate_mode()
        elif mode == CONFIG_MODE:
            self._handle_config_mode()
        elif mode == RESTTIMESUP_MODE:
            self._handle_rest_mode()

    def _handle_manual_mode(self):
        """手动模式 - 根据process值显示不同蓝色"""
        if self.device.able_to_turn_on and not self.device.led_state.is_on:
            process = self.device.config.process

            # 根据process选择颜色（来自固件的manualWipe）
            if process > 50:
                color = COLOR_PALE_BLUE  # 淡蓝色
            elif process > 25:
                color = COLOR_SKY_BLUE  # 天蓝色
            else:
                color = COLOR_DEEP_BLUE  # 深蓝色

            self.turn_light_on(color)
            self.device.tof_state.detection_active = True
            self.device.able_to_turn_on = False
            self.device.config.prev_game_mode = MANUAL_MODE

    def _handle_random_mode(self):
        """随机模式 - 从绿/黄/红中随机选择"""
        if self.device.able_to_turn_on and not self.device.led_state.is_on:
            # 固件中的随机模式：Green, Yellow, Red
            colors = [
                (0, 255, 0),      # Green
                (255, 255, 0),    # Yellow
                (255, 0, 0),      # Red
            ]
            random_color = random.choice(colors)

            self.turn_light_on(random_color)
            self.device.tof_state.detection_active = True
            self.device.able_to_turn_on = False
            self.device.config.prev_game_mode = RANDOM_MODE

            logger.info(f"[{self.device.name}] 随机模式选择颜色: RGB{random_color}")

    def _handle_rhythm_mode(self):
        """节奏模式 - 使用自定义RGB颜色"""
        if self.device.able_to_turn_on and not self.device.led_state.is_on:
            # 使用配置中的RGB值
            color = (
                self.device.config.red_value,
                self.device.config.green_value,
                self.device.config.blue_value
            )

            self.turn_light_on(color)

            # 根据sensor_mode决定是否启动TOF
            if self.device.config.sensor_mode in [1, 3]:  # 1=LiDAR, 3=Both
                self.device.tof_state.detection_active = True

            self.device.able_to_turn_on = False
            self.device.config.prev_game_mode = RHYTHM_MODE

            logger.info(f"[{self.device.name}] 节奏模式 RGB{color}, 传感器模式: {self.device.config.sensor_mode}")

    def _handle_double_mode(self):
        """双击模式 - Orange或Deep Blue"""
        if self.device.able_to_turn_on and not self.device.led_state.is_on:
            # 根据double_mode_index选择颜色
            if self.device.config.double_mode_index == 0:
                color = COLOR_ORANGE
            else:
                color = COLOR_DEEP_BLUE

            self.turn_light_on(color, dual_led=False)  # 固件使用dual_led=false
            self.device.able_to_turn_on = False
            self.device.config.prev_game_mode = DOUBLE_MODE

            # 发送通知
            if self.notification_callback:
                msg = f"double{self.device.config.double_mode_index}"
                self.notification_callback(msg)

    def _handle_opening_mode(self):
        """开启模式"""
        # 采集TOF基线
        if self.device.tof_state.amplitude > 0:
            self.device.tof_state.add_baseline_sample(self.device.tof_state.amplitude)

        self.turn_light_on(COLOR_WHITE)
        self.device.able_to_turn_on = False
        logger.info(f"[{self.device.name}] 开启模式 - 基线: {self.device.tof_state.baseline}")

    def _handle_closing_mode(self):
        """关闭模式"""
        self.turn_light_on(COLOR_WHITE)

    def _handle_terminate_mode(self):
        """终止模式"""
        self.turn_light_off()
        self.device.tof_state.reset()
        logger.info(f"[{self.device.name}] 终止模式")

    def _handle_config_mode(self):
        """配置模式 - 显示白光并闪烁"""
        count = self.device.config.config_blink_count
        logger.info(f"[{self.device.name}] 配置模式 - 显示数字: {count}")

        # 点亮白光
        self.turn_light_on(COLOR_WHITE)
        self.device.able_to_turn_on = False

        # 启动配置动画
        self.animation_running = True
        self.animation_type = "config"
        self.animation_start_time = time.time()

        if self.notification_callback:
            self.notification_callback(f"config:{count}")

    def _handle_rest_mode(self):
        """休息模式 - Tennis绿色倒计时"""
        self.turn_light_on(COLOR_TENNIS)
        self.device.able_to_turn_on = False

        # 启动休息动画（倒计时熄灭）
        self.animation_running = True
        self.animation_type = "rest"
        self.animation_start_time = time.time()

        logger.info(f"[{self.device.name}] 休息模式开始")

    def start_init_animation(self):
        """启动初始化动画（开机动画）"""
        self.animation_running = True
        self.animation_type = "init"
        self.animation_start_time = time.time()
        logger.info(f"[{self.device.name}] 启动初始化动画")

    def start_connected_animation(self):
        """启动连接成功动画"""
        self.animation_running = True
        self.animation_type = "connected"
        self.animation_start_time = time.time()
        logger.info(f"[{self.device.name}] 启动连接动画")

    def update(self, delta_time: float):
        """更新设备状态（每帧调用）"""
        # 更新蜂鸣器
        if self.device.buzzer_active:
            elapsed = (time.time() - self.device.buzzer_start_time) * 1000
            if elapsed >= self.device.config.buzzer_time:
                self.device.buzzer_active = False

        # 更新TOF冷却
        if self.device.tof_state.is_cooldown:
            elapsed = (time.time() - self.device.tof_state.cooldown_start) * 1000
            if elapsed >= COOLDOWN_DURATION:
                self.device.tof_state.is_cooldown = False
                self.device.tof_state.consecutive_detections = 0

        # 更新动画
        if self.animation_running:
            self._update_animation()

    def _update_animation(self):
        """更新动画状态"""
        elapsed = time.time() - self.animation_start_time

        if self.animation_type == "init":
            # 初始化动画 - 显示绿色主题随机渐变
            # 简化版：显示Brat Green 1秒后结束
            if elapsed < 1.0:
                # 显示渐变绿色
                progress = int(elapsed * 100) % 6
                colors = [
                    (138, 207, 0),   # Brat Green
                    (155, 225, 0),   # Yellow-Green
                    (195, 242, 0),   # Neon Yellow-Green
                    (115, 170, 0),   # Dark Brat Green
                    (125, 185, 0),   # Olive Green
                    (120, 227, 0),   # Vibrant Green
                ]
                self.device.led_state.set_all(colors[progress])
                if self.light_change_callback:
                    self.light_change_callback(self.device.led_state)
            else:
                self.animation_running = False
                self.turn_light_off()

        elif self.animation_type == "connected":
            # 连接动画 - Tennis绿色快速点亮
            if elapsed < 0.5:
                self.device.led_state.set_all(COLOR_TENNIS)
                if self.light_change_callback:
                    self.light_change_callback(self.device.led_state)
            else:
                self.animation_running = False
                self.turn_light_off()

        elif self.animation_type == "config":
            # 配置动画 - 白光闪烁
            blink_count = self.device.config.config_blink_count
            blink_duration = 0.4  # 每次闪烁400ms
            total_duration = blink_count * blink_duration * 2  # 亮+暗

            if elapsed < total_duration:
                cycle = int(elapsed / blink_duration)
                if cycle % 2 == 0:
                    # 亮
                    self.device.led_state.set_all(COLOR_WHITE)
                else:
                    # 暗
                    self.device.led_state.clear()

                if self.light_change_callback:
                    self.light_change_callback(self.device.led_state)
            else:
                self.animation_running = False
                self.turn_light_off()

        elif self.animation_type == "rest":
            # 休息模式动画 - 倒计时熄灭（简化版：3秒后关闭）
            if elapsed < 3.0:
                # 逐渐变暗
                brightness = 1.0 - (elapsed / 3.0)
                self.device.led_state.brightness = max(0.0, brightness)
                if self.light_change_callback:
                    self.light_change_callback(self.device.led_state)
            else:
                self.animation_running = False
                self.turn_light_off()


class TOFSensorController:
    """TOF传感器控制器"""

    def __init__(self, device: RizDevice):
        self.device = device
        self.detection_callback: Optional[Callable] = None

    def update_distance(self, distance: int):
        """更新距离值"""
        self.device.tof_state.distance = distance

        # 根据距离计算振幅（模拟真实传感器）
        if distance < 300:
            # 近距离 - 高振幅
            base_amplitude = 5000 + random.randint(0, 1000)
        else:
            # 远距离 - 低振幅
            base_amplitude = 100 + random.randint(0, 200)

        self.device.tof_state.amplitude = base_amplitude

    def check_detection(self) -> bool:
        """检查是否检测到物体"""
        if not self.device.tof_state.detection_active:
            return False

        # 冷却期
        if self.device.tof_state.is_cooldown:
            return False

        # 检测逻辑
        threshold = int(self.device.tof_state.baseline * AMPLITUDE_THRESHOLD_FACTOR)
        if threshold == 0:
            threshold = AMPLITUDE_THRESHOLD

        if self.device.tof_state.amplitude > threshold:
            self.device.tof_state.consecutive_detections += 1

            if self.device.tof_state.consecutive_detections >= CONSECUTIVE_READINGS:
                # 检测成功
                self._trigger_detection()
                return True
        else:
            self.device.tof_state.consecutive_detections = 0

        return False

    def _trigger_detection(self):
        """触发检测"""
        logger.info(f"[{self.device.name}] TOF检测到物体! 距离: {self.device.tof_state.distance}mm, 振幅: {self.device.tof_state.amplitude}")

        # 进入冷却期
        self.device.tof_state.is_cooldown = True
        self.device.tof_state.cooldown_start = time.time()
        self.device.tof_state.consecutive_detections = 0
        self.device.tof_state.detection_active = False

        # 记录统计
        self.device.stats.record_trigger()

        # 回调
        if self.detection_callback:
            self.detection_callback()

    def simulate_touch(self):
        """模拟手部触碰（强制触发）- 直接触发，不依赖check_detection"""
        if not self.device.tof_state.detection_active:
            logger.warning(f"[{self.device.name}] TOF检测未激活，无法模拟触碰")
            return

        if self.device.tof_state.is_cooldown:
            logger.warning(f"[{self.device.name}] TOF处于冷却期，无法模拟触碰")
            return

        # 直接触发检测（不修改distance/amplitude，避免影响update_all中的check_detection）
        logger.info(f"[{self.device.name}] 模拟触碰触发")
        self._trigger_detection()
