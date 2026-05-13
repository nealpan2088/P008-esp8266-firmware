# 固件综述 — 睿云智感环境监测

> 基于 ESP8266（NodeMCU）的多传感器固件体系。
> 采用 PlatformIO 编译，支持温湿度/温度探头/电流/空气质量/火焰/CO2 等多种传感器。
> 通过 WiFi 上报数据，支持 WiFiManager 配网、远程指令、OTA 预留。

---

## 架构总览

```
hardware/esp8266-sensor/
├── src/
│   ├── main.cpp              # 主程序（插电版/电池版通用，宏隔离）
│   ├── main-battery.cpp      # 电池版独立主程序（deepSleep 模式）
│   ├── sct013-rms.cpp/.h     # SCT-013 电流互感器 RMS 采样（雷电版）
│   └── relay-control.cpp/.h  # 继电器控制模块
├── include/
│   └── config.h              # 编译时配置（传感器开关/引脚/时间常量）
├── firmware-variants/        # 各变体独立配置文件
│   ├── config-dht22-test.h
│   ├── config-dht22-plug.h
│   ├── config-dht22-battery.h
│   ├── config-dht22-door-plug.h
│   ├── config-ds18b20-plug.h
│   ├── config-mq135-plug.h
│   ├── config-co2-plug.h
│   ├── config-fire-plug.h
│   ├── config-sct013-plug.h
│   └── config-relay-plug.h
├── lib/                      # 可选：第三方库补丁
├── platformio.ini            # PlatformIO 编译配置（含各环境定义）
├── VERSION                   # 当前版本号
├── CHANGELOG.md              # 版本变更历史
└── docs/                     # 文档目录
```

## 版本体系

### 两线独立版本

| 线系 | 版本号 | 定位 | 主要传感器 |
|------|--------|------|-----------|
| **定风版** | `3.x.x` | 通用环境监测 | DHT22 / DS18B20 / MQ-135 / 门磁 / CO2 / 火焰 |
| **雷电版** | `1.x.x` | 电流/功率监测 | SCT-013 电流互感器 |

两线互不干扰，各自独立维护版本号。

### 版本规则

| 版本 | 里程碑 |
|------|--------|
| v1.x.x | 雷电版初始框架 |
| v2.0.0 | WiFiManager + 动态配置 |
| v2.1.0 | 多传感器支持 + 本地缓存 + 远程指令 |
| v3.0.0 | SHA256 密钥 + 纯 RAM 缓存 + 停用检测 |
| v3.1.0 | 云端动态配置 + 定风稳定版 |
| v3.2.0 | 定风电池版（deepSleep，BATTERY_MODE 宏） |
| v3.3.0 | 门磁传感器 + 序列号命名规范化 + 固件自适应场景 |
| v3.4.0 | MQ-135 空气质量传感器 + 分压电路适配 |
| v3.5.0 | JW01-CO2 二氧化碳传感器 + 编译架构修复 |
| v3.6.0 | 雷电版 RMS 有效值采样 + 自适应变化阈值 v2 |

## 固件变体

当前支持的编译变体（`platformio.ini` 中的 `[env]`）：

| 环境名 | 传感器 | 供电 | 用途 |
|--------|--------|------|------|
| `fw-dht22-plug` | DHT22 温湿度 | USB 插电 | 通用温湿度监测 |
| `fw-dht22-battery` | DHT22 温湿度 | 锂电池 | 电池供电温湿度 |
| `fw-dht22-door-plug` | DHT22 + 门磁 | USB 插电 | 冷库/仓库门+温湿度 |
| `fw-ds18b20-plug` | DS18B20 温度探头 | USB 插电 | 低温/特殊测温场景 |
| `fw-mq135-plug` | MQ-135 空气质量 | USB 插电 | 气体质量监测 |
| `fw-co2-plug` | JW01-CO2 二氧化碳 | USB 插电 | CO2 浓度监测 |
| `fw-fire-plug` | 火焰传感器 + 蜂鸣器 | USB 插电 | 火灾预警 |
| `fw-sct013-plug` | SCT-013 电流互感器 | USB 插电 | 电流/功率监测 |
| `fw-relay-plug` | 继电器（可带传感器） | USB 插电 | 远程开关控制 |

> 完整变体说明见 [firmware-variants.md](./firmware-variants.md)

## 序列号命名规范

**格式**：`{类型}-{供电}-{芯片8位HEX}`

| 前缀 | 供电 | 示例 |
|------|------|------|
| DHT22-PL- | 插电 | `DHT22-PL-00FE7390` |
| DHT22- | 电池 | `DHT22-2CF4322B6350` |
| DS18B20-PL- | 插电 | `DS18B20-PL-00C370ED` |
| DOOR-PL- | 插电门磁 | `DOOR-PL-00FE7390` |
| SCT-PL- | 插电电流 | `SCT-PL-00FE7390` |
| FIRE-PL- | 插电火焰 | `FIRE-PL-00FE7390` |
| CO2-PL- | 插电CO2 | `CO2-PL-00FE7390` |
| RELAY-PL- | 插电继电器 | `RELAY-PL-00FE7390` |

## 三层场景匹配

固件上报后，后端按序列号前缀推断场景：

1. **精确匹配** → 细分场景（如 `SCT-PL-` → `POWER_MONITOR`）
2. **中间态** → `GENERAL`（如 `DHT22-PL-` → `GENERAL`）
3. **兜底** → `GENERAL`（如 `DHT22-` → `GENERAL`）

匹配表见后端 `constants.js` 的 `SERIAL_TO_SCENE`。

## 编译要点

```bash
# 编译指定变体
pio run -e fw-dht22-plug

# 编译并烧录
pio run -e fw-dht22-plug -t upload

# 查看串口输出
pio device monitor -b 115200
```

### config.h 守卫
所有固件变体 `config.h` 必须设置 `DEVICE_SERIAL_PREFIX`，否则编译时报 `#error`。

### 编译时决策
使用 `#if` / `#elif` / `#endif` 宏隔离不同传感器逻辑，不采用 run-time 分支，减小固件体积。

## 部署流程

1. 选择所需传感器和供电方式 → 确认固件变体
2. 修改 `firmware-variants/config-{变体}.h`（如需调整引脚/时间）
3. 编译：`pio run -e fw-{变体}`
4. 烧录：`pio run -e fw-{变体} -t upload`
5. 上电 → WiFiManager 配网 → 自动注册 → 开始上报

> 批量烧录指南见 [bulk-flashing.md](./bulk-flashing.md)
> 量产加固方案见 [production-hardening.md](./production-hardening.md)

## 文档索引

| 文档 | 内容 |
|------|------|
| [README.md](./README.md) | 固件开发概览（文档目录结构） |
| [firmware-overview.md](./firmware-overview.md) | ← 本文档，固件综述 |
| [firmware-variants.md](./firmware-variants.md) | 完整变体列表、接线图、数据格式 |
| [firmware-standards.md](./firmware-standards.md) | 开发规范、命名规则、自动化约束 |
| [firmware-decisions.md](./firmware-decisions.md) | 技术决策记录、选型依据 |
| [production-hardening.md](./production-hardening.md) | 量产加固、安全防护 |
| [bulk-flashing.md](./bulk-flashing.md) | 批量烧录操作指南 |
| [fw-ds18b20-plug-design.md](./fw-ds18b20-plug-design.md) | DS18B20 插电版设计说明 |
| [fw-ds18b20-windows-guide.md](./fw-ds18b20-windows-guide.md) | Windows 下 DS18B20 烧录指南 |
| [rms-sampling-spec.md](./rms-sampling-spec.md) | SCT-013 RMS 采样技术规格 |
| [firmware-v3.1-spec.md](./firmware-v3.1-spec.md) | v3.1 版本技术规格 |
| [firmware-v3.2-spec.md](./firmware-v3.2-spec.md) | v3.2 版本技术规格 |
| **VERSION** | 当前版本号（纯文本文件） |
| **CHANGELOG.md** | 版本变更历史 |
