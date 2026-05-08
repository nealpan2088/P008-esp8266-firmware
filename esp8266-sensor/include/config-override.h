// ============================================================
// DHT22 测试配置 — 快速验证 DHT22 模块是否正常工作
//
// 接线：
//   DHT22 红线(VCC)  → 3.3V (NodeMCU 3.3V 引脚)
//   DHT22 黑线(GND)  → GND (NodeMCU GND 引脚)
//   DHT22 黄线(DATA) → D4 (GPIO2)
//
// 使用方式：
//   1. 将本文件复制为 include/config.local.h
//   2. 或替换 main.cpp 中的 config.h 引用
//
// 测试方法：
//   烧录后打开串口监视器 (115200 baud)，
//   5秒内会看到温度湿度读数
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

// --------------- API ---------------
#ifndef API_BASE_URL
#define API_BASE_URL  "https://zghj.openyun.xin/api/v1"
#endif
#ifndef DEVICE_SERIAL
#define DEVICE_SERIAL ""  // 自动生成（DHT22- + chipId 后 5 位 hex）
#endif
#ifndef DEVICE_KEY
#define DEVICE_KEY    ""  // 自动生成（chipId 16 位 hex）
#endif

// --------------- 固件版本 ---------------
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "2.1.0-dht22-test"
#endif

// --------------- 时间配置 ---------------
#ifndef REPORT_INTERVAL_MS
#define REPORT_INTERVAL_MS  (60 * 1000UL)  // 测试用：1分钟上报一次
#endif
#ifndef DEEP_SLEEP_US
#define DEEP_SLEEP_US      (60 * 1000000ULL)  // 深度睡眠1分钟
#endif

// --------------- 串口 ---------------
#ifndef SERIAL_BAUD
#define SERIAL_BAUD   115200
#endif

// ============================================================
// 传感器开关 — 只开 DHT22，其他全关
// ============================================================
#ifndef USE_DS18B20
#define USE_DS18B20  0    // 关
#endif
#ifndef USE_DHT22
#define USE_DHT22    1    // ✅ 开 DHT22
#endif
#ifndef USE_SHT30
#define USE_SHT30    0    // 关
#endif
#ifndef USE_CURRENT
#define USE_CURRENT  0    // 关
#endif
#ifndef USE_DOOR
#define USE_DOOR     0    // 关
#endif

// --------------- 引脚定义 ---------------
#ifndef ONE_WIRE_BUS
#define ONE_WIRE_BUS  0   // 没用，保留默认
#endif
#ifndef DHT_PIN
#define DHT_PIN       2   // D4, DHT22 数据引脚 ← 接黄线
#endif
#ifndef CURRENT_PIN
#define CURRENT_PIN   A0  // 没用
#endif
#ifndef DOOR_PIN
#define DOOR_PIN      5   // 没用
#endif
#ifndef LED_PIN
#define LED_PIN       2   // D4, 板载 LED (低电平点亮)
#endif

#endif // CONFIG_H
