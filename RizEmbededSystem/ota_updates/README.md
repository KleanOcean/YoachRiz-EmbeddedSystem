# Riz OTA 更新系统

## 概述

OTA GUI 更新系统是一个跨平台的桌面应用程序，用于向 Riz 设备推送固件更新。系统提供图形化界面，简化固件编译、设备连接和 OTA 更新流程。

## 功能特性

- 🔧 **一键编译**: 集成 PlatformIO 自动编译最新固件
- 🔍 **设备扫描**: 自动发现周围的 PRO-XXXX 设备
- 🔗 **连接管理**: 维持稳定的 BLE 连接
- 📊 **进度显示**: 实时显示传输进度和速度
- 🧪 **测试功能**: 发送测试信号验证设备响应
- 📦 **批量更新**: 支持多设备批量更新（计划中）

## 快速开始

### 1. 安装依赖

```bash
# 进入OTA更新目录
cd ota_updates

# 安装Python依赖
pip install -r requirements.txt
```

### 2. 启动应用

```bash
python src/ota_gui.py
```

### 3. 使用流程

1. **编译固件**: 点击"📦 编译固件"按钮，自动编译最新代码
2. **扫描设备**: 点击"🔍 扫描设备"查找附近的 Riz 设备
3. **连接设备**: 从列表中选择设备并点击"🔗 连接设备"
4. **测试连接**: 点击"🧪 发送测试"验证设备响应
5. **开始更新**: 点击"🚀 开始更新"将固件推送到设备

## 系统架构

```
ota_updates/
├── src/                    # 源代码
│   ├── ota_gui.py         # 主界面应用
│   ├── ble_manager.py     # BLE通信管理
│   ├── firmware_compiler.py # 固件编译器
│   └── ota_uploader.py    # OTA上传器
├── docs/                   # 文档
│   └── ORG_OTA_GUI.md     # 模块定义文档
├── test/                   # 测试文件
├── resources/              # 资源文件
└── requirements.txt        # 依赖列表
```

## 环境要求

### 必需环境

- Python 3.7+
- PlatformIO CLI（用于编译固件）
- 蓝牙 4.0+ 适配器

### 操作系统支持

- ✅ macOS 10.15+
- ✅ Windows 10/11
- ✅ Ubuntu 20.04+

## 配置说明

### BLE 服务 UUID

系统使用以下 BLE 服务进行通信：

```python
# 消息服务
BLE_SERVICE_UUID = "ab0828b1-198e-4351-b779-901fa0e0371e"
BLE_MSG_CHAR_UUID = "4ac8a696-9736-4e5d-932b-e9b31405049c"

# OTA服务
BLE_OTA_SERVICE_UUID = "1d14d6ee-fd63-4fa1-bfa4-8f47b42119f0"
BLE_OTA_CONTROL_UUID = "f7bf3564-fb6d-4e53-88a4-5e37e0326063"
BLE_OTA_DATA_UUID = "984227f3-34fc-4045-a5d0-2c581f81a153"
```

### OTA 参数

```python
CHUNK_SIZE = 512    # 每个数据包大小（字节）
MAX_RETRIES = 3     # 最大重试次数
ACK_TIMEOUT = 5     # 确认超时（秒）
```

## 故障排除

### 常见问题

1. **找不到 PlatformIO**
   ```bash
   # 安装 PlatformIO
   pip install platformio

   # 或使用系统包管理器
   brew install platformio  # macOS
   ```

2. **蓝牙权限问题**
   - macOS: 系统偏好设置 → 安全性与隐私 → 蓝牙
   - Windows: 设置 → 隐私 → 蓝牙
   - Linux: 确保用户在 `dialout` 组

3. **连接失败**
   - 确保设备已开机且蓝牙已启用
   - 检查设备距离（建议 < 10 米）
   - 重启蓝牙服务

## 开发指南

### 添加新功能

1. 在 `src/` 目录下创建新模块
2. 在 `ota_gui.py` 中导入并集成
3. 更新 ORG 文档

### 测试模式

系统支持无硬件测试模式：

```python
# 在 ble_manager.py 中设置
BLEAK_AVAILABLE = False  # 启用模拟模式
```

## 版本历史

- **v1.0.0** (2025-11-23): 初始版本，基础 OTA 功能

## 相关文档

- [ORG 模块定义](docs/ORG_OTA_GUI.md)
- [蓝牙协议规范](../docs/protocol/蓝牙通信协议规范.md)
- [开发者指南](../docs/protocol/开发者实施指南.md)

## 联系支持

- 问题反馈: [GitHub Issues](https://github.com/...)
- 技术支持: support@yoach.com

---

**维护团队**: Riz 产品团队
**最后更新**: 2025-11-23