# 量产加固方案

> 固件版本：v2.2.0
> 项目版本：v0.6.1
> 日期：2026-05-01

## 背景

ESP8266 在量产部署中常见问题：
- 深度睡眠后无法唤醒（RST 引脚接触不良 / 供电不稳）
- WiFi 断连后重连卡死（重试逻辑不完善）
- HTTP 上报失败导致数据丢失（无缓存机制）
- 死机后无法自动恢复

## 加固措施

### 1. 硬件看门狗（WDT）

```
setup() 开头 → ESP.wdtEnable(8000ms)
                ↓
WiFi 连接循环 → ESP.wdtFeed() 每 500ms
                ↓
HTTP 重试退避 → ESP.wdtFeed() 每次重试前
                ↓
safeDeepSleep() → ESP.wdtDisable() → ESP.deepSleep()
```

- 超时 8 秒，足够覆盖所有正常操作
- 睡眠前必须关 WDT，否则 WDT 会在睡眠期间触发复位

### 2. 安全深度睡眠

```cpp
void safeDeepSleep(uint64_t sleepUs) {
    ESP.wdtDisable();  // 关 WDT
    delay(50);
    ESP.deepSleep(sleepUs);  // 睡
    // Never returns
}
```

所有 `ESP.deepSleep()` 调用都替换为 `safeDeepSleep()`，确保：
- 正常上报后的深度睡眠
- WiFiManager 配网超时后的睡眠
- 后续新增的睡眠路径

### 3. WiFi 多轮重试（connectWiFiWithRetry）

```
初始化 WiFi.begin() 无参（用 SDK 存储的凭据）
  ├── 第 1 轮：40 次 * 500ms = 20s，每次喂狗
  ├── 第 2 轮：断开 WiFi → 2s 延迟 → 重连，喂狗
  └── 第 3 轮：同上
       ↓ 全部失败
  进配网模式（WiFiManager AP）
```

### 4. HTTP 重试 + 本地缓存

```
POST 上报
  ├── 成功 (200) → 更新配置 + 刷新缓存
  └── 失败 (httpCode < 0)
       ├── 断 WiFi → 等 2s
       ├── WiFi.begin() 无参（SDK 凭据）
       │   ├── 恢复 → 重试 POST
       │   └── 失败 → cacheData() 写 SPIFFS
       └── 下次唤醒 flushCache() 补发
```

SPIFFS 缓存：FIFO 队列，上限 60 条（5 分钟间隔 = 5 小时）

## 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `WIFI_RETRY_MAX` | 3 | WiFi 连接最大重试轮数 |
| `HTTP_RETRY_MAX` | 2 | HTTP 上报最大重试次数 |
| `WDT_TIMEOUT_MS` | 8000 | 硬件看门狗超时时间(ms) |
| `MAX_CACHED_ENTRIES` | 60 | SPIFFS 缓存上限(条) |

## 验证方法

1. **正常链路**：上电 → WiFi 连 → 读传感器 → POST 200 → safeDeepSleep
2. **WiFi 断连**：拔网 → 3 轮重试 → 进配网
3. **HTTP 失败**：POST 第一次失败 → 重连 WiFi → 重试成功 → 正常流程
4. **HTTP + WiFi 双失败**：POST 失败 → 重连 WiFi 也失败 → cacheData → 下次醒来补发
5. **睡死保护**：按住 RST 断开 D0-GPIO16 → 8s 后 WDT 强制重启
