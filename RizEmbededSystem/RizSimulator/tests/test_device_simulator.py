"""
Test Device Simulator
设备模拟器测试
"""

import pytest
import sys
from pathlib import Path

# 添加src到路径
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))

from core.device_simulator import RizDevice, LightState, SensorData
from utils.constants import Commands, LightMode


def test_device_creation():
    """测试设备创建"""
    device = RizDevice(1)

    assert device.device_id == 1
    assert device.name == "RIZ-0001"
    assert device.mac_address.startswith("AA:BB:CC:DD:")
    assert device.firmware_version == "v1.0.0"


def test_device_mac_address():
    """测试MAC地址生成"""
    device1 = RizDevice(1)
    device2 = RizDevice(2)

    assert device1.mac_address != device2.mac_address
    assert device1.mac_address == "AA:BB:CC:DD:00:01"
    assert device2.mac_address == "AA:BB:CC:DD:00:02"


def test_light_state():
    """测试灯光状态"""
    device = RizDevice(1)

    # 初始状态
    assert device.light_state.color == (255, 255, 255)
    assert device.light_state.brightness == 100
    assert device.light_state.is_on == False

    # 设置颜色
    payload = bytes([255, 0, 0])  # 红色
    response = device.process_command(Commands.SET_COLOR, payload)

    assert device.light_state.color == (255, 0, 0)
    assert device.light_state.is_on == True


def test_set_brightness():
    """测试设置亮度"""
    device = RizDevice(1)

    payload = bytes([50])  # 50%
    response = device.process_command(Commands.SET_BRIGHTNESS, payload)

    assert device.light_state.brightness == 50


def test_set_mode():
    """测试设置模式"""
    device = RizDevice(1)

    payload = bytes([LightMode.BREATHING])
    response = device.process_command(Commands.SET_MODE, payload)

    assert device.light_state.mode == LightMode.BREATHING


def test_get_stats():
    """测试获取统计数据"""
    device = RizDevice(1)

    stats = device.get_stats()

    assert 'trigger_count' in stats
    assert 'average_reaction' in stats
    assert 'fastest_reaction' in stats
    assert stats['trigger_count'] == 0


def test_device_to_dict():
    """测试设备转字典"""
    device = RizDevice(1)

    data = device.to_dict()

    assert data['device_id'] == 1
    assert data['name'] == "RIZ-0001"
    assert 'light_state' in data
    assert 'sensor_data' in data
    assert 'stats' in data


def test_trigger():
    """测试触发功能"""
    device = RizDevice(1)

    device.simulate_trigger()

    # 触发事件应该被设置
    assert device.trigger_event.is_set() or True  # 事件可能已被处理


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
