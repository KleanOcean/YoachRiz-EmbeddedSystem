"""
测试TOF传感器模拟
"""

import sys
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QHBoxLayout, QSplitter
)
from PyQt6.QtCore import QTimer

sys.path.insert(0, '.')

from models import RizDevice
from device_core import DeviceController, TOFSensorController
from device_manager import DeviceManager
from widgets.device_widget import DeviceWidget
from widgets.tof_control import TOFControlWidget, TOFVisualizationWidget
from constants import *
from logger import get_logger

logger = get_logger("TestTOF")


class TOFTestWindow(QMainWindow):
    """TOF传感器测试窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("RizSimulator - TOF Sensor Test")
        self.setGeometry(100, 100, 1000, 600)

        # 创建设备管理器
        self.manager = DeviceManager()
        self.device = self.manager.create_device()
        self.controller = self.manager.get_controller(self.device.device_id)
        self.tof_controller = self.manager.get_tof_controller(self.device.device_id)

        # 设置回调
        self.controller.light_change_callback = self._on_light_change
        self.tof_controller.detection_callback = self._on_detection

        self._init_ui()

        # 更新定时器
        self.timer = QTimer()
        self.timer.timeout.connect(self._update)
        self.timer.start(16)  # 60fps

        logger.info("TOF测试窗口初始化完成")

    def _init_ui(self):
        """初始化UI"""
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)

        # 左侧：设备显示
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)

        self.device_widget = DeviceWidget(self.device)
        self.device_widget.trigger_clicked.connect(self._on_trigger)
        left_layout.addWidget(self.device_widget)

        # TOF可视化
        self.tof_viz = TOFVisualizationWidget()
        left_layout.addWidget(self.tof_viz)

        left_layout.addStretch()

        # 右侧：TOF控制面板
        self.tof_control = TOFControlWidget()
        self.tof_control.set_device(self.device)
        self.tof_control.distance_changed.connect(self._on_distance_changed)
        self.tof_control.simulate_touch.connect(self._on_simulate_touch)

        # 分割器
        splitter = QSplitter()
        splitter.addWidget(left_widget)
        splitter.addWidget(self.tof_control)
        splitter.setSizes([600, 400])

        main_layout.addWidget(splitter)

        # 初始设置为手动模式
        self.device.able_to_turn_on = True
        self.controller.handle_game_mode(MANUAL_MODE)

    def _update(self):
        """更新"""
        # 更新设备
        self.manager.update_all(0.016)

        # 更新显示
        self.device_widget.update_display()
        self.tof_control.update_display()

        # 更新TOF可视化
        tof = self.device.tof_state
        threshold = int(tof.baseline * 1.04) if tof.baseline > 0 else AMPLITUDE_THRESHOLD
        self.tof_viz.set_values(
            tof.distance,
            tof.amplitude,
            threshold,
            tof.detection_active
        )

    def _on_light_change(self, led_state):
        """灯光变化"""
        self.device_widget.update_display()

    def _on_detection(self):
        """检测到物体"""
        logger.info("🎯 TOF检测到物体！")
        self.controller.turn_light_off()

    def _on_distance_changed(self, distance: int):
        """距离变化"""
        self.tof_controller.update_distance(distance)

    def _on_simulate_touch(self):
        """模拟触碰"""
        logger.info("🖐️ 模拟手部触碰")
        self.tof_controller.simulate_touch()

    def _on_trigger(self, device):
        """手动触发"""
        logger.info("触发按钮点击 - 重新启动")
        self.device.able_to_turn_on = True
        self.controller.handle_game_mode(MANUAL_MODE)


def main():
    app = QApplication(sys.argv)
    window = TOFTestWindow()
    window.show()

    logger.info("=" * 60)
    logger.info("TOF传感器测试程序启动")
    logger.info("=" * 60)
    logger.info("📋 使用说明:")
    logger.info("1. 调整距离滑块改变传感器距离")
    logger.info("2. 距离 < 300mm 时振幅会自动升高")
    logger.info("3. 振幅 > 阈值(5000) 并连续3次时触发检测")
    logger.info("4. 点击'模拟手部触碰'快速触发检测")
    logger.info("5. 检测后进入400ms冷却期")
    logger.info("6. 点击'触发'按钮重新点亮LED")
    logger.info("=" * 60)

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
