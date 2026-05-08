# ESP8266 固件变更历史 — 睿云智感

> 固件路径：`hardware/esp8266-sensor/`
> 编译平台：PlatformIO

---

## v2.3.0（2026-05-01 22:50）

### 新增
- **分级日志系统**：`LOG_E` / `LOG_W` / `LOG_I` / `LOG_D` 四级，通过 `LOG_LEVEL` 宏控制（量产=3/INFO，调试=4/DEBUG）
- **`log.h` 轻量级日志头文件**：格式化输出 `[级别] [标签      ] 消息` 结构
- **`log.cpp` 循环缓冲区**（选配）：`LOG_RING_BUF_SIZE` 控制启用，量产默认关闭（0 RAM 占用）
- **config.h 新增日志配置**：`LOG_LEVEL`(默认3)、`LOG_RING_BUF_SIZE`(默认0)

### 规范
- 全部 73 处 `Serial.printf/println/print` 替换为分级 `LOG_X` 宏
- 错误/警告/信息/调试4个级别，日志格式统一：`[I] [WiFi     ] Connected, IP: 192.168.x.x`
- 去掉未使用变量 `finalKey`，消除编译 warning

## v2.2.0（2026-05-01 22:30）

### 量产加固
- **硬件看门狗（WDT）**：setup 开头 `ESP.wdtEnable(8000ms)`，防止死机/睡死不醒
- **`safeDeepSleep()`**：深度睡眠前 `ESP.wdtDisable()`，防止 WDT 在睡眠期误触发复位
- **`connectWiFiWithRetry()`**：多轮重试（`WIFI_RETRY_MAX=3`），每轮断 WiFi 重连 + WDT 喂狗，全失败进配网
- **report() HTTP 重试**：POST 失败（负值 httpCode）→ 断 WiFi 重连 → 重试 POST → WiFi 恢复失败则存 SPIFFS 缓存，下次唤醒补发
- **首次启动标记**：`_isFirstBoot = true`（之前漏设，导致首次设备信息不上报）

### 修复
- **`safeDeepSleep` 前向声明**：定义移到 `startConfigPortal()` 之前，修复编译错误
- **`httpPostWithRetry` 删除**：调用了不存在的 `httpPost()`，改用 report() 内部重试
- **WiFi.begin("") 空参数**：改为无参 `WiFi.begin()` 使用 SDK 存储凭据，修复 ESP8266 SDK 崩溃
- **HTTP 重试失败存缓存**：WiFi 恢复失败时补调 `cacheData()`，不再丢数据

## v2.1.0（2026-05-01）

### 新增
- **传感器开关体系**：config.h 新增 5 个宏（USE_DS18B20 / USE_DHT22 / USE_SHT30 / USE_CURRENT / USE_DOOR），按场景开关，未启用的传感器代码不编译
- **SHT30 高精度温湿度支持**：I2C 接口，自动切换门磁引脚到 GPIO12 避免冲突
- **首次启动上报设备信息**：POST 时带 firmwareVer / model / sensors / capabilities
- **本地数据缓存**：POST 失败时写入 SPIFFS（FIFO 队列，最多 60 条），下次唤醒先补发
- **远程指令响应**：从 POST 响应解析 pendingCommands，支持 SET_INTERVAL / SET_THRESHOLD / SET_ALERT_MODE / REBOOT
- **湿度上报**：POST body 新增 `humidity` 字段（DHT22 / SHT30 场景）
- **DHT22 测试固件**：独立环境 `nodemcuv3-dht22`，仅启用 DHT22（D4/GPIO2），其他传感器全关

### 修复
- **条件编译保护**：`readCurrent()` / `readDoor()` 调用处加 `#if USE_CURRENT` / `#if USE_DOOR`，关闭时正常编译
- **`url` 作用域修复**：`url.length()` 改为 `strlen(paramApiBaseUrl) > 0`，解决跨函数变量失效
- **`pinMode` 条件保护**：`DOOR_PIN` / `CURRENT_PIN` 的 pinMode 初始化加条件编译
- **config.h 缺少结尾 `#endif`**：文件末尾补 `#endif /* CONFIG_H */`，修复 `unterminated #ifndef`

### 场景预设搭配方案

| 场景名 | 传感器组合 | 适用 |
|--------|-----------|------|
| 🥩 冷库/冷链 | DS18B20 + 电流 + 门磁 | 冷柜内温度、压缩机启停、门状态 |
| 📦 仓库环境 | SHT30 + 门磁 | 环境温湿度、大门开关 |
| 🖥️ 机房温控 | SHT30 + 电流 + 门磁 | 环境温湿度、UPS负载、门禁 |
| 🍳 厨房明厨 | DS18B20 + SHT30 + 电流 + 门磁 | 冷柜温度+环境+设备+门 |
| 🌱 农业大棚 | SHT30 + 土壤湿度 | 环境温湿度、土壤含水量 |
| 🚪 门窗安防 | 门磁 + SHT30 | 门状态+环境监控 |

### 引脚规划（全功能模式）

```
GPIO0(D3)  → DS18B20 数据
GPIO4(D2)  → SHT30 SDA (I2C)
GPIO5(D1)  → SHT30 SCL (I2C) / 门磁(NC)
A0         → SCT-013 电流 或 土壤湿度（二选一）
GPIO12(D6) → 门磁(NC) —— 启用 SHT30 时自动切换
```

---

## v2.0.0（2026-04-30）

### 新增
- **智能配置动态化**：POST /data 响应中解析 config（reportInterval、alertInterval、thresholds）
- **超限加速上报**：数据超限时使用 alertInterval（更频繁），正常时用 reportInterval
- **WiFiManager 配网**：首次启动或长按 GPIO0 3秒进入配网 AP 页面

### 架构
- 基于 NodeMCU V3 (ESP8266)
- 传感器：DS18B20（温度）、SCT-013（电流）、MC-38（门磁）
- 通信：HTTPS POST JSON
- 唤醒：Deep sleep 模式

---

## v1.0.0（2026-04-20）

### 初始版本
- 基础框架：WiFi连接 → 传感器读取 → POST上报 → Deep sleep
- 支持 DS18B20 温度读取
- 支持 SCT-013 电流读取
- 支持门磁状态读取
- SPIFFS 持久化配置

## v2.3.1（2026-05-02 11:30）

### 新增
- **唤醒后整体超时保护**：`WIFI_TOTAL_TIMEOUT_MS=30000`（30秒），启动后超过30秒还没完成上报就直接深睡眠等下一轮
- **深睡眠间隔改为60秒**（之前5分钟），提高数据密度

### 修复
- **解决WiFi长时间卡住导致的数据空洞**：在WiFi重试循环中每次等待后检查总耗时，超时立即放弃
- HTTP重试后也检查超时，防止`http.POST()`多次重试累积超时

### 变更
- 配置新增：`WIFI_TOTAL_TIMEOUT_MS`（默认30000ms）

## v3.0.0（2026-05-02 13:30）

### 重构：彻底精简固件代码
- **从 v2.3.1 的 1146 行精简到 ~180 行**
- 代码量减少 84%，逻辑一目了然

### 删除的模块
- ❌ DS18B20、SHT30 支持（DHT22 专用固件）
- ❌ 电流互感器 (SCT-013) 检测
- ❌ 门磁 (MC-38) 检测
- ❌ MQTT 远程指令通道
- ❌ HTTPS 支持（统一走 HTTP）
- ❌ ArduinoJson 依赖（手工拼 JSON 串）
- ❌ SPIFFS 缓存队列
- ❌ 多轮 WiFi 重试（3 轮×40 次→一次性 20 次×0.5s=10 秒）
- ❌ WiFiManager 自定义参数面板（不再需要手动输序列号/密钥）

### 新增/简化
- ✅ **WiFi：最多等 10 秒(20×500ms)，超时直接进配网**
- ✅ **HTTP：5 秒超时，一次失败不重试，直接睡**
- ✅ **软件 10 秒超时宏 + 硬件 15 秒看门狗**—双重保障不死机
- ✅ 配网热点超时：从 10 分钟缩短到 2 分钟
- ✅ Flash 按钮 3 秒进入配网模式
- ✅ 序列号自动生成（DHT22-{chipId}），密钥自动生成

### v3.0.1（2026-05-02 14:20）
- **修复**：`safeDeepSleep()` → `safeDelayThenRestart()` 
  - 深睡眠 `ESP.deepSleep()` 需要 GPIO16→RST 短接才能唤醒
  - 改为 `delay(60s) + ESP.restart()`，不依赖硬件连线
  - 引脚未短接的板子也能正常循环工作

## v3.1.1（2026-05-02）— 定风版最终定型

### 新增
- **SHA256 设备密钥**：`SHA256(chipId + HW_SECRET)`，替代明文拼接 `chipId + flashChipId`
- **云端动态配置**：上报后 GET 拉取 `reportInterval`，实时调整间隔（10～3600秒）
- **离线缓存（纯 RAM）**：50 条环形队列，上报失败缓存、成功后补发（最多 5 条/次）
- **停用低功耗模式**：连续 5 次 404 后自动降为每 30 分钟轮询一次
- **重启后自动恢复**：被启用后下次轮询 Code:200 恢复 60 秒正常上报

### 变更
- `reportData()` 返回值从 `bool` 改为 `int code`，更精确区分 200/404/500/超时
- 缓存从 RTC 内存改为纯 RAM（`static struct`），去掉不可靠的 RTC_DATA_ATTR
- `fetchConfig()` 仅在 Code:200 时调用，减少无用请求

### 配置
- `config.h` 新增 `HW_SECRET`（密钥盐），移除 `FIRMWARE_VERSION`
- 新增源码常量：`REJECT_LIMIT=5`、`REJECT_POLL_MS=1800000`（30分钟）

---

**此版本为 DHT22 场景最终稳定版，命名"定风"（苏轼《定风波》）。**

### 重构: `safeDelayThenRestart()` → 正统 `loop()` 方案
- **去掉** `delay(60s) + ESP.restart()` 的 hack 方案
- **改为** 标准 `loop()`：WiFi 常连，每 60 秒读传感器 → 上报一次
- WiFi 断连自动重连，不用反复重启
- 看门狗 30 秒，只防死机不打扰正常流程
- 新增 `_lastReport` 计时保护，防止 millis() 溢出
- LED 熄灭表示正常运行中（非上电状态）
- **代码量**: ~150 行，逻辑一目了然

---

**此版本为 DHT22 场景稳定版，建议量产使用。**

---

## v3.2（2026-05-02）— 定风电池版

### 新增
- **定风电池版（deepSleep 版本）**：`#if BATTERY_MODE` 宏切换，单文件 `main.cpp` 同时维护插电/电池两个变体
- **deepSleep 低功耗模式**：每次唤醒读 DHT22 → WiFi 连网 → POST 上报 → sleep 5 分钟（`DEEP_SLEEP_US=300000000`）
- **安全 fallback**：`loop()` 内 `delay(60000) + ESP.restart()`，防止 deepSleep 失败无限空转
- **硬件看门狗配合**：sleep 前 `ESP.wdtDisable()`，setup 开头 `ESP.wdtEnable(8000ms)`
- **自动序列号+密钥**：`SHA256(chipId + HW_SECRET)`，插电/电池版两套 key 互不冲突
- **上报标识**：POST body 含 `power:"battery"`，后端可区分电池 vs 插电设备

### 变更
- 合并 `main-battery.cpp` → `main.cpp`，用 `#if BATTERY_MODE` 统一管理
- `platformio.ini` 新增 `[env:fw-dht22-battery]`（原 `nodemcuv3-battery`）
- `setup()` 必须调用 `strncpy(deviceSerial, _autoSerial, ...)` + `strncpy(deviceKey, _autoKey, ...)`（否则 URL 拼出 `/devices//data` → 401）

### 硬件要求
- **GPIO16(D0) → RST 跳线**：deepSleep 唤醒必需
- ⚠️ **烧录时必须断开跳线**，否则 `Failed to connect to ESP8266: Timed out waiting for packet header`
- 烧录完成后重新连接跳线，上电即进入 deepSleep 循环

### 不支持的插电版功能
- ❌ 云端动态配置（`fetchConfig`）— deepSleep 每次醒来重新初始化
- ❌ 离线缓存（RAM cache）— sleep 清空 RAM
- ❌ 停用降频轮询 — sleep 周期固定 5 分钟
- ❌ 远程指令响应 — 后端处理，固件不解析


## v3.3（2026-05-02）— 门磁+命名规范化

### 新增
- **门磁传感器变体** `fw-dht22-door-plug`：新增 `#if USE_DOOR_SENSOR` 宏
- **`generateIdentity()` 机制**：自动生成 `{前缀}{chip8位HEX}` 格式序列号
- **序列号命名新规范**：`{类型}-{供电}-{芯片8位HEX}`（如 `DHT22-PL-00FE7390`）
- **`config.h` 底部 `#error` 守卫**：忘记设置 `DEVICE_SERIAL_PREFIX` → 编译失败

### 变更
- **env 命名标准化**：`nodemcuv3-dht22` → `fw-dht22-plug`，`nodemcuv3-battery` → `fw-dht22-battery`
- **`DEVICE_SERIAL_PREFIX` 宏**：每个 env 的 `build_flags` 指定前缀字符串
- **`generateIdentity()`** 使用 `DEVICE_SERIAL_PREFIX` 替代硬编码 `DHT22-`
- **门磁引脚**：GPIO14(D5)，不与 LED(BUILTIN=GPIO2) 冲突
- **删除 `firmware-variants/` 目录**：8 个旧 config 头文件清理
- **PlatformIO string 转义**：`build_flags = -D DEVICE_SERIAL_PREFIX=\\\"DHT22-PL-\\\"`
- **WiFi 配网超时 120s→300s**，WiFi 连接超时 10s→30s

### 文档
- `docs/firmware-standards.md`：全新，8 章完整规范体系
- `docs/firmware-variants.md`：重写，含接线图+编译方法+数据格式
- `docs/firmware-decisions.md`：架构决策记录

### 固件自适应后端场景（v0.6.6 后端配合）
- `SERIAL_TO_SCENE` 三层匹配：精确前缀→细分场景 / 中间态→GENERAL / 兜底→GENERAL
- 添加新变体的三步流程：定前缀→通后端→落文档

### 自动化约束
| 层级 | 机制 | 状态 |
|:----:|------|:----:|
| 🚨 编译失败 | `config.h` `#error` 守卫 | ✅ |
| ⚠️ 启动警告 | 后端 `checkSerialMappings()` | ✅ |
| 🔔 提交提示 | `.githooks/pre-commit` | ✅（需 `git config core.hooksPath .githooks`） |
| 📋 人工兜底 | `firmware-standards.md` 第八章清单 | ✅ |

### 硬件设备验证
- 第三块板 `DHT22-PL-00FE7390`（"有温度的门"）编译烧录成功
- 序列号 `DHT22-PL-00FE7390` 首次 auto-register 通过
- 门磁状态 `doorOpen: true/false` 上报正常
- 后端返回 `reportInterval = 300s`（修复后走 GENERAL 场景 60s）
- ⚠️ 手动 UPDATE 数据库 repairInterval 60s & sceneType GENERAL

### 红线注意事项
- D0→RST 跳线烧录时必须断开
- 旧格式 `DHT22-` 序列号不再生成新设备（仅兼容已有两台）
- PlatformIO string 转义必须用 `\\\"` 三层反斜杠

---

## v3.4（2026-05-04）— MQ-135 空气质量传感器

### 新增
- **MQ-135 空气质量传感器变体** `fw-mq135-plug`：新增 `#if USE_MQ135` 宏
- **`config.h`**：新增 `USE_MQ135`、`MQ135_PIN`、`MQ135_RL`、`MQ135_R0` 宏定义
- **`main.cpp`**：MQ-135 ADC 读取 + 分压还原 + `airQuality` 分数计算
- **上报字段**：`airQuality`(0~100 综合分数) + `rawAdc`(原始 ADC 值) + `sensor:"MQ-135"`
- **序列号格式**：`AIR-PL-{chip8位HEX}`（e.g. `AIR-PL-00FE7392`）

### 硬件要求
- ⚠️ **必须加分压电路（10kΩ + 20kΩ）**，MQ-135 AO 5V 输出会烧 8266 ADC
- MQ-135 预热 2~3 分钟数据才稳定
- 仅支持 USB 插电版，不支持电池版（编译时 `#error` 拦截）

### 固件代码
- `reportData()` 参数改为 `float val1, float val2`，MQ-135 使用 `val1=rawAdc, val2=score`
- 三级编译分支：`#if USE_MQ135` > `#elif USE_DS18B20` > 默认 DHT22
- MQ-135 跳过离线缓存（数据波动大，缓存意义小）
- 编译环境不含 ArduinoJson / DHT / OneWire 依赖（MQ-135 纯 ADC）

### 文档
- `firmware-variants.md`：配置矩阵 + 完整接线图 + 分压电路说明 + 数据格式 + 空气质量控制参考表
