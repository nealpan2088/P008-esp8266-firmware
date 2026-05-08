# P008 睿云智感 — 固件仓库

> 智能环境监测设备固件：ESP8266 传感器 + ESP32/A7670 短信网关

## 目录结构

```
├── esp8266-sensor/           ← 主传感器固件 (v3.x)
│   ├── src/                  固件源码（main.cpp + 传感器驱动）
│   ├── include/              配置头文件（config-*.h）
│   ├── docs/                 固件文档（变体说明、烧录指南、设计规范）
│   ├── platformio.ini        PlatformIO 编译配置
│   └── CHANGELOG.md          变更历史
├── a7670-notifier/           ← ESP32 + A7670C 短信报警网关
├── sct-current-test/         ← 电流互感器测试固件
├── *.md                      通用硬件文档
└── VERSION                   当前固件版本号
```

## 固件变体一览

| 变体 | 传感器 | 供电 | 序列号前缀 |
|------|--------|------|-----------|
| `fw-dht22-plug` | DHT22 | USB 插电 | DHT22-PL- |
| `fw-dht22-battery` | DHT22 | 电池 deepSleep | DHT22-BT- |
| `fw-dht22-door-plug` | DHT22 + 门磁 | USB 插电 | DHT22-PL- |
| `fw-ds18b20-plug` | DS18B20 | USB 插电 | DS18B20-PL- |
| `fw-mq135-plug` | MQ-135 | USB 插电 | MQ135-PL- |

## 快速编译

```bash
cd esp8266-sensor
pio run -e fw-dht22-plug -t upload
```

详见各子系统 README。

## 相关仓库

- **主项目（后端+前端）**: [P008-env-monitor](https://github.com/nealpan2088/P008-env-monitor)
- **ESP32 固件**: [P008-esp32-firmware](https://github.com/nealpan2088/P008-esp32-firmware)

---

> 版本号：固件版本独立管理，与主项目版本无关联。
