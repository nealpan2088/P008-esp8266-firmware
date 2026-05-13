/**
 * P008 Environment Monitor - ESP8266 Firmware
 * -----------------------------------------------------------
 * Hardware: NodeMCU V3 (ESP8266) + DHT22 / DS18B20 / MQ-135 / 火焰探测器 / JW01-CO2 / SCT-013 电流互感器
 *
 * 模式（通过 build_flags 宏切换）:
 *   BATTERY_MODE=1:         定风电池版 deepSleep 方案
 *   USE_MQ135=1:            MQ-135 空气质量传感器版，上报 airQuality
 *   USE_FIRE_ALARM=1:       火焰探测 + 蜂鸣器报警版
 *   USE_CO2=1:              JW01-CO2 二氧化碳传感器版，上报 co2/temp/humidity
 *   USE_SCT013=1:           SCT-013 电流互感器版，上报 current（变化检测 + 自适应心跳）
 *   默认（无宏）:            DHT22 温湿度传感器版
 *
 * 首次配网: 手机连 P008-Env-Monitor 热点 → 192.168.4.1 配WiFi
 * 换WiFi:   按住 FLASH 按钮上电 → 进入配网模式
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BearSSLHelpers.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>

// 传感器头文件——编译时根据宏选择
#if USE_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#elif USE_MQ135
// MQ-135 直接用 ADC 读，不需要额外库
#elif USE_FIRE_ALARM
// 火焰传感器 + 蜂鸣器：纯数字 IO，不需要额外库
#elif USE_CO2
// JW01-CO2: UART 通讯，不需要额外库
#elif USE_SCT013
// SCT-013 电流互感器：ADC 读取，不需要额外库
#else
#include <DHT.h>
#endif

// DS18B20 + SCT013 复合版用到各自的头文件
#if USE_DS18B20 && USE_SCT013
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#include "config.h"

#if ENABLE_OTA
#include "ota_ca_cert.h"
#endif
#include "log.h"

// --------------- 全局变量 ---------------
WiFiClientSecure wifiClientSecure;
HTTPClient http;

#if USE_DS18B20 && !USE_SCT013
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
#elif USE_DS18B20 && USE_SCT013
OneWire oneWireComposite(ONE_WIRE_BUS);
DallasTemperature ds18b20Composite(&oneWireComposite);
#define ds18b20 ds18b20Composite
#elif USE_FIRE_ALARM
// 火焰报警版：不需要 DHT/DS18B20
#elif USE_CO2
// CO2 版：不需要 DHT/DS18B20
#elif USE_SCT013
// SCT-013 版：不需要 DHT/DS18B20，ADC直读
#else
DHT dht(DHT_PIN, DHT_TYPE);
#endif

// CO2 传感器专用（JW01-CO2），reportData 前由 loop 设置
#if USE_CO2
float _co2Value = 0;
#endif

char deviceSerial[32] = "";
char deviceKey[64]   = "";
char apiBaseUrl[128] = API_BASE_URL;
char chipIdHex[16]   = "";
char _autoSerial[32] = "";
char _autoKey[64]    = "";

unsigned long _bootStart = 0;
uint32_t _bootCount = 0;
unsigned long _lastReport = 0;
unsigned long _reportIntervalMs = 60000;  // 默认 60 秒，可从云端动态修改
int _rejectCount = 0;                     // 连续被后端拒绝次数（404=停用）
#if USE_DOOR_SENSOR
bool _doorLastState = HIGH;               // 门磁上次状态
bool _doorChanged = false;                // 门状态是否变化
#endif
#if USE_FIRE_ALARM
bool _lastFireDetected = false;           // 上次火焰检测状态
bool _fireStateChanged = false;           // 火焰状态是否变化
bool _buzzerActive = false;               // 当前蜂鸣器是否在响
unsigned long _buzzerOnTime = 0;          // 蜂鸣器开启的时间戳
unsigned long _buzzerLastToggle = 0;      // 上次切换蜂鸣器状态的时间
bool _buzzerState = false;                // 蜂鸣器当前物理状态
bool _forceReport = false;                // 强制上报（状态变化时）
int _fireConfirmCount = 0;                // 连续确认计数器
bool _alarmStateChanged = false;          // 报警状态变化标志
#endif

// SCT-013 电流监测专用 — RMS 有效值方案
#if USE_SCT013
#define SCT013_RMS_SAMPLES 200            // 每次 RMS 采 200 个点（覆盖 2 个 50Hz 周波）
#define SCT013_SAMPLE_INTERVAL_US 100     // 采样间隔 100μs（10kHz，覆盖 50Hz 正弦波）
#define SCT013_RMS_INTERVAL_MS 3000       // 每 3 秒采一次 RMS
#define SCT013_CALIBRATION_SAMPLES 30     // 基线校准采样数（固定）

// 以下参数可由云端 fetchConfig 动态覆盖
// 注意：初始值用"秒"单位，fetchConfig 中 *= 1000 转毫秒
// 这样云端没返回时乘 1000 后仍正确，不会把毫秒值再乘 1000
static float SCT013_THRESHOLD_A = 0.1;             // 变化阈值（A）
static unsigned long SCT013_HEARTBEAT_LOW_MS = 1800;       // 空载心跳（30min=1800秒）
static unsigned long SCT013_HEARTBEAT_MEDIUM_MS = 300;     // 轻载心跳（5min=300秒）
static unsigned long SCT013_HEARTBEAT_HIGH_MS = 60;        // 重载心跳（1min=60秒）
static float SCT013_LOAD_LOW_A = 0.5;              // 轻载/空载分界
static float SCT013_LOAD_HIGH_A = 5.0;             // 重载/轻载分界

float _currentRms = 0.0;                  // 最近一次 RMS 电流值
float _lastReportedCurrent = 0.0;         // 上次上报的电流值
float _currentBaseline = 0.0;             // 自学习零漂基线
int _calibrationCount = 0;                // 校准完成计数
unsigned long _lastRmsTime = 0;           // 上次 RMS 采样的时间
unsigned long _lastOkReport = 0;          // 上次成功上报（200）的时间，用于保底心跳
bool _sctInitialized = false;             // 初始上报标志

// 自适应变化阈值（SCT-013）
static const int NOISE_LEARN_SAMPLES = 20;     // 自学习采样次数（约 20~30 分钟）
static const int NOISE_WINDOW_MAX = 50;        // 滑动噪声窗口大小
float _noiseMin = 999.0;                       // 电流最小值（自学习窗口）
float _noiseMax = -999.0;                      // 电流最大值（自学习窗口）
int _learnedCount = 0;                         // 已学习次数
bool _cloudThresholdSet = false;               // 云端是否已设置阈值
#endif
unsigned long _rateLimitUntil = 0;        // 限流退避截止时间（429后默认 3 秒）
int _retryAfterSec = 3;                   // 429 后从后端解析的 retryAfter，默认 3 秒
#define WIFI_TIMEOUT_MS 30000   // WiFi 最久等 30 秒
#define HTTP_TIMEOUT_MS 5000    // HTTP 最久等 5 秒
#define WDT_TIMEOUT_US (30000 * 1000UL)   // 看门狗 30 秒
#define REJECT_LIMIT 5          // 连续5次被拒 → 进入低功耗轮询
#define REJECT_POLL_MS 1800000  // 被停用后每30分钟检查一次

#if BATTERY_MODE
  #define HTTP_TIMEOUT_MS_BATTERY 8000
  #define TOTAL_TIMEOUT_MS_BATTERY 20000
#endif

// --------------- 离线缓存 ---------------
// 纯 RAM 数组，断电丢失。正常运行时服务器重启不丢数据。
// 最多缓存 50 条，满了覆盖最旧的 10 条。
// 电池版（BATTERY_MODE=1）不要缓存，因为 deepSleep 醒来 RAM 清零。
#if !BATTERY_MODE
#define CACHE_MAX 50
static struct { float data[CACHE_MAX * 2]; uint16_t count; uint16_t head; } cache = { {0}, 0, 0 };

void cachePush(float t, float h) {
  if (cache.count >= CACHE_MAX) {
    cache.head = (cache.head + 10) % CACHE_MAX;
    cache.count = CACHE_MAX - 10;
  }
  int idx = (cache.head + cache.count) % CACHE_MAX;
  cache.data[idx * 2] = t;
  cache.data[idx * 2 + 1] = h;
  cache.count++;
}

void cacheClear() { cache.count = 0; cache.head = 0; }
#endif

// --------------- HMAC 密钥生成 ---------------
// BearSSL SHA256 已通过 ESP8266WiFi 自动链接
#include <bearssl/bearssl_hash.h>

void generateDeviceKey(const char* input, char* out, size_t outLen) {
  br_sha256_context ctx;
  br_sha256_init(&ctx);
  br_sha256_update(&ctx, input, strlen(input));
  br_sha256_update(&ctx, HW_SECRET, strlen(HW_SECRET));
  unsigned char hash[32];
  br_sha256_out(&ctx, hash);

  char* p = out;
  for (int i = 0; i < 32 && p < out + outLen - 3; i++) {
    p += sprintf(p, "%02x", hash[i]);
  }
  *p = '\0';
}

// --------------- 工具函数 ---------------
void generateIdentity() {
  uint32_t chipId = ESP.getChipId();
  snprintf(chipIdHex, sizeof(chipIdHex), "%08X", chipId);
  snprintf(_autoSerial, sizeof(_autoSerial), "%s%08X", DEVICE_SERIAL_PREFIX, chipId);
  generateDeviceKey(_autoSerial, _autoKey, sizeof(_autoKey));
  LOG_I("Identity", "Serial: %s", _autoSerial);
  LOG_I("Identity", "Key: %s", _autoKey);
}

void loadParams() {
  strncpy(deviceSerial, _autoSerial, sizeof(deviceSerial) - 1);
  strncpy(deviceKey, _autoKey, sizeof(deviceKey) - 1);
  LOG_I("Config", "Serial: %s", deviceSerial);
}

// --------------- WiFi 连接 ---------------
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  LOG_I("WiFi", "Connecting...");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      LOG_W("WiFi", "Timeout after %dms", WIFI_TIMEOUT_MS);
      return false;
    }
    delay(200);
    ESP.wdtFeed();
  }

  LOG_I("WiFi", "Connected, IP: %s", WiFi.localIP().toString().c_str());
  return true;
}

// --------------- 配网 Portal ---------------
void startConfigPortal() {
  LOG_I("WiFiManager", "Starting config portal...");
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);  // 5分钟超时
  wm.startConfigPortal(AP_NAME);
  LOG_I("WiFiManager", "Portal done, restarting...");
  delay(100);
  ESP.restart();
}

// --------------- OTA 指令检查（前向声明）---------------
void checkOTACommand();

// --------------- 上报数据 ---------------
// MQ-135 版: reportData(float airQualityRaw, float) — 第二个参数不用
// 其他版:    reportData(float temp, float humidity)
int reportData(float val1, float val2) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_W("Report", "WiFi not connected, skip");
    return -1;
  }

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/data", apiBaseUrl, deviceSerial);

  // WiFi RSSI：每次上报附带信号强度，用于后端监控网络稳定性
  int rssi = WiFi.RSSI();

  char body[360];
#if USE_CO2
  // JW01-CO2 上报：CO2 浓度 + 温度 + 湿度
  {
    snprintf(body, sizeof(body),
      "{\"co2\":%.0f,\"temp\":%.1f,\"humidity\":%.1f,\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"sensor\":\"JW01-CO2\",\"rssi\":%d"
      "}}",
      _co2Value, val1, val2, chipIdHex, rssi);
  }
#elif USE_MQ135
  // MQ-135 上报：airQuality 综合分数 + 原始 ADC 值
  {
    int rawAdc = (int)val1;             // 原始 ADC 读数 (0~1024)
    float airQualityScore = val2;       // 校准后的综合分数 (0~100)
    snprintf(body, sizeof(body),
      "{\"airQuality\":%.1f,\"rawAdc\":%d,\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"sensor\":\"MQ-135\",\"rssi\":%d"
      "}}",
      airQualityScore, rawAdc, chipIdHex, rssi);
  }
#elif USE_FIRE_ALARM
  // 火焰报警上报：火焰状态 + 蜂鸣器状态
  {
    bool fireDetected = (digitalRead(FIRE_SENSOR_PIN) == LOW);
    snprintf(body, sizeof(body),
      "{\"fireDetected\":%s,\"alarmActive\":%s,\"sensor\":\"FIRE-ALARM\",\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"deviceType\":\"FIRE\",\"rssi\":%d"
      "}}",
      fireDetected ? "true" : "false",
      _buzzerActive ? "true" : "false",
      chipIdHex, rssi);
  }
#elif USE_DS18B20 && USE_SCT013
  // DS18B20 + SCT013 复合上报：温度和电流都在同一个数据包
  {
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"sensor\":\"DS18B20\",\"current\":%.4f,\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"power\":\"plug\",\"multiSensor\":\"ds18b20+sct013\",\"rssi\":%d"
      "}}",
      val1, _currentRms, chipIdHex, rssi);
  }
#elif USE_SCT013
  // SCT-013 电流上报：current + 功率 + 其他参数
  // 注意：所有传感器数据字段必须放 otherData 内，顶层只放标准字段(temp/humidity/battery)
  // 前端统一从 otherData.current 读取电流值
  {
    snprintf(body, sizeof(body),
      "{\"battery\":0,\"otherData\":{"
      "\"current\":%.4f,\"voltage\":0,\"power\":%.2f,"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"sensor\":\"SCT-013\",\"deviceType\":\"CURRENT\",\"rssi\":%d"
      "}}",
      _currentRms, _currentRms * 220.0, chipIdHex, rssi);
  }
#elif BATTERY_MODE
  snprintf(body, sizeof(body),
    "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":1,\"otherData\":{"
    "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"power\":\"battery\",\"rssi\":%d"
    "}}",
    val1, val2, chipIdHex, rssi);
#else
  // 插电版 body
  #if USE_DOOR_SENSOR
    bool doorOpen = (digitalRead(DOOR_PIN) == HIGH);
    _doorChanged = (doorOpen != _doorLastState);
    _doorLastState = doorOpen;
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":0,\"doorOpen\":%s,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"rssi\":%d%s"
      "}}",
      val1, val2,
      doorOpen ? "true" : "false",
      chipIdHex, rssi,
      _doorChanged ? ",\"event\":\"door_toggle\"" : "");
  #else
    #if USE_DS18B20
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"sensor\":\"DS18B20\",\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"power\":\"plug\",\"rssi\":%d"
      "}}",
      val1, chipIdHex, rssi);
    #else
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"rssi\":%d"
      "}}",
      val1, val2, chipIdHex, rssi);
    #endif
  #endif
#endif

  LOG_I("HTTP", "POST %s", url);
  LOG_I("HTTP", "Body: %s", body);

  wifiClientSecure.setInsecure();
  http.begin(wifiClientSecure, url);
#if BATTERY_MODE
  http.setTimeout(HTTP_TIMEOUT_MS_BATTERY);
#else
  http.setTimeout(HTTP_TIMEOUT_MS);
#endif
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  ESP.wdtFeed();  // HTTPS POST 前喂狗，防止 WiFi 不稳时 SSL 卡死不返回
  int code = http.POST(body);
  LOG_I("HTTP", "Code: %d", code);

  // 解析 retryAfter（429 限流时后端返回的等待秒数）
  if (code == 429) {
    String payload = http.getString();
    int raIdx = payload.indexOf("\"retryAfter\"");
    if (raIdx >= 0) {
      int colon = payload.indexOf(':', raIdx);
      if (colon >= 0) {
        int s = colon + 1;
        while (s < (int)payload.length() && payload[s] == ' ') s++;
        int e = s;
        while (e < (int)payload.length() && payload[e] >= '0' && payload[e] <= '9') e++;
        if (e > s) {
          int parsed = payload.substring(s, e).toInt();
          if (parsed > 0 && parsed <= 3600) _retryAfterSec = parsed;
        }
      }
    }
  }
  http.end();

  // 上报成功后顺便检查 OTA 指令，避免独立轮询
  #if ENABLE_OTA
  if (code == 200) {
    checkOTACommand();
  }
  #endif

  return code;
}

// --------------- 拉取云端配置 ---------------
void fetchConfig() {
  if (WiFi.status() != WL_CONNECTED) return;

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/config", apiBaseUrl, deviceSerial);

  wifiClientSecure.setInsecure();
  http.begin(wifiClientSecure, url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("X-Device-Key", deviceKey);

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    // 找 "reportInterval":数字
    int keyIdx = payload.indexOf("\"reportInterval\"");
    if (keyIdx >= 0) {
      int colonIdx = payload.indexOf(':', keyIdx);
      if (colonIdx >= 0) {
        int valStart = colonIdx + 1;
        while (valStart < (int)payload.length() && payload[valStart] == ' ') valStart++;
        int valEnd = valStart;
        while (valEnd < (int)payload.length() && payload[valEnd] >= '0' && payload[valEnd] <= '9') valEnd++;
        if (valEnd > valStart) {
          long intervalSec = payload.substring(valStart, valEnd).toInt();
          if (intervalSec >= 10 && intervalSec <= 3600) {
            _reportIntervalMs = intervalSec * 1000UL;
            LOG_I("Config", "reportInterval=%lds (from cloud)", intervalSec);
          }
        }
      }
    }

#if USE_SCT013
    // 解析 SCT-013 边缘计算参数（云端可调）
    #define PARSE_SCT_PARAM(key, var) do { \
      int idx = payload.indexOf("\"" key "\""); \
      if (idx >= 0) { \
        int colon = payload.indexOf(':', idx + strlen(key) + 2); \
        if (colon >= 0) { \
          int s = colon + 1; while (s < (int)payload.length() && payload[s] == ' ') s++; \
          int e = s; while (e < (int)payload.length() && ((payload[e] >= '0' && payload[e] <= '9') || payload[e] == '.')) e++; \
          if (e > s) { var = payload.substring(s, e).toFloat(); LOG_I("Config", key "=%.4f", (float)var); } \
        } \
      } \
    } while(0)

    float oldThreshold = SCT013_THRESHOLD_A;
    PARSE_SCT_PARAM("sct013ChangeThreshold", SCT013_THRESHOLD_A);
    // 防呆：阈值太灵敏感 ADC 噪音，太迟钝变化捕捉不到
    if (SCT013_THRESHOLD_A < 0.05) SCT013_THRESHOLD_A = 0.05;
    if (SCT013_THRESHOLD_A > 5.0)  SCT013_THRESHOLD_A = 5.0;
    if (fabs(SCT013_THRESHOLD_A - oldThreshold) > 0.001) {
      _cloudThresholdSet = true;
      LOG_I("SCT013", "Threshold=%.2fA (cloud, from server)", SCT013_THRESHOLD_A);
    }
    PARSE_SCT_PARAM("sct013HeartbeatLowSec", SCT013_HEARTBEAT_LOW_MS);
    // 防呆：心跳间隔不能低于 60 秒，不能超过 24 小时
    if (SCT013_HEARTBEAT_LOW_MS < 60) SCT013_HEARTBEAT_LOW_MS = 60;
    if (SCT013_HEARTBEAT_LOW_MS > 86400) SCT013_HEARTBEAT_LOW_MS = 86400;

    PARSE_SCT_PARAM("sct013HeartbeatMedSec", SCT013_HEARTBEAT_MEDIUM_MS);
    if (SCT013_HEARTBEAT_MEDIUM_MS < 60) SCT013_HEARTBEAT_MEDIUM_MS = 60;
    if (SCT013_HEARTBEAT_MEDIUM_MS > 86400) SCT013_HEARTBEAT_MEDIUM_MS = 86400;

    PARSE_SCT_PARAM("sct013HeartbeatHighSec", SCT013_HEARTBEAT_HIGH_MS);
    if (SCT013_HEARTBEAT_HIGH_MS < 10) SCT013_HEARTBEAT_HIGH_MS = 10;
    if (SCT013_HEARTBEAT_HIGH_MS > 86400) SCT013_HEARTBEAT_HIGH_MS = 86400;

    PARSE_SCT_PARAM("sct013LoadLowA", SCT013_LOAD_LOW_A);
    if (SCT013_LOAD_LOW_A < 0.1) SCT013_LOAD_LOW_A = 0.1;
    if (SCT013_LOAD_LOW_A > 10.0) SCT013_LOAD_LOW_A = 10.0;

    PARSE_SCT_PARAM("sct013LoadHighA", SCT013_LOAD_HIGH_A);
    if (SCT013_LOAD_HIGH_A < 0.5) SCT013_LOAD_HIGH_A = 0.5;
    if (SCT013_LOAD_HIGH_A > 20.0) SCT013_LOAD_HIGH_A = 20.0;

    // 心跳时间从秒转毫秒（仅首次执行，避免同次启动重复 fetchConfig 时再乘 1000）
    // 云端配置的 sct013HeartbeatMedSec 是秒值，固件内部用毫秒
    static bool _sctHeartbeatConverted = false;
    if (!_sctHeartbeatConverted) {
      SCT013_HEARTBEAT_LOW_MS *= 1000;
      SCT013_HEARTBEAT_MEDIUM_MS *= 1000;
      SCT013_HEARTBEAT_HIGH_MS *= 1000;
      _sctHeartbeatConverted = true;
    }
#undef PARSE_SCT_PARAM
#endif
  } else {
    LOG_D("Config", "GET failed: %d", code);
  }
  http.end();
}

// --------------- OTA 升级检查 ---------------
#if ENABLE_OTA

// 前向声明
void reportStatus(const char* status, const String& version);

void checkOTACommand() {
  // 仅在数据上报成功后调用，无轮询开销

  // 从云端拉取挂起的 OTA 指令
  if (WiFi.status() != WL_CONNECTED) return;

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/commands/pending", apiBaseUrl, deviceSerial);

  wifiClientSecure.setInsecure();
  HTTPClient http2;
  http2.setTimeout(5000);
  http2.begin(wifiClientSecure, url);
  http2.addHeader("X-Device-Key", deviceKey);

  int code = http2.GET();
  if (code == 200) {
    String payload = http2.getString();
    http2.end();

    // 检查是否有 OTA_UPDATE 命令
    int cmdIdx = payload.indexOf("\"OTA_UPDATE\"");
    if (cmdIdx < 0) return;  // 没有 OTA 命令

    // 解析 otaUrl 和 md5
    int urlIdx = payload.indexOf("\"otaUrl\"");
    if (urlIdx < 0) return;
    int colonIdx = payload.indexOf(':', urlIdx);
    if (colonIdx < 0) return;
    int urlStart = colonIdx + 1;
    while (urlStart < (int)payload.length() && (payload[urlStart] == ' ' || payload[urlStart] == '"')) urlStart++;
    int urlEnd = urlStart;
    while (urlEnd < (int)payload.length() && payload[urlEnd] != '"') urlEnd++;
    if (urlEnd <= urlStart) return;

    String otaUrl = payload.substring(urlStart, urlEnd);

    // 拼上完整 URL（后端返回的是相对路径 /uploads/firmware/xxx.bin）
    String fullOtaUrl;
    if (otaUrl.startsWith("http://") || otaUrl.startsWith("https://")) {
      fullOtaUrl = otaUrl;
    } else {
      fullOtaUrl = "https://" + String(OTA_DOWNLOAD_HOST) + otaUrl;
    }

    // 解析版本号
    int verIdx = payload.indexOf("\"version\"");
    String targetVersion = "?";
    if (verIdx >= 0) {
      int vcolon = payload.indexOf(':', verIdx);
      int vStart = vcolon + 1;
      while (vStart < (int)payload.length() && (payload[vStart] == ' ' || payload[vStart] == '"')) vStart++;
      int vEnd = vStart;
      while (vEnd < (int)payload.length() && payload[vEnd] != '"') vEnd++;
      if (vEnd > vStart) targetVersion = payload.substring(vStart, vEnd);
    }

    // 解析 MD5（可选，用于 ESPhttpUpdate 的完整性校验）
    String targetMd5 = "";
    int md5Idx = payload.indexOf("\"md5\"");
    if (md5Idx >= 0) {
      int mcolon = payload.indexOf(':', md5Idx);
      int mStart = mcolon + 1;
      while (mStart < (int)payload.length() && (payload[mStart] == ' ' || payload[mStart] == '"')) mStart++;
      int mEnd = mStart;
      while (mEnd < (int)payload.length() && payload[mEnd] != '"') mEnd++;
      if (mEnd > mStart) targetMd5 = payload.substring(mStart, mEnd);
    }

    LOG_I("OTA", "Command received! Target=%s URL=%s", targetVersion.c_str(), otaUrl.c_str());

    // 上报升级开始
    reportStatus("ota:start", targetVersion);

    // 执行 OTA 升级
#if OTA_USE_HTTP
    // HTTP 下载 + MD5 校验：轻量可靠，固件被篡改 MD5 不匹配会中止升级
    WiFiClient otaClient;
    otaClient.setTimeout(OTA_CONNECT_TIMEOUT_MS);
    String httpOtaUrl = fullOtaUrl;
    httpOtaUrl.replace("https://", "http://");
    t_httpUpdate_return ret;
    if (targetMd5.length() > 0) {
      ret = ESPhttpUpdate.update(otaClient, httpOtaUrl, targetMd5);
    } else {
      ret = ESPhttpUpdate.update(otaClient, httpOtaUrl);
    }
#else
    WiFiClientSecure otaClient;
    {
      BearSSL::X509List cert(ota_ca_cert);
      otaClient.setTrustAnchors(&cert);
    }
    otaClient.setTimeout(OTA_CONNECT_TIMEOUT_MS);
    t_httpUpdate_return ret;
    if (targetMd5.length() > 0) {
      ret = ESPhttpUpdate.update(otaClient, fullOtaUrl, targetMd5);
    } else {
      ret = ESPhttpUpdate.update(otaClient, fullOtaUrl);
    }
#endif
    switch (ret) {
      case HTTP_UPDATE_OK:
        LOG_I("OTA", "Update success! Restarting...");
        // 升级成功后会重启，不会走到这里
        break;
      case HTTP_UPDATE_FAILED:
        LOG_E("OTA", "Update failed: %d (%s)", ESPhttpUpdate.getLastError(),
              ESPhttpUpdate.getLastErrorString().c_str());
        reportStatus("ota:failed", String(ESPhttpUpdate.getLastError()));
        break;
      case HTTP_UPDATE_NO_UPDATES:
        LOG_W("OTA", "No update available");
        break;
    }
  } else {
    http2.end();
  }
}

// 上报 OTA 状态到云端
void reportStatus(const char* status, const String& version) {
  if (WiFi.status() != WL_CONNECTED) return;

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/commands/callback/%s", apiBaseUrl, deviceSerial, "");

  WiFiClientSecure client;
  {
    BearSSL::X509List cert(ota_ca_cert);
    client.setTrustAnchors(&cert);
  }
  HTTPClient http3;
  http3.setTimeout(5000);
  http3.begin(client, url);
  http3.addHeader("Content-Type", "application/json");
  http3.addHeader("X-Device-Key", deviceKey);

  char body[256];
  snprintf(body, sizeof(body),
    "{\"command\":\"OTA_UPDATE\",\"status\":\"%s\",\"result\":{\"version\":\"%s\"}}",
    status, version.c_str());

  int httpCode = http3.POST(body);
  (void)httpCode;  // 抑制未使用变量警告
  LOG_D("OTA", "Status report: %s -> %d", status, httpCode);
  http3.end();
}
#endif

// --------------- setup ---------------
void setup() {
  _bootStart = millis();
  Serial.begin(SERIAL_BAUD);
  delay(100);

#if BATTERY_MODE
  // 电池版：deepSleep 方案，一次启动 → 上报 → 睡
  static uint32_t bootCount = 0;
  bootCount++;
  Serial.println("\n========================================\n");
  LOG_I("Boot", "P008 Env Monitor v3.2 (定风电池版)");
  LOG_I("Boot", "Boot #%d", bootCount);

  generateIdentity();
  // 电池版直接复制序列号和密钥（没有 loadParams）
  strncpy(deviceSerial, _autoSerial, sizeof(deviceSerial) - 1);
  strncpy(deviceKey, _autoKey, sizeof(deviceKey) - 1);

  // Flash 按钮按 3 秒 → 配网
  pinMode(0, INPUT_PULLUP);
  int holdMs = 0;
  while (digitalRead(0) == LOW && holdMs < 3000 && (millis() - _bootStart) < 5000) {
    delay(10);
    holdMs += 10;
  }
  if (holdMs >= 3000) {
    LOG_I("FlashBtn", "3s hold → config portal");
    startConfigPortal();
    return;
  }

  // WiFi
  bool wifiOk = connectWiFi();
  if (!wifiOk) {
    LOG_W("Main", "WiFi failed, entering config portal...");
    startConfigPortal();
    return;
  }

  // 传感器
  #if USE_MQ135
    #error "❌ MQ-135 不支持电池版（BATTERY_MODE=1）。MQ-135 需要加热预热，不适合 deepSleep。"
  #elif USE_FIRE_ALARM
    #error "❌ 火焰报警不支持电池版（BATTERY_MODE=1）。火焰探测器需常在线，不适合 deepSleep。"
  #elif USE_CO2
    #error "❌ JW01-CO2 不支持电池版（BATTERY_MODE=1）。CO2 传感器需常在线，UART 通讯不适合 deepSleep。"
  #else
    dht.begin();
    delay(200);

    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temp) || isnan(humidity)) {
      LOG_W("DHT22", "Read failed (temp=%f, hum=%f)", temp, humidity);
    } else if (temp < -20.0 || temp > 80.0 || humidity < 0.0 || humidity > 100.0) {
      LOG_W("DHT22", "Reading out of range (temp=%f, hum=%f), retrying...", temp, humidity);
      delay(2100);
      temp = dht.readTemperature();
      humidity = dht.readHumidity();
      if (isnan(temp) || isnan(humidity) || temp < -20.0 || temp > 80.0 || humidity < 0.0 || humidity > 100.0) {
        LOG_W("DHT22", "Retry still invalid (temp=%f, hum=%f), falling back to prev", temp, humidity);
      } else {
        LOG_I("DHT22", "Retry OK: Temp=%.1fC, Humidity=%.1f%%", temp, humidity);
      }
    } else {
      LOG_I("DHT22", "Temp=%.1fC, Humidity=%.1f%%", temp, humidity);
    }
  #endif

  // 上报（允许 2 次重试）
  int code = -1;
  for (int retry = 0; retry < 2 && code != 200 && (millis() - _bootStart) < TOTAL_TIMEOUT_MS_BATTERY; retry++) {
    if (retry > 0) {
      LOG_W("Report", "Retry #%d...", retry);
      delay(500);
    }
    code = reportData(temp, humidity);
  }

  float uptime = (millis() - _bootStart) / 1000.0;
  LOG_I("Sleep", "Uptime=%.1fs, going to sleep for %llus", uptime, DEEP_SLEEP_US / 1000000ULL);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.flush();
  Serial.end();
  ESP.deepSleep(DEEP_SLEEP_US);

#else
  // 插电版：loop 方案，WiFi 常连
  _bootCount++;
  Serial.println("\n========================================\n");
  #if USE_SCT013
    LOG_I("Boot", "P008 Env Monitor v" FIRMWARE_VERSION " (雷电版)");
  #else
    LOG_I("Boot", "P008 Env Monitor v" FIRMWARE_VERSION " (定风版)");
  #endif
  LOG_I("Boot", "Boot #%d", _bootCount);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  ESP.wdtEnable(WDT_TIMEOUT_US);

  generateIdentity();
  loadParams();

  // Flash 按钮按 3 秒 → 配网
  pinMode(0, INPUT_PULLUP);
  int holdMs = 0;
  while (digitalRead(0) == LOW && holdMs < 3000) {
    delay(10);
    holdMs += 10;
  }
  if (holdMs >= 3000) {
    LOG_I("FlashBtn", "3s hold → config portal");
    startConfigPortal();
  }

  // WiFi
  bool wifiOk = connectWiFi();
  if (!wifiOk) {
    LOG_W("Main", "WiFi failed, entering config portal...");
    startConfigPortal();
  }

  // 传感器
  #if USE_CO2
    // JW01-CO2：UART 通讯，9600 baud，用 Serial1（RX=GPIO12, TX=GPIO13）
    Serial1.begin(CO2_SERIAL_BAUD);
    LOG_I("CO2", "Serial1 started at %d baud (RX=GPIO12, TX=GPIO13)", CO2_SERIAL_BAUD);
  #elif USE_DS18B20 && USE_SCT013
    ds18b20.begin();
    LOG_I("DS18B20", "Init OK (composite)");
    // SCT-013：ADC 直接读取
    pinMode(A0, INPUT);
    {
      float baselineSum = 0;
      for (int i = 0; i < SCT013_CALIBRATION_SAMPLES; i++) {
        baselineSum += analogRead(A0);
        delay(10);
      }
      _currentBaseline = baselineSum / SCT013_CALIBRATION_SAMPLES;
    }
    _currentRms = 0;
    _lastReportedCurrent = 0;
    _sctInitialized = true;
    _noiseMin = 999.0;
    _noiseMax = -999.0;
    _learnedCount = 0;
    _cloudThresholdSet = false;
    LOG_I("SCT013", "Baseline ADC=%.1f (composite)", _currentBaseline);
  #elif USE_SCT013
    // SCT-013：ADC 直接读取，无需额外初始化
    // 注意：SCT-013 输出交流信号，需经过运放整流电路转换为 0~3.3V DC
    pinMode(A0, INPUT);
    // 自学习零漂基线：开机后快速采 30 个点
    LOG_I("SCT013", "Calibrating baseline... (30 samples)");
    float baselineSum = 0;
    for (int i = 0; i < SCT013_CALIBRATION_SAMPLES; i++) {
      baselineSum += analogRead(A0);
      delay(10);
    }
    _currentBaseline = baselineSum / SCT013_CALIBRATION_SAMPLES;
    _currentRms = 0;
    _lastReportedCurrent = 0;
    _sctInitialized = true;
    _noiseMin = 999.0;
    _noiseMax = -999.0;
    _learnedCount = 0;
    _cloudThresholdSet = false;
    LOG_I("SCT013", "Baseline ADC=%.1f (zero-current reference)", _currentBaseline);
  #elif USE_FIRE_ALARM
    // 火焰传感器：DO 输出数字信号（LOW=有火, HIGH=安全）
    pinMode(FIRE_SENSOR_PIN, INPUT);
    // 蜂鸣器：低电平触发，默认拉高（不响）
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);
    _lastFireDetected = (digitalRead(FIRE_SENSOR_PIN) == LOW);
    _buzzerActive = false;
    LOG_I("FireAlarm", "Init OK (fire=D1, buzzer=D2)");
    LOG_I("FireAlarm", "Initial fire state: %s", _lastFireDetected ? "FIRE!" : "SAFE");
  #elif USE_MQ135
    // MQ-135 不需要额外初始化，ADC 直接可用
    pinMode(MQ135_PIN, INPUT);
    LOG_I("MQ135", "Init OK (pin=%d)", MQ135_PIN);
  #elif USE_DS18B20 && !USE_SCT013
    ds18b20.begin();
    LOG_I("DS18B20", "Init OK");
  #else
    dht.begin();
  #endif

  // 门磁初始化（仅在插电版支持）
  #if USE_DOOR_SENSOR
    pinMode(DOOR_PIN, INPUT_PULLUP);
    _doorLastState = digitalRead(DOOR_PIN);
    LOG_I("Door", "Initial state: %s", _doorLastState == HIGH ? "OPEN" : "CLOSED");
  #endif

  // LED 熄（正常工作了）
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

// --------------- loop ---------------
void loop() {
#if BATTERY_MODE
  // deepSleep 唤醒后不会走到这里
  // 如果 GPIO16 没接 RST，卡在这里 60 秒重启
  delay(60000);
  ESP.restart();
  return;
#endif

  ESP.wdtFeed();

  // 检查 WiFi
  if (WiFi.status() != WL_CONNECTED) {
    LOG_W("Loop", "WiFi lost, reconnecting...");
    if (!connectWiFi()) {
      LOG_E("Loop", "WiFi reconnect failed, restarting...");
      delay(100);
      ESP.restart();
    }
  }

  // 到时间才上报（火焰报警版：事件驱动 + 定时心跳）
  unsigned long now = millis();
  #if USE_FIRE_ALARM
  // --- 火焰报警版 loop ---
  // 策略：连续确认防误判 → 状态变化立即上报 → 每 60s 心跳保活
  bool rawFire = (digitalRead(FIRE_SENSOR_PIN) == LOW);

  // 防误判：连续 FIRE_CONFIRM_COUNT 次读到 LOW 才算有火
  if (rawFire) {
    if (_fireConfirmCount < FIRE_CONFIRM_COUNT) {
      _fireConfirmCount++;
    }
  } else {
    if (_fireConfirmCount > 0) {
      _fireConfirmCount = 0;  // 只要读到一次 HIGH，立即清零
    }
  }

  bool fireNow = (_fireConfirmCount >= FIRE_CONFIRM_COUNT);
  _fireStateChanged = (fireNow != _lastFireDetected);

  // 蜂鸣器控制逻辑
  if (fireNow && !_lastFireDetected) {
    // 检测到火焰 → 开始节奏报警
    if (!_buzzerActive) {
      _buzzerActive = true;
      _buzzerOnTime = now;
      _buzzerState = true;
      _buzzerLastToggle = now;
      digitalWrite(BUZZER_PIN, LOW);  // 低电平触发，响
      _alarmStateChanged = true;
      LOG_I("FireAlarm", "🔥 FIRE DETECTED! Buzzer ON (rhythmic)");
    }
  }

  // 节奏控制：蜂鸣器激活时按节奏切换
  if (_buzzerActive) {
    unsigned long elapsedSinceToggle = now - _buzzerLastToggle;
    if (_buzzerState) {
      // 正在响 → 到时间关
      if (elapsedSinceToggle >= BUZZER_ON_MS) {
        digitalWrite(BUZZER_PIN, HIGH);  // 不响
        _buzzerState = false;
        _buzzerLastToggle = now;
        _alarmStateChanged = true;
      }
    } else {
      // 停着 → 到时间响
      if (elapsedSinceToggle >= BUZZER_OFF_MS) {
        digitalWrite(BUZZER_PIN, LOW);   // 响
        _buzzerState = true;
        _buzzerLastToggle = now;
        _alarmStateChanged = true;
      }
    }
  }

  // 火焰消失后延时关蜂鸣器
  if (!fireNow && _buzzerActive) {
    if (_buzzerOnTime > 0 && (now - _buzzerOnTime) >= BUZZER_HOLD_MS) {
      digitalWrite(BUZZER_PIN, HIGH);   // 确保关闭
      _buzzerActive = false;
      _buzzerState = false;
      _buzzerOnTime = 0;
      _alarmStateChanged = true;
      LOG_I("FireAlarm", "✅ Fire cleared, Buzzer OFF");
    }
  }

  _lastFireDetected = fireNow;

  // 火焰状态或蜂鸣器状态有变化 → 强制上报
  bool shouldReport = _fireStateChanged || _alarmStateChanged || _forceReport;
  unsigned long elapsedHeartbeat = (now >= _lastReport) ? (now - _lastReport) : (now + (0xFFFFFFFF - _lastReport));
  if (elapsedHeartbeat >= _reportIntervalMs) {
    shouldReport = true;
  }

  if (!shouldReport) {
    // LED 闪烁指示（有火快速闪，无火慢闪）
    digitalWrite(LED_BUILTIN, (now / 500) % 2);
    delay(100);
    return;
  }

  _forceReport = false;
  _alarmStateChanged = false;
  _lastReport = now;

  // 火焰版上报：val1 和 val2 不用传，reportData 内部读引脚
  int code = reportData(0, 0);

  // 上报失败 + 状态变化 → 标记强制重试
  if (code != 200 && _fireStateChanged) {
    _forceReport = true;
  }

  #endif  // USE_FIRE_ALARM

  #if USE_SCT013 && !USE_DS18B20
  // --- SCT-013 电流监测版 loop（RMS 有效值方案）---
  // 策略：每 3 秒密集采 200 个点算 RMS → 减基线 → 变化超阈值上报
  //        空载 30min 心跳、轻载 5min、重载 1min
  // 改动：不再"每1秒碰运气"，而是每次覆盖 2 个完整 50Hz 周波
  // ==========================================
  now = millis();
  unsigned long elapsedSinceRms = (now >= _lastRmsTime) ? (now - _lastRmsTime) : (now + (0xFFFFFFFF - _lastRmsTime));

  // 1. 每 3 秒采一次 RMS（非阻塞）
  if (elapsedSinceRms >= SCT013_RMS_INTERVAL_MS) {
    _lastRmsTime = now;

    // 密集采样 200 个点（10kHz，覆盖 2 个 50Hz 周波）
    float sumSquared = 0;
    int sampleCount = 0;
    unsigned long sampleStart = micros();
    for (int i = 0; i < SCT013_RMS_SAMPLES; i++) {
      float adc = analogRead(A0);
      float zeroed = adc - _currentBaseline;
      sumSquared += zeroed * zeroed;
      sampleCount++;
      if (micros() - sampleStart >= (unsigned long)SCT013_RMS_SAMPLES * SCT013_SAMPLE_INTERVAL_US) break;
      delayMicroseconds(SCT013_SAMPLE_INTERVAL_US);
    }

    // 计算 RMS 值（均方根）
    float rmsAdc = sqrt(sumSquared / sampleCount);
    // ADC → 电压（3.3V 参考，10 位分辨率）
    float rmsVoltage = rmsAdc * (3.3 / 1024.0);
    // SCT-013-30 型号：30A:1V 输出
    // 经真 RMS 检波后，电压直接反映有效值
    float rmsCurrent = rmsVoltage * 30.0;

    // 更新全局 RMS 值
    _currentRms = rmsCurrent;

    LOG_D("SCT013", "RMS ADC=%.1f, V=%.4f, I=%.4fA (samples=%d)",
      rmsAdc, rmsVoltage, rmsCurrent, sampleCount);

    // 安全过滤：RMS 超过 20A 或为负值时跳过（GND 虚接/ADC 异常导致）
    // 当前设备最大量程 20A，超过此值一定是异常
    if (_currentRms > 20.0 || _currentRms < 0.0) {
      LOG_W("SCT013", "RMS=%.4fA out of range, skip (ADC异常/GND虚接?)", _currentRms);
      _lastRmsTime = now;  // 更新时间戳，避免采样间隔自旋
      delay(50);
      return;
    }

    // ==========================================
    // 自适应变化阈值
    // 云端配置优先 → 自学习兜底 → 默认 0.1A
    // ==========================================
    if (!_cloudThresholdSet) {
      if (_currentRms < _noiseMin) _noiseMin = _currentRms;
      if (_currentRms > _noiseMax) _noiseMax = _currentRms;
      _learnedCount++;

      // 每 7 次 RMS 计算一次（约 21 秒）
      if (_learnedCount >= 7 && (_learnedCount % 7 == 0)) {
        float noiseFloor = _noiseMax - _noiseMin;
        float rawTh = noiseFloor * 3.0;
        if (rawTh < 0.1) rawTh = 0.1;
        if (rawTh > 2.0) rawTh = 2.0;

        // EMA 平滑（首次直接赋值）
        float newTh = (_learnedCount == 7) ? rawTh : (SCT013_THRESHOLD_A * 0.85 + rawTh * 0.15);
        if (newTh < 0.1) newTh = 0.1;
        if (newTh > 2.0) newTh = 2.0;
        SCT013_THRESHOLD_A = newTh;

        // 重置窗口——清空，让下一个周期重新收集真实波动范围
        // 注意：不能用 currentRms±0.1 的"小窗口"重置，否则在有负载的场景下
        // noiseMin/noiseMax 永远被撑大，阈值会持续上涨，无法收敛
        _noiseMin = 999.0;
        _noiseMax = -999.0;
        LOG_I("SCT013", "Threshold=%.2fA (adaptive, noiseFloor=%.2fA)", newTh, noiseFloor);
      }
    }
  }

  // 2. 判断是否需要上报（只在 RMS 更新后判断）
  bool currentChanged = (fabs(_currentRms - _lastReportedCurrent) > SCT013_THRESHOLD_A);
  unsigned long elapsedSinceReport = (now >= _lastReport) ? (now - _lastReport) : (now + (0xFFFFFFFF - _lastReport));

  // 自适应心跳间隔：根据当前负载决定
  unsigned long heartbeatInterval;
  if (_currentRms < SCT013_LOAD_LOW_A) {
    heartbeatInterval = SCT013_HEARTBEAT_LOW_MS;      // 空载 30min
  } else if (_currentRms < SCT013_LOAD_HIGH_A) {
    heartbeatInterval = SCT013_HEARTBEAT_MEDIUM_MS;    // 轻载 5min
  } else {
    heartbeatInterval = SCT013_HEARTBEAT_HIGH_MS;      // 重载 1min
  }

  // 限流退避：如果上次请求被 429，等够 3 秒再试
  bool isRateLimited = (_rateLimitUntil > 0 && now < _rateLimitUntil);
  // 保底心跳：距离上次成功上报（200）超过心跳间隔时强制触发
  unsigned long elapsedSinceOk = (_lastOkReport > 0 && now >= _lastOkReport) ? (now - _lastOkReport) : 0xFFFFFFFF;

  // 首次启动 || 电流变化超阈值 || 保底心跳（靠谱上报200后）|| 到上报时间
  bool shouldReport = !_sctInitialized || currentChanged
    || (elapsedSinceOk >= heartbeatInterval && _lastOkReport > 0)
    || (elapsedSinceReport >= heartbeatInterval);
  // 限流期间跳过上报（不开倒车，等限流期结束）
  if (isRateLimited) shouldReport = false;

  if (!shouldReport) {
    digitalWrite(LED_BUILTIN, (now / 500) % 2);
    delay(50);
    return;
  }

  _lastReport = now;
  _sctInitialized = true;
  _lastReportedCurrent = _currentRms;

  // 上报
  int code = reportData(_currentRms, 0);

  if (code == 200) {
    _rejectCount = 0;
    _lastOkReport = now;      // 记录成功上报时间，用于独立保底心跳
    _rateLimitUntil = 0;      // 成功后清除限流标记
    const char* thresholdSrc = _cloudThresholdSet ? "cloud" :
      (_learnedCount >= 7 ? "adaptive" : "default");
    LOG_I("SCT013", "RMS=%.4fA (changed=%s, interval=%lus, th=%.2fA/%s)",
      _currentRms, currentChanged ? "YES" : "NO", heartbeatInterval / 1000,
      SCT013_THRESHOLD_A, thresholdSrc);
    fetchConfig();
  } else if (code == 429) {
    LOG_W("SCT013", "Rate limited (th=%.2fA), waiting %ds...", SCT013_THRESHOLD_A, _retryAfterSec);
    // 收到 429 说明上报太频繁，从后端响应解析退避时间，不更新 _lastOkReport
    _rateLimitUntil = millis() + _retryAfterSec * 1000UL;
  } else if (code == 404) {
    _rejectCount++;
    LOG_W("Report", "Device disabled (%d/%d)", _rejectCount, REJECT_LIMIT);
    if (_rejectCount >= REJECT_LIMIT) {
      _reportIntervalMs = REJECT_POLL_MS;
      _rejectCount = 0;
    }
  } else {
    LOG_W("SCT013", "Report failed: %d", code);
  }

  #endif  // USE_SCT013

  #if !USE_FIRE_ALARM && !USE_SCT013
  unsigned long elapsed = (now >= _lastReport) ? (now - _lastReport) : (now + (0xFFFFFFFF - _lastReport));
  if (elapsed < _reportIntervalMs) {
    delay(100);
    return;
  }

  _lastReport = now;

  // 读传感器
  float val1 = 0;   // temp (DHT/DS18B20/CO2) 或 rawAdc (MQ-135)
  float val2 = 0;   // humidity (DHT/CO2)  或 airQualityScore (MQ-135)
  #if USE_CO2
  float val3 = 0;   // co2 (JW01-CO2 专用)
  #endif
  #if USE_CO2
    // JW01-CO2: 读 UART 解析 CO2/Temp/Humidity
    {
      String line = "";
      bool gotNewline = false;
      unsigned long co2Start = millis();
      // 等最多 1200ms 收集数据
      while ((millis() - co2Start) < 1200) {
        while (Serial1.available()) {
          char c = (char)Serial1.read();
          if (c == '\n' || c == '\r') { gotNewline = true; break; }
          line += c;
        }
        if (gotNewline) break;
        delay(1);
      }
      line.trim();
      if (line.length() > 0) {
        LOG_I("CO2", "RAW: %s", line.c_str());
        // 格式: ,, 00551ppm ,22.3C, 44.5%,ATMEL,
        int ppmIdx = line.indexOf("ppm");
        if (ppmIdx > 0) {
          int start = ppmIdx - 1;
          while (start >= 0 && line[start] >= '0' && line[start] <= '9') start--;
          val3 = line.substring(start + 1, ppmIdx).toInt();  // CO2 ppm

          int cIdx = line.indexOf("C,");
          if (cIdx < 0) cIdx = line.indexOf("C ");
          if (cIdx > 0) {
            start = cIdx - 1;
            while (start >= 0 && ((line[start] >= '0' && line[start] <= '9') || line[start] == '.')) start--;
            val1 = line.substring(start + 1, cIdx).toFloat();  // temp
          }

          int hIdx = line.indexOf("%,");
          if (hIdx < 0) hIdx = line.indexOf("% ");
          if (hIdx > 0) {
            start = hIdx - 1;
            while (start >= 0 && ((line[start] >= '0' && line[start] <= '9') || line[start] == '.')) start--;
            val2 = line.substring(start + 1, hIdx).toFloat();  // humidity
          }
          LOG_I("CO2", "CO2=%.0fppm Temp=%.1fC Hum=%.1f%%", val3, val1, val2);
        }
      } else {
        LOG_W("CO2", "No data from Serial1 (JW01-CO2)");
      }
    }
  #elif USE_MQ135
    // MQ-135: 读 ADC → 换算成 0~100 空气质量分数
    int rawAdc = analogRead(MQ135_PIN);            // 0~1024
    float adcVoltage = rawAdc * (3.3 / 1024.0);     // 分压后电压
    float sensorVoltage = adcVoltage * 3.0;          // 还原原始电压 (10k+20k 分压比 1:3)
    // 简单校准：电压越低 = 浓度越高 = 分数越低
    // 0V=极差, 5V=极好, 映射到 0~100
    float score = (sensorVoltage / 5.0) * 100.0;
    if (score > 100.0) score = 100.0;
    if (score < 0.0) score = 0.0;
    val1 = (float)rawAdc;
    val2 = score;
    LOG_I("MQ135", "ADC=%d, V=%.2fV, Score=%.1f", rawAdc, sensorVoltage, score);
  #elif USE_DS18B20
    ds18b20.requestTemperatures();
    val1 = ds18b20.getTempCByIndex(0);
    // DS18B20 不测湿度

    if (val1 == -127.0 || val1 == 85.0) {
      LOG_W("DS18B20", "Read failed (temp=%f)", val1);
    } else {
      LOG_I("DS18B20", "Temp=%.1fC", val1);
    }
  #else
  val1 = dht.readTemperature();
  val2 = dht.readHumidity();

  if (isnan(val1) || isnan(val2)) {
    LOG_W("DHT22", "Read failed (temp=%f, hum=%f)", val1, val2);
  } else if (val1 < -20.0 || val1 > 80.0 || val2 < 0.0 || val2 > 100.0) {
    LOG_W("DHT22", "Reading out of range (temp=%f, hum=%f), retrying...", val1, val2);
    delay(2100);
    val1 = dht.readTemperature();
    val2 = dht.readHumidity();
    if (isnan(val1) || isnan(val2) || val1 < -20.0 || val1 > 80.0 || val2 < 0.0 || val2 > 100.0) {
      LOG_W("DHT22", "Retry still invalid (temp=%f, hum=%f), keeping as-is", val1, val2);
    } else {
      LOG_I("DHT22", "Retry OK: Temp=%.1fC, Humidity=%.1f%%", val1, val2);
    }
  } else {
    LOG_I("DHT22", "Temp=%.1fC, Humidity=%.1f%%", val1, val2);
  }
  #endif

  // CO2 版：设置 co2 全局变量供 reportData 使用
  #if USE_CO2
  _co2Value = val3;
  #endif

  // 上报
  int code = reportData(val1, val2);

  if (code == 404) {
    // 设备被后端停用
    _rejectCount++;
    LOG_W("Report", "Device disabled (%d/%d), will switch to low-power poll", _rejectCount, REJECT_LIMIT);
    if (_rejectCount >= REJECT_LIMIT) {
      LOG_I("Report", "Entering low-power poll (every 30min)");
      _reportIntervalMs = REJECT_POLL_MS;  // 30 分钟一次
      _rejectCount = 0;
    }
    } else if (code == 200) {
    _rejectCount = 0;
    _rateLimitUntil = 0;      // 成功后清除限流标记
#if !BATTERY_MODE
    // 有缓存 → 补发缓存的旧数据（最多 5 条，每条间隔 2 秒，避免被限流）
    int resent = 0;
    while (cache.count > 0 && resent < 5) {
      delay(2000);  // 每条间隔 2 秒，遵守后端限流
      int idx = cache.head;
      float ct = cache.data[idx * 2];
      float ch = cache.data[idx * 2 + 1];
      cache.head = (cache.head + 1) % CACHE_MAX;
      cache.count--;
      int retry = reportData(ct, ch);
      if (retry == 200) {
        resent++;
      } else {
        break;  // 补发失败就停，下次心跳再说
      }
    }
    if (cache.count == 0) cache.head = 0;
#endif
  } else if (code == 429) {
    // 被限流，退避后重试（不缓存，429 的数据已正常读取，只是发太快）
    LOG_W("Report", "Rate limited, waiting %ds...", _retryAfterSec);
    _rateLimitUntil = millis() + _retryAfterSec * 1000UL;
    delay(100);
  } else if (code > 0 && code != 404) {
#if !BATTERY_MODE && !USE_MQ135 && !USE_FIRE_ALARM
    // 服务器其他错误（500 等）→ 缓存（MQ-135 / 火焰版不缓存）
    cachePush(val1, val2);
#endif
  } else if (code <= 0) {
#if !BATTERY_MODE && !USE_MQ135 && !USE_FIRE_ALARM
    // 网络错误（-1, 超时等）→ 缓存（MQ-135 / 火焰版不缓存）
    cachePush(val1, val2);
#endif
  }

  // 拉取云端配置（更新下次上报间隔）
  if (code == 200) {
    fetchConfig();
  }
#elif USE_DS18B20 && USE_SCT013
  // --- DS18B20 + SCT013 复合版 loop ---
  // 策略：同时读温度 + 电流，按 _reportIntervalMs（默认 120s）定时上报
  // ==========================================
  now = millis();
  unsigned long compRmsElapsed = (now >= _lastRmsTime) ? (now - _lastRmsTime) : (now + (0xFFFFFFFF - _lastRmsTime));

  // 每 3 秒采一次 RMS
  if (compRmsElapsed >= SCT013_RMS_INTERVAL_MS) {
    _lastRmsTime = now;
    float sumSquared = 0;
    int sampleCount = 0;
    unsigned long sampleStart = micros();
    for (int i = 0; i < SCT013_RMS_SAMPLES; i++) {
      float adc = analogRead(A0);
      float zeroed = adc - _currentBaseline;
      sumSquared += zeroed * zeroed;
      sampleCount++;
      if (micros() - sampleStart >= (unsigned long)SCT013_RMS_SAMPLES * SCT013_SAMPLE_INTERVAL_US) break;
      delayMicroseconds(SCT013_SAMPLE_INTERVAL_US);
    }
    float rmsAdc = sqrt(sumSquared / sampleCount);
    float rmsVoltage = rmsAdc * (3.3 / 1024.0);
    float rmsCurrent = rmsVoltage * 30.0;
    _currentRms = rmsCurrent;

    if (_currentRms > 20.0 || _currentRms < 0.0) {
      _currentRms = 0;  // 异常值归零
    }
  }

  // 定时上报
  unsigned long elapsed = (now >= _lastReport) ? (now - _lastReport) : (now + (0xFFFFFFFF - _lastReport));
  if (elapsed < _reportIntervalMs) {
    digitalWrite(LED_BUILTIN, (now / 500) % 2);
    delay(100);
    return;
  }

  _lastReport = now;

  // 读 DS18B20（失败重试一次）
  ds18b20.requestTemperatures();
  float temp = ds18b20.getTempCByIndex(0);
  if (temp == -127.0 || temp > 125.0 || temp < -55.0) {
    LOG_W("DS18B20", "Read failed (temp=%.1f), retrying...", temp);
    delay(750);
    ds18b20.requestTemperatures();
    temp = ds18b20.getTempCByIndex(0);
    if (temp == -127.0 || temp > 125.0 || temp < -55.0) {
      LOG_W("DS18B20", "Retry still failed (temp=%.1f)", temp);
      temp = 0;
    } else {
      LOG_I("DS18B20", "Retry OK: Temp=%.1fC", temp);
    }
  }
  LOG_I("DS18B20", "Temp=%.1fC", temp);

  // 限流退避
  bool compLimited = (_rateLimitUntil > 0 && now < _rateLimitUntil);
  if (compLimited) {
    LOG_W("Composite", "Rate limited, skip this round");
    delay(100);
    return;
  }

  // 上报（val1=temp）
  int compCode = reportData(temp, 0);

  if (compCode == 200) {
    _rejectCount = 0;
    _rateLimitUntil = 0;
    LOG_I("Composite", "Temp=%.1fC Current=%.4fA", temp, _currentRms);
    fetchConfig();
  } else if (compCode == 429) {
    LOG_W("Composite", "Rate limited, backing off %ds...", _retryAfterSec);
    _rateLimitUntil = millis() + _retryAfterSec * 1000UL;
  } else if (compCode == 404) {
    _rejectCount++;
    if (_rejectCount >= REJECT_LIMIT) {
      _reportIntervalMs = REJECT_POLL_MS;
      _rejectCount = 0;
    }
  }
#endif  // !USE_FIRE_ALARM && !USE_SCT013 — 关闭非火焰/非SCT版 loop 分支

}
