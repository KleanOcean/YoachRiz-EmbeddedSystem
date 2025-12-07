#!/bin/bash

# Riz OTA GUI 启动脚本

echo "====================================="
echo "      Riz OTA 更新系统 v1.0.0"
echo "====================================="
echo ""

# 检查Python版本
PYTHON_VERSION=$(python3 --version 2>&1 | grep -Po '(?<=Python )\d+\.\d+')
REQUIRED_VERSION="3.7"

if [ "$(printf '%s\n' "$REQUIRED_VERSION" "$PYTHON_VERSION" | sort -V | head -n1)" != "$REQUIRED_VERSION" ]; then
    echo "错误: 需要 Python $REQUIRED_VERSION 或更高版本"
    echo "当前版本: Python $PYTHON_VERSION"
    exit 1
fi

echo "✓ Python 版本: $PYTHON_VERSION"

# 检查并安装依赖
echo ""
echo "检查依赖..."

# 检查bleak是否安装
if ! python3 -c "import bleak" 2>/dev/null; then
    echo "安装 bleak (蓝牙库)..."
    pip3 install bleak --user
else
    echo "✓ bleak 已安装"
fi

# 检查tkinter是否可用
if ! python3 -c "import tkinter" 2>/dev/null; then
    echo ""
    echo "警告: tkinter 未安装"
    echo "请根据您的系统安装 tkinter:"
    echo "  Ubuntu/Debian: sudo apt-get install python3-tk"
    echo "  macOS: 通常已包含"
    echo "  Windows: 通常已包含"
    exit 1
else
    echo "✓ tkinter 可用"
fi

# 检查PlatformIO
if command -v pio &> /dev/null; then
    echo "✓ PlatformIO 已安装"
else
    echo ""
    echo "警告: PlatformIO 未安装"
    echo "固件编译功能将不可用"
    echo "安装方法: pip3 install platformio --user"
    echo ""
    echo "是否继续运行？(y/n)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 0
    fi
fi

echo ""
echo "====================================="
echo "启动 OTA GUI..."
echo "====================================="
echo ""

# 启动应用
cd "$(dirname "$0")"
python3 src/ota_gui.py