"""
测试所有游戏模式
Test All Game Modes
"""

import sys
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QHBoxLayout, QGridLayout,
    QPushButton, QLabel, QSlider, QSpinBox, QGroupBox
)
from PyQt6.QtCore import QTimer, Qt

sys.path.insert(0, '.')

from models import RizDevice
from device_core import DeviceController
from device_manager import DeviceManager
from widgets.device_widget import DeviceWidget
from constants import *
from logger import get_logger

logger = get_logger("TestGameModes")


class GameModeTestWindow(QMainWindow):
    """游戏模式测试窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("RizSimulator - Game Mode Test")
        self.setGeometry(100, 100, 1200, 700)

        # 创建3个设备用于测试
        self.manager = DeviceManager()
        self.devices = [
            self.manager.create_device(),
            self.manager.create_device(),
            self.manager.create_device()
        ]

        self.controllers = [
            self.manager.get_controller(d.device_id) for d in self.devices
        ]

        # 设置回调
        for ctrl in self.controllers:
            ctrl.light_change_callback = self._on_light_change

        self._init_ui()

        # 更新定时器
        self.timer = QTimer()
        self.timer.timeout.connect(self._update)
        self.timer.start(16)  # 60fps

        logger.info("游戏模式测试窗口初始化完成")

    def _init_ui(self):
        """初始化UI"""
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)

        # 左侧：设备显示
        left_widget = self._create_device_display()
        main_layout.addWidget(left_widget)

        # 右侧：控制面板
        right_widget = self._create_control_panel()
        main_layout.addWidget(right_widget)

    def _create_device_display(self) -> QWidget:
        """创建设备显示区域"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        title = QLabel("设备显示")
        title.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(title)

        # 设备行
        device_layout = QHBoxLayout()
        self.device_widgets = []

        for i, device in enumerate(self.devices):
            dw = DeviceWidget(device)
            dw.trigger_clicked.connect(self._on_trigger)
            self.device_widgets.append(dw)
            device_layout.addWidget(dw)

        device_layout.addStretch()
        layout.addLayout(device_layout)
        layout.addStretch()

        return widget

    def _create_control_panel(self) -> QWidget:
        """创建控制面板"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        title = QLabel("游戏模式控制")
        title.setStyleSheet("font-weight: bold; font-size: 16px;")
        layout.addWidget(title)

        # 基础模式组
        basic_group = self._create_basic_modes_group()
        layout.addWidget(basic_group)

        # 高级模式组
        advanced_group = self._create_advanced_modes_group()
        layout.addWidget(advanced_group)

        # 系统模式组
        system_group = self._create_system_modes_group()
        layout.addWidget(system_group)

        # 参数控制组
        param_group = self._create_parameter_group()
        layout.addWidget(param_group)

        layout.addStretch()

        return widget

    def _create_basic_modes_group(self) -> QGroupBox:
        """创建基础模式组"""
        group = QGroupBox("基础游戏模式")
        layout = QGridLayout(group)

        modes = [
            ("Manual Mode", MANUAL_MODE, "深蓝色 (根据process)"),
            ("Random Mode", RANDOM_MODE, "绿/黄/红随机"),
            ("Rhythm Mode", RHYTHM_MODE, "自定义RGB"),
            ("Double Mode", DOUBLE_MODE, "橙色/深蓝"),
        ]

        for i, (name, mode, desc) in enumerate(modes):
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, m=mode: self._set_mode(m))
            layout.addWidget(btn, i, 0)

            label = QLabel(desc)
            label.setStyleSheet("color: gray; font-size: 10px;")
            layout.addWidget(label, i, 1)

        return group

    def _create_advanced_modes_group(self) -> QGroupBox:
        """创建高级模式组"""
        group = QGroupBox("高级模式")
        layout = QVBoxLayout(group)

        # 配置模式
        config_layout = QHBoxLayout()
        config_btn = QPushButton("Config Mode (闪烁)")
        config_btn.clicked.connect(lambda: self._set_mode(CONFIG_MODE))
        config_layout.addWidget(config_btn)

        config_layout.addWidget(QLabel("闪烁次数:"))
        self.config_count_spin = QSpinBox()
        self.config_count_spin.setRange(1, 10)
        self.config_count_spin.setValue(3)
        config_layout.addWidget(self.config_count_spin)
        config_layout.addStretch()

        layout.addLayout(config_layout)

        # 休息模式
        rest_btn = QPushButton("Rest Mode (休息倒计时)")
        rest_btn.clicked.connect(lambda: self._set_mode(RESTTIMESUP_MODE))
        layout.addWidget(rest_btn)

        return group

    def _create_system_modes_group(self) -> QGroupBox:
        """创建系统模式组"""
        group = QGroupBox("系统模式")
        layout = QGridLayout(group)

        modes = [
            ("Opening Mode", OPENING_MODE, "蓝色采集基线"),
            ("Closing Mode", CLOSING_MODE, "红色闪烁"),
            ("Terminate Mode", TERMINATE_MODE, "关闭灯光"),
        ]

        for i, (name, mode, desc) in enumerate(modes):
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, m=mode: self._set_mode(m))
            layout.addWidget(btn, i, 0)

            label = QLabel(desc)
            label.setStyleSheet("color: gray; font-size: 10px;")
            layout.addWidget(label, i, 1)

        return group

    def _create_parameter_group(self) -> QGroupBox:
        """创建参数控制组"""
        group = QGroupBox("参数设置")
        layout = QVBoxLayout(group)

        # Process参数
        process_layout = QHBoxLayout()
        process_layout.addWidget(QLabel("Process (影响Manual颜色):"))
        self.process_slider = QSlider(Qt.Orientation.Horizontal)
        self.process_slider.setRange(0, 100)
        self.process_slider.setValue(30)
        self.process_slider.valueChanged.connect(self._on_process_changed)
        process_layout.addWidget(self.process_slider)

        self.process_label = QLabel("30")
        self.process_label.setMinimumWidth(30)
        process_layout.addWidget(self.process_label)

        layout.addLayout(process_layout)

        # Double Mode Index
        double_layout = QHBoxLayout()
        double_layout.addWidget(QLabel("Double Mode Index:"))
        self.double_index_spin = QSpinBox()
        self.double_index_spin.setRange(0, 1)
        self.double_index_spin.setValue(0)
        double_layout.addWidget(self.double_index_spin)
        double_layout.addWidget(QLabel("0=橙色, 1=深蓝"))
        double_layout.addStretch()

        layout.addLayout(double_layout)

        # Rhythm Mode RGB
        layout.addWidget(QLabel("Rhythm Mode RGB:"))
        rgb_layout = QHBoxLayout()

        self.rgb_sliders = []
        for color_name in ["R", "G", "B"]:
            rgb_layout.addWidget(QLabel(f"{color_name}:"))
            slider = QSlider(Qt.Orientation.Horizontal)
            slider.setRange(0, 255)
            slider.setValue(255 if color_name == "R" else 140 if color_name == "G" else 0)
            rgb_layout.addWidget(slider)
            self.rgb_sliders.append(slider)

        layout.addLayout(rgb_layout)

        # 动画按钮
        anim_layout = QHBoxLayout()
        init_btn = QPushButton("启动初始化动画")
        init_btn.clicked.connect(self._start_init_animation)
        anim_layout.addWidget(init_btn)

        connected_btn = QPushButton("启动连接动画")
        connected_btn.clicked.connect(self._start_connected_animation)
        anim_layout.addWidget(connected_btn)

        layout.addLayout(anim_layout)

        return group

    def _set_mode(self, mode: int):
        """设置游戏模式"""
        # 更新所有设备配置
        for device in self.devices:
            device.able_to_turn_on = True
            device.config.game_mode = mode
            device.config.process = self.process_slider.value()
            device.config.double_mode_index = self.double_index_spin.value()
            device.config.config_blink_count = self.config_count_spin.value()
            device.config.red_value = self.rgb_sliders[0].value()
            device.config.green_value = self.rgb_sliders[1].value()
            device.config.blue_value = self.rgb_sliders[2].value()

        # 执行模式
        for controller in self.controllers:
            controller.handle_game_mode(mode)

        logger.info(f"设置所有设备为模式: {mode}")

    def _on_process_changed(self, value: int):
        """Process值变化"""
        self.process_label.setText(str(value))

    def _start_init_animation(self):
        """启动初始化动画"""
        for controller in self.controllers:
            controller.start_init_animation()

    def _start_connected_animation(self):
        """启动连接动画"""
        for controller in self.controllers:
            controller.start_connected_animation()

    def _on_trigger(self, device):
        """触发设备"""
        logger.info(f"触发设备: {device.name}")
        # 重新启用并设置为当前模式
        device.able_to_turn_on = True
        controller = self.manager.get_controller(device.device_id)
        controller.handle_game_mode(device.config.game_mode)

    def _on_light_change(self, led_state):
        """灯光变化回调"""
        # 更新所有设备显示
        for dw in self.device_widgets:
            dw.update_display()

    def _update(self):
        """更新"""
        self.manager.update_all(0.016)

        # 更新设备显示
        for dw in self.device_widgets:
            dw.update_display()


def main():
    app = QApplication(sys.argv)
    window = GameModeTestWindow()
    window.show()

    logger.info("=" * 60)
    logger.info("游戏模式测试程序启动")
    logger.info("=" * 60)
    logger.info("📋 可用模式:")
    logger.info("  - Manual Mode: 深蓝/天蓝/淡蓝 (根据Process值)")
    logger.info("  - Random Mode: 随机绿/黄/红")
    logger.info("  - Rhythm Mode: 自定义RGB颜色")
    logger.info("  - Double Mode: 橙色或深蓝 (根据Index)")
    logger.info("  - Config Mode: 白光闪烁N次")
    logger.info("  - Rest Mode: Tennis绿倒计时")
    logger.info("  - Opening Mode: 蓝色基线采集")
    logger.info("  - Closing Mode: 红色闪烁")
    logger.info("  - Terminate Mode: 关闭灯光")
    logger.info("=" * 60)

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
