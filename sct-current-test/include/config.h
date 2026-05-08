// P008 — SCT-013-000 电流测试固件配置
// ========================================
// 所有值均经 build_flags 传入，此处仅作为默认值声明
// 修改配置请编辑 platformio.ini 的 build_flags

#ifndef CONFIG_H
#define CONFIG_H

// --------------- 引脚 ---------------
#ifndef CURRENT_PIN
#define CURRENT_PIN    A0
#endif

// --------------- 时间 ---------------
#ifndef REPORT_INTERVAL_MS
#define REPORT_INTERVAL_MS   5000
#endif

#ifndef WIFI_TIMEOUT_MS
#define WIFI_TIMEOUT_MS      30000
#endif

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS      5000
#endif

// --------------- 身份 ---------------
#ifndef FW_VERSION
#define FW_VERSION           "1.0"
#endif

#ifndef FW_CHANNEL
#define FW_CHANNEL           "test"
#endif

// --------------- 安全 ---------------
#ifndef HW_SECRET
#define HW_SECRET            "P008@2026!SecretKey"
#endif

// --------------- 网络 ---------------
#ifndef API_BASE_URL
#define API_BASE_URL         "https://zghj.openyun.xin/api/v1"
#endif

#ifndef AP_NAME
#define AP_NAME              "P008-SCT-Test"
#endif

// --------------- 日志 ---------------
#ifndef SERIAL_BAUD
#define SERIAL_BAUD          115200
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL            3     // 3=INFO, 4=DEBUG
#endif

#endif // CONFIG_H
