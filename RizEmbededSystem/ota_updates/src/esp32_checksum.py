#!/usr/bin/env python3
"""
ESP32固件校验和验证工具
基于ESP-IDF的固件格式和校验和算法
"""

import sys
import struct
from pathlib import Path

def calculate_esp32_checksum(data: bytes) -> int:
    """
    计算ESP32固件的校验和
    ESP32使用简单的XOR校验和算法
    """
    checksum = 0xEF

    # ESP32 image header structure
    # 固件头部是24字节，之后是段数据
    if len(data) < 24:
        print("固件太小，无法包含有效的ESP32头部")
        return -1

    # 读取段数量
    num_segments = data[1]
    print(f"段数量: {num_segments}")

    # 计算整个镜像的校验和
    for i in range(len(data)):
        checksum ^= data[i]

    return checksum & 0xFF

def parse_esp32_image(firmware_path: str):
    """
    解析ESP32固件镜像
    """
    path = Path(firmware_path)
    if not path.exists():
        print(f"文件不存在: {firmware_path}")
        return

    with open(path, 'rb') as f:
        data = f.read()

    print(f"固件大小: {len(data)} 字节")
    print(f"固件前32字节 (hex):")
    for i in range(0, min(32, len(data)), 16):
        hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
        print(f"  {i:08x}: {hex_str}")

    # 检查魔术字节
    if data[0] != 0xE9:
        print(f"警告: 无效的魔术字节 0x{data[0]:02X} (期望 0xE9)")
        return

    # 解析头部
    magic = data[0]
    segment_count = data[1]
    spi_mode = data[2]
    spi_speed_size = data[3]
    entry_addr = struct.unpack('<I', data[4:8])[0]

    print(f"\nESP32镜像头部:")
    print(f"  魔术字节: 0x{magic:02X}")
    print(f"  段数量: {segment_count}")
    print(f"  SPI模式: 0x{spi_mode:02X}")
    print(f"  SPI速度/大小: 0x{spi_speed_size:02X}")
    print(f"  入口地址: 0x{entry_addr:08X}")

    # ESP32 bootloader在固件末尾查找校验和
    # 通常在倒数第二个字节的位置
    if len(data) >= 2:
        # 尝试不同的校验和位置
        positions = {
            "倒数第1字节": data[-1],
            "倒数第2字节": data[-2],
            "偏移0x20": data[0x20] if len(data) > 0x20 else None,
            "偏移0x21": data[0x21] if len(data) > 0x21 else None,
        }

        print(f"\n可能的校验和位置:")
        for pos, val in positions.items():
            if val is not None:
                print(f"  {pos}: 0x{val:02X}")

    # 计算XOR校验和
    checksum = calculate_esp32_checksum(data)
    print(f"\n计算的XOR校验和: 0x{checksum:02X}")

    # ESP32 OTA验证可能使用的是简单的字节和
    byte_sum = sum(data) & 0xFF
    print(f"字节和 (mod 256): 0x{byte_sum:02X}")

    # 计算除了最后一个字节外的校验和
    if len(data) > 1:
        checksum_without_last = 0xEF
        for i in range(len(data) - 1):
            checksum_without_last ^= data[i]
        checksum_without_last &= 0xFF
        print(f"不含最后字节的XOR校验和: 0x{checksum_without_last:02X}")

        # 检查是否匹配最后一个字节
        if checksum_without_last == data[-1]:
            print(f"✓ 校验和匹配! 固件末尾存储了XOR校验和")
        elif checksum_without_last == 0x9d:
            print(f"! 计算值0x9d匹配ESP32报告的值")

    print(f"\nESP32报告的错误: 'Calculated 0x9d read 0x20'")
    print(f"这表明ESP32计算出0x9d但从固件读取到0x20")

    # 查找0x20在固件中的位置
    occurrences = [i for i, b in enumerate(data) if b == 0x20]
    print(f"\n0x20在固件中出现 {len(occurrences)} 次")
    if occurrences and len(occurrences) < 20:
        print(f"前几个位置: {occurrences[:10]}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        firmware_path = sys.argv[1]
    else:
        firmware_path = "/Users/klean/TheGitHub/comma/EmbededSystem/.pio/build/esp32doit-devkit-v1/firmware.bin"

    parse_esp32_image(firmware_path)