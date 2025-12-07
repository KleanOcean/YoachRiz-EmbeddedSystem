"""
Control Panel Widget
控制面板组件
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QPushButton, QSlider, QSpinBox, QGroupBox,
    QTabWidget, QFrame
)
from PyQt6.QtCore import Qt, pyqtSignal

from device_manager import DeviceManager
from gui.ble_panel import BLEControlPanel
from constants import *
from logger import get_logger

logger = get_logger("ControlPanel")


class ControlPanelWidget(QWidget):
    """控制面板组件"""

    mode_changed = pyqtSignal(int, dict)  # 模式变化: (mode, params)
    animation_requested = pyqtSignal(str)  # 动画请求: animation_type

    def __init__(self, device_manager: DeviceManager, parent=None):
        super().__init__(parent)
        self.device_manager = device_manager
        self.selected_devices = []

        self._init_ui()

    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)

        # 标题
        title = QLabel("控制面板")
        title.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(title)

        # 选中设备信息
        self.selection_label = QLabel("未选中设备")
        self.selection_label.setStyleSheet("color: gray;")
        layout.addWidget(self.selection_label)

        # Tab页
        tabs = QTabWidget()
        tabs.addTab(self._create_game_modes_tab(), "游戏模式")
        tabs.addTab(self._create_parameters_tab(), "参数设置")
        tabs.addTab(self._create_animations_tab(), "动画特效")

        # BLE通信标签页
        self.ble_panel = BLEControlPanel(self.device_manager)
        tabs.addTab(self.ble_panel, "BLE通信")

        layout.addWidget(tabs)

    def _create_game_modes_tab(self) -> QWidget:
        """创建游戏模式标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # 基础模式
        basic_group = QGroupBox("基础模式")
        basic_layout = QGridLayout(basic_group)

        basic_modes = [
            ("Manual", MANUAL_MODE, "手动模式 - 蓝色"),
            ("Random", RANDOM_MODE, "随机模式 - 绿/黄/红"),
            ("Rhythm", RHYTHM_MODE, "节奏模式 - 自定义RGB"),
            ("Double", DOUBLE_MODE, "双击模式 - 橙/蓝"),
        ]

        for i, (name, mode, desc) in enumerate(basic_modes):
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, m=mode: self._apply_mode(m))
            basic_layout.addWidget(btn, i // 2, i % 2)

        layout.addWidget(basic_group)

        # 系统模式
        system_group = QGroupBox("系统模式")
        system_layout = QGridLayout(system_group)

        system_modes = [
            ("Opening", OPENING_MODE, "开启"),
            ("Closing", CLOSING_MODE, "关闭"),
            ("Terminate", TERMINATE_MODE, "终止"),
            ("Rest", RESTTIMESUP_MODE, "休息"),
            ("Config", CONFIG_MODE, "配置"),
        ]

        for i, (name, mode, desc) in enumerate(system_modes):
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, m=mode: self._apply_mode(m))
            system_layout.addWidget(btn, i // 3, i % 3)

        layout.addWidget(system_group)

        layout.addStretch()

        return widget

    def _create_parameters_tab(self) -> QWidget:
        """创建参数设置标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # Process参数
        process_group = QGroupBox("Process (Manual模式)")
        process_layout = QVBoxLayout(process_group)

        process_slider_layout = QHBoxLayout()
        process_slider_layout.addWidget(QLabel("值:"))

        self.process_slider = QSlider(Qt.Orientation.Horizontal)
        self.process_slider.setRange(0, 100)
        self.process_slider.setValue(30)
        self.process_slider.valueChanged.connect(self._on_process_changed)
        process_slider_layout.addWidget(self.process_slider)

        self.process_label = QLabel("30")
        self.process_label.setMinimumWidth(30)
        self.process_label.setStyleSheet("font-weight: bold;")
        process_slider_layout.addWidget(self.process_label)

        process_layout.addLayout(process_slider_layout)

        # 预设按钮
        preset_layout = QHBoxLayout()
        preset_layout.addWidget(QLabel("预设:"))
        for name, value in [("低", 10), ("中", 40), ("高", 80)]:
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, v=value: self.process_slider.setValue(v))
            preset_layout.addWidget(btn)
        preset_layout.addStretch()

        process_layout.addLayout(preset_layout)
        layout.addWidget(process_group)

        # Double Mode参数
        double_group = QGroupBox("Double Mode")
        double_layout = QHBoxLayout(double_group)
        double_layout.addWidget(QLabel("Index:"))

        self.double_index_spin = QSpinBox()
        self.double_index_spin.setRange(0, 1)
        self.double_index_spin.setValue(0)
        double_layout.addWidget(self.double_index_spin)

        double_layout.addWidget(QLabel("(0=橙色, 1=蓝色)"))
        double_layout.addStretch()

        layout.addWidget(double_group)

        # Rhythm Mode RGB
        rhythm_group = QGroupBox("Rhythm Mode RGB")
        rhythm_layout = QVBoxLayout(rhythm_group)

        self.rgb_sliders = []
        for color_name, default_value in [("R", 255), ("G", 140), ("B", 0)]:
            slider_layout = QHBoxLayout()
            slider_layout.addWidget(QLabel(f"{color_name}:"))

            slider = QSlider(Qt.Orientation.Horizontal)
            slider.setRange(0, 255)
            slider.setValue(default_value)
            slider_layout.addWidget(slider)

            value_label = QLabel(str(default_value))
            value_label.setMinimumWidth(30)
            value_label.setStyleSheet("font-weight: bold;")
            slider.valueChanged.connect(
                lambda v, lbl=value_label: lbl.setText(str(v))
            )
            slider_layout.addWidget(value_label)

            self.rgb_sliders.append(slider)
            rhythm_layout.addLayout(slider_layout)

        # RGB预览
        preview_layout = QHBoxLayout()
        preview_layout.addWidget(QLabel("预览:"))

        self.rgb_preview = QFrame()
        self.rgb_preview.setFixedSize(100, 30)
        self.rgb_preview.setStyleSheet("background-color: rgb(255, 140, 0); border: 1px solid black;")
        preview_layout.addWidget(self.rgb_preview)

        preview_layout.addStretch()
        rhythm_layout.addLayout(preview_layout)

        # 连接RGB滑块到预览更新
        for slider in self.rgb_sliders:
            slider.valueChanged.connect(self._update_rgb_preview)

        layout.addWidget(rhythm_group)

        # Config Mode参数
        config_group = QGroupBox("Config Mode")
        config_layout = QHBoxLayout(config_group)
        config_layout.addWidget(QLabel("闪烁次数:"))

        self.config_blink_spin = QSpinBox()
        self.config_blink_spin.setRange(1, 10)
        self.config_blink_spin.setValue(3)
        config_layout.addWidget(self.config_blink_spin)

        config_layout.addStretch()

        layout.addWidget(config_group)

        layout.addStretch()

        return widget

    def _create_animations_tab(self) -> QWidget:
        """创建动画特效标签页"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # 启动动画
        startup_group = QGroupBox("启动动画")
        startup_layout = QVBoxLayout(startup_group)

        init_btn = QPushButton("🎨 初始化动画 (绿色主题)")
        init_btn.clicked.connect(lambda: self._request_animation("init"))
        startup_layout.addWidget(init_btn)

        desc = QLabel("显示渐变绿色主题，模拟设备启动")
        desc.setStyleSheet("color: gray; font-size: 10px;")
        startup_layout.addWidget(desc)

        layout.addWidget(startup_group)

        # 连接动画
        connected_group = QGroupBox("连接动画")
        connected_layout = QVBoxLayout(connected_group)

        connected_btn = QPushButton("🔗 连接成功动画 (Tennis绿)")
        connected_btn.clicked.connect(lambda: self._request_animation("connected"))
        connected_layout.addWidget(connected_btn)

        desc2 = QLabel("快速闪烁Tennis绿色，表示连接成功")
        desc2.setStyleSheet("color: gray; font-size: 10px;")
        connected_layout.addWidget(desc2)

        layout.addWidget(connected_group)

        layout.addStretch()

        return widget

    def set_selected_devices(self, devices: list):
        """设置选中的设备"""
        self.selected_devices = devices

        if devices:
            self.selection_label.setText(f"已选中 {len(devices)} 个设备")
            self.selection_label.setStyleSheet("color: green; font-weight: bold;")
        else:
            self.selection_label.setText("未选中设备")
            self.selection_label.setStyleSheet("color: gray;")

        # 更新BLE面板
        self.ble_panel.set_selected_devices(devices)

    def _apply_mode(self, mode: int):
        """应用游戏模式"""
        if not self.selected_devices:
            logger.warning("未选中设备")
            return

        # 收集参数
        params = {
            "process": self.process_slider.value(),
            "double_index": self.double_index_spin.value(),
            "rgb": (
                self.rgb_sliders[0].value(),
                self.rgb_sliders[1].value(),
                self.rgb_sliders[2].value()
            ),
            "blink_count": self.config_blink_spin.value(),
        }

        self.mode_changed.emit(mode, params)

    def _request_animation(self, animation_type: str):
        """请求动画"""
        if not self.selected_devices:
            logger.warning("未选中设备")
            return

        self.animation_requested.emit(animation_type)

    def _on_process_changed(self, value: int):
        """Process值变化"""
        self.process_label.setText(str(value))

    def _update_rgb_preview(self):
        """更新RGB预览"""
        r = self.rgb_sliders[0].value()
        g = self.rgb_sliders[1].value()
        b = self.rgb_sliders[2].value()

        self.rgb_preview.setStyleSheet(
            f"background-color: rgb({r}, {g}, {b}); border: 1px solid black;"
        )
