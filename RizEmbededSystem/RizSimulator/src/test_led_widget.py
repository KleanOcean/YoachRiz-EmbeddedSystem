"""
测试LED显示组件
"""

import sys
from PyQt6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QPushButton
from PyQt6.QtCore import QTimer

# 添加src到路径
sys.path.insert(0, '.')

from models import RizDevice
from device_core import DeviceController
from widgets.device_widget import DeviceWidget
from constants import *
from logger import get_logger

logger = get_logger("TestLED")


class TestWindow(QMainWindow):
    """测试窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("RizSimulator - LED Widget Test")
        self.setGeometry(100, 100, 800, 400)

        # 创建设备
        self.device1 = RizDevice(device_id=1)
        self.device2 = RizDevice(device_id=2)
        self.device3 = RizDevice(device_id=3)

        self.controller1 = DeviceController(self.device1)
        self.controller2 = DeviceController(self.device2)
        self.controller3 = DeviceController(self.device3)

        # 设置回调
        self.controller1.light_change_callback = lambda state: self.widget1.update_display()
        self.controller2.light_change_callback = lambda state: self.widget2.update_display()
        self.controller3.light_change_callback = lambda state: self.widget3.update_display()

        self._init_ui()

        # 更新定时器
        self.timer = QTimer()
        self.timer.timeout.connect(self._update)
        self.timer.start(16)  # 60fps

    def _init_ui(self):
        """初始化UI"""
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)

        # 设备显示区
        device_layout = QHBoxLayout()

        self.widget1 = DeviceWidget(self.device1)
        self.widget2 = DeviceWidget(self.device2)
        self.widget3 = DeviceWidget(self.device3)

        self.widget1.trigger_clicked.connect(self._on_trigger)
        self.widget2.trigger_clicked.connect(self._on_trigger)
        self.widget3.trigger_clicked.connect(self._on_trigger)

        device_layout.addWidget(self.widget1)
        device_layout.addWidget(self.widget2)
        device_layout.addWidget(self.widget3)
        device_layout.addStretch()

        main_layout.addLayout(device_layout)

        # 控制按钮
        control_layout = QHBoxLayout()

        btn_manual = QPushButton("Manual Mode (Red)")
        btn_manual.clicked.connect(lambda: self._set_modes(MANUAL_MODE))
        control_layout.addWidget(btn_manual)

        btn_random = QPushButton("Random Mode")
        btn_random.clicked.connect(lambda: self._set_modes(RANDOM_MODE))
        control_layout.addWidget(btn_random)

        btn_rhythm = QPushButton("Rhythm Mode (Yellow)")
        btn_rhythm.clicked.connect(lambda: self._set_modes(RHYTHM_MODE))
        control_layout.addWidget(btn_rhythm)

        btn_off = QPushButton("Turn Off")
        btn_off.clicked.connect(self._turn_off_all)
        control_layout.addWidget(btn_off)

        main_layout.addLayout(control_layout)

        logger.info("测试窗口初始化完成")

    def _set_modes(self, mode: int):
        """设置游戏模式"""
        self.device1.able_to_turn_on = True
        self.device2.able_to_turn_on = True
        self.device3.able_to_turn_on = True

        self.controller1.handle_game_mode(mode)
        self.controller2.handle_game_mode(mode)
        self.controller3.handle_game_mode(mode)

        logger.info(f"设置模式: {mode}")

    def _turn_off_all(self):
        """关闭所有灯"""
        self.controller1.turn_light_off()
        self.controller2.turn_light_off()
        self.controller3.turn_light_off()

    def _on_trigger(self, device):
        """触发设备"""
        logger.info(f"触发设备: {device.name}")
        # 模拟TOF检测
        if device.device_id == 1:
            self.controller1.turn_light_off()
        elif device.device_id == 2:
            self.controller2.turn_light_off()
        elif device.device_id == 3:
            self.controller3.turn_light_off()

    def _update(self):
        """更新"""
        self.controller1.update(0.016)
        self.controller2.update(0.016)
        self.controller3.update(0.016)


def main():
    app = QApplication(sys.argv)
    window = TestWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
