#!/bin/bash
# RizSimulator启动脚本

echo "================================"
echo "  RizSimulator v1.0.0"
echo "  Riz反应灯ESP32设备模拟器"
echo "================================"
echo ""

# 检查Python版本
PYTHON_CMD=""
if command -v python3 &> /dev/null; then
    PYTHON_CMD="python3"
elif command -v python &> /dev/null; then
    PYTHON_CMD="python"
else
    echo "错误: 未找到Python。请安装Python 3.8或更高版本。"
    exit 1
fi

echo "使用Python: $PYTHON_CMD"
$PYTHON_CMD --version
echo ""

# 检查依赖
echo "检查依赖..."
$PYTHON_CMD -m pip install -q -r requirements.txt

echo ""
echo "启动 RizSimulator..."
echo ""

# 运行应用
$PYTHON_CMD rizsimulator.py
