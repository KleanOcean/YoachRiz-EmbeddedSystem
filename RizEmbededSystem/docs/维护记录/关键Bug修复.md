# 产品需求文档 (PRD) - 关键Bug修复

## 文档信息
- **项目名称**: Yoach1 嵌入式系统固件
- **文档版本**: v1.0
- **创建日期**: 2025-11-23
- **优先级**: P0 (最高优先级 - 需立即修复)
- **影响范围**: 系统稳定性、可靠性

---

## 1. 执行摘要

本文档描述了在 Yoach1 固件代码中发现的两个关键级别的并发控制bug。这些bug可能导致系统崩溃、数据损坏或完全无响应。需要立即修复以确保产品的稳定性和可靠性。

---

## 2. Bug #1: 互斥锁使用错误导致的竞态条件

### 2.1 问题描述

**Bug位置**:
- `src/main.cpp:304` (TERMINATE_MODE 处理)
- `src/main.cpp:402-406` (TOF对象检测处理)

**问题根源**:
共享变量 `hasTOFDetectionTask` 被声明为由 `xSensorMutex` 保护，但在两处代码中却错误地使用了 `xObjectDetectedMutex` 来访问该变量。

**当前错误代码**:
```cpp
// 位置 1: main.cpp:303-306
if (takeMutexWithLogging(xObjectDetectedMutex, 100, MODULE_MAIN, "ObjectDetected")) {
    hasTOFDetectionTask = false;  // ❌ 使用了错误的互斥锁！
    giveMutexWithLogging(xObjectDetectedMutex, MODULE_MAIN, "ObjectDetected");
    LOG_DEBUG(MODULE_MAIN, "Detection flags reset in TERMINATE mode");

// 位置 2: main.cpp:402-406
if (takeMutexWithLogging(xObjectDetectedMutex, 10, MODULE_TOF, "ObjectDetected")) {
    hasTOFDetectionTask = false;  // ❌ 使用了错误的互斥锁！
    giveMutexWithLogging(xObjectDetectedMutex, MODULE_TOF, "ObjectDetected");
    LOG_DEBUG(MODULE_TOF, "Object detection handled, task flag reset");
}
```

**变量保护设计** (main.cpp:68-72):
```cpp
SensorData sensorData = {0, 0, false, 0, 0};    // 应由 xSensorMutex 保护
bool hasTOFDetectionTask = false;               // 应由 xSensorMutex 保护 ✓
bool objectDetectedFlag = false;                // 应由 xObjectDetectedMutex 保护 ✓
```

### 2.2 问题影响

**严重程度**: 🔴 关键 (Critical)

**影响分析**:

1. **竞态条件 (Race Condition)**:
   - ProcessingTask (Core 0) 和 TOFSensorTask (Core 1) 可能同时读写 `hasTOFDetectionTask`
   - 没有正确的互斥保护，导致数据不一致

2. **可能的故障场景**:
   ```
   时间轴:
   T1: TOFSensorTask 获取 xSensorMutex，读取 hasTOFDetectionTask = true
   T2: ProcessingTask 获取 xObjectDetectedMutex (错误的锁!)
   T3: ProcessingTask 设置 hasTOFDetectionTask = false
   T4: TOFSensorTask 继续执行，认为 flag 还是 true
   T5: 结果：传感器读取可能被跳过或重复执行
   ```

3. **实际后果**:
   - 传感器检测失败或误检
   - 对象检测响应不稳定
   - 系统行为不可预测
   - 难以复现和调试的间歇性故障

4. **多核CPU风险**:
   - ESP32 使用双核架构 (Core 0 和 Core 1)
   - 不同核心间的内存可见性问题
   - 缓存一致性问题可能加剧bug影响

### 2.3 解决方案

**修复方法**: 使用正确的互斥锁 `xSensorMutex`

**修复后的代码**:

```cpp
// 位置 1: main.cpp:303-306 (TERMINATE mode)
if (takeMutexWithLogging(xSensorMutex, 100, MODULE_MAIN, "Sensor")) {  // ✅ 正确的锁
    hasTOFDetectionTask = false;
    giveMutexWithLogging(xSensorMutex, MODULE_MAIN, "Sensor");
    LOG_DEBUG(MODULE_MAIN, "Detection flags reset in TERMINATE mode");

    // radar 和 TOF 操作保持不变
    TOF_SENSOR.stopReading();
    TOF_SENSOR.resetDetection();
}

// 位置 2: main.cpp:402-406 (Object detection)
if (takeMutexWithLogging(xSensorMutex, 10, MODULE_TOF, "Sensor")) {  // ✅ 正确的锁
    hasTOFDetectionTask = false;
    giveMutexWithLogging(xSensorMutex, MODULE_TOF, "Sensor");
    LOG_DEBUG(MODULE_TOF, "Object detection handled, task flag reset");
}
```

### 2.4 验证方法

**测试步骤**:

1. **压力测试**:
   - 快速切换模式 (MANUAL → TERMINATE → RANDOM → TERMINATE)
   - 持续运行 1000+ 次循环
   - 检查是否有传感器读取失败

2. **并发测试**:
   - 在对象检测期间发送 TERMINATE 命令
   - 验证传感器任务正确停止
   - 检查是否有竞态条件迹象

3. **日志验证**:
   ```
   预期日志序列:
   [TOF] Acquired Sensor mutex
   [TOF] hasTOFDetectionTask set to false
   [TOF] Released Sensor mutex
   [MAIN] Acquired Sensor mutex
   [MAIN] Reading hasTOFDetectionTask (no race)
   [MAIN] Released Sensor mutex
   ```

4. **长期稳定性测试**:
   - 连续运行 24 小时
   - 监控内存使用
   - 检查是否有异常重启

---

## 3. Bug #2: 互斥锁无限等待导致的死锁风险

### 3.1 问题描述

**Bug位置**: `include/TF_Luna_UART.h:123-188`

**问题根源**:
TF_Luna_UART 类的所有 getter 函数使用 `portMAX_DELAY` 作为互斥锁超时时间，这意味着如果无法获取锁，任务将永远等待。

**当前错误代码**:
```cpp
// TF_Luna_UART.h - 多个函数都有此问题

uint16_t getAmplitude() {
    xSemaphoreTake(xLidarMutex, portMAX_DELAY);  // ❌ 无限等待！
    uint16_t amp = Lidar.u16Amp;
    xSemaphoreGive(xLidarMutex);
    return amp;
}

uint16_t getDistance() {
    xSemaphoreTake(xLidarMutex, portMAX_DELAY);  // ❌ 无限等待！
    uint16_t dist = Lidar.u16Dist;
    xSemaphoreGive(xLidarMutex);
    return dist;
}

bool isObjectDetected() {
    xSemaphoreTake(xLidarMutex, portMAX_DELAY);  // ❌ 无限等待！
    bool detected = objectDetected;
    xSemaphoreGive(xLidarMutex);
    return detected;
}

// ... 还有更多函数
```

### 3.2 问题影响

**严重程度**: 🔴 关键 (Critical)

**影响分析**:

1. **系统完全挂起**:
   - 如果持有锁的任务崩溃或陷入死循环
   - 所有尝试调用这些 getter 的任务将永远阻塞
   - 系统失去响应

2. **死锁场景示例**:
   ```
   场景 1: 任务崩溃
   T1: Task A 获取 xLidarMutex
   T2: Task A 因其他bug崩溃（未释放锁）
   T3: Task B 调用 getAmplitude()
   T4: Task B 永远等待... (系统挂起)

   场景 2: 中断问题
   T1: Task A 获取 xLidarMutex
   T2: 硬件中断处理延迟
   T3: Task B 等待锁
   T4: 看门狗定时器可能不会触发（任务在等待，不算死循环）
   ```

3. **调试困难**:
   - 无法通过串口日志诊断（串口也可能被阻塞）
   - 看门狗计时器可能无法检测到（任务处于等待状态）
   - 必须通过JTAG调试器才能诊断

4. **生产环境风险**:
   - 用户设备变砖
   - 需要硬件复位才能恢复
   - 损害产品声誉

### 3.3 解决方案

**修复方法**: 所有互斥锁操作使用合理的超时时间，并处理超时情况

**方案 A: 保守方案 (推荐用于生产环境)**

```cpp
// TF_Luna_UART.h - 为每个 getter 添加超时和错误处理

// 配置部分添加超时常量
#define LIDAR_MUTEX_TIMEOUT_MS 100  // 100ms 超时

uint16_t getAmplitude() {
    if (xSemaphoreTake(xLidarMutex, pdMS_TO_TICKS(LIDAR_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        uint16_t amp = Lidar.u16Amp;
        xSemaphoreGive(xLidarMutex);
        return amp;
    } else {
        LOG_ERROR(MODULE_TOF, "Failed to acquire mutex in getAmplitude() - timeout");
        return 0;  // 返回安全的默认值
    }
}

uint16_t getDistance() {
    if (xSemaphoreTake(xLidarMutex, pdMS_TO_TICKS(LIDAR_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        uint16_t dist = Lidar.u16Dist;
        xSemaphoreGive(xLidarMutex);
        return dist;
    } else {
        LOG_ERROR(MODULE_TOF, "Failed to acquire mutex in getDistance() - timeout");
        return 0;
    }
}

bool isObjectDetected() {
    if (xSemaphoreTake(xLidarMutex, pdMS_TO_TICKS(LIDAR_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        bool detected = objectDetected;
        xSemaphoreGive(xLidarMutex);
        return detected;
    } else {
        LOG_ERROR(MODULE_TOF, "Failed to acquire mutex in isObjectDetected() - timeout");
        return false;  // 超时时假设未检测到对象
    }
}

// 对所有其他 getter 函数应用相同的模式...
```

**方案 B: 激进方案 (更好的性能)**

使用原子变量或无锁数据结构（适用于简单的读操作）：

```cpp
// 对于简单的 uint16_t 读取，可以使用 std::atomic
#include <atomic>

class TF_Luna_UART {
private:
    std::atomic<uint16_t> atomic_amplitude;
    std::atomic<uint16_t> atomic_distance;
    std::atomic<bool> atomic_objectDetected;

public:
    uint16_t getAmplitude() {
        return atomic_amplitude.load(std::memory_order_acquire);
    }

    uint16_t getDistance() {
        return atomic_distance.load(std::memory_order_acquire);
    }

    bool isObjectDetected() {
        return atomic_objectDetected.load(std::memory_order_acquire);
    }
};
```

**推荐方案**: 方案 A（保守方案）
- 更安全、更容易验证
- 与现有代码架构兼容
- 100ms 超时对于传感器读取足够长

### 3.4 附加改进建议

**增加系统级保护**:

```cpp
// 在 main.cpp 中添加互斥锁健康检查

void checkMutexHealth() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 5000) {  // 每 5 秒检查一次
        lastCheck = millis();

        // 尝试获取所有关键互斥锁
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            xSemaphoreGive(xSensorMutex);
        } else {
            LOG_ERROR(MODULE_MAIN, "xSensorMutex appears to be stuck!");
            // 考虑系统重启或其他恢复措施
        }

        if (xSemaphoreTake(xLidarMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            xSemaphoreGive(xLidarMutex);
        } else {
            LOG_ERROR(MODULE_MAIN, "xLidarMutex appears to be stuck!");
            // 考虑系统重启或其他恢复措施
        }
    }
}

// 在 loop() 中调用
void loop() {
    checkMutexHealth();
    // ... 其他代码
}
```

### 3.5 验证方法

**测试步骤**:

1. **故障注入测试**:
   ```cpp
   // 测试代码：模拟锁持有时间过长
   void testMutexTimeout() {
       xSemaphoreTake(xLidarMutex, portMAX_DELAY);
       // 故意不释放锁
       delay(200);  // 超过 100ms 超时

       // 在另一个任务中调用 getAmplitude()
       // 应该在 100ms 后返回错误，而不是永远等待
   }
   ```

2. **性能测试**:
   - 测量修复前后的传感器读取延迟
   - 确保超时处理不影响正常操作
   - 典型读取应该在 1-10ms 内完成

3. **压力测试**:
   - 高频率调用 getter 函数 (>100 Hz)
   - 同时运行多个任务访问传感器数据
   - 验证没有死锁或性能下降

4. **日志监控**:
   ```
   正常情况：无超时错误日志

   异常情况（应该被捕获）：
   [TOF][ERROR] Failed to acquire mutex in getAmplitude() - timeout
   [MAIN][ERROR] xLidarMutex appears to be stuck!
   ```

---

## 4. 实施计划

### 4.1 优先级和时间表

| Bug | 严重程度 | 修复时间 | 测试时间 | 总计 |
|-----|---------|---------|---------|------|
| Bug #1: 互斥锁使用错误 | P0 | 30分钟 | 2小时 | 2.5小时 |
| Bug #2: 无限等待死锁 | P0 | 2小时 | 4小时 | 6小时 |
| **总计** | | **2.5小时** | **6小时** | **8.5小时** |

### 4.2 修复步骤

**阶段 1: Bug #1 修复** (立即开始)
1. 修改 main.cpp:304 和 main.cpp:402-406
2. 编译并上传固件
3. 执行基本功能测试 (30分钟)
4. 执行竞态条件压力测试 (1.5小时)

**阶段 2: Bug #2 修复** (Bug #1 完成后)
1. 修改 TF_Luna_UART.h 中所有 getter 函数
2. 添加 LIDAR_MUTEX_TIMEOUT_MS 常量
3. 编译并上传固件
4. 执行超时测试 (2小时)
5. 执行长期稳定性测试 (24小时后台运行)

**阶段 3: 代码审查和文档**
1. 同行代码审查
2. 更新技术文档
3. 添加单元测试（如果适用）

### 4.3 回归测试检查清单

- [ ] 所有游戏模式正常工作 (MANUAL, RANDOM, RHYTHM, TIMED, DOUBLE)
- [ ] OPENING 和 CLOSING 模式正常
- [ ] TERMINATE 命令立即停止传感器
- [ ] 对象检测响应正确
- [ ] BLE 连接和通信稳定
- [ ] 传感器基线校准正常
- [ ] LED 灯光控制正常
- [ ] 蜂鸣器工作正常
- [ ] 电池电量显示准确
- [ ] 系统可连续运行 24+ 小时无崩溃

### 4.4 风险评估

**修复风险**: 🟢 低

- 修改范围小且局部
- 不改变功能逻辑，只修正并发控制
- 向后兼容

**不修复风险**: 🔴 高

- 生产环境可能出现间歇性故障
- 用户体验差
- 难以调试和支持
- 可能导致产品召回

---

## 5. 成功标准

### 5.1 功能标准

✅ **必须满足**:
1. 所有互斥锁使用正确的保护范围
2. 没有无限等待的互斥锁操作
3. 超时情况有适当的错误处理
4. 所有现有功能保持正常

### 5.2 性能标准

✅ **必须满足**:
1. 传感器读取延迟 < 20ms (99百分位)
2. 对象检测响应时间 < 50ms
3. 模式切换时间 < 100ms
4. 系统可连续运行 48+ 小时无崩溃

### 5.3 质量标准

✅ **必须满足**:
1. 无编译警告
2. 通过所有单元测试
3. 通过压力测试 (1000+ 次模式切换)
4. 代码审查通过

---

## 6. 附录

### 6.1 参考文档

- ESP32 FreeRTOS 互斥锁文档
- TF-Luna 传感器数据手册
- Yoach1 系统架构文档
- 并发编程最佳实践

### 6.2 相关代码文件

- `src/main.cpp` - 主要任务逻辑
- `include/TF_Luna_UART.h` - 传感器接口
- `src/TF_Luna_UART.cpp` - 传感器实现
- `include/Global_VAR.h` - 全局配置

### 6.3 联系人

- **开发负责人**: [待填写]
- **测试负责人**: [待填写]
- **技术审核**: [待填写]

---

## 7. 变更历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| 1.0 | 2025-11-23 | Claude | 初始版本 - 描述两个关键bug |

---

**文档状态**: ✅ 待审核
**下一步行动**: 开发团队审核并开始实施修复
