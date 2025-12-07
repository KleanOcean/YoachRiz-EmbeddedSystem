"""
Main Window
RizSimulator主窗口
"""

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QMenuBar, QMenu, QStatusBar, QMessageBox
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QAction

from device_manager import DeviceManager
from gui.device_grid import DeviceGridWidget
from gui.control_panel import ControlPanelWidget
from gui.statistics_panel import StatisticsPanelWidget
from constants import *
from logger import get_logger

logger = get_logger("MainWindow")


class MainWindow(QMainWindow):
    """RizSimulator主窗口"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("RizSimulator - Riz ESP32 Device Simulator")
        self.setGeometry(100, 50, WINDOW_WIDTH, WINDOW_HEIGHT)
        self.setMinimumSize(MIN_WIDTH, MIN_HEIGHT)

        # 设备管理器
        self.device_manager = DeviceManager()
        self.selected_devices = []  # 当前选中的设备

        self._init_ui()
        self._init_menu()
        self._init_status_bar()

        # 主更新定时器
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self._update)
        self.update_timer.start(16)  # 60fps

        # 统计更新定时器
        self.stats_timer = QTimer()
        self.stats_timer.timeout.connect(self._update_statistics)
        self.stats_timer.start(1000)  # 1fps

        logger.info("RizSimulator主窗口初始化完成")

    def _init_ui(self):
        """初始化UI"""
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(5, 5, 5, 5)

        # 创建主分割器
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # 左侧：设备网格
        self.device_grid = DeviceGridWidget(self.device_manager)
        self.device_grid.device_selected.connect(self._on_device_selected)
        self.device_grid.device_triggered.connect(self._on_device_triggered)
        main_splitter.addWidget(self.device_grid)

        # 右侧：垂直分割器
        right_splitter = QSplitter(Qt.Orientation.Vertical)

        # 控制面板
        self.control_panel = ControlPanelWidget(self.device_manager)
        self.control_panel.mode_changed.connect(self._on_mode_changed)
        self.control_panel.animation_requested.connect(self._on_animation_requested)
        right_splitter.addWidget(self.control_panel)

        # 统计面板
        self.statistics_panel = StatisticsPanelWidget(self.device_manager)
        right_splitter.addWidget(self.statistics_panel)

        # 设置右侧分割比例
        right_splitter.setSizes([500, 300])

        main_splitter.addWidget(right_splitter)

        # 设置主分割比例
        main_splitter.setSizes([900, 500])

        main_layout.addWidget(main_splitter)

    def _init_menu(self):
        """初始化菜单栏"""
        menubar = self.menuBar()

        # 设备菜单
        device_menu = menubar.addMenu("设备(&D)")

        add_device_action = QAction("添加设备", self)
        add_device_action.setShortcut("Ctrl+N")
        add_device_action.triggered.connect(self._add_device)
        device_menu.addAction(add_device_action)

        remove_device_action = QAction("移除选中设备", self)
        remove_device_action.setShortcut("Delete")
        remove_device_action.triggered.connect(self._remove_selected_devices)
        device_menu.addAction(remove_device_action)

        device_menu.addSeparator()

        clear_all_action = QAction("清除所有设备", self)
        clear_all_action.triggered.connect(self._clear_all_devices)
        device_menu.addAction(clear_all_action)

        # 控制菜单
        control_menu = menubar.addMenu("控制(&C)")

        start_all_action = QAction("启动所有设备", self)
        start_all_action.triggered.connect(self._start_all_devices)
        control_menu.addAction(start_all_action)

        stop_all_action = QAction("停止所有设备", self)
        stop_all_action.triggered.connect(self._stop_all_devices)
        control_menu.addAction(stop_all_action)

        control_menu.addSeparator()

        reset_stats_action = QAction("重置统计", self)
        reset_stats_action.triggered.connect(self._reset_statistics)
        control_menu.addAction(reset_stats_action)

        # 帮助菜单
        help_menu = menubar.addMenu("帮助(&H)")

        about_action = QAction("关于", self)
        about_action.triggered.connect(self._show_about)
        help_menu.addAction(about_action)

    def _init_status_bar(self):
        """初始化状态栏"""
        self.statusBar = QStatusBar()
        self.setStatusBar(self.statusBar)
        self.statusBar.showMessage("就绪")

    def _update(self):
        """主更新循环"""
        # 更新设备管理器
        self.device_manager.update_all(0.016)

        # 更新设备网格显示
        self.device_grid.update_display()

        # 更新状态栏
        device_count = len(self.device_manager.devices)
        selected_count = len(self.selected_devices)
        self.statusBar.showMessage(
            f"设备: {device_count} | 选中: {selected_count}"
        )

    def _update_statistics(self):
        """更新统计显示"""
        self.statistics_panel.update_statistics()

    def _on_device_selected(self, devices: list):
        """设备选中事件"""
        self.selected_devices = devices
        self.control_panel.set_selected_devices(devices)
        logger.debug(f"选中 {len(devices)} 个设备")

    def _on_device_triggered(self, device):
        """设备触发事件（模拟TOF检测到物体）"""
        logger.info(f"设备触发: {device.name}")
        controller = self.device_manager.get_controller(device.device_id)

        # 如果灯是亮的，模拟TOF检测关灯
        if device.led_state.is_on:
            tof_controller = self.device_manager.get_tof_controller(device.device_id)
            tof_controller.simulate_touch()
            controller.turn_light_off()
        else:
            # 如果灯是关的，重新触发游戏模式（开灯）
            device.able_to_turn_on = True
            controller.handle_game_mode(device.config.game_mode)

    def _on_mode_changed(self, mode: int, params: dict):
        """模式变化事件"""
        if not self.selected_devices:
            logger.warning("未选中任何设备")
            return

        # 应用到所有选中设备
        for device in self.selected_devices:
            device.able_to_turn_on = True
            device.config.game_mode = mode

            # 应用参数
            if "process" in params:
                device.config.process = params["process"]
            if "double_index" in params:
                device.config.double_mode_index = params["double_index"]
            if "rgb" in params:
                r, g, b = params["rgb"]
                device.config.red_value = r
                device.config.green_value = g
                device.config.blue_value = b
            if "blink_count" in params:
                device.config.config_blink_count = params["blink_count"]

            # 执行模式
            controller = self.device_manager.get_controller(device.device_id)
            controller.handle_game_mode(mode)

        logger.info(f"应用模式 {mode} 到 {len(self.selected_devices)} 个设备")

    def _on_animation_requested(self, animation_type: str):
        """动画请求事件"""
        if not self.selected_devices:
            logger.warning("未选中任何设备")
            return

        for device in self.selected_devices:
            controller = self.device_manager.get_controller(device.device_id)
            if animation_type == "init":
                controller.start_init_animation()
            elif animation_type == "connected":
                controller.start_connected_animation()

        logger.info(f"启动 {animation_type} 动画，{len(self.selected_devices)} 个设备")

    def _add_device(self):
        """添加设备"""
        if len(self.device_manager.devices) >= MAX_DEVICES:
            QMessageBox.warning(self, "警告", f"最多支持 {MAX_DEVICES} 个设备")
            return

        device = self.device_manager.create_device()
        self.device_grid.add_device(device)
        logger.info(f"添加设备: {device.name}")

    def _remove_selected_devices(self):
        """移除选中设备"""
        if not self.selected_devices:
            return

        count = len(self.selected_devices)
        reply = QMessageBox.question(
            self,
            "确认",
            f"确定要移除 {count} 个设备吗？",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            for device in self.selected_devices:
                self.device_manager.remove_device(device.device_id)
                self.device_grid.remove_device(device)

            self.selected_devices.clear()
            logger.info(f"移除 {count} 个设备")

    def _clear_all_devices(self):
        """清除所有设备"""
        if not self.device_manager.devices:
            return

        reply = QMessageBox.question(
            self,
            "确认",
            "确定要清除所有设备吗？",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            self.device_manager.remove_all_devices()
            self.device_grid.clear_all()
            self.selected_devices.clear()
            logger.info("清除所有设备")

    def _start_all_devices(self):
        """启动所有设备"""
        for device in self.device_manager.devices.values():
            device.able_to_turn_on = True
            controller = self.device_manager.get_controller(device.device_id)
            controller.handle_game_mode(device.config.game_mode)

        logger.info("启动所有设备")

    def _stop_all_devices(self):
        """停止所有设备"""
        for device in self.device_manager.devices.values():
            controller = self.device_manager.get_controller(device.device_id)
            controller.handle_game_mode(TERMINATE_MODE)

        logger.info("停止所有设备")

    def _reset_statistics(self):
        """重置统计"""
        for device in self.device_manager.devices.values():
            device.stats.reset()

        logger.info("重置统计")

    def _show_about(self):
        """显示关于对话框"""
        QMessageBox.about(
            self,
            "关于 RizSimulator",
            """<h3>RizSimulator</h3>
            <p>Riz ESP32 反应灯设备模拟器</p>
            <p>版本: 0.1.0</p>
            <p>基于 ESP32 固件完整复刻设备行为</p>
            <br>
            <p><b>功能特性:</b></p>
            <ul>
            <li>48颗LED双圈显示 (内24+外24)</li>
            <li>TOF激光传感器模拟</li>
            <li>完整游戏模式支持</li>
            <li>BLE通信协议</li>
            <li>多设备并发模拟</li>
            </ul>
            """
        )

    def closeEvent(self, event):
        """关闭事件"""
        logger.info("RizSimulator关闭")
        event.accept()
