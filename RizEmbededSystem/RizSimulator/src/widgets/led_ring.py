"""
LED Ring Widget
双圈LED显示组件
"""

import math
from typing import Tuple
from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, QPointF, QRectF, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QRadialGradient

from constants import (
    INNER_RING_COUNT, OUTER_RING_COUNT,
    INNER_RING_RADIUS, OUTER_RING_RADIUS, LED_SIZE
)


class LEDRingWidget(QWidget):
    """
    双圈LED显示组件
    内圈: 24个LED
    外圈: 24个LED
    总计: 48个LED
    """

    clicked = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(200, 200)

        # LED状态
        self.inner_ring_colors = [(0, 0, 0)] * INNER_RING_COUNT
        self.outer_ring_colors = [(0, 0, 0)] * OUTER_RING_COUNT
        self.brightness = 1.0
        self.is_on = False

    def set_all_leds(self, color: Tuple[int, int, int]):
        """设置所有LED为相同颜色"""
        self.inner_ring_colors = [color] * INNER_RING_COUNT
        self.outer_ring_colors = [color] * OUTER_RING_COUNT
        self.is_on = any(c != (0, 0, 0) for c in self.inner_ring_colors + self.outer_ring_colors)
        self.update()

    def set_inner_ring(self, color: Tuple[int, int, int]):
        """设置内圈颜色"""
        self.inner_ring_colors = [color] * INNER_RING_COUNT
        self.update()

    def set_outer_ring(self, color: Tuple[int, int, int]):
        """设置外圈颜色"""
        self.outer_ring_colors = [color] * OUTER_RING_COUNT
        self.update()

    def set_brightness(self, brightness: float):
        """设置亮度 (0.0 - 1.0)"""
        self.brightness = max(0.0, min(1.0, brightness))
        self.update()

    def clear(self):
        """清除所有LED"""
        self.inner_ring_colors = [(0, 0, 0)] * INNER_RING_COUNT
        self.outer_ring_colors = [(0, 0, 0)] * OUTER_RING_COUNT
        self.is_on = False
        self.update()

    def paintEvent(self, event):
        """绘制LED圆环"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # 获取中心点
        rect = self.rect()
        center_x = rect.width() / 2
        center_y = rect.height() / 2
        center = QPointF(center_x, center_y)

        # 绘制背景圆圈（可选）
        painter.setPen(QPen(QColor(50, 50, 50), 1))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(center, OUTER_RING_RADIUS + 10, OUTER_RING_RADIUS + 10)

        # 绘制外圈LED
        self._draw_ring(painter, center, OUTER_RING_RADIUS, self.outer_ring_colors, OUTER_RING_COUNT)

        # 绘制内圈LED
        self._draw_ring(painter, center, INNER_RING_RADIUS, self.inner_ring_colors, INNER_RING_COUNT)

    def _draw_ring(self, painter: QPainter, center: QPointF, radius: float,
                   colors: list, count: int):
        """绘制一圈LED"""
        angle_step = 360.0 / count

        for i in range(count):
            # 计算LED位置（从顶部开始，顺时针）
            angle = -90 + (i * angle_step)  # -90度从顶部开始
            angle_rad = math.radians(angle)

            x = center.x() + radius * math.cos(angle_rad)
            y = center.y() + radius * math.sin(angle_rad)

            # 获取颜色并应用亮度
            r, g, b = colors[i]
            r = int(r * self.brightness)
            g = int(g * self.brightness)
            b = int(b * self.brightness)

            # 绘制LED（带发光效果）
            self._draw_led(painter, QPointF(x, y), (r, g, b))

    def _draw_led(self, painter: QPainter, pos: QPointF, color: Tuple[int, int, int]):
        """绘制单个LED带发光效果"""
        r, g, b = color

        # 如果LED是关闭的，显示为暗灰色
        if r == 0 and g == 0 and b == 0:
            painter.setPen(QPen(QColor(80, 80, 80), 1))
            painter.setBrush(QBrush(QColor(30, 30, 30)))
            painter.drawEllipse(pos, LED_SIZE, LED_SIZE)
            return

        # LED开启 - 绘制发光效果
        # 外层光晕
        gradient = QRadialGradient(pos, LED_SIZE * 2)
        gradient.setColorAt(0, QColor(r, g, b, 180))
        gradient.setColorAt(0.5, QColor(r, g, b, 100))
        gradient.setColorAt(1, QColor(r, g, b, 0))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QBrush(gradient))
        painter.drawEllipse(pos, LED_SIZE * 2, LED_SIZE * 2)

        # LED主体
        led_gradient = QRadialGradient(pos, LED_SIZE)
        led_gradient.setColorAt(0, QColor(min(255, r + 50), min(255, g + 50), min(255, b + 50)))
        led_gradient.setColorAt(0.7, QColor(r, g, b))
        led_gradient.setColorAt(1, QColor(max(0, r - 50), max(0, g - 50), max(0, b - 50)))

        painter.setBrush(QBrush(led_gradient))
        painter.setPen(QPen(QColor(r, g, b), 1))
        painter.drawEllipse(pos, LED_SIZE, LED_SIZE)

    def mousePressEvent(self, event):
        """鼠标点击事件"""
        self.clicked.emit()
        super().mousePressEvent(event)
