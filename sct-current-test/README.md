# P008 — SCT-013-000 电流测试固件

> 版本：v1.0 | 项目：P008 环境监测 SaaS | 硬件：NodeMCU v3 + SCT-013-000

## 概述

NodeMCU v3 + **SCT-013-000**（10A 电压输出型开合式电流互感器）测试固件。
专用于验证 SCT-013-000 的 ADC 采样精度和 P008 平台注册上报流程。

### 功能
- ADC 采样 2000 点计算 RMS 电流值
- 启动时自动校准偏置（无负载时采样 1000 次）
- 死区 0.1A 过滤空载噪声
- 自动注册到 P008 平台（序列号前缀 `SCT-TEST-`）
- 每 5 秒上报电流值（`otherData.currentA`）
- 串口实时打印调试日志

## 文件结构

```
hardware/sct-current-test/
├── README.md           ← 本文档
├── VERSION             ← 固件版本号
├── CHANGELOG.md        ← 变更日志
├── platformio.ini      ← PlatformIO 编译配置
├── include/
│   └── config.h        ← 配置宏
└── src/
    └── main.cpp        ← 主程序
```

## 引脚

| 引脚 | 信号 | 说明 |
|------|------|------|
| A0 | SCT-013 信号线（红线） | ADC 采样 |
| GND | SCT-013 GND（黑线） | |

**注意：** SCT-013 夹在用电设备的一根电源线上，零火不分正反。

## 编译烧录

```bash
cd hardware/sct-current-test
pio run -e nodemcuv3 -t upload --upload-port COM7
pio device monitor -p COM7 -b 115200
```

## P008 规范符合性

| 原则 | 状态 | 说明 |
|------|------|------|
| 序列号前缀 | ✅ | `SCT-TEST-` |
| 版本号上报 | ✅ | `firmwareVer="1.0"` |
| 密钥生成 | ✅ | SHA256(chipId + HW_SECRET) |
| 日志级别控制 | ✅ | `LOG_LEVEL=3` (INFO) |
| API 版本 | ✅ | `/api/v1/` |
| 配置宏化 | ✅ | 所有值 build_flags 覆盖 |

## 校准

SCT-013-000 参数：
- 量程：0-10A
- 输出：0-1V 直流（0.1V/A）
- 偏置：0.5V（无电流时输出）

如需校准，修改 `main.cpp` 中的：
- `ADC_BIAS_V` — 无负载时的偏置电压（默认 0.5V）
- `FACTOR` — 电压→电流系数（默认 10.0）
