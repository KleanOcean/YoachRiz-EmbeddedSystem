"""
BLE GATT Server Simulator
模拟ESP32的BLE GATT服务器
"""

import asyncio
from typing import Optional, Dict, Callable
from dataclasses import dataclass, field

from constants import *
from logger import get_logger

logger = get_logger("BLEServer")

try:
    from bleak import BleakServer, BleakGATTCharacteristic, BleakGATTService
    BLEAK_AVAILABLE = True
except ImportError:
    BLEAK_AVAILABLE = False
    logger.warning("Bleak未安装，BLE功能将被禁用。运行: pip install bleak")


@dataclass
class BLECharacteristic:
    """BLE特征值"""
    uuid: str
    properties: list
    value: bytes = b""
    notify_callback: Optional[Callable] = None
    write_callback: Optional[Callable] = None
    read_callback: Optional[Callable] = None


class BLEGATTServer:
    """BLE GATT服务器模拟器"""

    def __init__(self, device_id: int, device_name: str):
        self.device_id = device_id
        self.device_name = device_name
        self.is_connected = False
        self.is_advertising = False

        # GATT特征值
        self.characteristics: Dict[str, BLECharacteristic] = {}

        # 回调函数
        self.on_connect_callback: Optional[Callable] = None
        self.on_disconnect_callback: Optional[Callable] = None
        self.on_message_callback: Optional[Callable] = None

        # Bleak服务器（如果可用）
        self.bleak_server: Optional[BleakServer] = None

        self._init_characteristics()

    def _init_characteristics(self):
        """初始化GATT特征值"""
        # 主消息特征值 (Read, Write, Notify)
        self.characteristics[CHARACTERISTIC_MSG_UUID] = BLECharacteristic(
            uuid=CHARACTERISTIC_MSG_UUID,
            properties=["read", "write", "notify"],
            value=b"checc",
        )

        # TX特征值 (Notify)
        self.characteristics[CHARACTERISTIC_TX_UUID] = BLECharacteristic(
            uuid=CHARACTERISTIC_TX_UUID,
            properties=["notify"],
            value=b"",
        )

        # OTA特征值 (Write)
        self.characteristics[CHARACTERISTIC_OTA_UUID] = BLECharacteristic(
            uuid=CHARACTERISTIC_OTA_UUID,
            properties=["write"],
            value=b"",
        )

        logger.info(f"[{self.device_name}] GATT特征值初始化完成")

    async def start_advertising(self):
        """开始广播"""
        if not BLEAK_AVAILABLE:
            logger.warning(f"[{self.device_name}] Bleak未安装，使用模拟模式")
            self.is_advertising = True
            logger.info(f"[{self.device_name}] 开始广播 (模拟)")
            return

        self.is_advertising = True
        logger.info(f"[{self.device_name}] 开始BLE广播")

        # TODO: 实际的Bleak服务器启动
        # 由于Bleak的限制，这里使用模拟模式

    def stop_advertising(self):
        """停止广播"""
        self.is_advertising = False
        logger.info(f"[{self.device_name}] 停止广播")

    def simulate_connect(self, client_address: str = "00:00:00:00:00:00"):
        """模拟客户端连接"""
        if self.is_connected:
            logger.warning(f"[{self.device_name}] 已经连接")
            return

        self.is_connected = True
        self.is_advertising = False

        logger.info(f"[{self.device_name}] 客户端连接: {client_address}")

        # 触发连接回调
        if self.on_connect_callback:
            self.on_connect_callback()

    def simulate_disconnect(self):
        """模拟客户端断开"""
        if not self.is_connected:
            return

        self.is_connected = False

        logger.info(f"[{self.device_name}] 客户端断开")

        # 触发断开回调
        if self.on_disconnect_callback:
            self.on_disconnect_callback()

        # 重新开始广播
        self.is_advertising = True

    def handle_write(self, characteristic_uuid: str, data: bytes):
        """处理写入"""
        if characteristic_uuid not in self.characteristics:
            logger.warning(f"[{self.device_name}] 未知特征值: {characteristic_uuid}")
            return

        char = self.characteristics[characteristic_uuid]
        char.value = data

        # 解析消息
        try:
            message = data.decode('utf-8')
            logger.info(f"[{self.device_name}] 收到消息: {message}")

            # 触发消息回调
            if self.on_message_callback:
                self.on_message_callback(message)

        except UnicodeDecodeError:
            logger.error(f"[{self.device_name}] 消息解码失败")

    def handle_read(self, characteristic_uuid: str) -> bytes:
        """处理读取"""
        if characteristic_uuid not in self.characteristics:
            logger.warning(f"[{self.device_name}] 未知特征值: {characteristic_uuid}")
            return b""

        char = self.characteristics[characteristic_uuid]
        logger.debug(f"[{self.device_name}] 读取特征值: {characteristic_uuid}")
        return char.value

    def send_notification(self, message: str):
        """发送通知"""
        if not self.is_connected:
            logger.warning(f"[{self.device_name}] 未连接，无法发送通知")
            return

        # 更新主特征值
        char = self.characteristics[CHARACTERISTIC_MSG_UUID]
        char.value = message.encode('utf-8')

        logger.info(f"[{self.device_name}] 发送通知: {message}")

        # TODO: 实际的BLE通知
        # 在模拟模式下，通知会被记录但不会真正发送

    def get_device_info(self) -> dict:
        """获取设备信息"""
        return {
            "device_id": self.device_id,
            "device_name": self.device_name,
            "is_connected": self.is_connected,
            "is_advertising": self.is_advertising,
            "service_uuid": SERVICE_UUID,
            "characteristics": {
                uuid: {
                    "properties": char.properties,
                    "value": char.value.decode('utf-8', errors='ignore')
                }
                for uuid, char in self.characteristics.items()
            }
        }


class BLEMessageParser:
    """BLE消息解析器"""

    @staticmethod
    def parse_message(message: str) -> dict:
        """解析BLE消息

        消息格式:
        - Manual: "1"
        - Random: "2"
        - Rhythm: "5,R,G,B,timer,buzzer,sensor_mode"
        - Double: "4,index"
        - Config: "config:N" 或 "100,N"
        - Opening: "11"
        - Closing: "12"
        - Terminate: "13"
        """
        message = message.strip()

        # Config模式特殊处理
        if message.startswith("config:"):
            count = int(message.split(":")[1])
            return {
                "mode": CONFIG_MODE,
                "blink_count": count
            }

        # 分割参数
        parts = message.split(",")

        try:
            mode = int(parts[0])

            result = {"mode": mode}

            # Rhythm模式 (5,R,G,B,timer,buzzer,sensor_mode)
            if mode == RHYTHM_MODE and len(parts) >= 7:
                result["red"] = int(parts[1])
                result["green"] = int(parts[2])
                result["blue"] = int(parts[3])
                result["timer"] = int(parts[4])
                result["buzzer"] = int(parts[5])
                result["sensor_mode"] = int(parts[6])

            # Double模式 (4,index)
            elif mode == DOUBLE_MODE and len(parts) >= 2:
                result["double_index"] = int(parts[1])

            # Config模式 (100,N)
            elif mode == CONFIG_MODE and len(parts) >= 2:
                result["blink_count"] = int(parts[1])

            return result

        except (ValueError, IndexError) as e:
            logger.error(f"消息解析失败: {message}, 错误: {e}")
            return {"mode": TERMINATE_MODE}

    @staticmethod
    def create_message(mode: int, **kwargs) -> str:
        """创建BLE消息"""
        if mode == RHYTHM_MODE:
            return f"{mode},{kwargs.get('red', 255)},{kwargs.get('green', 140)},{kwargs.get('blue', 0)},{kwargs.get('timer', 0)},{kwargs.get('buzzer', 0)},{kwargs.get('sensor_mode', 1)}"

        elif mode == DOUBLE_MODE:
            return f"{mode},{kwargs.get('double_index', 0)}"

        elif mode == CONFIG_MODE:
            return f"config:{kwargs.get('blink_count', 3)}"

        else:
            return str(mode)
