"""
RizSimulator Device Manager
设备管理器 - 管理多个设备实例
"""

from typing import Dict, List, Optional
from models import RizDevice
from device_core import DeviceController, TOFSensorController
from ble.ble_server import BLEGATTServer, BLEMessageParser
from constants import (
    MAX_DEVICES, STATE_CONNECTED, STATE_ADVERTISING, STATE_DISCONNECTED,
    CHARACTERISTIC_MSG_UUID
)
from logger import get_logger

logger = get_logger("DeviceManager")


class DeviceManager:
    """设备管理器"""

    def __init__(self):
        self.devices: Dict[int, RizDevice] = {}
        self.controllers: Dict[int, DeviceController] = {}
        self.tof_controllers: Dict[int, TOFSensorController] = {}
        self.ble_servers: Dict[int, BLEGATTServer] = {}
        self.next_id = 1

    def create_device(self) -> RizDevice:
        """创建新设备"""
        if len(self.devices) >= MAX_DEVICES:
            raise ValueError(f"最多支持 {MAX_DEVICES} 个设备")

        device_id = self.next_id
        device = RizDevice(device_id=device_id)
        self.devices[device_id] = device

        # 创建控制器
        self.controllers[device_id] = DeviceController(device)
        self.tof_controllers[device_id] = TOFSensorController(device)

        # 创建BLE服务器
        ble_server = BLEGATTServer(device_id, device.name)
        ble_server.on_connect_callback = lambda: self._on_device_connect(device_id)
        ble_server.on_disconnect_callback = lambda: self._on_device_disconnect(device_id)
        ble_server.on_message_callback = lambda msg: self._on_device_message(device_id, msg)
        self.ble_servers[device_id] = ble_server
        device.ble_server = ble_server

        # 默认开始广播
        device.connection_state = STATE_ADVERTISING
        ble_server.is_advertising = True

        self.next_id += 1
        logger.info(f"创建设备: {device.name} (当前设备数: {len(self.devices)})")

        return device

    def remove_device(self, device_id: int) -> bool:
        """移除设备"""
        if device_id not in self.devices:
            logger.warning(f"设备ID {device_id} 不存在")
            return False

        device = self.devices[device_id]

        # 停止BLE服务器
        if device_id in self.ble_servers:
            self.ble_servers[device_id].stop_advertising()
            del self.ble_servers[device_id]

        del self.devices[device_id]
        del self.controllers[device_id]
        del self.tof_controllers[device_id]

        logger.info(f"移除设备: {device.name} (剩余设备数: {len(self.devices)})")
        return True

    def get_device(self, device_id: int) -> Optional[RizDevice]:
        """获取设备"""
        return self.devices.get(device_id)

    def get_controller(self, device_id: int) -> Optional[DeviceController]:
        """获取设备控制器"""
        return self.controllers.get(device_id)

    def get_tof_controller(self, device_id: int) -> Optional[TOFSensorController]:
        """获取TOF控制器"""
        return self.tof_controllers.get(device_id)

    def get_all_devices(self) -> List[RizDevice]:
        """获取所有设备"""
        return list(self.devices.values())

    def get_device_count(self) -> int:
        """获取设备数量"""
        return len(self.devices)

    def get_connected_count(self) -> int:
        """获取已连接设备数"""
        return sum(1 for d in self.devices.values() if d.connection_state == STATE_CONNECTED)

    def get_advertising_count(self) -> int:
        """获取广播中设备数"""
        return sum(1 for d in self.devices.values() if d.connection_state == STATE_ADVERTISING)

    def start_all_advertising(self):
        """启动所有设备广播"""
        count = 0
        for device in self.devices.values():
            if device.connection_state == STATE_DISCONNECTED:
                device.connection_state = STATE_ADVERTISING
                count += 1

        logger.info(f"启动 {count} 个设备广播")

    def stop_all_advertising(self):
        """停止所有设备广播"""
        count = 0
        for device in self.devices.values():
            if device.connection_state == STATE_ADVERTISING:
                device.connection_state = STATE_DISCONNECTED
                count += 1

        logger.info(f"停止 {count} 个设备广播")

    def disconnect_all(self):
        """断开所有设备"""
        count = 0
        for device in self.devices.values():
            if device.connection_state == STATE_CONNECTED:
                device.connection_state = STATE_DISCONNECTED
                count += 1

        logger.info(f"断开 {count} 个设备连接")

    def update_all(self, delta_time: float):
        """更新所有设备"""
        for device_id, controller in self.controllers.items():
            controller.update(delta_time)

            # 检查TOF检测
            tof_controller = self.tof_controllers[device_id]
            if tof_controller.check_detection():
                # 检测到物体 - 关灯并发送通知
                controller.turn_light_off()
                self._send_notification(device_id)

    def _send_notification(self, device_id: int):
        """发送BLE通知"""
        device = self.devices.get(device_id)
        if not device:
            return

        # 根据之前的游戏模式发送通知
        mode = device.config.prev_game_mode
        msg = ""

        if mode == 1:  # MANUAL_MODE
            msg = "manual"
        elif mode == 2:  # RANDOM_MODE
            msg = "random"
        elif mode == 5:  # RHYTHM_MODE
            msg = "rhythm"

        if msg:
            logger.info(f"[{device.name}] 发送通知: {msg}")

    def get_summary(self) -> dict:
        """获取设备管理器摘要"""
        # 计算活跃设备（LED亮起的设备）
        active_devices = sum(1 for d in self.devices.values() if d.led_state.is_on)

        # 计算总触发次数
        total_triggers = sum(d.stats.trigger_count for d in self.devices.values())

        # 计算平均反应时间
        total_reaction_time = sum(d.stats.total_reaction_time for d in self.devices.values())
        trigger_count = sum(d.stats.trigger_count for d in self.devices.values())
        avg_response_time = total_reaction_time / trigger_count if trigger_count > 0 else 0

        # 按连接状态分组
        by_state = {}
        for device in self.devices.values():
            state = device.connection_state
            by_state[state] = by_state.get(state, 0) + 1

        # 按游戏模式分组
        by_mode = {}
        for device in self.devices.values():
            mode = device.config.game_mode
            by_mode[mode] = by_mode.get(mode, 0) + 1

        return {
            'total_devices': len(self.devices),
            'active_devices': active_devices,
            'total_triggers': total_triggers,
            'average_response_time': avg_response_time,
            'by_state': by_state,
            'by_mode': by_mode,
            'connected': self.get_connected_count(),
            'advertising': self.get_advertising_count(),
            'disconnected': len(self.devices) - self.get_connected_count() - self.get_advertising_count(),
            'max_devices': MAX_DEVICES,
        }

    # ===== BLE回调方法 =====

    def _on_device_connect(self, device_id: int):
        """设备连接回调"""
        device = self.devices.get(device_id)
        if not device:
            return

        device.connection_state = STATE_CONNECTED
        logger.info(f"[{device.name}] BLE连接成功")

        # 触发连接动画
        controller = self.controllers.get(device_id)
        if controller:
            controller.start_connected_animation()

    def _on_device_disconnect(self, device_id: int):
        """设备断开回调"""
        device = self.devices.get(device_id)
        if not device:
            return

        device.connection_state = STATE_ADVERTISING
        logger.info(f"[{device.name}] BLE断开连接")

    def _on_device_message(self, device_id: int, message: str):
        """设备消息回调"""
        device = self.devices.get(device_id)
        controller = self.controllers.get(device_id)

        if not device or not controller:
            return

        logger.info(f"[{device.name}] 收到BLE消息: {message}")

        # 解析消息
        parsed = BLEMessageParser.parse_message(message)
        mode = parsed.get("mode")

        if mode is None:
            logger.warning(f"[{device.name}] 无效的BLE消息: {message}")
            return

        # 重置设备状态
        device.able_to_turn_on = True

        # 应用参数
        if "red" in parsed:
            device.config.red_value = parsed["red"]
            device.config.green_value = parsed["green"]
            device.config.blue_value = parsed["blue"]
            device.config.sensor_mode = parsed.get("sensor_mode", 1)

        if "double_index" in parsed:
            device.config.double_mode_index = parsed["double_index"]

        if "blink_count" in parsed:
            device.config.config_blink_count = parsed["blink_count"]

        # 执行游戏模式
        controller.handle_game_mode(mode)

    # ===== BLE操作方法 =====

    def connect_device(self, device_id: int, client_address: str = "00:00:00:00:00:00"):
        """连接设备"""
        ble_server = self.ble_servers.get(device_id)
        if ble_server:
            ble_server.simulate_connect(client_address)

    def disconnect_device(self, device_id: int):
        """断开设备"""
        ble_server = self.ble_servers.get(device_id)
        if ble_server:
            ble_server.simulate_disconnect()

    def send_message_to_device(self, device_id: int, message: str):
        """向设备发送消息"""
        ble_server = self.ble_servers.get(device_id)
        if ble_server:
            ble_server.handle_write(CHARACTERISTIC_MSG_UUID, message.encode('utf-8'))
