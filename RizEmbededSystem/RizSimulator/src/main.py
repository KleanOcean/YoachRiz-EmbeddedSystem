"""
RizSimulator - Riz ESP32 Device Simulator
Main Application Entry Point
"""

import sys
from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt

from gui.main_window import MainWindow
from logger import get_logger

logger = get_logger("RizSimulator")


def main():
    """主函数"""
    # 启用高DPI支持
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )

    app = QApplication(sys.argv)
    app.setApplicationName("RizSimulator")
    app.setOrganizationName("RizLab")

    # 设置应用样式
    app.setStyle("Fusion")

    logger.info("=" * 70)
    logger.info("RizSimulator - Riz ESP32 Device Simulator")
    logger.info("=" * 70)
    logger.info("🚀 应用程序启动")
    logger.info("📦 基于 ESP32 固件完整模拟")
    logger.info("💡 支持 48 LED 双圈显示 (内24 + 外24)")
    logger.info("📡 支持 TOF 激光传感器模拟")
    logger.info("🎮 支持所有游戏模式")
    logger.info("📊 支持多设备并发 (最多20个)")
    logger.info("=" * 70)

    # 创建主窗口
    window = MainWindow()
    window.show()

    logger.info("✅ 主窗口已显示")
    logger.info("💡 提示: 使用 Ctrl+点击 进行多选设备")

    # 运行应用
    exit_code = app.exec()

    logger.info("👋 应用程序退出")
    logger.info("=" * 70)

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
