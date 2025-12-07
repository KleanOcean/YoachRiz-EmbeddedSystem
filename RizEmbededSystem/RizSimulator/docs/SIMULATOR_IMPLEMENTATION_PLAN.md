# RizSimulator 实现计划 - 基于ESP32固件

## 项目目标

创建一个完全模拟Riz反应灯ESP32设备的Python GUI应用，能够真实复现硬件设备的所有功能和行为。

## 硬件设备分析（基于ESP32固件）

### 核心组件

#### 1. RGB LED 灯带
- **型号**: WS2812/NeoPixel
- **数量**: 48个LED (LED_COUNT = 48)
- **引脚**: GPIO 23
- **物理布局**:
  - 内圈: 24个LED (索引 0-23)，圆形排列
  - 外圈: 24个LED (索引 24-47)，圆形排列
- **特性**:
  - 可编程彩色LED
  - 支持多种颜色和亮度 (0-255)
  - 两个圆圈可以显示不同颜色
  - 支持动画效果

#### 2. TOF激光测距传感器
- **型号**: TF-Luna UART
- **通信**: UART (RX: GPIO 27, TX: GPIO 26)
- **波特率**: 921600
- **功能**:
  - 检测物体距离 (范围: 30-2000mm)
  - 返回距离值和信号强度(amplitude)
  - 用于检测手部触碰
- **检测参数**:
  - 振幅阈值: 5000
  - 阈值因子: 1.04
  - 连续读取次数: 3
  - 消抖时间: 50ms
  - 冷却时间: 400ms
  - 基线历史: 30个样本
  - 移动平均: 3个样本

#### 3. 蜂鸣器
- **引脚**: GPIO 4
- **功能**: 声音提示
- **默认时长**: 500ms
- **可配置**: 通过BLE设置时长

#### 4. 单色LED
- **引脚**: GPIO 2
- **功能**: 状态指示灯

#### 5. 按钮
- **引脚**: GPIO 16 (INPUT_PULLUP)
- **功能**: 重启设备

### 游戏模式

**模式定义**:
- MANUAL_MODE = 1 (手动模式)
- RANDOM_MODE = 2 (随机模式)
- TIMED_MODE = 3 (定时模式)
- DOUBLE_MODE = 4 (双击模式)
- RHYTHM_MODE = 5 (节奏模式)
- MOVEMENT_MODE = 6 (移动模式 - 未实现)
- OPENING_MODE = 11 (开启模式)
- CLOSING_MODE = 12 (关闭模式)
- TERMINATE_MODE = 13 (终止模式)
- RESTTIMESUP_MODE = 14 (休息时间到)
- PROCESSED_MODE = 99 (已处理)
- CONFIG_MODE = 100 (配置模式)

### RGB灯光颜色方案

**预定义颜色**:
- Pink: (255, 182, 193)
- Green: (0, 255, 0)
- Yellow: (255, 140, 0)
- Sky Blue: (0, 255, 255)
- White: (255, 255, 255)
- Cherry Red: (121, 6, 4)
- Brat Green: (138, 207, 0)

**启动动画主题** (4种):
1. **绿色主题** (默认)
   - Brat Green, Yellow-Green, Neon Yellow-Green
   - Dark Brat Green, Olive Green, Vibrant Green

2. **蓝色主题**
   - Deep Blue, Sky Blue, Turquoise
   - Navy, Royal Blue, Electric Blue

3. **橙红主题**
   - Deep Orange, Bright Orange, Red-Orange
   - Coral, Burnt Orange, Fire Orange

4. **红色主题**
   - Deep Red, Ruby Red, Crimson
   - Wine Red, Cherry Red, Scarlet

**灯光效果**:
- emit: 基础发光效果，支持双色LED
- emitSlowly: 渐进式点亮
- emitRandomly: 随机颜色
- manualWipe: 手动模式灯效 (Cherry Red)
- randomWipe: 随机模式灯效 (随机颜色)
- timedWipe: 定时模式灯效
- doubleWipe: 双击模式灯效
- rhythmWipe: 节奏模式灯效 (Yellow)
- connectedWipe: 连接成功灯效 (Sky Blue)
- configNumberWipe: 配置编号显示 (闪烁N次)

### 工作流程

**典型流程**:
1. 设备启动 → 初始化传感器和BLE
2. 显示启动动画 (根据电量显示)
3. 等待BLE连接
4. 接收游戏模式命令
5. 点亮RGB LED (颜色根据模式)
6. 启动TOF传感器检测
7. 检测到物体 (距离 < 300mm 且 振幅 > 5000)
8. 关闭LED
9. 发送BLE通知到App
10. 进入冷却期 (400ms)
11. 等待下一轮

**核心任务** (ESP32 FreeRTOS):
- Core 0: ProcessingTask, LightControlTask
- Core 1: TOFSensorTask, MMWaveSensorTask (已禁用)

### BLE通信协议

**Service UUID**: ab0828b1-198e-4351-b779-901fa0e0371e

**Characteristics**:
1. **MSG UUID**: 4ac8a696-9736-4e5d-932b-e9b31405049c
   - 接收游戏模式命令
   - 接收配置参数

2. **TX UUID**: 62ec0272-3ec5-11eb-b378-0242ac130003
   - 发送通知到App
   - 发送状态更新

3. **OTA UUID**: 62ec0272-3ec5-11eb-b378-0242ac130005
   - 固件OTA更新

**通知消息**:
- "manual" - 手动模式触发
- "random" - 随机模式触发
- "rhythm" - 节奏模式触发
- "double<N>" - 双击模式第N个
- "timed" - 定时模式触发
- "config:<N>" - 配置编号N
- "configDone:<N>" - 配置完成

### 配置参数

**默认值**:
- DEFAULT_GAMEMODE: 13 (TERMINATE_MODE)
- DEFAULT_BLINKBREAK: 500ms
- DEFAULT_TIMEDBREAK: 500ms
- DEFAULT_BUFFER: 500ms
- DEFAULT_BUZZER: 1 (开启)
- DEFAULT_BUZZERTIME: 500ms

**可配置项**:
- 游戏模式
- 闪烁间隔
- 定时间隔
- 缓冲时间
- 蜂鸣器开关
- 蜂鸣器时长
- Double模式索引
- 传感器模式 (1=LiDAR, 2=MMWave, 3=Both)
- 配置闪烁次数

## 模拟器实现需求

### 1. GUI界面设计

#### 1.1 设备显示组件
**需求**:
- 显示设备名称 (RIZ-0001, RIZ-0002...)
- 双圈LED可视化
  - 内圈: 24个LED小圆点
  - 外圈: 24个LED小圆点
  - 圆形排列
- 连接状态显示
- TOF传感器状态
- 触发按钮

**布局示意**:
```
┌────────────┐
│  RIZ-0001  │
│   ●●●●●    │  ← 外圈 (24 LEDs)
│  ● ○○○ ●   │  ← 内圈 (24 LEDs)
│ ● ○   ○ ●  │
│ ● ○   ○ ●  │
│  ● ○○○ ●   │
│   ●●●●●    │
│  已连接 ✓  │
│   [触发]   │
└────────────┘
```

#### 1.2 TOF传感器控制面板
**需求**:
- 距离滑块 (30-2000mm)
- "模拟手部触碰" 按钮 (快速触发)
- 距离显示 (实时)
- 振幅显示 (计算)
- 基线值显示
- 检测状态指示灯
- 冷却状态显示

#### 1.3 游戏模式控制面板
**需求**:
- 模式选择下拉框 (Manual, Random, Rhythm, etc.)
- 蜂鸣器开关
- 蜂鸣器时长设置
- Break时间设置
- Buffer时间设置
- 传感器模式选择
- "发送模式" 按钮

#### 1.4 主窗口布局
**区域**:
1. 菜单栏: 文件, 设备, 模式, 帮助
2. 工具栏: 添加设备, 删除设备, 启动/停止, 清除日志
3. 设备网格区: 显示1-20个设备
4. 控制面板区: 游戏模式和传感器控制
5. 日志输出区: 实时显示系统日志
6. 状态栏: 设备统计, 系统状态

### 2. 设备核心逻辑

#### 2.1 设备状态管理
**状态包含**:
- LED状态 (48个LED的RGB值)
- TOF传感器状态 (距离, 振幅, 基线)
- 游戏模式
- 连接状态
- 蜂鸣器状态
- 配置参数

#### 2.2 游戏模式处理
**每种模式的行为**:

**Manual Mode**:
- 点亮Cherry Red颜色
- 启动TOF检测
- 检测到物体 → 关灯 → 发送"manual"通知

**Random Mode**:
- 点亮随机颜色
- 启动TOF检测
- 检测到物体 → 关灯 → 发送"random"通知

**Rhythm Mode**:
- 点亮Yellow颜色
- 根据sensorMode决定是否启动TOF
- 检测到物体 → 关灯 → 发送"rhythm"通知
- 支持倒计时蜂鸣器

**Double Mode**:
- 点亮Cyan颜色
- 显示索引号
- 发送"double<N>"通知

**Config Mode**:
- 闪烁指定次数
- 显示配置编号

**Opening Mode**:
- 采集TOF基线
- 点亮灯光
- 禁止再次点亮

**Closing Mode**:
- 点亮灯光

**Terminate Mode**:
- 关闭所有灯光
- 停止传感器
- 重置检测标志

#### 2.3 TOF传感器模拟
**检测算法**:
1. 采集基线值 (初始30个样本平均)
2. 实时读取距离和振幅
3. 计算动态阈值 (基线 × 1.04)
4. 连续3次读取超过阈值 → 触发检测
5. 进入400ms冷却期
6. 重置计数器

**振幅计算**:
- 距离 < 300mm → 振幅 5000-6000
- 距离 > 300mm → 振幅 100-300
- 加入随机噪声模拟真实传感器

#### 2.4 LED灯光效果
**效果实现**:
- 所有LED同色 (简化版)
- 渐变动画
- 闪烁效果
- 流水效果
- 随机颜色
- 配置编号显示 (闪烁N次)

### 3. BLE通信模拟

#### 3.1 服务器实现
**功能**:
- 创建BLE GATT服务
- 监听连接/断开
- 接收命令
- 发送通知

#### 3.2 命令解析
**支持命令**:
- 设置游戏模式
- 设置配置参数
- 获取状态
- 触发检测

#### 3.3 通知发送
**通知类型**:
- 模式触发通知
- 状态更新
- 配置完成

### 4. 实现步骤

#### Phase 1: 核心架构 (1天)
- 创建项目结构
- 定义数据模型
- 实现设备基础类
- 设置日志系统

#### Phase 2: LED显示 (1-2天)
- 创建双圈LED Widget
- 实现LED绘制 (48个圆点)
- 实现颜色更新
- 测试性能

#### Phase 3: TOF传感器 (1天)
- 创建TOF传感器Widget
- 实现距离滑块
- 实现检测算法
- 实现振幅计算
- 添加触发按钮

#### Phase 4: 游戏模式 (2天)
- 实现所有游戏模式处理
- 实现灯光效果函数
- 实现模式切换逻辑
- 测试每种模式

#### Phase 5: GUI集成 (1-2天)
- 创建主窗口
- 集成设备网格
- 集成控制面板
- 集成日志显示
- 布局优化

#### Phase 6: BLE通信 (1天)
- 实现BLE服务器
- 实现命令解析
- 实现通知发送
- 测试通信

#### Phase 7: 测试优化 (1-2天)
- 完整流程测试
- 多设备并发测试
- 性能优化
- Bug修复

### 5. 技术要点

#### 5.1 性能优化
- 使用QTimer控制动画帧率 (60fps)
- 批量更新LED状态
- 使用QPainter优化绘制
- 异步事件循环

#### 5.2 真实性保证
- 严格按照ESP32固件逻辑
- 复现TOF检测算法
- 准确的颜色和亮度
- 真实的时序控制

#### 5.3 扩展性
- 支持新增游戏模式
- 支持自定义颜色主题
- 支持配置保存/加载
- 支持录制/回放

### 6. 成功标准

**功能完整性**:
- ✓ 48个LED双圈显示
- ✓ TOF传感器真实模拟
- ✓ 所有游戏模式实现
- ✓ 灯光效果一致
- ✓ BLE通信正常

**性能指标**:
- ✓ 响应时间 < 50ms
- ✓ 支持20个设备并发
- ✓ CPU < 25%
- ✓ 内存 < 500MB
- ✓ 帧率 ≥ 30fps

**用户体验**:
- ✓ 界面直观易用
- ✓ 实时状态反馈
- ✓ 日志清晰
- ✓ 操作流畅

### 7. 参考文件

**ESP32固件**:
- src/main.cpp - 主逻辑和任务管理
- src/LightControl.cpp - LED控制和灯光效果
- src/TF_Luna_UART.cpp - TOF传感器驱动
- src/BluetoothControl.cpp - BLE通信
- src/DataControl.cpp - 数据管理
- include/Global_VAR.h - 常量和宏定义

**文档**:
- docs/SIMULATOR_IMPLEMENTATION_PLAN.md - 本实现计划
- README.md - 项目说明
