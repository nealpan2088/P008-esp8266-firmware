# P008 睿云智感 — 硬件固件目录

> 所有 IoT 传感器设备固件

## 目录结构

```
hardware/
├── esp8266-sensor/         # 主传感器固件（多传感器变体）
│   ├── src/main.cpp        # 主程序
│   ├── include/config.h    # 编译时配置（传感器开关、引脚、时间）
│   ├── platformio.ini      # PlatformIO 编译配置
│   ├── docs/               # 开发文档
│   ├── VERSION.md          # 版本管理说明
│   └── CHANGELOG.md        # 版本变更历史
├── sct-current-test/       # SCT-013-000 电流测试固件（独立）
│   ├── src/main.cpp
│   ├── platformio.ini
│   ├── VERSION.md
│   ├── README.md
│   └── CHANGELOG.md
└── README.md               # 本文档
```

## 固件一览

| 固件 | 硬件 | 传感器 | 当前版本 |
|------|------|--------|---------|
| `esp8266-sensor` | NodeMCU V3 | DHT22 / DS18B20 / MQ-135 / JW01-CO2 / 门磁 | v3.4 |
| `sct-current-test` | NodeMCU V3 | SCT-013-000（电流互感器） | v1.0 |

## 开发原则

1. **固件是上游，字段名一次定死** — 不改固件去适配大屏
2. **编译时决策** — 用 `#if` 宏隔离功能，不用 run-time `if` 分支
3. **后端归一化兜底** — 新旧字段映射在入库前处理
4. **优先兼容旧版大屏** — 新版去适配旧版格式

详细规范见各固件目录下的 `docs/firmware-standards.md`。
