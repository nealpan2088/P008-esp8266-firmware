# P008 睿云智感 固件文档索引

## 固件概述

ESP8266 传感器固件（定风版），支持多变体编译：

| 变体 | 环境名 | 传感器 | 供电 | 序列号示例 |
|:----:|--------|--------|:----:|-----------|
| DHT22 插电版 | `fw-dht22-plug` | DHT22 温湿度 | USB | `DHT22-PL-00FE7390` |
| DHT22 电池版 | `fw-dht22-battery` | DHT22 温湿度 | 锂电池 | `DHT22-9805C123` |
| DHT22 门磁版 | `fw-dht22-door-plug` | DHT22 + 门磁 | USB | `DHT22-PL-00FE7390` |
| **DS18B20 防水** | `fw-ds18b20-plug` | **DS18B20 温度** | USB | **`DS18B20-PL-00C370ED`** |
| **MQ-135 空气** | `fw-mq135-plug` | **MQ-135 空气质量** | USB | **`AIR-PL-xxxx`** |

## 版本

当前固件版本：**v3.4**

```
hardware/esp8266-sensor/VERSION
```

查看 [CHANGELOG](../CHANGELOG.md)

## 文档列表

| 文档 | 说明 |
|------|------|
| [README](./docs/README.md) | 固件文档入口 |
| [定风版规范](./docs/firmware-standards.md) | 编译隔离 / 命名规范 / 代码组织 |
| [固件变体清单](./docs/firmware-variants.md) | 所有变体的配置详细清单 |
| [场景预设](./docs/SCENE_PRESETS.md) | 场景配置 + 云端自适应 |
| [序列号到场景映射](./docs/SERIAL_TO_SCENE.md) | 前缀匹配规则 |
| [DS18B20 设计文档](./docs/fw-ds18b20-plug-design.md) | DS18B20 防水传感器设计 |
| [Windows 烧录指南](./docs/fw-ds18b20-windows-guide.md) | 在 Windows 下烧录 DS18B20 固件 |
| [批量烧录](./docs/bulk-flashing.md) | 批量烧录方案 |
| [生产加固](./docs/production-hardening.md) | 生产环境安全加固 |
| [技术决策](./docs/firmware-decisions.md) | 历史技术决策记录 |

## 固件结构

```
hardware/esp8266-sensor/
├── include/
│   ├── config.h           # 全局配置 + 编译宏
│   ├── dht_scene_presets.h # 场景预设表
│   └── serial_to_scene.h  # 序列号→场景映射（备用）
├── src/
│   ├── main.cpp           # 主程序（#if 隔离所有变体）
│   └── OneWire.cpp        # DS18B20 读取库
├── lib/
│   ├── ConfigManager/     # 远程配置管理库
│   └── SceneManager/      # 场景管理库
├── test/                  # 单元测试
├── platformio.ini         # 编译环境配置
├── VERSION                # 固件版本号
└── CHANGELOG.md           # 固件变更记录
```

## 编译与烧录

```bash
# 安装依赖
pio pkg install

# 编译指定环境
pio run -e fw-dht22-plug

# 烧录
pio run -e fw-dht22-plug -t upload

# 查看串口输出
pio device monitor
```

> Windows 用户请参考 [Windows 烧录指南](./docs/fw-ds18b20-windows-guide.md)

## 架构原则

1. **编译时决策**：所有变体差异通过 `#if` 宏在编译时决定，无运行时分支
2. **单文件入口**：`main.cpp` 是唯一入口，通过 `#if` 包含不同逻辑
3. **序列号即身份**：`DEVICE_SERIAL_PREFIX` 定义设备类型，后端据此自动分配场景
4. **版本文件**：`VERSION` 文件是固件版本唯一来源，`main.cpp` 在启动时读取
5. **`config.h` 守卫**：末尾的 `#error` 确保必须定义 `DEVICE_SERIAL_PREFIX`
