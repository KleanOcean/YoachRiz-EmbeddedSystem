"""
TOF Sensor Control Widget
TOF传感器控制面板
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QSlider, QPushButton, QGroupBox, QGridLayout
)
from PyQt6.QtCore import Qt, pyqtSignal

from models import RizDevice
from constants import AMPLITUDE_THRESHOLD, COOLDOWN_DURATION


class TOFControlWidget(QWidget):
    """TOF传感器控制面板"""

    distance_changed = pyqtSignal(int)  # 距离变化
    simulate_touch = pyqtSignal()  # 模拟触碰

    def __init__(self, parent=None):
        super().__init__(parent)
        self.device = None
        self._init_ui()

    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)

        # 标题
        title = QLabel("TOF激光传感器控制")
        title.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(title)

        # 距离控制组
        distance_group = self._create_distance_group()
        layout.addWidget(distance_group)

        # 传感器状态组
        status_group = self._create_status_group()
        layout.addWidget(status_group)

        # 控制按钮组
        button_group = self._create_button_group()
        layout.addWidget(button_group)

        layout.addStretch()

    def _create_distance_group(self) -> QGroupBox:
        """创建距离控制组"""
        group = QGroupBox("距离控制")
        layout = QVBoxLayout(group)

        # 距离滑块
        slider_layout = QHBoxLayout()
        slider_layout.addWidget(QLabel("距离:"))

        self.distance_slider = QSlider(Qt.Orientation.Horizontal)
        self.distance_slider.setRange(30, 2000)  # 30-2000mm
        self.distance_slider.setValue(1000)
        self.distance_slider.setTickPosition(QSlider.TickPosition.TicksBelow)
        self.distance_slider.setTickInterval(500)
        self.distance_slider.valueChanged.connect(self._on_distance_changed)
        slider_layout.addWidget(self.distance_slider)

        self.distance_value_label = QLabel("1000 mm")
        self.distance_value_label.setMinimumWidth(70)
        self.distance_value_label.setStyleSheet("font-weight: bold;")
        slider_layout.addWidget(self.distance_value_label)

        layout.addLayout(slider_layout)

        # 预设距离按钮
        preset_layout = QHBoxLayout()
        preset_layout.addWidget(QLabel("快速设置:"))

        presets = [
            ("远", 1500),
            ("中", 500),
            ("近", 100),
            ("超近", 50)
        ]

        for name, value in presets:
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, v=value: self.distance_slider.setValue(v))
            preset_layout.addWidget(btn)

        preset_layout.addStretch()
        layout.addLayout(preset_layout)

        return group

    def _create_status_group(self) -> QGroupBox:
        """创建状态显示组"""
        group = QGroupBox("传感器状态")
        layout = QGridLayout(group)

        # 振幅
        layout.addWidget(QLabel("振幅:"), 0, 0)
        self.amplitude_label = QLabel("100")
        self.amplitude_label.setStyleSheet("font-weight: bold; color: blue;")
        layout.addWidget(self.amplitude_label, 0, 1)

        # 基线
        layout.addWidget(QLabel("基线:"), 1, 0)
        self.baseline_label = QLabel("0")
        self.baseline_label.setStyleSheet("font-weight: bold; color: green;")
        layout.addWidget(self.baseline_label, 1, 1)

        # 阈值
        layout.addWidget(QLabel("阈值:"), 2, 0)
        self.threshold_label = QLabel(str(AMPLITUDE_THRESHOLD))
        self.threshold_label.setStyleSheet("font-weight: bold; color: orange;")
        layout.addWidget(self.threshold_label, 2, 1)

        # 连续检测次数
        layout.addWidget(QLabel("连续检测:"), 3, 0)
        self.consecutive_label = QLabel("0 / 3")
        self.consecutive_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.consecutive_label, 3, 1)

        # 检测状态
        layout.addWidget(QLabel("状态:"), 4, 0)
        self.detection_status_label = QLabel("⚫ 待机")
        self.detection_status_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.detection_status_label, 4, 1)

        return group

    def _create_button_group(self) -> QWidget:
        """创建按钮组"""
        widget = QWidget()
        layout = QVBoxLayout(widget)

        # 模拟触碰按钮
        self.touch_btn = QPushButton("🖐️ 模拟手部触碰")
        self.touch_btn.setStyleSheet("""
            QPushButton {
                background-color: #4CAF50;
                color: white;
                font-weight: bold;
                padding: 10px;
                border-radius: 5px;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:disabled {
                background-color: #cccccc;
            }
        """)
        self.touch_btn.clicked.connect(self._on_simulate_touch)
        layout.addWidget(self.touch_btn)

        # 重置基线按钮
        reset_btn = QPushButton("重置基线")
        reset_btn.clicked.connect(self._on_reset_baseline)
        layout.addWidget(reset_btn)

        return widget

    def set_device(self, device: RizDevice):
        """设置当前设备"""
        self.device = device
        self.update_display()

    def update_display(self):
        """更新显示"""
        if not self.device:
            return

        tof = self.device.tof_state

        # 更新距离（不触发信号）
        self.distance_slider.blockSignals(True)
        self.distance_slider.setValue(tof.distance)
        self.distance_slider.blockSignals(False)
        self.distance_value_label.setText(f"{tof.distance} mm")

        # 更新振幅
        self.amplitude_label.setText(str(tof.amplitude))
        if tof.amplitude > AMPLITUDE_THRESHOLD:
            self.amplitude_label.setStyleSheet("font-weight: bold; color: red;")
        else:
            self.amplitude_label.setStyleSheet("font-weight: bold; color: blue;")

        # 更新基线
        self.baseline_label.setText(str(tof.baseline))

        # 更新阈值
        threshold = int(tof.baseline * 1.04) if tof.baseline > 0 else AMPLITUDE_THRESHOLD
        self.threshold_label.setText(str(threshold))

        # 更新连续检测
        self.consecutive_label.setText(f"{tof.consecutive_detections} / 3")
        if tof.consecutive_detections >= 2:
            self.consecutive_label.setStyleSheet("font-weight: bold; color: red;")
        elif tof.consecutive_detections >= 1:
            self.consecutive_label.setStyleSheet("font-weight: bold; color: orange;")
        else:
            self.consecutive_label.setStyleSheet("font-weight: bold; color: black;")

        # 更新检测状态
        if tof.is_cooldown:
            self.detection_status_label.setText("🟠 冷却中")
            self.detection_status_label.setStyleSheet("font-weight: bold; color: orange;")
            self.touch_btn.setEnabled(False)
        elif tof.detection_active:
            self.detection_status_label.setText("🟢 检测中")
            self.detection_status_label.setStyleSheet("font-weight: bold; color: green;")
            self.touch_btn.setEnabled(True)
        else:
            self.detection_status_label.setText("⚫ 待机")
            self.detection_status_label.setStyleSheet("font-weight: bold; color: gray;")
            self.touch_btn.setEnabled(False)

    def _on_distance_changed(self, value: int):
        """距离变化"""
        self.distance_value_label.setText(f"{value} mm")
        self.distance_changed.emit(value)

    def _on_simulate_touch(self):
        """模拟触碰"""
        self.simulate_touch.emit()

    def _on_reset_baseline(self):
        """重置基线"""
        if self.device:
            self.device.tof_state.baseline_history.clear()
            self.device.tof_state.baseline = 0
            self.update_display()


class TOFVisualizationWidget(QWidget):
    """TOF传感器可视化组件"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(100)
        self.distance = 1000
        self.amplitude = 100
        self.threshold = AMPLITUDE_THRESHOLD
        self.is_detecting = False

    def set_values(self, distance: int, amplitude: int, threshold: int, detecting: bool):
        """设置值"""
        self.distance = distance
        self.amplitude = amplitude
        self.threshold = threshold
        self.is_detecting = detecting
        self.update()

    def paintEvent(self, event):
        """绘制可视化"""
        from PyQt6.QtGui import QPainter, QColor, QPen, QBrush
        from PyQt6.QtCore import QRectF

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        rect = self.rect()
        width = rect.width()
        height = rect.height()

        # 绘制背景
        painter.fillRect(rect, QColor(240, 240, 240))

        # 绘制传感器（左侧）
        sensor_rect = QRectF(10, height/2 - 15, 30, 30)
        painter.setBrush(QBrush(QColor(100, 100, 100)))
        painter.setPen(QPen(QColor(50, 50, 50), 2))
        painter.drawRect(sensor_rect)

        # 绘制激光束
        beam_width = max(10, int((2000 - self.distance) / 2000 * 50))
        beam_color = QColor(255, 0, 0, 100) if self.is_detecting else QColor(255, 0, 0, 50)
        painter.setBrush(QBrush(beam_color))
        painter.setPen(Qt.PenStyle.NoPen)

        beam_x = 45
        beam_y = height/2 - beam_width/2
        beam_length = min(width - 150, int((self.distance / 2000) * (width - 150)))
        painter.drawRect(QRectF(beam_x, beam_y, beam_length, beam_width))

        # 绘制物体（手）
        hand_x = beam_x + beam_length
        hand_size = 40
        painter.setBrush(QBrush(QColor(255, 200, 150)))
        painter.setPen(QPen(QColor(200, 150, 100), 2))
        painter.drawEllipse(QRectF(hand_x - hand_size/2, height/2 - hand_size/2, hand_size, hand_size))

        # 绘制文字信息
        painter.setPen(QPen(QColor(0, 0, 0)))
        info_x = width - 140
        painter.drawText(info_x, 20, f"距离: {self.distance} mm")
        painter.drawText(info_x, 40, f"振幅: {self.amplitude}")

        # 绘制振幅条
        bar_width = 120
        bar_height = 20
        bar_x = info_x
        bar_y = 50

        # 背景条
        painter.setBrush(QBrush(QColor(220, 220, 220)))
        painter.drawRect(QRectF(bar_x, bar_y, bar_width, bar_height))

        # 振幅条
        if self.amplitude > 0:
            amp_ratio = min(1.0, self.amplitude / 6000)
            amp_width = bar_width * amp_ratio
            amp_color = QColor(255, 0, 0) if self.amplitude > self.threshold else QColor(0, 255, 0)
            painter.setBrush(QBrush(amp_color))
            painter.drawRect(QRectF(bar_x, bar_y, amp_width, bar_height))

        # 阈值线
        threshold_ratio = min(1.0, self.threshold / 6000)
        threshold_x = bar_x + bar_width * threshold_ratio
        painter.setPen(QPen(QColor(255, 165, 0), 2))
        painter.drawLine(int(threshold_x), bar_y, int(threshold_x), bar_y + bar_height)
