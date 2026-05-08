# 固件开发文档

## 目录结构

```
hardware/esp8266-sensor/
├── src/
│   └── main.cpp          # 主程序
├── include/
│   └── config.h          # 编译时配置（传感器开关、引脚、时间）
├── firmware-variants/    # 各场景独立配置
│   └── config-dht22-test.h
├── lib/                  # 可选：第三方库补丁
├── platformio.ini        # PlatformIO 编译配置
├── CHANGELOG.md          # 固件版本历史
└── docs/
    ├── README.md         ← 本文档
    └── firmware-variants.md  # 固件变体说明
```

## 核心文件说明

### `src/main.cpp`
主程序入口，包含：
- 传感器读取（DS18B20 / DHT22 / SHT30）
- WiFiManager 配网
- HTTPS 上报
- 数据缓存（SPIFFS）
- 远程指令响应

当前版本：
- **v3.4（MQ-135 版）**：MQ-135 空气质量传感器，AO 经分压电路后接 ADC
- **v3.3（门磁版）**：DHT22 + 门磁，序列号规范化 `{类型}-{供电}-{芯片ID}`
- **v3.1（插电版）**：标准 60 秒间隔 WiFi 常连，支持云端动态配置、停用降频
- **v3.2（定风电池版）**：deepSleep 5 分钟一次，`#if BATTERY_MODE` 宏切换

## 版本管理规则

| 版本 | 含义 |
|------|------|
| v1.x.x | 初始框架 |
| v2.0.0 | WiFiManager + 动态配置 |
| v2.1.0 | 多传感器支持 + 本地缓存 + 远程指令 |
| v3.0.0 | SHA256 密钥 + 纯 RAM 缓存 + 停用检测 |
| v3.1.0 | 云端动态配置 + 定风稳定版 |
| v3.2.0 | 定风电池版（deepSleep，BATTERY_MODE 宏） |
| v3.3.0 | 门磁传感器 + 序列号命名规范化 + 固件自适应场景 |
| v3.4.0 | MQ-135 空气质量传感器 + 分压电路适配 |

## 文档清单

| 文档 | 说明 |
|------|------|
| `docs/README.md` | ← 本文档，固件开发概览 |
| `docs/firmware-v3.1-spec.md` | 插电版 v3.1 规格说明 |
| `docs/firmware-v3.2-spec.md` | 电池版 v3.2（定风电池版）规格说明 |
| `docs/firmware-variants.md` | 固件变体说明 |
| `docs/production-hardening.md` | 量产加固方案 |
| `docs/bulk-flashing.md` | 批量烧录指南 |
| `docs/firmware-decisions.md` | 技术决策记录 |
| `CHANGELOG.md` | 固件版本历史 |
