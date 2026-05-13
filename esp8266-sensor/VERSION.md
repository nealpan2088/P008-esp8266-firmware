# ESP8266 传感器固件 — 版本管理

> 最后更新：2026-05-10

## 概览

**位置**：`hardware/esp8266-sensor/`
**编译平台**：PlatformIO（`platformio.ini`）
**当前版本**：v3.4（`config.h` 中 `FIRMWARE_VERSION` 宏）
**硬件**：NodeMCU V3（ESP-12E，4MB Flash）
**通信**：WiFi + HTTPS（平台上报）

## 固件变体

| 变体名称 | 传感器 | 供电方式 | 上报间隔 | 序列号前缀 |
|---------|--------|---------|---------|-----------|
| `fw-dht22-plug` | DHT22 | 插电（USB） | 60s | `DHT22-PL-` |
| `fw-dht22-battery` | DHT22 | 电池（deepSleep） | 5min | `DHT22-BT-` |
| `fw-dht22-door-plug` | DHT22 + 门磁 | 插电 | 60s | `DHT22-PL-`（门磁变体） |
| `fw-ds18b20-plug` | DS18B20 | 插电 | 60s | `DS18B20-PL-` |
| `fw-mq135-plug` | MQ-135 | 插电 | 60s | `MQ135-PL-` |
| `fw-co2-plug` | JW01-CO2 | 插电 | 60s | `CO2-PL-` |

## 固件变体文档

详细信息见 [`docs/firmware-variants.md`](./docs/firmware-variants.md)（全量变体表格，415 行）。

## 版本演进

| 版本 | 日期 | 关键变更 |
|------|------|---------|
| v3.4 | 2026-05-02 | DS18B20 门禁警戒模式 + 场景智能抑制 |
| v3.3 | 2026-05-02 | MQ-135 空气质量 + 继电器控制框架 |
| v3.2 | 2026-05-02 | 门磁独立场景 + CO2 变体备用 |
| v3.1 | 2026-05-02 | 电流监测 + 自动注册 + 电池版深度睡眠 |
| v3.0 | 2026-05-01 | DHT22 首个功能验证（温度/湿度） |
| v2.3 | 2026-05-01 | 分级日志系统（LOG_E/W/I/D） |
| v2.2 | 2026-05-01 | 量产加固（WDT/重试/SPIFFS 缓存） |

详细变更见 [`CHANGELOG.md`](./CHANGELOG.md)。

## 固件开发规范

参见 [`docs/firmware-standards.md`](./docs/firmware-standards.md)：

- 命名规则：`{类型}-{供电}-{芯片8位HEX}`
- 编译时决策：`#if` 宏隔离，不用 run-time `if` 分支
- 字段名定死不动（潘哥铁律 v2026-05-10）
- 序号由 chipId 决定，烧录不改变

## 部署方式

通过 PlatformIO 编译烧录，详见各 var 环境的 `platformio.ini`。
批量烧录方案：[`docs/bulk-flashing.md`](./docs/bulk-flashing.md)
