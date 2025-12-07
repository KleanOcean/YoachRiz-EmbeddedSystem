"""
Device Grid Widget
设备网格显示组件
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QPushButton, QScrollArea, QFrame
)
from PyQt6.QtCore import Qt, pyqtSignal

from device_manager import DeviceManager
from models import RizDevice
from widgets.device_widget import DeviceWidget
from logger import get_logger

logger = get_logger("DeviceGrid")


class DeviceGridWidget(QWidget):
    """设备网格显示组件"""

    device_selected = pyqtSignal(list)  # 设备选中信号
    device_triggered = pyqtSignal(object)  # 设备触发信号

    def __init__(self, device_manager: DeviceManager, parent=None):
        super().__init__(parent)
        self.device_manager = device_manager
        self.device_widgets = {}  # device_id -> DeviceWidget
        self.selected_devices = []

        self._init_ui()

        # 初始创建3个设备
        self._create_initial_devices()

    def _init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        # 标题栏
        header = self._create_header()
        layout.addWidget(header)

        # 滚动区域
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)

        # 网格容器
        grid_container = QWidget()
        self.grid_layout = QGridLayout(grid_container)
        self.grid_layout.setSpacing(10)
        self.grid_layout.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)

        scroll.setWidget(grid_container)
        layout.addWidget(scroll)

    def _create_header(self) -> QWidget:
        """创建标题栏"""
        header = QFrame()
        header.setFrameShape(QFrame.Shape.StyledPanel)
        header.setMaximumHeight(50)

        layout = QHBoxLayout(header)

        title = QLabel("设备列表")
        title.setStyleSheet("font-weight: bold; font-size: 14px;")
        layout.addWidget(title)

        layout.addStretch()

        # 添加设备按钮
        add_btn = QPushButton("➕ 添加设备")
        add_btn.clicked.connect(self._add_device_btn_clicked)
        layout.addWidget(add_btn)

        # 全选按钮
        select_all_btn = QPushButton("全选")
        select_all_btn.clicked.connect(self._select_all)
        layout.addWidget(select_all_btn)

        # 取消选择按钮
        clear_btn = QPushButton("取消选择")
        clear_btn.clicked.connect(self._clear_selection)
        layout.addWidget(clear_btn)

        return header

    def _create_initial_devices(self):
        """创建初始设备"""
        for _ in range(3):
            device = self.device_manager.create_device()
            self.add_device(device)

        logger.info("创建3个初始设备")

    def add_device(self, device: RizDevice):
        """添加设备到网格"""
        widget = DeviceWidget(device)
        widget.clicked.connect(self._on_device_clicked)
        widget.trigger_clicked.connect(self._on_device_triggered)

        self.device_widgets[device.device_id] = widget

        # 计算网格位置 (每行4个)
        index = len(self.device_widgets) - 1
        row = index // 4
        col = index % 4

        self.grid_layout.addWidget(widget, row, col)

    def remove_device(self, device: RizDevice):
        """移除设备"""
        widget = self.device_widgets.pop(device.device_id, None)
        if widget:
            self.grid_layout.removeWidget(widget)
            widget.deleteLater()

            # 重新布局
            self._relayout_grid()

            # 从选中列表移除
            if device in self.selected_devices:
                self.selected_devices.remove(device)

    def clear_all(self):
        """清除所有设备"""
        for widget in self.device_widgets.values():
            self.grid_layout.removeWidget(widget)
            widget.deleteLater()

        self.device_widgets.clear()
        self.selected_devices.clear()

    def _relayout_grid(self):
        """重新布局网格"""
        # 先移除所有widget
        widgets = list(self.device_widgets.values())
        for widget in widgets:
            self.grid_layout.removeWidget(widget)

        # 重新添加
        for index, widget in enumerate(widgets):
            row = index // 4
            col = index % 4
            self.grid_layout.addWidget(widget, row, col)

    def update_display(self):
        """更新显示"""
        for widget in self.device_widgets.values():
            widget.update_display()

    def _on_device_clicked(self, device: RizDevice):
        """设备点击事件"""
        # Ctrl多选
        from PyQt6.QtWidgets import QApplication
        modifiers = QApplication.keyboardModifiers()

        if modifiers == Qt.KeyboardModifier.ControlModifier:
            # 切换选中状态
            if device in self.selected_devices:
                self.selected_devices.remove(device)
                self.device_widgets[device.device_id].set_selected(False)
            else:
                self.selected_devices.append(device)
                self.device_widgets[device.device_id].set_selected(True)
        else:
            # 单选
            self._clear_selection()
            self.selected_devices = [device]
            self.device_widgets[device.device_id].set_selected(True)

        self.device_selected.emit(self.selected_devices)

    def _on_device_triggered(self, device: RizDevice):
        """设备触发事件"""
        self.device_triggered.emit(device)

    def _add_device_btn_clicked(self):
        """添加设备按钮点击"""
        device = self.device_manager.create_device()
        self.add_device(device)

    def _select_all(self):
        """全选"""
        self.selected_devices = list(self.device_manager.devices.values())
        for widget in self.device_widgets.values():
            widget.set_selected(True)

        self.device_selected.emit(self.selected_devices)

    def _clear_selection(self):
        """清除选择"""
        self.selected_devices.clear()
        for widget in self.device_widgets.values():
            widget.set_selected(False)

        self.device_selected.emit(self.selected_devices)
