# 固件变体配置说明

> 本文档记录所有预设场景的固件配置及编译环境。
> 编译时决定传感器组合，改场景需重新编译烧录。

---

## 配置矩阵

| 代号 | 环境名 | DHT22 | 门磁 | DS18B20 | 电流 | MQ-135 | CO2 | 火焰 | 蜂鸣器 | 供电 | 上报间隔 |
|------|--------|-------|------|---------|------|--------|:---:|------|--------|------|----------|
| DHT-插电 | `fw-dht22-plug` | ✅ | - | - | - | - | - | - | - | USB | 60s |
| DHT门磁-插电 | `fw-dht22-door-plug` | ✅ | ✅ | - | - | - | - | - | - | USB | 60s |
| DHT-电池 | `fw-dht22-battery` | ✅ | - | - | - | - | - | - | - | 电池 | 5min deepSleep |
| DS18B20-插电 | `fw-ds18b20-plug` | - | - | ✅ | - | - | - | - | - | USB | 120s |
| MQ-135-插电 | `fw-mq135-plug` | - | - | - | - | ✅ | - | - | - | USB | 60s |
| CO2-插电 | `fw-co2-plug` | - | - | - | - | - | ✅ | - | - | USB | 60s |
| 火焰报警-插电 | `fw-fire-alarm-plug` | - | - | - | - | - | - | ✅ | ✅ | USB | 事件驱动+60s心跳 |
| SCT-013电流-插电 | `fw-sct013-plug` | - | - | - | ✅ | - | - | - | - | USB | 变化监测+自适应心跳 |

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

---

## FW-CO2-PLUG（CO2-插电）

**用途**：JW01-CO2 二氧化碳浓度监测，USB 供电常在线

**编译**：
```bash
pio run -e fw-co2-plug -t upload
```

### 引脚

| 组件 | 引脚 | 说明 |
|------|:----:|------|
| JW01-CO2 TX | D6 (GPIO12) | 模块 A 口（← 接 NodeMCU RX） |
| JW01-CO2 RX | D7 (GPIO13) | 模块 B 口（→ 接 NodeMCU TX） |
| JW01-CO2 VCC | VU (5V) | A7670C 需 5V 供电 |
| JW01-CO2 GND | GND | 共地 |

### 序列号

| 规则 | 说明 | 示例 |
|------|------|------|
| `CO2-PL-{芯片8位HEX}` | 插电版 | `CO2-PL-00EBE370` |

### 设备用途

- 后端映射：`defaultUsage: 'CO2_MONITOR'`
- 默认报警规则：CO₂ > 1500ppm（需激活）

### 上报数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `co2` | int | CO₂浓度（ppm），取值范围 0~10000 |
| `temp` | float | 温度（°C），模块自带温度补偿 |
| `humidity` | float | 湿度（%），模块自带湿度补偿 |
| `otherData.firmwareVer` | string | 固件版本 `v3.5` |
| `otherData.sensor` | string | 固定为 `JW01-CO2` |

---

## FW-SCT013-PLUG（SCT-013 电流-插电）

**用途**：SCT-013 交流电流互感器，监测设备电流/功率，USB 供电常在线

**创建日期**：2026-05-11

**编译**：
```bash
pio run -e fw-sct013-plug -t upload
```

### ⚠️ 电路要求

SCT-013 输出交流信号（0~1V），**必须经过运放整流电路**转换为 0~3.3V DC 后接 A0。

```
SCT-013 输出(+) → 运放整流电路 → A0 (ESP8266 ADC)
SCT-013 输出(-) → GND（共地）
```

| 型号 | 量程 | 输出 | 固件换算 |
|------|------|------|---------|
| SCT-013-030 | 0~30A AC | 0~1V AC | I = (Vdc / 0.9) × 30 |
| SCT-013-050 | 0~50A AC | 0~1V AC | I = (Vdc / 0.9) × 50 |
| SCT-013-100 | 0~100A AC | 0~1V AC | I = (Vdc / 0.9) × 100 |

### 引脚

| 引脚 | 连接 |
|------|------|
| A0 | 运放整流电路输出（SCT-013 经整流后 DC 电压） |

### 开关宏

```c
#define USE_SCT013 1    // 启用 SCT-013 电流监测
```

### 工作逻辑

1. **开机自学习零漂**：30 个 ADC 采样点求均值作为基线（空载无电流时的本底值）
2. **1 秒采样**：每 1 秒读一次 ADC，减去基线估算电流
3. **滑动窗口均值**：10 点滑动平均去噪（滤除瞬间波动）
4. **变化检测上报**：当前均值与上次上报值差 > 0.1A → 立即上报
5. **自适应心跳**：

| 负载状态 | 条件 | 心跳间隔 |
|---------|------|:--------:|
| 空载 | 电流 < 0.5A | 30 分钟 |
| 轻载 | 0.5A ≤ 电流 < 5A | 5 分钟 |
| 重载 | 电流 ≥ 5A | 1 分钟 |

### 上报数据格式

```json
{
  "current": 1.2345,
  "voltage": 0,
  "power": 271.59,
  "battery": 0,
  "otherData": {
    "firmwareVer": "3.7",
    "channel": "self",
    "chipId": "00FE7395",
    "sensor": "SCT-013",
    "deviceType": "CURRENT"
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `current` | float | 有效电流值（A），4 位小数 |
| `voltage` | int | 固定 0（预留，未接电压采样） |
| `power` | float | 近似功率（W）= current × 220V |
| `otherData.sensor` | string | 固定为 `SCT-013` |
| `otherData.deviceType` | string | 固定为 `CURRENT`（后端自动映射到 CURRENT_METER） |

### 后端映射

```js
ALLOWED_SERIAL_PREFIXES: ['...', 'SCT-PL-']
SERIAL_TO_SCENE: [
  { prefix: 'SCT-PL-', scene: 'POWER_MONITOR', defaultUsage: 'CURRENT_METER' },
]
```

- 场景：`POWER_MONITOR`（用电监测，300s 心跳兜底）
- 用途：`CURRENT_METER`（电流计量）
- 默认报警：电流 < 0.5A（设备关机告警）

### 已知限制

- ⚠️ 不包含电压采样，功率计算基于固定 220V 估算
- ⚠️ 滑动窗口用近似 FIFO，每 15 个采样周期完全重置一次防止累积漂移
- ⚠️ 电流换算基于 SCT-013-30 型号，换型号需改 `main.cpp` 换算系数
- ⚠️ 不支持电池版（需要 1s 持续采样，不适合 deepSleep）

---

## FW-DS18B20-SCT013-PLUG（DS18B20 温度探头 + SCT-013 电流互感器，复合版）

**用途**：一块 8266 同时接 DS18B20 温度探头和 SCT-013 电流互感器，用于冷柜/设备用电监测

**创建日期**：2026-05-11

**固件版本**：雷电版 v1.0.7

**编译**：
```bash
pio run -e fw-ds18b20-sct013-plug -t upload
```

### ⚠️ 电路要求

**DS18B20**：模块自带 4.7kΩ 上拉电阻，直接接 3V3/GND/D3 即可。

**SCT-013**：输出交流信号（0~1V），**必须经过运放整流电路**转换为 0~3.3V DC 后接 A0。

### 接线

| 模块 | → | 引脚 |
|------|---|------|
| DS18B20 VCC | → | 3V3 |
| DS18B20 DATA | → | D3 (GPIO0) |
| DS18B20 GND | → | GND |
| SCT-013 信号线 | → | 运放整流电路 → A0 |
| SCT-013 地 | → | GND |

### 开关宏

```c
#define USE_DS18B20 1    // 启用 DS18B20
#define USE_SCT013 1     // 启用 SCT-013
```

### 序列号

| 规则 | 说明 | 示例 |
|------|------|------|
| `DS18B20-SCT-{芯片8位HEX}` | 复合板 | `DS18B20-SCT-001A6584` |

### 工作逻辑

复合版 loop 两个传感器独立运行：

1. **SCT-013 采样**：每 3 秒采 200 个样本（10kHz），连续计算 RMS 有效值
2. **DS18B20 读取**：在 report 间隔读取温度，首次失败 -127°C 后 delay(750ms) 重试一次
3. **上报**：温度 + 电流一起上报，带 `multiSensor: "ds18b20+sct013"` 标记

### 上报数据格式

```json
{
  "temp": 24.5,
  "current": 0.6832,
  "sensor": "DS18B20",
  "battery": 0,
  "otherData": {
    "firmwareVer": "1.0.7",
    "chipId": "001A6584",
    "power": "plug",
    "channel": "self",
    "multiSensor": "ds18b20+sct013",
    "current": 0.68
  }
}
```

### 后端映射

```js
ALLOWED_SERIAL_PREFIXES: ['...', 'DS18B20-SCT-']
SERIAL_TO_SCENE: [
  { prefix: 'DS18B20-SCT-', scene: 'POWER_MONITOR', defaultUsage: 'CURRENT_METER' },
]
```

### 约束

- 8266 单板最多 2 个传感器模块
- 复合变体代码放在 `#elif USE_DS18B20 && USE_SCT013` 独立块中
- 变量命名加 `comp` 前缀避免与 SCT013-only 块冲突

### 前端展示

- **TV 大屏**：按 `multiSensor` 拆成多张独立卡片（温度/电流）
- **设备列表**：展平为两行，分别显示 🌡️ 温度和 ⚡ 电流趋势图
- **SparkLineChart**：支持 `showCurrent` 模式画电流走势（绿色曲线）
