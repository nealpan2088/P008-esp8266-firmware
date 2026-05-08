# ESP8266 DHT22 固件 v3.1 规格说明

> 适用版本: v3.1（2026-05-02）
> 硬件: NodeMCU V3 (ESP8266) + DHT22
> 场景: 温湿度环境监控（USB 插电）

---

## 1. 核心逻辑

```
setup():  配WiFi → 连上保持在线
loop():   每 N 秒 → 读DHT22 → HTTPS上报 → 拉取云端配置 → 等待下次上报
```

每次上报后自动从云端获取 `reportInterval`，实时调整上报间隔。

---

## 2. 编译配置（config.h）

| 宏 | 默认值 | 说明 | 生产建议 |
|----|--------|------|---------|
| `API_BASE_URL` | `https://zghj.openyun.xin/api/v1` | 后端 API 地址 | 按客户改 |
| `HW_SECRET` | `P008@2026!SecretKey` | 密钥生成盐值，SHA256(chipId + HW_SECRET) | **量产必须改** |
| `DEEP_SLEEP_US` | `60000000` (60s) | 上报间隔（微秒），代码层已改为动态拉取，此值为本地默认 | 可不动 |
| `DHT_PIN` | 5 (GPIO5/D1) | DHT22 数据引脚 | 按接线改 |
| `DHT_TYPE` | 22 | DHT22 | 不要改 |
| `SERIAL_BAUD` | 115200 | 串口波特率 | 调试用 |
| `AP_NAME` | `P008-Env-Monitor` | 配网热点名 | 按客户改 |
| `LOG_LEVEL` | 3 | 0=静默 3=INFO 4=DEBUG | 量产建议 3 |

## 3. 代码常量（main.cpp）

| 常量 | 值 | 说明 |
|------|-----|------|
| `WIFI_TIMEOUT_MS` | 10000 (10s) | WiFi 连接超时 |
| `HTTP_TIMEOUT_MS` | 5000 (5s) | HTTP 请求超时 |
| `WDT_TIMEOUT_US` | 30000000 (30s) | 硬件看门狗 |
| `_reportIntervalMs` | 60000 (60s) | **动态值**，默认60秒，云端可改 |

---

## 4. 云端可调控参数

通过 `PUT /devices/:serial/config` 修改，设备下次上报后自动拉取生效：

```json
{
  "reportInterval": 60
}
```

| 参数 | 范围 | 单位 | 说明 |
|------|------|------|------|
| `reportInterval` | 10~3600 | 秒 | 两次上报间隔 |

---

## 5. 设备密钥

生成逻辑: `SHA256(chipId + HW_SECRET)`

- `chipId`: `ESP.getChipId()` — 芯片唯一 ID
- `HW_SECRET`: `config.h` 中定义的编译时常量
- 长度: 64 位 hex 字符
- 输出示例: `f63496f159d801912eb60f09a0a836b0a140ce4023df0d28b9549e70df3b1b`

**量产强制要求**: 每批烧录前修改 `HW_SECRET` 值，不同客户使用不同盐值。

---

## 6. 接线

| DHT22 | ESP8266 |
|-------|---------|
| 红 (VCC) | 3.3V |
| 黑 (GND) | GND |
| 黄 (DATA) | D1 (GPIO5) |

板上 LED 行为：
- 上电：亮
- WiFi 连接成功：灭
- 正常运行中：灭

---

## 7. 首次使用流程

1. 烧录固件 → 上电
2. 手机搜 WiFi 热点 `P008-Env-Monitor` 连上
3. 浏览器打开 `192.168.4.1`
4. 选择客户 WiFi → 输密码 → 保存
5. 自动重启，开始上报

**换 WiFi**: 按住 FLASH 按钮上电 3 秒 → 重新进入配网模式。

---

## 8. 量产烧录命令

```bash
git clone --branch v0.6.3 git@github.com:nealpan2088/P008-env-monitor.git
cd P008-env-monitor/hardware/esp8266-sensor
```

修改 `include/config.h`（至少改 `HW_SECRET` 和 `AP_NAME`）后：

```bash
pio run -e nodemcuv3-dht22 -t upload --upload-port COM3
```

---

## 9. 文件结构

```
hardware/esp8266-sensor/
├── src/main.cpp        # 主程序 (~200行)
├── include/
│   ├── config.h        # 编译配置
│   └── log.h           # 日志系统
├── platformio.ini      # 编译环境
├── CHANGELOG.md        # 固件版本历史
└── docs/
    ├── README.md
    ├── firmware-v3.1-spec.md      ← 本文档
    ├── firmware-decisions.md
    ├── firmware-variants.md
    ├── bulk-flashing.md
    └── production-hardening.md
```

---

## 10. 定风电池版 v3.2

> 独立固件文件：`src/main-battery.cpp`（插电版用 `src/main.cpp`）
> 编译环境：`pio run -e nodemcuv3-battery`

### 与定风版（loop）的核心差异

| 特性 | 定风版 v3.1 (loop) | 定风电池版 v3.2 (deepSleep) |
|------|-------------------|--------------------------|
| 代码量 | ~200 行 | ~150 行 |
| 供电 | USB 插电（持续 80mA） | 锂电池（休眠 20μA） |
| 续航 | 不限 | 18650 3000mAh ≈ 300 天 |
| 云端配置 | ✅ 动态拉取 | ❌ 不支持（状态丢失） |
| 缓存补发 | ✅ RAM 50 条 | ❌ 不支持 |
| 停用检测 | ✅ 连续 5 次 404 降频 | ❌ 不支持 |
| GPIO16→RST | 不需要 | **必须短接** |
| firmwareVer | 3.1 | 3.2 |
| body.otherData.power | undefined | "battery" |
| body.battery | 0 | 1 |

### 接线（电池版）

| 元件 | ESP8266 | 说明 |
|------|---------|------|
| DHT22 DATA | **D1 (GPIO5)** | 与定风版一致 |
| DHT22 VCC | 3.3V 或电池直供 | |
| DHT22 GND | GND | |
| **GPIO16** | **→ RST** | **deepSleep 唤醒必须** |
| 锂电池+ | VIN (5V 输入) | 通过 18650 充电模块 |
| 锂电池- | GND | |

### 烧录命令

```bash
# 电池版
pio run -e nodemcuv3-battery -t upload --upload-port COM3

# 定风版（插电）
pio run -e nodemcuv3-dht22 -t upload --upload-port COM3
```

### 功耗计算

| 阶段 | 电流 | 持续时间 |
|------|------|---------|
| 上报（WiFi 发射） | ~170mA | 约 3~5 秒 |
| deepSleep 休眠 | ~20μA | 5 分钟（DEEP_SLEEP_US=300s） |

18650 电池（3000mAh）理论续航：
```
每天唤醒次数: 24×60÷5 = 288 次
每天上报耗时: 288 × 4 秒 = 1152 秒 ≈ 0.32 小时
每天休眠时间: 24 - 0.32 = 23.68 小时
每天耗电: 170mA × 0.32h + 0.02mA × 23.68h ≈ 54.4 + 0.47 ≈ 55 mAh
续航: 3000mAh ÷ 55mAh/天 ≈ 54 天
```

**实际建议**：
- 上报间隔 `DEEP_SLEEP_US` 调大到 600s（10 分钟）→ 续航约 50 天
- 上报间隔 1800s（30 分钟）→ 续航约 200 天
- **推荐 10 分钟一次**，兼顾数据密度和续航

### 注意事项

1. **串口调试时不会 deepSleep**：串口供电时芯片不休眠。配网后拔掉 USB 线才是电池模式
2. **GPIO16→RST 必须接**：没接的话上电后一直跑 `loop()`，不睡，电池半天就没
3. **首次配网**：可以用 USB 供电，配好 WiFi 后拔 USB 上电池
4. **不支持动态配置**：deepSleep 后 RAM 清零，reportInterval 只能通过 `platformio.ini` 的 `DEEP_SLEEP_US` 编译时固定
