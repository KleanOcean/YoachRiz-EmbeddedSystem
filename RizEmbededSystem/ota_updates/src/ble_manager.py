#!/usr/bin/env python3
"""
BLE管理器 - 处理蓝牙设备扫描、连接和通信
"""

import asyncio
import platform
import struct
import time
import threading
from typing import List, Dict, Optional, Callable

# 跨平台BLE库
try:
    from bleak import BleakScanner, BleakClient
    BLEAK_AVAILABLE = True
except ImportError:
    print("警告: bleak库未安装，使用模拟模式")
    print("请运行: pip install bleak")
    BLEAK_AVAILABLE = False

# BLE服务和特征UUID
# 注意：固件使用单一服务架构，OTA是服务内的特征而非独立服务
BLE_SERVICE_UUID = "ab0828b1-198e-4351-b779-901fa0e0371e"  # 主服务
BLE_MSG_CHAR_UUID = "4ac8a696-9736-4e5d-932b-e9b31405049c"  # 消息特征
BLE_TX_CHAR_UUID = "62ec0272-3ec5-11eb-b378-0242ac130003"   # TX/状态特征
BLE_OTA_CHAR_UUID = "62ec0272-3ec5-11eb-b378-0242ac130005"  # OTA特征（实际使用的）

# 这些UUID在当前固件中未使用（保留用于兼容性检查）
BLE_OTA_SERVICE_UUID = "1d14d6ee-fd63-4fa1-bfa4-8f47b42119f0"  # 未实现的独立OTA服务
BLE_OTA_CONTROL_UUID = "f7bf3564-fb6d-4e53-88a4-5e37e0326063"  # 未使用
BLE_OTA_DATA_UUID = "984227f3-34fc-4045-a5d0-2c581f81a153"     # 未使用

class BLEManager:
    """BLE设备管理器"""

    def __init__(self):
        self.client: Optional[BleakClient] = None
        self.connected_device = None
        self.loop = None
        self.loop_thread = None
        self.ota_ack_received = False
        self.notification_queue = asyncio.Queue()

        # 创建独立的事件循环在后台线程中运行
        self._setup_event_loop()

    def _setup_event_loop(self):
        """设置事件循环在独立线程中运行"""
        def run_loop():
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_forever()

        self.loop_thread = threading.Thread(target=run_loop, daemon=True)
        self.loop_thread.start()

        # 等待循环启动
        while self.loop is None:
            time.sleep(0.01)

    def _notification_handler(self, sender, data):
        """处理BLE通知"""
        # OTA确认通知处理
        print(f"[OTA通知] 收到通知: sender={sender}, data={data.hex() if data else 'None'}, 长度={len(data) if data else 0}")
        self.ota_ack_received = True

    def _run_async(self, coro):
        """在事件循环中运行协程"""
        if not self.loop:
            self._setup_event_loop()

        future = asyncio.run_coroutine_threadsafe(coro, self.loop)
        try:
            return future.result(timeout=30)  # 30秒超时
        except Exception as e:
            print(f"异步操作失败: {e}")
            return None

    def __del__(self):
        """析构函数，确保断开连接"""
        if self.client and self.client.is_connected:
            self.disconnect()

        # 停止事件循环
        if self.loop and self.loop.is_running():
            self.loop.call_soon_threadsafe(self.loop.stop)

    def scan_devices(self, timeout: float = 5.0) -> List[Dict]:
        """
        扫描BLE设备

        Args:
            timeout: 扫描超时时间（秒）

        Returns:
            设备列表，每个设备包含 name, address, rssi
        """
        if not BLEAK_AVAILABLE:
            # 返回模拟数据
            return self._get_mock_devices()

        devices = []

        async def _scan():
            try:
                scanner = BleakScanner()
                found_devices = await scanner.discover(timeout=timeout)

                for device in found_devices:
                    # 获取设备信息
                    device_info = {
                        'name': device.name or 'Unknown',
                        'address': device.address,
                        'rssi': device.rssi if hasattr(device, 'rssi') else -50
                    }

                    # 只添加PRO-开头的设备或用于测试的设备
                    if device_info['name'].startswith('PRO-') or 'Riz' in device_info['name']:
                        devices.append(device_info)

            except Exception as e:
                print(f"扫描设备出错: {e}")

        # 在事件循环中运行扫描
        self._run_async(_scan())

        return devices

    def connect(self, address: str) -> bool:
        """
        连接到指定设备

        Args:
            address: 设备MAC地址

        Returns:
            连接是否成功
        """
        if not BLEAK_AVAILABLE:
            print("模拟模式: 假装连接成功")
            self.connected_device = address
            return True

        async def _connect():
            try:
                # 断开现有连接
                if self.client and self.client.is_connected:
                    await self.client.disconnect()

                # 创建客户端并连接
                self.client = BleakClient(address)
                await self.client.connect(timeout=10.0)

                if self.client.is_connected:
                    self.connected_device = address
                    print(f"成功连接到设备: {address}")

                    # 发现服务 - 处理不同版本的bleak
                    try:
                        # 获取服务集合
                        services = self.client.services

                        # BleakGATTServiceCollection是可迭代的，但不能直接len()
                        service_count = 0
                        ota_char_found = False
                        msg_char_found = False
                        tx_char_found = False

                        for service in services:
                            service_count += 1
                            print(f"  服务UUID: {service.uuid}")

                            # 检查是否是主服务
                            if BLE_SERVICE_UUID.lower() in str(service.uuid).lower():
                                print("  → 发现主服务!")

                                # 检查服务内的特征
                                for char in service.characteristics:
                                    print(f"    特征UUID: {char.uuid}")

                                    if BLE_OTA_CHAR_UUID.lower() in str(char.uuid).lower():
                                        print("    → 发现OTA特征! ✓")
                                        ota_char_found = True
                                    elif BLE_MSG_CHAR_UUID.lower() in str(char.uuid).lower():
                                        print("    → 发现消息特征! ✓")
                                        msg_char_found = True
                                    elif BLE_TX_CHAR_UUID.lower() in str(char.uuid).lower():
                                        print("    → 发现TX特征! ✓")
                                        tx_char_found = True

                                        # 订阅TX特征的通知（用于OTA确认）
                                        try:
                                            await self.client.start_notify(char.uuid, self._notification_handler)
                                            print("    → 已订阅TX特征通知，用于OTA确认")
                                        except Exception as e:
                                            print(f"    → 订阅通知失败: {e}")

                        print(f"共发现 {service_count} 个服务")

                        if ota_char_found:
                            print("✅ OTA功能可用")
                        else:
                            print("⚠️ 未发现OTA特征，设备可能不支持OTA更新")

                    except Exception as e:
                        print(f"警告: 无法枚举服务 ({e})，但连接成功")

                    return True
                else:
                    print("连接失败")
                    return False

            except Exception as e:
                print(f"连接设备出错: {e}")
                return False

        result = self._run_async(_connect())
        return result if result is not None else False

    def disconnect(self) -> bool:
        """
        断开当前连接

        Returns:
            断开是否成功
        """
        if not BLEAK_AVAILABLE:
            self.connected_device = None
            return True

        async def _disconnect():
            try:
                if self.client and self.client.is_connected:
                    await self.client.disconnect()
                    print("已断开连接")
                self.connected_device = None
                return True
            except Exception as e:
                print(f"断开连接出错: {e}")
                return False

        result = self._run_async(_disconnect())
        return result if result is not None else False

    def is_connected(self) -> bool:
        """检查是否已连接"""
        if not BLEAK_AVAILABLE:
            return self.connected_device is not None

        return self.client and self.client.is_connected

    def get_mtu(self) -> int:
        """获取当前MTU大小"""
        if not self.is_connected():
            return 517  # iOS典型协商值

        if not BLEAK_AVAILABLE:
            return 517  # iOS典型值

        # ESP32 with NimBLE typically negotiates 517 MTU with iOS
        # iOS uses maximumWriteValueLength - 3 for chunk size
        return 517  # ESP32 NimBLE典型MTU值

    def send_command(self, command: str) -> bool:
        """
        发送命令到设备

        Args:
            command: 要发送的命令字符串

        Returns:
            发送是否成功
        """
        if not self.is_connected():
            print("设备未连接")
            return False

        if not BLEAK_AVAILABLE:
            print(f"模拟模式: 发送命令 {command}")
            return True

        async def _send():
            try:
                # 将命令转换为字节
                data = command.encode('utf-8')

                # 写入消息特征
                # 尝试使用UUID字符串或查找特征
                try:
                    await self.client.write_gatt_char(
                        BLE_MSG_CHAR_UUID,
                        data,
                        response=True
                    )
                except Exception as e:
                    # 如果UUID格式有问题，尝试查找特征
                    print(f"尝试使用UUID直接写入失败: {e}")
                    print("尝试查找特征...")

                    char_found = False
                    for service in self.client.services:
                        for char in service.characteristics:
                            if BLE_MSG_CHAR_UUID.lower() in str(char.uuid).lower():
                                await self.client.write_gatt_char(
                                    char,
                                    data,
                                    response=True
                                )
                                print(f"通过特征对象发送成功")
                                char_found = True
                                break
                        if char_found:
                            break

                    if not char_found:
                        print(f"错误: 未找到消息特征UUID {BLE_MSG_CHAR_UUID}")
                        return False

                print(f"命令已发送: {command}")
                return True

            except Exception as e:
                print(f"发送命令出错: {e}")
                return False

        result = self._run_async(_send())
        return result if result is not None else False

    def start_ota(self, firmware_size: int) -> bool:
        """
        开始OTA更新 - iOS协议

        iOS不发送控制包，ESP32在收到第一个包（0xE9开头）时自动进入OTA模式。

        Args:
            firmware_size: 固件大小（字节）

        Returns:
            是否成功开始OTA
        """
        if not self.is_connected():
            return False

        if not BLEAK_AVAILABLE:
            print(f"[OTA-iOS] 模拟模式: 固件大小 {firmware_size} 字节")
            return True

        print(f"[OTA-iOS] 准备传输 {firmware_size} 字节")
        print(f"[OTA-iOS] 协议: 510字节块, 每块等待ACK (ESP32要求)")
        print(f"[OTA-iOS] 预计时间: {firmware_size / 1024 / 15:.1f} 秒 (基于15KB/s)")
        return True

    def send_ota_data_ios_style(self, data: bytes, wait_ack: bool = True) -> bool:
        """
        发送OTA数据 - 完全匹配iOS实现

        iOS代码参考:
        peripheral.writeValue(chunk, for: otaCharacteristic, type: .withoutResponse)

        Args:
            data: 原始固件数据块（无头部）
            wait_ack: 是否等待ACK（iOS每2块等一次）

        Returns:
            发送是否成功
        """
        if not self.is_connected():
            print("[OTA-iOS] 错误: 设备未连接")
            return False

        if not BLEAK_AVAILABLE:
            time.sleep(0.001)
            return True

        async def _send_ios_style():
            try:
                # iOS直接发送原始数据，无任何包装
                print(f"[OTA-iOS] 发送块: {len(data)}字节, 前8字节={data[:8].hex() if len(data) >= 8 else data.hex()}, wait_ack={wait_ack}")

                # 使用WRITE_NO_RESPONSE，匹配iOS
                await self.client.write_gatt_char(
                    BLE_OTA_CHAR_UUID,
                    data,
                    response=False  # iOS: .withoutResponse
                )

                # 给ESP32一点处理时间
                await asyncio.sleep(0.02)  # 20ms delay for ESP32 to process

                if wait_ack:
                    # ESP32发送1字节ACK
                    print("[OTA-iOS] 等待ACK")
                    self.ota_ack_received = False
                    wait_time = 0
                    max_wait = 5.0  # 给ESP32更多时间

                    while not self.ota_ack_received and wait_time < max_wait:
                        await asyncio.sleep(0.01)
                        wait_time += 0.01

                    if not self.ota_ack_received:
                        print(f"[OTA-iOS] ⚠ 未收到ACK ({wait_time:.2f}秒超时)")
                        return False
                    else:
                        print(f"[OTA-iOS] ✓ ACK收到 ({wait_time:.2f}秒)")

                return True

            except Exception as e:
                print(f"[OTA-iOS] 错误: {e}")
                return False

        result = self._run_async(_send_ios_style())
        return result if result is not None else False

    def send_ota_data_burst(self, data: bytes, wait_ack: bool = True) -> bool:
        """
        发送OTA数据（burst模式，匹配iOS实现）

        Args:
            data: 数据块
            wait_ack: 是否等待ACK（仅在批次最后一个块时等待）

        Returns:
            发送是否成功
        """
        if not self.is_connected():
            print("错误: 设备未连接")
            return False

        if not BLEAK_AVAILABLE:
            time.sleep(0.001)
            return True

        async def _send_burst():
            try:
                # 记录发送的数据详情
                print(f"[OTA发送] 大小={len(data)}字节, 前10字节={data[:10].hex() if len(data) >= 10 else data.hex()}, wait_ack={wait_ack}")

                # 直接发送数据，不等待响应
                await self.client.write_gatt_char(
                    BLE_OTA_CHAR_UUID,
                    data,
                    response=False  # Write without response
                )

                # 给ESP32一点处理时间，即使不等待ACK
                if not wait_ack:
                    await asyncio.sleep(0.01)  # 10ms delay between chunks

                if wait_ack:
                    # 等待ESP32的ACK（仅在批次结束时）
                    print("[OTA] 等待ACK...")
                    self.ota_ack_received = False
                    wait_time = 0
                    while not self.ota_ack_received and wait_time < 2:
                        await asyncio.sleep(0.01)
                        wait_time += 0.01

                    if not self.ota_ack_received:
                        print(f"[OTA警告] 未收到ACK (等待了{wait_time:.2f}秒)")
                        return False
                    else:
                        print(f"[OTA] 收到ACK (等待时间: {wait_time:.2f}秒)")

                return True

            except Exception as e:
                print(f"[OTA错误] 发送数据失败: {e}")
                import traceback
                traceback.print_exc()
                return False

        result = self._run_async(_send_burst())
        return result if result is not None else False

    def send_ota_data_raw(self, data: bytes) -> bool:
        """
        发送原始OTA数据块（不添加偏移量头）

        Args:
            data: 数据块（最大512字节）

        Returns:
            发送是否成功
        """
        if not self.is_connected():
            return False

        if not BLEAK_AVAILABLE:
            # 模拟传输延迟
            time.sleep(0.01)
            return True

        async def _send_raw_data():
            try:
                # 重置确认标志
                self.ota_ack_received = False

                # 直接发送原始数据，不添加任何头部
                await self.client.write_gatt_char(
                    BLE_OTA_CHAR_UUID,  # 使用正确的OTA特征
                    data,
                    response=False  # 不等待响应以提高速度
                )

                # 等待ESP32处理并发送确认（最多等待2秒）
                wait_time = 0
                while not self.ota_ack_received and wait_time < 2:
                    await asyncio.sleep(0.05)  # 50ms检查一次
                    wait_time += 0.05

                if not self.ota_ack_received:
                    # 如果没收到确认，稍等一下再继续（可能ESP32还在处理）
                    await asyncio.sleep(0.1)

                return True

            except Exception as e:
                print(f"发送OTA数据出错: {e}")
                return False

        result = self._run_async(_send_raw_data())
        return result if result is not None else False

    def send_ota_data(self, data: bytes, offset: int) -> bool:
        """
        发送OTA数据块

        Args:
            data: 数据块（最大512字节）
            offset: 数据偏移量

        Returns:
            发送是否成功
        """
        if not self.is_connected():
            return False

        if not BLEAK_AVAILABLE:
            # 模拟传输延迟
            time.sleep(0.01)
            return True

        async def _send_data():
            try:
                # 添加偏移量到数据包头部
                packet = struct.pack('<I', offset) + data

                # 使用实际的OTA特征UUID发送数据
                try:
                    await self.client.write_gatt_char(
                        BLE_OTA_CHAR_UUID,  # 使用正确的OTA特征
                        packet,
                        response=False  # 不等待响应以提高速度
                    )
                except:
                    # 如果失败，尝试使用特征对象
                    for service in self.client.services:
                        if BLE_SERVICE_UUID.lower() in str(service.uuid).lower():
                            for char in service.characteristics:
                                if BLE_OTA_CHAR_UUID.lower() in str(char.uuid).lower():
                                    await self.client.write_gatt_char(
                                        char,
                                        packet,
                                        response=False
                                    )
                                    break

                return True

            except Exception as e:
                print(f"发送OTA数据出错: {e}")
                return False

        result = self._run_async(_send_data())
        return result if result is not None else False

    def finish_ota(self) -> bool:
        """
        完成OTA更新 - iOS协议

        iOS不发送结束包。ESP32在接收完所有数据后自动完成OTA。

        Returns:
            是否成功完成OTA
        """
        if not self.is_connected():
            return False

        if not BLEAK_AVAILABLE:
            print("[OTA-iOS] 模拟模式: OTA完成")
            return True

        # iOS协议：不需要发送结束包
        # ESP32会在收到所有固件数据后自动完成并重启
        print("[OTA-iOS] 传输完成，设备将自动验证并重启")
        return True


    def _get_mock_devices(self) -> List[Dict]:
        """获取模拟设备列表（用于测试）"""
        return [
            {'name': 'PRO-1234', 'address': 'AA:BB:CC:DD:EE:01', 'rssi': -45},
            {'name': 'PRO-5678', 'address': 'AA:BB:CC:DD:EE:02', 'rssi': -62},
            {'name': 'PRO-9012', 'address': 'AA:BB:CC:DD:EE:03', 'rssi': -78},
            {'name': 'Riz-Test', 'address': 'AA:BB:CC:DD:EE:04', 'rssi': -55},
        ]

# 测试代码
if __name__ == "__main__":
    manager = BLEManager()

    print("扫描设备...")
    devices = manager.scan_devices()

    print(f"\n发现 {len(devices)} 个设备:")
    for device in devices:
        print(f"  - {device['name']} ({device['address']}) RSSI: {device['rssi']} dBm")

    if devices:
        print(f"\n尝试连接到第一个设备: {devices[0]['name']}")
        if manager.connect(devices[0]['address']):
            print("连接成功!")

            print("\n发送测试命令...")
            if manager.send_command("11,999,999,999,999,999,999,999"):
                print("命令发送成功")

            print("\n断开连接...")
            manager.disconnect()
        else:
            print("连接失败")