
## v3.5.0（2026-05-09）

### ✨ 新功能
- **火焰探测 + 蜂鸣器报警变体** `fw-fire-alarm-plug`：新增 `#if USE_FIRE_ALARM` 宏
- **火焰红外传感器**：数字 DO 接口（LOW=有火, HIGH=安全），纯数字 IO 无需分压电阻
- **有源蜂鸣器**：低电平触发报警，检测到火立即拉低，火焰消失延迟 30 秒关闭
- **事件驱动上报**：状态变化（火警/蜂鸣器开关）立即上报，正常时 60s 心跳保活

### 🔧 新增配置
- **`config.h`**：`USE_FIRE_ALARM`、`FIRE_SENSOR_PIN`、`BUZZER_PIN`、`FIRE_ALARM_INTERVAL_MS`、`BUZZER_HOLD_MS`
- **`platformio.ini`**：`[env:fw-fire-alarm-plug]` 环境，引脚 D1/D2
- **`main.cpp`**：火焰读取 + 蜂鸣器控制 + 事件驱动上报逻辑

### 📊 上报字段
- `fireDetected`(bool)：火焰状态，`alarmActive`(bool)：蜂鸣器状态
- `sensor: "FIRE-ALARM"` 标识传感器类型
- 序列号格式：`FIRE-PL-{芯片8位HEX}`（e.g. `FIRE-PL-00FE7394`）

### ⚠️ 硬件要求
- 火焰传感器 DO 输出数字信号，**不需要分压电阻**
- 蜂鸣器低电平触发，初始化默认不响
- 仅支持 USB 插电版，不支持电池版（编译时 `#error` 拦截）

### 📦 固件依赖
- 仅需 `WiFiManager` 库，不依赖 DHT/OneWire/ArduinoJson 等

### 🔧 后端映射
- `constants.js`：新增 `FIRE-PL-` 和 `FIRE-` 序列号匹配规则
- 默认场景：`GENERAL`，用途：`FIRE_ALARM`

### 📝 文档
- `firmware-variants.md`：配置矩阵新增火焰报警行 + 完整接线说明 + 工作逻辑
