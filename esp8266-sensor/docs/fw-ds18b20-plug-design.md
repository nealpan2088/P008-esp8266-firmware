# FW-DS18B20-PLUG — DS18B20 全防水插电版（设计文档）

> 状态：📝 设计阶段，待潘哥确认
> 创建：2026-05-03

---

## 一、用途说明

DS18B20 全防水温度传感器模块（探头+模块带线），适用于：
- 冷库/冷柜温度监控（防水探头可直接放入水箱/冷柜）
- 液槽温度监测
- 室外/潮湿环境测温

不同于 DHT22（温湿度一体），DS18B20 只测温度，但精度更高（±0.5°C）、防水、可接线延长。

---

## 二、接线方案

### DS18B20 模块引脚（从右到左）

| 模块引脚 | 对应功能 | → | NodeMCU 引脚 |
|---------|---------|---|-------------|
| 右（VCC） | 电源 3.3V | → | 3V3 |
| 中（DATA） | 数据线 | → | **D3 (GPIO0)** |
| 左（GND） | 地线 | → | GND |

### 关键接线说明

- **DATA 与 VCC 之间需接 4.7kΩ 上拉电阻**（DS18B20 模块通常已自带，若没有需外加）
- 模块是 3.3V 供电，直接接 NodeMCU 的 3V3，不接 5V
- OneWire 总线允许多个 DS18B20 挂同一根数据线（扩展预留）

### 引脚对照表

| ESP8266 GPIO | NodeMCU 标号 | 连接 |
|-------------|-------------|------|
| GPIO0 | D3 | DS18B20 DATA（已预定义 ONE_WIRE_BUS=0） |
| — | 3V3 | DS18B20 VCC |
| — | GND | DS18B20 GND |

---

## 三、涉及到的设备选择

使用 **插电版 8266（USB供电）**，理由：
- DS18B20 需要持续供电测温（非 deepSleep 方式）
- 固定安装场景（放冷库/水箱旁），有 USB 电源
- 不涉及电池供电

**哪台 8266？** 潘哥决定：
- **选项A**：新拿一台 NodeMCU（推荐，不干扰现有 3 台 DHT22 设备）
- **选项B**：替换其中一台 DHT22 插电版
- **选项C**：替换有温度的门（DHT22+门磁）— 不推荐，门磁是独立的

建议用 **选项A**，新设备独立运行。

---

## 四、命名与场景映射

### 序列号前缀规范

```
DS18B20-PL-{chipId}
│        │
│        └─ PL = Plug（USB供电）
└─ DS18B20（传感器类型）
```

例：`DS18B20-PL-00FE7391`

### 后端场景映射（需追加到 SERIAL_TO_SCENE）

```javascript
// 在 constants.js 的 SERIAL_TO_SCENE 数组中插入（建议放第一层最前面）:
{ prefix: 'DS18B20-PL-', scene: 'COLD_STORAGE' },
```

理由：DS18B20 防水探头天生适合冷库/冷柜测温，直接映射 COLD_STORAGE 场景。
场景预设会应用：温度阈值 -5~8°C，实时告警模式。

---

## 五、固件改动清单

### 5.1 config.h — 补充 DS18B20 宏定义

```c
// --------------- DS18B20 温度传感器 ---------------
#ifndef USE_DS18B20
#define USE_DS18B20 0       // 0=不启用, 1=启用
#endif
#ifndef ONE_WIRE_BUS
#define ONE_WIRE_BUS 0      // D3 (GPIO0) — DS18B20 数据引脚
#endif
```

### 5.2 main.cpp — 增加 DS18B20 读取分支

主要改动点：
1. `#include <OneWire.h>` + `#include <DallasTemperature.h>`
2. 声明：`OneWire oneWire(ONE_WIRE_BUS); DallasTemperature ds18b20(&oneWire);`
3. 传感器读取函数中加 `#if USE_DS18B20` 分支：
   - `ds18b20.requestTemperatures();`
   - `float temp = ds18b20.getTempCByIndex(0);`
   - 温度有效判断：`temp != -127.0 && temp != 85.0`
4. 上报 JSON 中增加：`"sensor": "DS18B20"` 标识

### 5.3 platformio.ini — 新增编译环境

```ini
; ============================================================
; FW-DS18B20-PLUG — DS18B20 全防水插电版
;
; 用途：冷库/液槽防水测温，USB 供电常在线
;
; 接线:
;   DS18B20 VCC  → 3V3
;   DS18B20 DATA → D3 (GPIO0)
;   DS18B20 GND  → GND
;   DATA-VCC 之间需 4.7kΩ 上拉电阻（模块通常自带）
;   供电: USB 5V
;
; 编译上传:
;   pio run -e fw-ds18b20-plug -t upload --upload-port /dev/ttyUSB0
; ============================================================
[env:fw-ds18b20-plug]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
    tzapu/WiFiManager@^2.0.16
    paulstoffregen/OneWire@^2.3.7
    milesburton/DallasTemperature@^3.11.0

build_flags =
    -D SERIAL_BAUD=115200
    -D DEEP_SLEEP_US=60000000
    -D ONE_WIRE_BUS=0       ; D3 (GPIO0)
    -D LED_BUILTIN=2
    -D LOG_LEVEL=3
    -D USE_DS18B20=1
    -D DEVICE_SERIAL_PREFIX=\"DS18B20-PL-\"
```

### 5.4 firmware-variants.md — 更新配置矩阵

在配置矩阵中新增一行：

| 代号 | 环境名 | DHT22 | 门磁 | DS18B20 | 电流 | 供电 | 上报间隔 |
|------|--------|-------|------|---------|------|------|----------|
| DS18B20-插电 | `fw-ds18b20-plug` | - | - | ✅ | - | USB | 60s |

### 5.5 constants.js — 追加场景映射

```javascript
// 在第一层（精确匹配）追加：
{ prefix: 'DS18B20-PL-', scene: 'COLD_STORAGE' },
```

---

## 六、上报数据格式

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

说明：
- `temp`: 温度值（DS18B20 精度 ±0.5°C）
- 无 `humidity` 字段（DS18B20 不测湿度）
- `sensor: "DS18B20"` 标识传感器类型，方便后端/前端识别
- 后端已有的 `COLD_STORAGE` 场景告警规则（temp min -5 / max 8）可直接复用

---

## 七、实施步骤

| # | 步骤 | 预估时间 |
|---|------|---------|
| 1 | 潘哥确认本设计文档 | — |
| 2 | 修改 `config.h` 补充 DS18B20 宏 | 5 分钟 |
| 3 | 修改 `main.cpp` 增加 DS18B20 读取分支 | 15 分钟 |
| 4 | 修改 `platformio.ini` 新增编译环境 | 5 分钟 |
| 5 | 更新 `firmware-variants.md` 文档 | 5 分钟 |
| 6 | 更新后端 `constants.js` 追加场景映射 | 5 分钟 |
| 7 | 接线 → 编译 → 烧录 → 验证 | 15 分钟 |
| | **合计** | **约 50 分钟** |

---

## 八、待潘哥确认

1. **用哪台 8266？** 新拿一台 / 替换现有某台
2. **场景映射**：COLD_STORAGE 是否合适？还是需要其他场景？
3. **上报间隔**：默认 60s 是否太密？冷库建议 120s
4. **接线地点**：模块放哪里？（冷库 / 水箱 / 其他）
