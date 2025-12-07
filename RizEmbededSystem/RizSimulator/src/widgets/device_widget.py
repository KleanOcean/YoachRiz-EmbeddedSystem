"""
Device Widget
单个设备显示组件
"""

from PyQt6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton
from PyQt6.QtCore import Qt, pyqtSignal

from models import RizDevice
from constants import STATE_CONNECTED, STATE_ADVERTISING, STATE_DISCONNECTED
from widgets.led_ring import LEDRingWidget


class DeviceWidget(QWidget):
    """单个设备显示组件"""

    clicked = pyqtSignal(object)  # 点击设备
    trigger_clicked = pyqtSignal(object)  # 点击触发按钮

    def __init__(self, device: RizDevice, parent=None):
        super().__init__(parent)
        self.device = device
        self.is_selected = False

        self.setFixedSize(180, 280)
        self._init_ui()

    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(5)

        # 设备名称
        self.name_label = QLabel(self.device.name)
        self.name_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.name_label.setStyleSheet("font-weight: bold; font-size: 12px;")
        layout.addWidget(self.name_label)

        # LED圆环显示
        self.led_widget = LEDRingWidget()
        self.led_widget.setFixedSize(160, 160)
        self.led_widget.clicked.connect(self._on_led_clicked)
        layout.addWidget(self.led_widget, alignment=Qt.AlignmentFlag.AlignCenter)

        # 状态信息
        info_layout = QVBoxLayout()
        info_layout.setSpacing(2)

        # 连接状态
        self.status_label = QLabel("未连接")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_label.setStyleSheet("font-size: 10px;")
        info_layout.addWidget(self.status_label)

        # TOF状态
        self.tof_label = QLabel("TOF: 待机")
        self.tof_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.tof_label.setStyleSheet("font-size: 9px; color: gray;")
        info_layout.addWidget(self.tof_label)

        layout.addLayout(info_layout)

        # 触发按钮
        self.trigger_btn = QPushButton("触发")
        self.trigger_btn.setFixedHeight(25)
        self.trigger_btn.clicked.connect(self._on_trigger)
        layout.addWidget(self.trigger_btn)

        # 初始更新
        self.update_display()

    def update_display(self):
        """更新显示"""
        # 更新LED
        if self.device.led_state.is_on:
            # 使用内圈第一个LED的颜色（简化版）
            if self.device.led_state.inner_ring:
                color = self.device.led_state.inner_ring[0]
                self.led_widget.set_all_leds(color)
        else:
            self.led_widget.clear()

        # 更新亮度
        self.led_widget.set_brightness(self.device.led_state.brightness)

        # 更新连接状态
        self.update_status()

        # 更新TOF状态
        self.update_tof_status()

    def update_status(self):
        """更新连接状态"""
        state_text = {
            STATE_DISCONNECTED: "未连接",
            STATE_ADVERTISING: "广播中",
            STATE_CONNECTED: "已连接 ✓",
        }

        state_colors = {
            STATE_DISCONNECTED: "color: gray;",
            STATE_ADVERTISING: "color: #0080FF; font-weight: bold;",
            STATE_CONNECTED: "color: #00FF00; font-weight: bold;",
        }

        text = state_text.get(self.device.connection_state, "未知")
        color = state_colors.get(self.device.connection_state, "")

        self.status_label.setText(text)
        self.status_label.setStyleSheet(f"font-size: 10px; {color}")

    def update_tof_status(self):
        """更新TOF状态"""
        if self.device.tof_state.is_cooldown:
            text = "TOF: 冷却中"
            color = "color: orange;"
        elif self.device.tof_state.detection_active:
            text = f"TOF: 检测中 ({self.device.tof_state.distance}mm)"
            color = "color: green;"
        else:
            text = f"TOF: 待机 ({self.device.tof_state.distance}mm)"
            color = "color: gray;"

        self.tof_label.setText(text)
        self.tof_label.setStyleSheet(f"font-size: 9px; {color}")

    def set_selected(self, selected: bool):
        """设置选中状态"""
        self.is_selected = selected
        if selected:
            self.setStyleSheet("""
                DeviceWidget {
                    background-color: #E0E0FF;
                    border: 2px solid #0000FF;
                    border-radius: 5px;
                }
            """)
        else:
            self.setStyleSheet("""
                DeviceWidget {
                    background-color: #F5F5F5;
                    border: 1px solid #CCCCCC;
                    border-radius: 5px;
                }
            """)

    def _on_led_clicked(self):
        """LED区域点击"""
        # 如果LED是点亮状态，模拟TOF触发（关灯）
        if self.device.led_state.is_on:
            self.trigger_clicked.emit(self.device)
        else:
            # LED未点亮时，正常选择设备
            self.clicked.emit(self.device)

    def _on_trigger(self):
        """触发按钮点击"""
        self.trigger_clicked.emit(self.device)

    def mousePressEvent(self, event):
        """鼠标点击事件"""
        self.clicked.emit(self.device)
        super().mousePressEvent(event)
