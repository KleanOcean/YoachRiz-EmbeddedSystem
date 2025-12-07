"""
Statistics Panel Widget
统计面板组件
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QGroupBox, QFrame
)
from PyQt6.QtCore import Qt

from device_manager import DeviceManager
from logger import get_logger

logger = get_logger("StatisticsPanel")


class StatisticsPanelWidget(QWidget):
    """统计面板组件"""

    def __init__(self, device_manager: DeviceManager, parent=None):
        super().__init__(parent)
        self.device_manager = device_manager

        self._init_ui()

    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)

        # 标题
        title = QLabel("统计信息")
        title.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(title)

        # 总体统计
        overall_group = self._create_overall_stats()
        layout.addWidget(overall_group)

        # 设备统计
        device_group = self._create_device_stats()
        layout.addWidget(device_group)

        layout.addStretch()

    def _create_overall_stats(self) -> QGroupBox:
        """创建总体统计组"""
        group = QGroupBox("总体统计")
        layout = QGridLayout(group)

        # 设备数量
        layout.addWidget(QLabel("设备总数:"), 0, 0)
        self.total_devices_label = QLabel("0")
        self.total_devices_label.setStyleSheet("font-weight: bold; color: blue;")
        layout.addWidget(self.total_devices_label, 0, 1)

        # 活跃设备
        layout.addWidget(QLabel("活跃设备:"), 1, 0)
        self.active_devices_label = QLabel("0")
        self.active_devices_label.setStyleSheet("font-weight: bold; color: green;")
        layout.addWidget(self.active_devices_label, 1, 1)

        # 总触发次数
        layout.addWidget(QLabel("总触发次数:"), 2, 0)
        self.total_triggers_label = QLabel("0")
        self.total_triggers_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.total_triggers_label, 2, 1)

        # 平均反应时间
        layout.addWidget(QLabel("平均反应时间:"), 3, 0)
        self.avg_response_label = QLabel("0 ms")
        self.avg_response_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(self.avg_response_label, 3, 1)

        return group

    def _create_device_stats(self) -> QGroupBox:
        """创建设备统计组"""
        group = QGroupBox("设备详情")
        layout = QVBoxLayout(group)

        # 状态统计
        status_layout = QGridLayout()

        status_layout.addWidget(QLabel("已连接:"), 0, 0)
        self.connected_count_label = QLabel("0")
        self.connected_count_label.setStyleSheet("color: green;")
        status_layout.addWidget(self.connected_count_label, 0, 1)

        status_layout.addWidget(QLabel("广播中:"), 1, 0)
        self.advertising_count_label = QLabel("0")
        self.advertising_count_label.setStyleSheet("color: blue;")
        status_layout.addWidget(self.advertising_count_label, 1, 1)

        status_layout.addWidget(QLabel("未连接:"), 2, 0)
        self.disconnected_count_label = QLabel("0")
        self.disconnected_count_label.setStyleSheet("color: gray;")
        status_layout.addWidget(self.disconnected_count_label, 2, 1)

        layout.addLayout(status_layout)

        # 分隔线
        line = QFrame()
        line.setFrameShape(QFrame.Shape.HLine)
        line.setFrameShadow(QFrame.Shadow.Sunken)
        layout.addWidget(line)

        # 模式统计
        mode_layout = QGridLayout()
        mode_layout.addWidget(QLabel("当前模式分布:"), 0, 0, 1, 2)

        self.mode_labels = {}
        mode_names = [
            ("Manual", 1),
            ("Random", 2),
            ("Rhythm", 5),
            ("Double", 4),
        ]

        for i, (name, mode_id) in enumerate(mode_names):
            mode_layout.addWidget(QLabel(f"{name}:"), i + 1, 0)
            label = QLabel("0")
            self.mode_labels[mode_id] = label
            mode_layout.addWidget(label, i + 1, 1)

        layout.addLayout(mode_layout)

        return group

    def update_statistics(self):
        """更新统计信息"""
        summary = self.device_manager.get_summary()

        # 总体统计
        self.total_devices_label.setText(str(summary["total_devices"]))
        self.active_devices_label.setText(str(summary["active_devices"]))
        self.total_triggers_label.setText(str(summary["total_triggers"]))

        avg_response = summary["average_response_time"]
        self.avg_response_label.setText(f"{avg_response:.1f} ms")

        # 连接状态统计
        state_counts = summary["by_state"]
        self.connected_count_label.setText(str(state_counts.get(2, 0)))  # STATE_CONNECTED
        self.advertising_count_label.setText(str(state_counts.get(1, 0)))  # STATE_ADVERTISING
        self.disconnected_count_label.setText(str(state_counts.get(0, 0)))  # STATE_DISCONNECTED

        # 模式统计
        mode_counts = summary["by_mode"]
        for mode_id, label in self.mode_labels.items():
            count = mode_counts.get(mode_id, 0)
            label.setText(str(count))
