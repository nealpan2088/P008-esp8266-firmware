# ESP8266 固件变更历史 — 睿云智感

> 固件路径：`hardware/esp8266-sensor/`
> 编译平台：PlatformIO

## 雷电版 v1.0.8（2026-05-12）— OTA 远程升级支持 🚀

### 新增
- **OTA 远程升级**（`#if ENABLE_OTA` 宏控制）
  - `checkOTACommand()` 函数：轮询后端 DeviceCommand 表获取 OTA 指令
  - 解析 `payload.otaUrl`、`payload.md5`、`payload.version`
  - 调用 `ArduinoOTA.handle()` 或 `Update.begin()` 执行升级
  - loop() 每次迭代优先检查 OTA，不受上报间隔影响
- **云端参数防呆约束**：sct013* 参数在 fetchConfig 时做上限/下限/兜底校验
  - changeThreshold 下限 0.01A（防止 ADC 噪音触发）
  - heartbeat 范围 10-86400s
  - LoadA 范围 0.1-50A

### 修复
- **DHT22 分支 OTA 检查阻塞**：`if (elapsed < _reportIntervalMs)` 中的 `delay(100); return` 跳过了 OTA 检查
  - 修复：在 return 前加 `#if ENABLE_OTA checkOTACommand(); #endif`
- **后端 429 频率保护兼容**：电流设备 POST /data 最小间隔 3s，执行器 5s

### 变更
- **VERSION**：定风版 4.1 / 雷电版 1.0.8 两线独立版本
- **全量编译验证通过**：fw-dht22-plug, fw-dht22-battery, fw-dht22-door-plug, fw-ds18b20-plug, fw-ds18b20-sct013-plug, fw-mq135-plug, fw-co2-plug, fw-fire-alarm, fw-sct013-plug, fw-relay-plug
- **生产烧录**：所有在线设备已通过 OTA 升级到 v4.1 定风版

### 文档
- `docs/ota-upgrade-plan.md` — OTA 远程升级完整方案
- `docs/firmware-standards.md` — 增加 OTA 章节
- `docs/firmware-variants.md` — 配置矩阵增加 OTA 标记
- `CHANGELOG.md` — 雷电版 v1.0.8 条目

### Git
- 基亍定风版 v4.1 分支开发
- 功能开关宏隔离，失效时零开销

### 新增
- **DS18B20 + SCT-013 复合变体**（`fw-ds18b20-sct013-plug`）
  - 序列号前缀 `DS18B20-SCT-`
  - 编译环境：`fw-ds18b20-sct013-plug`（nodemcuv2，4MB flash）
  - 接线：DS18B20(D3/GPIO0), SCT-013(A0)
  - 复合版 loop：SCT-013 每 3s 采 200 个样本（10kHz RMS）+ DS18B20 定时读取
  - 上报 body：`{temp, current, sensor: "DS18B20", otherData: {multiSensor: "ds18b20+sct013", ...}}`

### 修复
- **复合板 loop 进入 SCT013-only 分支（根因）**：第 785 行 `#if USE_SCT013` 是独立 `#if`（非 `#elif`），复合板满足 `USE_SCT013=1` 先执行 SCT013-only 分支就结束，永远走不到复合 loop
  - 修复：`#if USE_SCT013` → `#if USE_SCT013 && !USE_DS18B20`（纯 SCT-013 板不受影响）
- **复合变体变量名冲突**：`elapsedSinceRms`/`code`/`isRateLimited` 与 SCT013-only 块重名
  - 修复：全部加 `comp` 前缀（`compRmsElapsed`/`compCode`/`compLimited`）
- **DS18B20 首次读取 -127°C**：复合版 loop 中第一次失败后 delay(750ms) 重试一次
- **429 限流退避**：收到 429 后设 `_rateLimitUntil = now + 3000`，后续循环检查退避期跳过上报
- **宏优先级修复**：复合版 body/初始化 `#elif USE_DS18B20 && USE_SCT013` 前置到 `#elif USE_SCT013` 之前

### 变体编译对照表（新增）
```
fw-ds18b20-sct013-plug    → DS18B20 温度探头 + SCT-013 电流互感器（复合板）
```

### 约束
- 8266 单板最多 2 个传感器模块
- 复合变体使用 `&&` 条件覆盖（非 `#elif` 互斥）

### 相关 commit
- `b3d6fcf` — `fix: 复合板 loop 进入 SCT013-only 分支的根因修复`
- `d5d0b58` — `fix: 复合版 loop 加 429 限流退避 + DS18B20 重读`
- `2ad0ee2` — `fix: 复合变体 reportData 和 setup 宏优先级`
- `1c26532` — `fix: 复合变体 loop 变量名冲突（elapsedSinceRms/code 重定义）`
- `a92d7ed` — `feat: DS18B20 + SCT-013 复合变体 fw-ds18b20-sct013-plug`

---

## v4.1（2026-05-12）— OTA 生产稳定版 ✅（当前最新）

### 新增
- **OTA 远程升级**（`ENABLE_OTA` 宏 + `checkOTACommand()`）🚀
  - 设备上报成功后顺便检查 OTA 指令，平时代价 = 0（不轮询）
  - HTTP 固件下载（`OTA_USE_HTTP` 宏，通过 HTTP 下载防 SSL 卡死）
  - MD5 完整性校验 + 双重保险
  - 升级成功自动重启，失败回退跑旧版

### 修复
- **OTA 轮询刷串口**：去掉独立轮询（之前 2s → 10s → 0），改为上报成功后检查
- **`checkOTACommand` 函数前向声明**：修复编译顺序问题
- **`reportStatus` 证书验证**：`setInsecure()` → `setTrustAnchors()` + BearSSL::X509List
- **DHT22 分支 OTA 检查漏执行**：修复 `elapsed < _reportIntervalMs` 内 return 前未调用 `checkOTACommand()`

### 变更
- **版本号重新锚定**：v4.1 为当前生产稳定版
- **条件编译隔离**：`#if ENABLE_OTA` — 已有变体完全不受影响
- **WiFiClientSecure 的 `setInsecure()` 保留**（`reportData`/`fetchConfig` 中），待后续统一改造

### 变体
- `fw-dht22-door-plug` → DHT22 + 门磁 + 插电（主要 OTA 测试机）
- **编译验证**：RAM 39.2% / Flash 44.5% ✅

### 文档
- `firmware-standards.md`：新增 OTA 章节
- `firmware-variants.md`：OAT 配置矩阵
- 此变更记录

### Git
- `192e2ec` — latest fix: 前向声明 + setCACert_P 回退
- `3f784dc` — refactor: 去掉 OTA 独立轮询 + reportStatus 证书
- `568e807` — fix: OTA 检查间隔 2s→10s

---

## v4.0（2026-05-12 12:50）— OTA 远程升级验证版

### 新增
- **OTA 远程升级**开山版：第一次全链路验证从 v3.99 → v4.0 成功
- 前端固件管理页面 + 设备列表 OTA 触发按钮
- 后端 Firmware 表 + firmware.routes.js（上传/激活/触发/DeviceCommand）

### 修复
- `checkOTACommand()` 在 loop() 内每 2s 轮询（后改为 10s 再到 0）

### Git
- `065c83d` — version bump to 4.0

---

## v3.99（2026-05-12 12:39）— OTA 功能开发中间版

### 新增
- **OTA 远程升级（调试中）**：ENABLE_OTA 宏 + `checkOTACommand()`
- 后端：Firmware 模型、上传/激活/OTA 触发 API
- 前端：固件管理页面、设备 OTA 按钮、在线调试日志

### 调试遗留（后续清理）
- OTA 轮询每 2s 检查（调试用）
- `setInsecure()` 在 checkOTACommand 中
- 临时调试日志

### Git
- `331e4ad` — version bump to 3.99


### 修复
- **WiFi 断连永久离线**：`loop()` 中 `connectWiFi()` 超时后 `ESP.restart()` 硬重启，防止设备变成死设备
- **SSL 握手卡死**：HTTPS POST 前喂 WDT（`ESP.wdtFeed()`），防止网络不稳时不返回

### 新增
- **开租户工具**：前端管理页 `/admin/tenant-creator`，支持一键创建买家账号+PRO 租户+项目+设备绑定
- **登录页图形验证码**：SVG 数学算式验证码（`3+7=?`），增加安全层
- **商品展示页**：`/listing-card.html` 挂闲鱼用

### 变更
- 仅插电版受 WiFi 断连修复影响，电池版（BATTERY_MODE）deepSleep 架构不受影响


>
> 版本管理说明：
> - **`1.x.x`** — 雷电版系列（SCT-013 电流互感器固件）
> - **`3.x.x`** — 定风版系列（DHT22/DS18B20/MQ-135/火焰/CO2 固件）
> - 两线独立版本，互不干扰

---

## 雷电版 v1.0.6（2026-05-11 15:41）

### 修复
- **保底心跳 + 限流退避**：新增 `_lastOkReport`（仅 200 时更新），`_rateLimitUntil`（429 后 3 秒退避）
- **云端参数防呆加固**：所有通过 fetchConfig 下发的 SCT 参数加上下限约束（阈值 0.05A~5.0A、心跳 60s~86400s、负载分界 0.1A~20.0A）
- **干烧保护**：后端改配置时，不合理值被固件兜底，不会导致固件跑飞

### 后端同步变更
- CURRENT_METER 限流：5s → 10s（配合固件退避）
- 后端通用异常值入库前过滤（>20A 或负值丢弃）

### 文档
- 防呆设计规范写入 firmware-standards.md

---

## 雷电版 v1.0.5（2026-05-11 14:28）

### Bug 修复
- **`*= 1000` 二次污染（根本原因）**：fetchConfig 每次运行时无条件 `*= 1000`，导致心跳间隔从 300s → 300000s → 300000000s... 永不上报
- **修复**：后端 `GET /config` 和 `POST /data` 响应中用 `??` 始终返回 SCT 边缘参数默认值；固件加 `static bool _sctHeartbeatConverted` 守卫，`*= 1000` 只执行一次
- **后端电流异常值过滤**：POST /data 入库前检查 `otherData.current` >20A 或 <0 时丢弃
- **后端通用异常值过滤**：temp >125°C / <-40°C、humidity >100% / <0%、co2 >10000ppm / <0 时丢弃

### Git
- `73dde0c` + `0019cd5` + `d9ced97` | tag: `firmware-sct013-v1.0.5`

---

## 雷电版 v1.0.4（2026-05-11 13:39）

### Bug 修复
- **心跳间隔初始值秒→毫秒**：初始值 `SCT013_HEARTBEAT_MEDIUM_MS` 用秒单位（300），fetchConfig 中 `*= 1000` 后变为 300000ms。但不完整——第二次 fetchConfig 仍会再乘 1000。
- **不完整修复**：v1.0.5 才彻底解决。

### Git
- `97859cb` | tag: `firmware-sct013-v1.0.4`

---

## 雷电版 v1.0.3（2026-05-11 10:48）

### 修复
- **自适应阈值收敛死循环**：重置噪声窗口时用绝对清零（`_noiseMin=999, _noiseMax=-999`），而非 `currentRms ± 0.1` 小窗口。有负载时小窗口会被持续撑大，阈值永远无法收敛。
- **RMS 安全过滤**：`_currentRms > 20.0 || _currentRms < 0.0` 时跳过自学习和上报，防止 GND 虚接导致的异常数据污染。
- **后端 SCT_CURRENT_MIN 0.08A → 0.2A**：匹配 SCT-013-000（100A:50mA）实际最小可测量

### Git
- `2450bf9` + `ec30c9c` | tag: `firmware-sct013-v1.0.3`（此标签不含后端改动，后端改动在 v0.10.1 内）

---

## 雷电版 v1.0.2（2026-05-11 09:52）

### 修复
- **SCT-013 上报字段位置修复**：`current` 从 JSON 顶层移到 `otherData` 内，前端统一从 `otherData.current` 读取
- **固件规范铁律**：firmware-standards.md 新增「上报字段放对位置」规范

### Git
- `5d6bb3d` | tag: `firmware-sct013-v1.0.2`

---

## v3.5（2026-05-10）— CO₂ 传感器 + 编译修复

### 新增
- **CO₂ 传感器变体 `fw-co2-plug`**：JW01-CO2 UART 模块，Serial1（RX=GPIO12, TX=GPIO13, 9600bps）
- **`config.h` 新增宏**：`USE_CO2`、`CO2_SERIAL_BAUD=9600`
- **`_co2Value` 全局变量**：在 `reportData()` 中输出 `co2` 字段
- **CO₂ 数据零值上报**：传感器数据不可用时仍上报 0 值以维持云端连接

### 修复
- **`fw-dht22-plug` 编译冲突**：添加 `src_filter = +<*> -<main-battery.cpp>` 排除电池版主文件
- **`fw-dht22-door-plug` 编译冲突**：同上
- **CO₂ 波特率修正**：从 38400 改为 9600（JW01-CO2 模块实际规格）

### 变更
- 平台版本更新至 v0.9.3
- 后端新增 CO₂ 报警字段支持

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
