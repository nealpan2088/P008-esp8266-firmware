# ESP8266 传感器固件 — 睿云智感

P008 环境监测平台的物联网终端固件。

---

## 概述

ESP8266 (NodeMCU V3, ESP-12E, 4MB Flash) 物联网传感器固件。
支持多种传感器变体编译，单源文件多变体编译（`#if` 宏隔离）。
上报数据至「睿云智感」云平台（https://zghj.openyun.xin）。

## 当前状态

| 项目 | 值 |
|------|-----|
| 当前定风版 | **v4.1**（2026-05-12，OTA 生产稳定版 ✅） |
| 当前雷电版 | **v1.0.8**（2026-05-12，SCT-013 电流 + 复合版 ✅） |
| 编译框架 | PlatformIO（单 `platformio.ini`，16 个编译环境） |
| OTA 支持 | ✅ 已上线（`ENABLE_OTA` 宏，v4.1 起全部支持） |
| 在线设备 | 6 台（生产环境） |

## 目录结构

```
hardware/esp8266-sensor/
├── src/                    # 固件源码
│   ├── main.cpp            # 主程序入口（所有变体共享）
│   ├── config.h            # 全局编译配置（版本号、OTA 开关等）
│   ├── ConfigManager.h     # 云端配置拉取（reportInterval 等）
│   ├── Secrets.h           # 设备身份（序列号、API Key）
│   ├── Sensors.h/.cpp      # 传感器抽象基类
│   ├── DHT22Sensor.h/.cpp
│   ├── DS18B20Sensor.h/.cpp
│   ├── MQ135Sensor.h/.cpp
│   ├── CO2Sensor.h/.cpp
│   ├── FireAlarmSensor.h/.cpp
│   └── CurrentSensor.h/.cpp
├── include/
│   └── config-*.h          # 各变体专用编译配置（引脚定义等）
├── docs/
│   ├── firmware-standards.md   # 固件开发规范（铁律）
│   ├── firmware-variants.md    # 变体对应表 + 接线图（全矩阵）
│   ├── firmware-overview.md    # 固件架构总览
│   └── bulk-flashing.md        # 批量烧录方案
├── platformio.ini          # PlatformIO 编译环境定义（16 个 env）
├── CHANGELOG.md            # 完整版本变更历史
├── VERSION                 # 版本对照表（定风版 + 雷电版）
├── VERSION.md              # 版本管理说明
└── README.md               # 本文档
```

## 编译变体矩阵

| 变体名 | 传感器 | 供电 | 序列号前缀 | 用途 |
|--------|--------|------|-----------|------|
| `fw-dht22-plug` | DHT22 | 插电 | `DHT22-PL-` | 温湿度监测 |
| `fw-dht22-battery` | DHT22 | 电池 + deepSleep | `DHT22-BT-` | 温湿度监测（低功耗） |
| `fw-dht22-door-plug` | DHT22 + 门磁 | 插电 | `DHT22-PL-` | 温湿度 + 开关门监测 |
| `fw-ds18b20-plug` | DS18B20 | 插电 | `DS18B20-PL-` | 单温度探头 |
| `fw-mq135-plug` | MQ-135 | 插电 | `MQ135-PL-` | 空气质量 |
| `fw-co2-plug` | JW01-CO2 | 插电 | `CO2-PL-` | CO₂ 浓度监测 |
| `fw-fire-alarm-plug` | 火焰传感器 | 插电 | `FIRE-PL-` | 火焰报警 |
| `fw-sct013-plug` | SCT-013 | 插电 | `SCT-PL-` | 交流电流监测 |
| `fw-ds18b20-sct013-plug` | DS18B20 + SCT-013 | 插电 | `DS18B20-SCT-` | 温度 + 电流复合监测 |

## OTA 远程升级

v4.1 起支持 OTA 远程固件升级。

**流程**：
1. 管理后台 → 上传固件 → 激活
2. 点击设备 OTA 升级 → 后端写入 DeviceCommand
3. 设备下次上报后拉取命令 → HTTP 下载固件 → 校验 MD5 → 重启运行新版

**条件**：需在 `config.h` 中启用 `#define ENABLE_OTA 1`（默认已打开）

## 快速编译

```bash
# Windows PowerShell（使用完整路径）
& $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e fw-dht22-plug

# 带烧录
& $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e fw-dht22-plug -t upload --upload-port COM3
```

## 序列号规范

格式：`{传感器类型}-{供电方式}-{芯片MAC后8位HEX}`

示例：`DHT22-PL-00FE7390`

| 类型 | 编码 | 供电 | 编码 |
|------|------|------|------|
| DHT22 | `DHT22` | 插电（USB） | `PL` |
| DS18B20 | `DS18B20` | 电池（deepSleep） | `BT` |
| SCT-013 | `SCT` |  |  |
| DS18B20+SCT-013 | `DS18B20-SCT` |  |  |

## 上报数据格式

```json
{
  "temp": 26.1,
  "humidity": 67.2,
  "battery": 4.2,
  "otherData": {
    "deviceType": "TEMP_HUMID",
    "sensors": ["temp", "humid"],
    "firmwareVer": "4.1",
    "chipId": "00FE7390"
  }
}
```

**铁律**：
- 传感器自定义数据必须放 `otherData` 内
- `battery`、`temp`、`humidity`、`co2` 放顶层（后端预定义字段）
- 其他所有数据（current、doorOpen、airQuality 等）放 `otherData`
- 字段名一次定死不改，大屏适配固件（非固件适配大屏）

## 版本系列

| 系列 | 版本号格式 | 覆盖设备 |
|------|-----------|---------|
| **定风版** | x.x.x | DHT22、DS18B20、MQ-135、火焰、CO₂ |
| **雷电版** | 1.x.x | SCT-013 电流互感器、DS18B20+SCT-013 复合版 |

## 文档导航

- [固件开发规范](docs/firmware-standards.md) — 命名规则、代码风格、#if 隔离铁律
- [固件变体列表](docs/firmware-variants.md) — 全量变体矩阵 + 接线图 + 上报格式
- [固件架构总览](docs/firmware-overview.md) — 代码架构、批次流程、状态机
- [批量烧录方案](docs/bulk-flashing.md) — 产线烧录流程
- [后端序列号映射](apps/backend/src/config/constants.js) — `SERIAL_TO_SCENE`、`TV_CONFIG_MAP`

## 配套文档

- 后端 API 文档：[apps/backend/src/routes/](apps/backend/src/routes/)
- TV 大屏渲染逻辑：[apps/frontend/src/pages/tv/TvVintagePage.tsx](apps/frontend/src/pages/tv/TvVintagePage.tsx)
- 后端 TV 配置映射：[apps/backend/src/config/constants.js -> `TV_CONFIG_MAP`](apps/backend/src/config/constants.js)
