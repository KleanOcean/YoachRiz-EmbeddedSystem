#!/usr/bin/env python3
"""
OTA上传器 - 处理固件上传到设备
"""

import os
import time
import hashlib
from typing import Dict, Optional, Callable
from pathlib import Path

class OTAUploader:
    """OTA固件上传器"""

    # OTA配置 - 基于ESP32 OTA.cpp实际代码
    # ESP32期望510字节的块（见OTA.cpp第80行）
    CHUNK_SIZE = 510  # ESP32 OTA.cpp硬编码期望510字节
    CHUNKS_PER_ACK = 1 # 每个块后都发送ACK（见OTA.cpp第71-72行）
    MAX_RETRIES = 3   # 最大重试次数
    ACK_TIMEOUT = 5   # 确认超时（秒）- 给ESP32更多时间

    def __init__(self):
        self.current_offset = 0
        self.total_size = 0
        self.firmware_data = None

    def upload(
        self,
        ble_manager,
        firmware_path: str,
        progress_callback: Optional[Callable[[int, str], None]] = None
    ) -> Dict:
        """
        上传固件到设备

        Args:
            ble_manager: BLE管理器实例
            firmware_path: 固件文件路径
            progress_callback: 进度回调函数(百分比, 消息)

        Returns:
            上传结果，包含 success, error
        """
        result = {
            'success': False,
            'error': None,
            'bytes_sent': 0,
            'time_elapsed': 0
        }

        start_time = time.time()

        try:
            # 1. 读取固件文件
            if progress_callback:
                progress_callback(0, "读取固件文件...")

            # Verify the firmware path is valid
            print(f"[OTA] 准备读取固件文件: {firmware_path}")
            if not os.path.exists(firmware_path):
                result['error'] = f"固件文件不存在: {firmware_path}"
                return result

            firmware_data = self._read_firmware(firmware_path)
            if not firmware_data:
                result['error'] = f"无法读取固件文件: {firmware_path}"
                return result

            self.firmware_data = firmware_data
            self.total_size = len(firmware_data)

            print(f"固件文件路径: {firmware_path}")
            print(f"固件大小: {self.total_size} 字节")
            print(f"固件前16字节: {firmware_data[:16].hex()}")
            print(f"固件前32字节: {firmware_data[:32].hex()}")

            # 验证固件格式
            if firmware_data[0] != 0xE9:
                print(f"错误: 固件第一个字节不是0xE9 (实际: 0x{firmware_data[0]:02X})")
                print("这不是有效的ESP32固件文件")

                # Check if it might be a text file by mistake
                try:
                    text_preview = firmware_data[:100].decode('utf-8', errors='ignore')
                    if text_preview.isprintable():
                        print(f"错误: 文件似乎是文本文件，内容预览: {text_preview[:50]}...")
                        result['error'] = "选择的文件不是有效的ESP32固件二进制文件"
                        return result
                except:
                    pass

                # Still allow with warning, but make it clear
                user_response = input("⚠️ 警告: 这不是标准ESP32固件文件。是否继续? (y/n): ")
                if user_response.lower() != 'y':
                    result['error'] = "用户取消: 固件格式无效"
                    return result

            # 验证固件头部结构 (ESP32 image header)
            # ESP32固件格式: [0xE9, num_segments, SPI mode, SPI speed/size, entry_point]
            if len(firmware_data) >= 24:
                num_segments = firmware_data[1]
                spi_mode = firmware_data[2]
                spi_speed_size = firmware_data[3]
                entry_point = int.from_bytes(firmware_data[4:8], 'little')
                print(f"ESP32固件头部信息:")
                print(f"  - 段数量: {num_segments}")
                print(f"  - SPI模式: 0x{spi_mode:02X}")
                print(f"  - SPI速度/大小: 0x{spi_speed_size:02X}")
                print(f"  - 入口点: 0x{entry_point:08X}")

            # 2. 计算固件校验和
            if progress_callback:
                progress_callback(5, "计算校验和...")

            checksum = self._calculate_checksum(firmware_data)
            print(f"固件校验和: {checksum}")

            # 3. 开始OTA流程
            if progress_callback:
                progress_callback(10, "初始化OTA...")

            if not ble_manager.start_ota(self.total_size):
                result['error'] = "无法启动OTA流程"
                return result

            # 等待设备准备
            time.sleep(1)

            # 4. 分块传输固件（iOS协议：发送2块，等待ACK，重复）
            if progress_callback:
                progress_callback(15, "开始传输固件...")

            # ESP32 OTA.cpp期望510字节的块
            # 小于510字节的块被视为最后一个块
            actual_chunk_size = self.CHUNK_SIZE  # 510字节

            self.current_offset = 0
            total_chunks = (self.total_size + actual_chunk_size - 1) // actual_chunk_size
            chunk_index = 0

            # ESP32 OTA传输循环
            print(f"[OTA] 开始传输: 总大小={self.total_size}字节, 块大小={actual_chunk_size}字节, 总块数={total_chunks}")
            print(f"[OTA] 使用ESP32协议: 510字节块, 每块等待ACK")

            while chunk_index < total_chunks:
                # ESP32每个块都需要ACK
                start = chunk_index * actual_chunk_size
                end = min(start + actual_chunk_size, self.total_size)
                chunk_data = firmware_data[start:end]

                # 调试第一个块
                if chunk_index == 0:
                    print(f"[OTA] 第一个块: 大小={len(chunk_data)}字节")
                    print(f"[OTA] 前16字节: {chunk_data[:16].hex()}")
                    print(f"[OTA] 前32字节: {chunk_data[:32].hex()}")

                    # Double-check we're sending binary firmware, not text
                    if len(chunk_data) >= 8:
                        as_text = chunk_data[:8].decode('utf-8', errors='ignore')
                        print(f"[OTA] 前8字节解码为文本: '{as_text}' (应该是乱码)")

                    if len(chunk_data) > 0 and chunk_data[0] == 0xE9:
                        print(f"[OTA] ✓ 魔术字节正确: 0xE9")
                    else:
                        print(f"[OTA] ⚠ 魔术字节: 0x{chunk_data[0]:02X} (期望0xE9)")

                # 记录关键块的信息
                if chunk_index >= total_chunks - 3 or chunk_index < 3:
                    print(f"[OTA] 块 {chunk_index}/{total_chunks-1}: 大小={len(chunk_data)}字节, 偏移={start}")

                # 发送单个块（ESP32要求每块都ACK）
                chunk_success = False
                retry_count = 0

                while not chunk_success and retry_count < self.MAX_RETRIES:
                    try:
                        # ESP32: 每个块都等待ACK
                        if not ble_manager.send_ota_data_ios_style(chunk_data, wait_ack=True):
                            raise Exception(f"块 {chunk_index} 发送失败")

                        # 成功发送 - 只在成功时更新offset，且在retry循环外
                        chunk_success = True

                    except Exception as e:
                        retry_count += 1
                        if retry_count < self.MAX_RETRIES:
                            print(f"[OTA] 块 {chunk_index} 失败，重试 {retry_count}/{self.MAX_RETRIES}: {e}")
                            time.sleep(0.5)  # 重试前短暂延迟
                        else:
                            result['error'] = f"块 {chunk_index} 传输失败: {e}"
                            return result

                # 只有在成功发送后才更新offset（移到retry循环外）
                self.current_offset += len(chunk_data)

                chunk_index += 1

                # 更新进度
                progress_percent = 15 + int(chunk_index / total_chunks * 75)
                bytes_sent = min(chunk_index * actual_chunk_size, self.total_size)
                speed_kbps = bytes_sent / (time.time() - start_time) / 1024

                if progress_callback:
                    progress_callback(
                        progress_percent,
                        f"传输中... {bytes_sent}/{self.total_size} 字节 ({speed_kbps:.1f} KB/s)"
                    )

                result['bytes_sent'] = bytes_sent

            # 记录传输完成统计
            print(f"[OTA] 传输完成: 发送了 {chunk_index} 块, 共 {self.current_offset} 字节")
            print(f"[OTA] 期望: {total_chunks} 块, {self.total_size} 字节")

            if self.current_offset != self.total_size:
                print(f"[OTA] ⚠️ 警告: 字节数不匹配! 差异: {self.total_size - self.current_offset} 字节")

            # 验证最后一个块是否小于510字节（ESP32用来识别传输结束）
            last_chunk_size = self.total_size % actual_chunk_size
            if last_chunk_size == 0:
                last_chunk_size = actual_chunk_size
            print(f"[OTA] 最后一个块大小: {last_chunk_size} 字节")
            if last_chunk_size >= 510:
                print(f"[OTA] ⚠️ 警告: 最后一个块不小于510字节，ESP32可能无法识别传输结束！")

            # 计算并显示实际传输的数据校验和
            sent_checksum = self._calculate_checksum(firmware_data[:self.current_offset])
            print(f"[OTA] 已发送数据的MD5: {sent_checksum}")
            print(f"[OTA] 原始固件的MD5: {checksum}")

            # 5. 不需要处理失败的块（已在主循环中处理）

            # 6. 完成OTA
            # ESP32应该在收到小于510字节的块后自动触发验证
            # 不需要额外的完成命令
            if progress_callback:
                progress_callback(95, "等待ESP32验证固件...")

            # 给ESP32时间来处理最后的块和验证
            print("[OTA] 等待ESP32完成验证...")
            time.sleep(2)

            # 注意：不调用finish_ota()，因为ESP32会在收到小块后自动完成
            # if not ble_manager.finish_ota():
            #     result['error'] = "无法完成OTA流程"
            #     return result

            # 7. 等待设备重启
            if progress_callback:
                progress_callback(98, "等待设备重启...")

            time.sleep(3)

            # 计算总耗时
            result['time_elapsed'] = time.time() - start_time
            result['success'] = True

            if progress_callback:
                progress_callback(100, f"更新成功! 耗时 {result['time_elapsed']:.1f} 秒")

            print(f"OTA更新成功! 传输 {result['bytes_sent']} 字节，耗时 {result['time_elapsed']:.1f} 秒")

        except Exception as e:
            result['error'] = f"OTA上传异常: {str(e)}"
            print(f"OTA上传异常: {e}")

        return result

    def _read_firmware(self, firmware_path: str) -> Optional[bytes]:
        """
        读取固件文件

        Args:
            firmware_path: 固件文件路径

        Returns:
            固件数据，如果失败返回None
        """
        try:
            path = Path(firmware_path)
            if not path.exists():
                print(f"固件文件不存在: {firmware_path}")
                return None

            with open(path, 'rb') as f:
                data = f.read()

            print(f"读取固件文件成功: {len(data)} 字节")
            return data

        except Exception as e:
            print(f"读取固件文件失败: {e}")
            return None

    def _calculate_checksum(self, data: bytes) -> str:
        """
        计算数据校验和

        Args:
            data: 数据

        Returns:
            十六进制校验和字符串
        """
        md5 = hashlib.md5()
        md5.update(data)
        return md5.hexdigest()

    def verify_upload(self, ble_manager) -> bool:
        """
        验证固件上传是否成功

        Args:
            ble_manager: BLE管理器实例

        Returns:
            验证是否成功
        """
        # 这里可以实现固件验证逻辑
        # 例如：读取设备版本号，对比校验和等
        return True

    def get_progress(self) -> Dict:
        """
        获取当前上传进度

        Returns:
            进度信息
        """
        if self.total_size == 0:
            percent = 0
        else:
            percent = int(self.current_offset / self.total_size * 100)

        return {
            'percent': percent,
            'bytes_sent': self.current_offset,
            'total_bytes': self.total_size
        }

# 测试代码
if __name__ == "__main__":
    from ble_manager import BLEManager

    # 创建实例
    uploader = OTAUploader()
    ble_manager = BLEManager()

    # 模拟固件路径
    firmware_path = "../test/mock_firmware.bin"

    # 创建模拟固件文件
    test_firmware = Path(firmware_path)
    test_firmware.parent.mkdir(exist_ok=True, parents=True)
    test_firmware.write_bytes(b'TEST_FIRMWARE' + b'\x00' * 1024 * 50)  # 50KB

    # 进度回调
    def progress_callback(percent, message):
        print(f"[{percent:3d}%] {message}")

    # 模拟设备连接
    print("模拟设备连接...")
    devices = ble_manager.scan_devices()
    if devices:
        if ble_manager.connect(devices[0]['address']):
            print("开始OTA更新...")

            result = uploader.upload(
                ble_manager,
                firmware_path,
                progress_callback
            )

            if result['success']:
                print(f"\n更新成功!")
                print(f"传输字节: {result['bytes_sent']}")
                print(f"耗时: {result['time_elapsed']:.1f} 秒")
            else:
                print(f"\n更新失败: {result['error']}")

            ble_manager.disconnect()
        else:
            print("连接失败")