# RizSimulator

Riz ESP32 反应灯设备模拟器 - 基于真实ESP32固件的完整Python GUI模拟

## 功能特性

### 硬件模拟
- ✅ **48颗RGB LED** - 双圈布局 (内圈24 + 外圈24)
- ✅ **TOF激光传感器** - 距离检测与振幅模拟
- ✅ **蜂鸣器** - 可配置时长
- ✅ **电池状态** - 电量显示

### 游戏模式
- ✅ **Manual Mode** - 手动模式 (蓝色，根据process变化)
- ✅ **Random Mode** - 随机模式 (绿/黄/红随机)
- ✅ **Rhythm Mode** - 节奏模式 (自定义RGB)
- ✅ **Double Mode** - 双击模式 (橙色/深蓝)
- ✅ **Opening Mode** - 开启模式 (基线采集)
- ✅ **Closing Mode** - 关闭模式 (闪烁)
- ✅ **Config Mode** - 配置模式 (白光闪烁)
- ✅ **Rest Mode** - 休息模式 (倒计时)
- ✅ **Terminate Mode** - 终止模式

### 动画特效
- ✅ **启动动画** - 绿色主题渐变
- ✅ **连接动画** - Tennis绿闪烁
- ✅ **配置闪烁** - 可配置次数
- ✅ **休息倒计时** - 渐暗效果

### 多设备支持
- ✅ 同时模拟最多 **20个设备**
- ✅ 独立设备状态管理
- ✅ 批量控制 (Ctrl+点击多选)
- ✅ 实时统计信息

## 快速开始

### 安装依赖

```bash
pip install -r requirements.txt
```

### 运行主程序

```bash
cd src
python main.py
```

### 运行测试程序

```bash
# LED显示测试
python test_led_widget.py

# TOF传感器测试
python test_tof_sensor.py

# 游戏模式测试
python test_game_modes.py
```

## 项目结构

```
RizSimulator/
├── src/
│   ├── main.py                 # 主程序入口
│   ├── constants.py            # 常量定义
│   ├── logger.py               # 日志系统
│   ├── models.py               # 数据模型
│   ├── device_core.py          # 设备核心逻辑
│   ├── device_manager.py       # 设备管理器
│   ├── gui/                    # GUI组件
│   │   ├── main_window.py      # 主窗口
│   │   ├── device_grid.py      # 设备网格
│   │   ├── control_panel.py    # 控制面板
│   │   └── statistics_panel.py # 统计面板
│   ├── widgets/                # 自定义组件
│   │   ├── led_ring.py         # LED圆环显示
│   │   ├── device_widget.py    # 单设备显示
│   │   └── tof_control.py      # TOF控制面板
│   └── test_*.py               # 测试程序
├── docs/                       # 文档
│   ├── PRD_RizSimulator.md     # 产品需求文档
│   └── SIMULATOR_IMPLEMENTATION_PLAN.md
├── logs/                       # 日志文件
└── README.md                   # 本文件
```

## 使用说明

### 主界面

1. **设备网格** (左侧)
   - 显示所有模拟设备
   - 点击选中设备 (Ctrl+点击多选)
   - 双圈LED实时显示
   - 显示连接状态和TOF状态

2. **控制面板** (右上)
   - **游戏模式** - 切换不同游戏模式
   - **参数设置** - 调整模式参数
   - **动画特效** - 触发启动/连接动画

3. **统计面板** (右下)
   - 设备总数与活跃数
   - 触发次数统计
   - 模式分布
   - 连接状态分布

### 快捷键

- `Ctrl+N` - 添加新设备
- `Delete` - 删除选中设备
- `Ctrl+点击` - 多选设备

### 菜单功能

#### 设备菜单
- 添加设备
- 移除选中设备
- 清除所有设备

#### 控制菜单
- 启动所有设备
- 停止所有设备
- 重置统计

## TOF传感器模拟

### 检测算法
1. 距离 < 300mm 时振幅升高
2. 振幅 > 阈值(5000)
3. 连续3次检测到
4. 触发检测 → 400ms冷却期

### 控制方式
- 距离滑块: 30-2000mm
- "模拟手部触碰" 按钮快速触发
- 可视化激光束显示

## 开发说明

### 添加新游戏模式

1. 在 `constants.py` 添加模式常量
2. 在 `device_core.py` 的 `DeviceController` 添加处理函数
3. 在控制面板添加按钮

### 自定义动画

在 `device_core.py` 的 `_update_animation()` 方法添加新动画类型

## 技术栈

- **PyQt6** - GUI框架
- **Python 3.8+** - 编程语言
- **dataclasses** - 数据模型
- **typing** - 类型提示
- **colorlog** - 彩色日志

## 许可证

本项目为内部开发工具，基于ESP32固件开发

## 更新日志

### v0.1.0 (2025-01-23)
- ✅ 完成核心架构
- ✅ 实现双圈LED显示
- ✅ 实现TOF传感器模拟
- ✅ 实现所有游戏模式
- ✅ 完成主GUI界面
- ✅ 多设备支持
- ✅ 统计功能

## 待开发功能

- [ ] BLE通信协议 (Phase 6)
- [ ] 性能优化 (Phase 7)
- [ ] 网络同步
- [ ] 数据导出
