// ============================================================
// ESP8266 环境监测传感器 — 精简配置文件 (v3.0)
// ============================================================
#ifndef CONFIG_H
#define CONFIG_H

// --------------- WiFi ---------------
#ifndef WIFI_SSID
#define WIFI_SSID     "your_wifi_ssid"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your_wifi_password"
#endif

// --------------- API ---------------
#ifndef API_BASE_URL
#define API_BASE_URL  "https://zghj.openyun.xin/api/v1"
#endif
#ifndef DEVICE_SERIAL
#define DEVICE_SERIAL ""
#endif
#ifndef DEVICE_KEY
#define DEVICE_KEY    ""
#endif

// --------------- 自动序列号前缀 ---------------
// 新设备按命名规范 {类型}-{供电}-{芯片ID}
// 例: DHT22-PL-002B6350 (插电版DHT22)
//     DHT22-BT-00C6264D (电池版DHT22)
// 由 platformio.ini 的 build_flags 传入
#ifndef DEVICE_SERIAL_PREFIX
#define DEVICE_SERIAL_PREFIX ""  // 旧设备默认空前缀，保持兼容
#endif

// --------------- 安全配置 ---------------
#ifndef HW_SECRET
#define HW_SECRET "P008@2026!SecretKey"  // 编译时密钥，用于 HMAC 生成设备密钥
#endif

// --------------- 固件版本 ---------------
// 版本号唯一来源：hardware/esp8266-sensor/VERSION
// 平台io.ini 中的 build_flags 统一传入
#define FIRMWARE_VERSION "3.4"

// --------------- 固件发布渠道 ---------------
// official = 官方发布的 bin 文件（给别人烧录的）
// self     = 自己编译调试用的
// 发布 bin 前记得改成 official，自己烧时改成 self
// 可在 platformio.ini 的 build_flags 中覆盖此值
#ifndef FIRMWARE_CHANNEL
#define FIRMWARE_CHANNEL "self"
#endif

// --------------- 时间配置 ---------------
#ifndef DEEP_SLEEP_US
#define DEEP_SLEEP_US      (60 * 1000000ULL)  // 深度睡眠 60 秒
#endif
#ifndef SERIAL_BAUD
#define SERIAL_BAUD   115200
#endif

// --------------- 引脚定义 ---------------
#ifndef DHT_PIN
#define DHT_PIN       5   // D1 (GPIO5)
#endif
#ifndef DHT_TYPE
#define DHT_TYPE      22  // DHT22
#endif
#ifndef LED_BUILTIN
#define LED_BUILTIN   2   // D4 (GPIO2)
#endif

// --------------- DS18B20 温度传感器 ---------------
#ifndef USE_DS18B20
#define USE_DS18B20 0       // 0=不启用, 1=启用
#endif
#ifndef ONE_WIRE_BUS
#define ONE_WIRE_BUS 0      // D3 (GPIO0) — DS18B20 数据引脚
#endif

// --------------- 门磁传感器 ---------------
#ifndef USE_DOOR_SENSOR
#define USE_DOOR_SENSOR 0  // 0=不启用, 1=启用（仅在插电版有效）
#endif
#ifndef DOOR_PIN
#define DOOR_PIN       14  // D5 (GPIO14) — 不与 LED 冲突
                            // 门磁不通电时 HIGH=门开，LOW=门关
#endif

// --------------- MQ-135 空气质量传感器 ---------------
#ifndef USE_MQ135
#define USE_MQ135 0         // 0=不启用, 1=启用
#endif
#ifndef MQ135_PIN
#define MQ135_PIN A0        // 经分压电阻后接 A0（模拟输入）
#endif
#ifndef MQ135_RL
#define MQ135_RL 20.0       // 负载电阻值 (kΩ)，模块通常自带 20kΩ
#endif
#ifndef MQ135_R0
#define MQ135_R0 100.0      // 洁净空气中校准的 R₀ 值 (kΩ)，需实际校准
#endif

// --------------- AP 热点名 ---------------
#ifndef AP_NAME
#define AP_NAME "P008-Env-Monitor"
#endif

// --------------- 日志级别 ---------------
#ifndef LOG_LEVEL
#define LOG_LEVEL 3  // 3=INFO, 4=DEBUG
#endif

#endif /* CONFIG_H */

// ============================================================
// 自动化约束（请勿删除）
// 编译时检查：忘记设 DEVICE_SERIAL_PREFIX 直接编译失败
// ============================================================
#ifndef DEVICE_SERIAL_PREFIX
  #error "❌ DEVICE_SERIAL_PREFIX 未定义！新设备必须设置正确的序列号前缀。\n例：build_flags = -D DEVICE_SERIAL_PREFIX='\\\"DHT22-PL-\\\"'"
#endif
