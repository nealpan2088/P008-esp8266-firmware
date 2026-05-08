# ESP8266 DHT22 固件 v3.2 规格说明（定风电池版）

> 适用版本：v3.2（2026-05-02）
> 硬件：NodeMCU V3 (ESP8266) + DHT22
> 场景：温湿度环境监控（电池供电，deepSleep 低功耗）
> 编译环境：`[env:nodemcuv3-battery]`（`platformio.ini`）

---

## 一、核心设计

### 1.1 功耗模式

| 阶段 | 功耗 | 持续时间 | 说明 |
|------|------|----------|------|
| deepSleep 休眠 | ~10 µA | 5 分钟（`DEEP_SLEEP_US=300000000`） | LED 熄灭，ESP 深度睡眠 |
| 唤醒 → WiFi 连网 | ~80 mA | 2~5 秒 | 视信号强度 |
| 传感器读取 | ~5 mA | 100 ms | DHT22 单次采样 |
| HTTP POST 上报 | ~170 mA | 1~2 秒 | 含响应等待 |
| **单次循环总能耗** | — | **~5 分钟 + 10 秒** | 98% 时间在 sleep |

**一组 CR2032 纽扣电池理论续航**（DHT22 不吃电，仅 ESP8266）约 **6~12 小时**。
**两节 18650 锂电串联（7.4V 降压）** 搭配 AMS1117-3.3V 调压板，实测约 **1~3 天**。
**如需长期电池运行**，建议更换为 ESP32（深度睡眠功耗 ~5 µA，带 RTC 内存）。

### 1.2 启动流程

```
上电/唤醒
  │
  ├─ ESP.wdtEnable(8000ms)  ← 硬件看门狗 8 秒
  ├─ 序列号自动生成: DHT22-{chipId}
  ├─ 密钥自动生成: SHA256(chipId + HW_SECRET)
  ├─ WiFi 连网（最长 120 秒，超时即睡）
  │     ├─ 成功 → HTTP POST 上报
  │     │        ├─ Code:200 → deepSleep(5min)
  │     │        └─ 失败 → deepSleep(5min)
  │     └─ 超时 → deepSleep(5min)
  └─ WDT 8 秒兜底（极少触发）
```

### 1.3 上报数据格式

```json
{
  "temp": 22.5,
  "humidity": 74.3,
  "firmwareVer": "3.2",
  "chipId": "00C6264D",
  "power": "battery",
  "reportInterval": 60
}
```

## 二、支持/不支持的插电版功能

| 功能 | 插电版 v3.1 | 电池版 v3.2 | 原因 |
|------|------------|------------|------|
| DHT22 温湿度读取 | ✅ | ✅ | 共享受硬件 |
| 自动序列号+密钥 | ✅ | ✅ | 共用初始化 |
| SHA256 密钥生成 | ✅ | ✅ | 共用初始化 |
| HTTP 上报 | ✅ | ✅ | 核心功能 |
| 安全 fallback 重启 | ❌（loop） | ✅ 60s超时重启 | 防 sleep 失败 |
| 云端动态配置 | ✅ | ❌ | sleep 清空 RAM |
| 离线 RAM 缓存 | ✅ | ❌ | sleep 清空 RAM |
| 停用降频轮询 | ✅ | ❌ | sleep 周期固定 |
| 远程指令响应 | ✅ | ❌ | 后端处理 |

## 三、引脚定义

| 引脚 | GPIO | 用途 |
|------|------|------|
| D4 (LED) | GPIO2 | LED 指示灯（低电平亮） |
| D1 | GPIO5 | DHT22 DATA 引脚 |
| D0 | GPIO16 | **deepSleep 唤醒（→RST 跳线）** |

## 四、硬件要求

### 4.1 必需
- **GPIO16(D0) → RST 引脚短接**，否则 deepSleep 无法唤醒，芯片深度睡眠后永久"假死"
- 烧录时必须**断开该跳线**，否则 `Failed to connect to ESP8266: Timed out waiting for packet header`
- 烧录完成后再重新连接跳线

### 4.2 推荐供电方案
- **两节 18650（7.4V）+ AMS1117-3.3V 调压板**
- **三节干电池（4.5V）+ AMS1117-3.3V 调压板**
- NodeMCU V3 板载 3.3V LDO，输入电压范围 5V~12V（不建议极限使用）

## 五、编译与烧录

### Windows 命令
```powershell
& $env:USERPROFILE\.platformio\penv\Scripts\platformio.exe run -e nodemcuv3-battery -t upload --upload-port COM6
```

### 编译标志
```ini
[env:nodemcuv3-battery]
platform = espressif8266
board = nodemcuv3
framework = arduino
monitor_speed = 115200
build_flags = 
  -D SERIAL_BAUD=115200
  -D DEEP_SLEEP_US=300000000
  -D DHT_PIN=5
  -D LOG_LEVEL=3
  -D BATTERY_MODE=1
```

## 六、调试要点

| 症状 | 原因 | 解决 |
|------|------|------|
| 上传失败：`Timed out waiting for packet header` | D0→RST 跳线未断开 | 断开跳线重试 |
| 上传成功但无数据 | `setup()` 缺 strncpy 序列号 | 检查 `_autoSerial` 赋值 |
| 电池短时间耗尽 | WiFi 连网反复失败 | 取回改插电版部署 |
| 电脑不识别 USB | CH340 vs CP2102 驱动 | 装对应驱动（代码兼容） |

## 七、变体记录

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-05-02 17:00 | v3.2 (定风电池版) | 初版 deepSleep，独立 main-battery.cpp |
| 2026-05-02 17:15 | v3.2 (定风电池版) | 修复 setup 缺少 strncpy（401 bug） |
| 2026-05-02 17:20 | v3.2 (定风电池版) | 合并入 main.cpp，用 BATTERY_MODE 宏 |
