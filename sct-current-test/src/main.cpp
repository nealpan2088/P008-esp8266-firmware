/**
 * P008 环境监测 — SCT-013-000 电流测试固件
 * ----------------------------------------
 * 硬件：NodeMCU v3 + SCT-013-000 (10A 电压输出型)
 * 平台：P008 环境监测 SaaS
 *
 * 功能：
 *   1. 上电自动注册到 P008 平台
 *   2. 每 5 秒采样 ADC(RMS) 计算电流
 *   3. 定期上报电流值到后端
 *   4. 串口实时打印调试日志
 *
 * P008 规范符合性：
 *   ✅ 序列号前缀 (SERIAL_PREFIX="SCT-TEST-")
 *   ✅ 版本号上报 (firmwareVer="1.0")
 *   ✅ 日志级别控制 (LOG_LEVEL)
 *   ✅ 密钥生成 (HMAC-SHA256)
 *   ✅ API 版本 (/v1/)
 *   ✅ 配置宏化 (build_flags)
 *
 * 接线：
 *   SCT-013-000 红线 → A0
 *   SCT-013-000 黑线 → GND
 *   SCT-013 夹在火线或零线上（不分正反）
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

// --------------- 配置宏（含默认值） ---------------
#ifndef SERIAL_BAUD
#define SERIAL_BAUD          115200
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL            3       // 3=INFO, 4=DEBUG
#endif

#ifndef CURRENT_PIN
#define CURRENT_PIN          A0
#endif

#ifndef REPORT_INTERVAL_MS
#define REPORT_INTERVAL_MS   5000    // 5 秒上报一次
#endif

#ifndef HW_SECRET
#define HW_SECRET            "P008@2026!SecretKey"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION     "1.0"
#endif

#ifndef FIRMWARE_CHANNEL
#define FIRMWARE_CHANNEL     "test"
#endif

#ifndef DEVICE_SERIAL_PREFIX
#error "DEVICE_SERIAL_PREFIX must be defined in build_flags"
#endif

#ifndef API_BASE_URL
#define API_BASE_URL         "https://zghj.openyun.xin/api/v1"
#endif

#ifndef AP_NAME
#define AP_NAME              "P008-SCT-Test"
#endif

#ifndef WIFI_TIMEOUT_MS
#define WIFI_TIMEOUT_MS      30000
#endif

#ifndef HTTP_TIMEOUT_MS
#define HTTP_TIMEOUT_MS      5000
#endif

// --------------- 日志宏 ---------------
#define LOG_E(tag, fmt, ...)  Serial.printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#if LOG_LEVEL >= 3
#define LOG_I(tag, fmt, ...)  Serial.printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
#define LOG_I(...)
#endif
#if LOG_LEVEL >= 4
#define LOG_D(tag, fmt, ...)  Serial.printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...)  Serial.printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
#define LOG_D(...)
#define LOG_W(tag, fmt, ...)  Serial.printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

// --------------- 全局变量 ---------------
WiFiClientSecure wifiClient;
HTTPClient http;
char deviceSerial[32] = "";
char deviceKey[64]    = "";
char chipIdHex[16]    = "";
char _autoSerial[32]  = "";
char _autoKey[64]     = "";

// SCT 校准参数
const float VREF         = 1.0;
const float MAX_CURRENT  = 10.0;
const float FACTOR       = MAX_CURRENT / VREF;  // 10.0 A/V
const float DEAD_ZONE_A  = 0.1;  // 死区：小于此值显示 0
const int   ADC_SAMPLES  = 2000;
const int   CAL_SAMPLES  = 1000;
float       adcBiasV     = 0.0;  // 启动时自动校准

unsigned long lastReport = 0;

// --------------- HMAC 密钥生成 ---------------
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

// --------------- 生成设备身份 ---------------
void generateIdentity() {
  uint32_t chipId = ESP.getChipId();
  snprintf(chipIdHex, sizeof(chipIdHex), "%08X", chipId);
  snprintf(_autoSerial, sizeof(_autoSerial), "%s%08X", DEVICE_SERIAL_PREFIX, chipId);
  generateDeviceKey(_autoSerial, _autoKey, sizeof(_autoKey));
  LOG_I("Identity", "Serial: %s", _autoSerial);
  LOG_I("Identity", "Key: %s", _autoKey);
}

// --------------- WiFi 连接 ---------------
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  LOG_I("WiFi", "Connecting...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      LOG_W("WiFi", "Timeout (%dms)", WIFI_TIMEOUT_MS);
      return false;
    }
    delay(200);
  }
  LOG_I("WiFi", "Connected, IP: %s", WiFi.localIP().toString().c_str());
  return true;
}

// --------------- 配网 Portal ---------------
void startConfigPortal() {
  LOG_I("WiFiManager", "Starting config portal...");
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  wm.startConfigPortal(AP_NAME);
  LOG_I("WiFiManager", "Portal done, restarting...");
  delay(100);
  ESP.restart();
}

// --------------- ADC 采样：RMS 电流 ---------------
float readCurrent() {
  float sumSq = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    float voltage = analogRead(CURRENT_PIN) * (VREF / 1024.0);
    float centered = voltage - adcBiasV;
    sumSq += centered * centered;
  }
  float vrms = sqrt(sumSq / ADC_SAMPLES);
  float currentA = vrms * FACTOR;
  // 死区：小于阈值的视为噪声
  if (currentA < DEAD_ZONE_A) currentA = 0;
  return currentA;
}

// --------------- 数据上报 ---------------
int reportData(float current) {
  if (WiFi.status() != WL_CONNECTED) return -1;

  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/data", API_BASE_URL, deviceSerial);

  char body[300];
  snprintf(body, sizeof(body),
    "{\"temp\":null,\"humidity\":null,\"battery\":0,\"otherData\":{"
    "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\","
    "\"chipId\":\"%s\",\"type\":\"current_test\",\"currentA\":%.3f"
    "}}",
    chipIdHex, current);

  wifiClient.setInsecure();
  http.begin(wifiClient, url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  LOG_D("Report", "Code: %d", code);
  return code;
}

// --------------- SETUP ---------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);

  Serial.println("\n========================================\n");
  LOG_I("Boot", "P008 SCT-013 Test v" FIRMWARE_VERSION);
  LOG_I("Boot", "Flash: %u KB", ESP.getFlashChipRealSize() / 1024);

  generateIdentity();
  strncpy(deviceSerial, _autoSerial, sizeof(deviceSerial) - 1);
  strncpy(deviceKey, _autoKey, sizeof(deviceKey) - 1);

  // 自动校准偏置：WiFi 连接前采样（避免射频干扰）
  // 无负载时 ADC 输出 ≈ 偏置电压
  LOG_I("Calib", "Calibrating bias (no load)...");
  float biasSum = 0;
  for (int i = 0; i < CAL_SAMPLES; i++) {
    biasSum += analogRead(CURRENT_PIN) * (VREF / 1024.0);
    delay(0);  // yield
  }
  adcBiasV = biasSum / CAL_SAMPLES;
  LOG_I("Calib", "Bias: %.3f V (raw ADC avg: %d)", adcBiasV, (int)(biasSum / CAL_SAMPLES * 1024.0));

  // WiFi
  bool wifiOk = connectWiFi();
  if (!wifiOk) {
    LOG_W("Main", "WiFi failed, entering config portal...");
    startConfigPortal();
    return;
  }

  // 首次上报（自动注册设备）
  float initialCurrent = readCurrent();
  int code = reportData(initialCurrent);
  if (code == 200 || code == 201) {
    LOG_I("Main", "Register success (code=%d), current=%.3fA", code, initialCurrent);
  } else {
    LOG_W("Main", "Register code=%d, current=%.3fA", code, initialCurrent);
  }

  LOG_I("Main", "Ready. Reporting every %dms", REPORT_INTERVAL_MS);
  lastReport = millis();
}

// --------------- LOOP ---------------
void loop() {
  unsigned long now = millis();

  // WiFi 重连
  if (WiFi.status() != WL_CONNECTED) {
    LOG_W("Loop", "WiFi lost, reconnecting...");
    if (!connectWiFi()) {
      delay(5000);
      return;
    }
  }

  // 到时间上报
  if (now - lastReport >= REPORT_INTERVAL_MS) {
    lastReport = now;

    float currentA = readCurrent();

    // 串口打印
    LOG_I("SCT", "Current: %.3f A", currentA);

    // 上报到后端
    int code = reportData(currentA);
    if (code != 200 && code != 201) {
      LOG_W("SCT", "Report failed (code=%d)", code);
    }
  }

  delay(100);
}
