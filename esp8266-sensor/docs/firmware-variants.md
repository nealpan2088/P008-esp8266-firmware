# 固件变体配置说明

> 本文档记录所有预设场景的固件配置及编译环境。
> 编译时决定传感器组合，改场景需重新编译烧录。

---

## 配置矩阵

| 代号 | 环境名 | DHT22 | 门磁 | DS18B20 | 电流 | MQ-135 | 火焰 | 蜂鸣器 | 供电 | 上报间隔 |
|------|--------|-------|------|---------|------|--------|------|--------|------|----------|
| DHT-插电 | `fw-dht22-plug` | ✅ | - | - | - | - | - | - | USB | 60s |
| DHT门磁-插电 | `fw-dht22-door-plug` | ✅ | ✅ | - | - | - | - | - | USB | 60s |
| DHT-电池 | `fw-dht22-battery` | ✅ | - | - | - | - | - | - | 电池 | 5min deepSleep |
| DS18B20-插电 | `fw-ds18b20-plug` | - | - | ✅ | - | - | - | - | USB | 120s |
| MQ-135-插电 | `fw-mq135-plug` | - | - | - | - | ✅ | - | - | USB | 60s |
| 火焰报警-插电 | `fw-fire-alarm-plug` | - | - | - | - | - | ✅ | ✅ | USB | 事件驱动+60s心跳 |

---

## FW-DHT22-PLUG（DHT-插电）

**用途**：DHT22 温湿度监测，USB 供电常在线

**编译**：
```bash
pio run -e fw-dht22-plug -t upload
```

### 引脚

| 模块 | 引脚 |
|------|------|
| DHT22 DATA | D1 (GPIO5) |

### 上报数据格式

```json
{
  "temp": 22.3,
  "humidity": 65.0,
  "battery": 0,
  "otherData": {
    "firmwareVer": "3.1",
    "chipId": "002B6350"
  }
}
```

---

## FW-DHT22-DOOR-PLUG（DHT门磁-插电）

**用途**：DHT22 温湿度 + 门磁状态监测，USB 供电

**创建日期**：2026-05-02

**编译**：
```bash
pio run -e fw-dht22-door-plug -t upload
```

### 接线

| 模块 | → | 引脚 |
|------|---|------|
| DHT22 DATA | → | D1 (GPIO5) |
| 门磁 DATA | → | D5 (GPIO14) |
| 门磁 GND | → | 任意 GND（与 DHT22 共地） |

### 开关宏

```c
#define USE_DOOR_SENSOR 1    // 启用门磁
```

### 上报数据格式

```json
{
  "temp": 22.3,
  "humidity": 65.0,
  "battery": 0,
  "doorOpen": true,
  "otherData": {
    "firmwareVer": "3.1",
    "chipId": "002B6350",
    "event": "door_toggle"
  }
}
```

- `doorOpen`: 当前门状态（true=开, false=关）
- `event: "door_toggle"`: 仅在门状态变化时出现（后端可据此判断开关事件）

### 报警规则

后端 `alert-engine.js` 内置门磁检测规则：
- 连续 2 次上报 `doorOpen=true` → 触发"门未关好"报警

---

## FW-DHT22-BATTERY（DHT-电池）

**用途**：DHT22 温湿度监测，电池供电 deepSleep

**编译**：
```bash
pio run -e fw-dht22-battery -t upload
```

### 条件

⚠️ GPIO16(D0) **必须**接 RST——否则永远醒不来
⚠️ 配网后**必须断开串口**，串口供电会阻止 deepSleep

### 引脚

| 模块 | 引脚 |
|------|------|
| DHT22 DATA | D1 (GPIO5) |
| D0 | → RST（深睡唤醒） |

### 上报数据格式

```json
{
  "temp": 22.3,
  "humidity": 65.0,
  "battery": 1,
  "otherData": {
    "firmwareVer": "3.2",
    "chipId": "00C6264D",
    "power": "battery"
  }
}
```

---

## FW-DS18B20-PLUG（DS18B20-插电）

**用途**：DS18B20 全防水温度监测，USB 供电常在线

**创建日期**：2026-05-03

**编译**：
```bash
pio run -e fw-ds18b20-plug -t upload
```

### 接线

| 模块 | → | 引脚 |
|------|---|------|
| DS18B20 VCC | → | 3V3 |
| DS18B20 DATA | → | D3 (GPIO0) |
| DS18B20 GND | → | GND |

> ⚠️ DATA 与 VCC 之间需 4.7kΩ 上拉电阻（模块通常自带）

### 开关宏

```c
#define USE_DS18B20 1    // 启用 DS18B20
```

### 上报数据格式

```json
{
  "temp": 4.2,
  "sensor": "DS18B20",
  "battery": 0,
  "otherData": {
    "firmwareVer": "3.3",
    "chipId": "00FE7391",
    "power": "plug"
  }
}
```

- `sensor`: 标识为 DS18B20（便于后端/前端识别）
- 无 `humidity` 字段（DS18B20 不测湿度）

### 场景映射

后端自动匹配规则：
- `DS18B20-PL-{chipId}` → `COLD_STORAGE` 场景（冷库/冷柜）
- `DS18B20-{chipId}` → `GENERAL` 场景（兜底）

---

## 添加新变体

1. 在 `platformio.ini` 添加 `[env:fw-xxx-xxx]` 块（遵循命名规则）
2. 在 `config.h` 添加必要的 `#ifndef` 宏
3. 在 `main.cpp` 用 `#if` 宏隔离新功能代码
4. 更新本文档的配置矩阵和说明
5. 更新 `CHANGELOG.md`
6. 提交到 Git

---

## FW-MQ135-PLUG（MQ-135 空气质量-插电）

**用途**：MQ-135 空气质量传感器，监测 NH₃/NOₓ/苯系物/CO₂ 综合空气质量

**创建日期**：2026-05-04

**编译**：
```bash
pio run -e fw-mq135-plug -t upload
```

### ⚠️ 重要：必须加分压电路！

MQ-135 AO 输出 0~5V，而 ESP8266 ADC 最大输入 3.3V。直接接会永久损坏 ADC。

**分压电路**（10kΩ + 20kΩ，分压比 1:3）：
```
MQ-135 AO ──┬── 10kΩ ──┬── A0 (ESP8266 ADC)
            │           │
            │           └── 20kΩ ── GND
            │
            └── DO（不接，留作扩展报警）
```

- 5V × 20/(10+20) ≈ 3.33V ✅（安全范围内）
- 固件已内建分压比还原计算，后端拿到的 restore 电压即为真实值

### 接线

| 模块 | → | 引脚 | 备注 |
|------|---|------|------|
| MQ-135 VCC | → | 3V3 | 3.3V 供电可工作，预热略慢 |
| MQ-135 GND | → | GND | 共地 |
| MQ-135 AO | → | 分压电路 → A0 | ⚠️ 不可直连！ |
| MQ-135 DO | → | 不接 | 留作扩展报警 |

> MQ-135 加热丝需要预热 2~3 分钟数据才稳定，刚上电的前几次读数偏高。

### 开关宏

```c
#define USE_MQ135 1    // 启用 MQ-135
```

### 上报数据格式

```json
{
  "airQuality": 72.5,
  "rawAdc": 512,
  "battery": 0,
  "otherData": {
    "firmwareVer": "3.4",
    "chipId": "00FE7392",
    "sensor": "MQ-135"
  }
}
```

| 字段 | 说明 |
|------|------|
| `airQuality` | 综合空气质量分数 (0~100)，越高越好 |
| `rawAdc` | 原始 ADC 读数 (0~1024)，越低浓度越高 |
| `sensor` | 标识传感器类型为 MQ-135 |

### 空气质量控制参考

| airQuality 分数 | 含义 | 建议操作 |
|:---------------:|------|----------|
| 80~100 | 优 | 正常 |
| 60~80 | 良 | 注意通风 |
| 40~60 | 中 | 建议开窗/开排风 |
| 20~40 | 差 | 有明显异味，需处理 |
| 0~20 | 严重 | 强烈刺激气味，立即通风 |

### 场景映射

后端自动匹配规则：
- `AIR-PL-{chipId}` → `GENERAL` 场景（通用监测，60s 上报）
- 后续可在后台手动切换到 KITCHEN（厨房油烟场景）

### 已知限制

- ⚠️ 第一版不包含温度补偿（MQ-135 读数受温湿度影响），后续版本可加 DHT22 做补偿
- `airQuality` 分数为线性映射，非精确 ppm 浓度

---

## fw-relay-plug（继电器远程开关）

> 独立目录：`hardware/esp01-relay/`（ESP-01 模块，引脚与 NodeMCU 不同）

- **硬件**: ESP-01 / ESP-01S + 继电器模块
- **序列号**: `RELAY-PL-{芯片8位HEX}`
- **功能**: 通过 P008 平台远程控制继电器通断（开/关/重启）
- **工作原理**: 上电自动注册 → 每 10 秒轮询指令 → 执行后回执
- **详情**: 见 `hardware/esp01-relay/docs/README.md`

### 配置矩阵

| 参数 | 值 |
|------|-----|
| 环境名 | `fw-relay-plug` |
| 开发板 | `esp01_1m` |
| 供电 | USB 5V |
| 序列号前缀 | `RELAY-PL-` |
| 指令轮询间隔 | 10 秒 |
| 可用指令 | POWER_ON / POWER_OFF / REBOOT |
| 设备类型 | relay（无传感器，只有开关） |

### 引脚

| 引脚 | 连接 |
|------|------|
| GPIO0 | 继电器控制（模块板载已接好） |
| GPIO2 | LED（心跳指示） |

### 后端映射

```js
ALLOWED_SERIAL_PREFIXES: ['RELAY-']
SERIAL_TO_SCENE: [
  { prefix: 'RELAY-PL-', scene: 'GENERAL', defaultUsage: 'CUSTOM' },
]
```

---

## FW-FIRE-ALARM-PLUG（火焰报警-插电）

**用途**：火焰红外探测 + 有源蜂鸣器报警，可用于厨房/库房火灾预警

**创建日期**：2026-05-09

**编译**：
```bash
pio run -e fw-fire-alarm-plug -t upload
```

### ⚠️ 重要

- 火焰传感器 DO 是数字输出（0/1），**不需要 ADC，不需要分压电阻**
- 蜂鸣器低电平触发，默认为 HIGH（不响），检测到火才拉低
- 不支持电池版（编译时 `#error` 拦截）

### 接线

| 模块 | → | 引脚 | 备注 |
|------|---|------|------|
| 火焰传感器 VCC | → | 3V3 | 工作电压 3.3~5V ✅ |
| 火焰传感器 GND | → | GND | 共地 |
| 火焰传感器 DO | → | D1 (GPIO5) | LOW=检测到火焰 |
| 蜂鸣器模块 VCC | → | 3V3 | 工作电压 3.3~5V ✅ |
| 蜂鸣器模块 GND | → | GND | 共地 |
| 蜂鸣器模块 I/O | → | D2 (GPIO4) | 低电平触发 |

### 开关宏

```c
#define USE_FIRE_ALARM 1       // 启用火焰报警
#define FIRE_SENSOR_PIN 5      // D1 (GPIO5) — 火焰传感器 DO
#define BUZZER_PIN 4           // D2 (GPIO4) — 蜂鸣器 I/O
```

### 工作逻辑

| 场景 | 蜂鸣器 | 上报 |
|------|--------|------|
| 正常运行（无火） | 不响 | 每 60s 心跳上报 `fireDetected:false` |
| 检测到火焰 | 立即响 | 立即上报 `fireDetected:true, alarmActive:true` |
| 火焰持续存在 | 一直响 | 心跳保持上报（60s 间隔） |
| 火焰熄灭 | 延迟 30 秒关 | 立即上报 `fireDetected:false, alarmActive:true` |
| 延迟结束 | 关闭 | 下轮心跳上报 `alarmActive:false` |

### 上报数据格式

```json
{
  "fireDetected": true,
  "alarmActive": true,
  "sensor": "FIRE-ALARM",
  "battery": 0,
  "otherData": {
    "firmwareVer": "3.5",
    "chipId": "00FE7394",
    "deviceType": "FIRE"
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `fireDetected` | bool | true=检测到火焰, false=安全 |
| `alarmActive` | bool | true=蜂鸣器正在响, false=静音 |
| `sensor` | string | 固定为 `FIRE-ALARM` |

### 后端映射

```js
ALLOWED_SERIAL_PREFIXES: ['FIRE-']
SERIAL_TO_SCENE: [
  { prefix: 'FIRE-PL-', scene: 'GENERAL', defaultUsage: 'FIRE_ALARM' },
  { prefix: 'FIRE-',    scene: 'GENERAL', defaultUsage: 'FIRE_ALARM' },
]
```

### 场景建议

- **KITCHEN**：厨房火灾监控（推荐结合 DHT22 做温度辅助判断）
- **WAREHOUSE**：仓库火焰预警
- **GENERAL**：通用火焰检测
