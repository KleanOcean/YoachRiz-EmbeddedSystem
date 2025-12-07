@echo off
REM RizSimulator启动脚本 (Windows)

echo ================================
echo   RizSimulator v1.0.0
echo   Riz反应灯ESP32设备模拟器
echo ================================
echo.

REM 检查Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到Python。请安装Python 3.8或更高版本。
    pause
    exit /b 1
)

echo 使用Python:
python --version
echo.

REM 检查依赖
echo 检查依赖...
python -m pip install -q -r requirements.txt

echo.
echo 启动 RizSimulator...
echo.

REM 运行应用
python rizsimulator.py

pause
