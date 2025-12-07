"""
BLE Control Panel
BLE通信控制面板
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QPushButton, QLineEdit, QTextEdit,
    QGroupBox, QComboBox
)
from PyQt6.QtCore import Qt, pyqtSignal

from device_manager import DeviceManager
from ble.ble_server import BLEMessageParser
from constants import *
from logger import get_logger

logger = get_logger("BLEPanel")


class BLEControlPanel(QWidget):
    """BLE控制面板"""

    def __init__(self, device_manager: DeviceManager, parent=None):
        super().__init__(parent)
        self.device_manager = device_manager
        self.selected_devices = []

        self._init_ui()

    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)

        # 标题
        title = QLabel("BLE通信控制")
        title.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(title)

        # 连接控制组
        conn_group = self._create_connection_group()
        layout.addWidget(conn_group)

        # 消息发送组
        msg_group = self._create_message_group()
        layout.addWidget(msg_group)

        # 快捷命令组
        quick_group = self._create_quick_commands_group()
        layout.addWidget(quick_group)

        # 消息日志
        log_group = self._create_log_group()
        layout.addWidget(log_group)

        layout.addStretch()

    def _create_connection_group(self) -> QGroupBox:
        """创建连接控制组"""
        group = QGroupBox("连接管理")
        layout = QVBoxLayout(group)

        # 连接按钮行
        btn_layout = QHBoxLayout()

        connect_btn = QPushButton("📡 连接选中设备")
        connect_btn.clicked.connect(self._connect_selected)
        btn_layout.addWidget(connect_btn)

        disconnect_btn = QPushButton("🔌 断开选中设备")
        disconnect_btn.clicked.connect(self._disconnect_selected)
        btn_layout.addWidget(disconnect_btn)

        layout.addLayout(btn_layout)

        # 批量操作
        batch_layout = QHBoxLayout()

        connect_all_btn = QPushButton("连接所有")
        connect_all_btn.clicked.connect(self._connect_all)
        batch_layout.addWidget(connect_all_btn)

        disconnect_all_btn = QPushButton("断开所有")
        disconnect_all_btn.clicked.connect(self._disconnect_all)
        batch_layout.addWidget(disconnect_all_btn)

        layout.addLayout(batch_layout)

        # 状态显示
        self.connection_status_label = QLabel("未选中设备")
        self.connection_status_label.setStyleSheet("color: gray; font-size: 10px;")
        layout.addWidget(self.connection_status_label)

        return group

    def _create_message_group(self) -> QGroupBox:
        """创建消息发送组"""
        group = QGroupBox("发送BLE消息")
        layout = QVBoxLayout(group)

        # 消息输入
        input_layout = QHBoxLayout()
        input_layout.addWidget(QLabel("消息:"))

        self.message_input = QLineEdit()
        self.message_input.setPlaceholderText("例如: 5,255,140,0,0,0,1")
        input_layout.addWidget(self.message_input)

        send_btn = QPushButton("发送")
        send_btn.clicked.connect(self._send_message)
        input_layout.addWidget(send_btn)

        layout.addLayout(input_layout)

        # 示例提示
        hint = QLabel("示例: Manual=1, Random=2, Rhythm=5,R,G,B,timer,buzzer,sensor")
        hint.setStyleSheet("color: gray; font-size: 9px;")
        layout.addWidget(hint)

        return group

    def _create_quick_commands_group(self) -> QGroupBox:
        """创建快捷命令组"""
        group = QGroupBox("快捷命令")
        layout = QGridLayout(group)

        quick_commands = [
            ("Manual (1)", "1"),
            ("Random (2)", "2"),
            ("Rhythm Yellow (5)", "5,255,140,0,0,0,1"),
            ("Double Orange (4)", "4,0"),
            ("Opening (11)", "11"),
            ("Closing (12)", "12"),
            ("Terminate (13)", "13"),
            ("Config 3x (config)", "config:3"),
        ]

        for i, (name, cmd) in enumerate(quick_commands):
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, c=cmd: self._send_quick_command(c))
            layout.addWidget(btn, i // 2, i % 2)

        return group

    def _create_log_group(self) -> QGroupBox:
        """创建日志组"""
        group = QGroupBox("BLE消息日志")
        layout = QVBoxLayout(group)

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumHeight(150)
        self.log_text.setStyleSheet("font-family: monospace; font-size: 10px;")
        layout.addWidget(self.log_text)

        # 清除按钮
        clear_btn = QPushButton("清除日志")
        clear_btn.clicked.connect(self.log_text.clear)
        layout.addWidget(clear_btn)

        return group

    def set_selected_devices(self, devices: list):
        """设置选中的设备"""
        self.selected_devices = devices

        if devices:
            count = len(devices)
            connected_count = sum(1 for d in devices if d.connection_state == STATE_CONNECTED)
            self.connection_status_label.setText(
                f"已选中 {count} 个设备 | 已连接: {connected_count}"
            )
            self.connection_status_label.setStyleSheet("color: green; font-weight: bold;")
        else:
            self.connection_status_label.setText("未选中设备")
            self.connection_status_label.setStyleSheet("color: gray;")

    def _connect_selected(self):
        """连接选中设备"""
        if not self.selected_devices:
            self._log("❌ 未选中任何设备")
            return

        count = 0
        for device in self.selected_devices:
            if device.connection_state != STATE_CONNECTED:
                self.device_manager.connect_device(device.device_id)
                count += 1

        self._log(f"✅ 连接 {count} 个设备")

    def _disconnect_selected(self):
        """断开选中设备"""
        if not self.selected_devices:
            self._log("❌ 未选中任何设备")
            return

        count = 0
        for device in self.selected_devices:
            if device.connection_state == STATE_CONNECTED:
                self.device_manager.disconnect_device(device.device_id)
                count += 1

        self._log(f"📴 断开 {count} 个设备")

    def _connect_all(self):
        """连接所有设备"""
        count = 0
        for device in self.device_manager.get_all_devices():
            if device.connection_state != STATE_CONNECTED:
                self.device_manager.connect_device(device.device_id)
                count += 1

        self._log(f"✅ 连接所有设备 ({count}个)")

    def _disconnect_all(self):
        """断开所有设备"""
        count = 0
        for device in self.device_manager.get_all_devices():
            if device.connection_state == STATE_CONNECTED:
                self.device_manager.disconnect_device(device.device_id)
                count += 1

        self._log(f"📴 断开所有设备 ({count}个)")

    def _send_message(self):
        """发送消息"""
        message = self.message_input.text().strip()
        if not message:
            return

        if not self.selected_devices:
            self._log("❌ 未选中任何设备")
            return

        for device in self.selected_devices:
            if device.connection_state == STATE_CONNECTED:
                self.device_manager.send_message_to_device(device.device_id, message)
                self._log(f"📤 [{device.name}] 发送: {message}")
            else:
                self._log(f"⚠️ [{device.name}] 未连接，无法发送")

        self.message_input.clear()

    def _send_quick_command(self, command: str):
        """发送快捷命令"""
        self.message_input.setText(command)
        self._send_message()

    def _log(self, message: str):
        """添加日志"""
        self.log_text.append(message)
        # 滚动到底部
        scrollbar = self.log_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())
