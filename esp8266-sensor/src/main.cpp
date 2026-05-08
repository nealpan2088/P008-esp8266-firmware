/**
 * P008 Environment Monitor - ESP8266 Firmware
 * -----------------------------------------------------------
 * Hardware: NodeMCU V3 (ESP8266) + DHT22 / DS18B20 / MQ-135
 *
 * 三种模式（通过 build_flags 宏切换）:
 *   BATTERY_MODE=0 (默认): 定风版 loop 方案 —— WiFi 常连，每 N 秒上报一次
 *   BATTERY_MODE=1:        定风电池版 deepSleep 方案 —— 上报完就睡
 *   USE_MQ135=1:           MQ-135 空气质量传感器版，上报 airQuality
 *
 * 定风版（插电）:
 *   setup:    配 WiFi → 连上后保持在线
 *   loop:    每 60 秒读传感器 → HTTPS 上报一次
 *   不做:    deepSleep / MQTT / 多传感器
 *   Pinout:   D1(GPIO5): DHT22 DATA, D4(GPIO2): LED_BUILTIN
 *
 * 定风电池版:
 *   setup:    唤醒 → 读DHT22 → WiFi连网 → HTTPS上报 → deepSleep
 *   loop:    不用（每次启动唤醒一次，上报完就睡）
 *   条件:     ⚠️ GPIO16(D0) 必须接 RST，否则永远不睡
 *   Pinout:   D0(GPIO16): → RST, D1(GPIO5): DHT22 DATA
 *
 * MQ-135 版:
 *   setup:    同定风版，但读 MQ-135 ADC 代替 DHT22
 *   loop:    每 60 秒读 AO（经分压）→ 计算 airQuality 分数 → 上报
 *   条件:     ⚠️ AO 输出 0~5V，8266 ADC 0~3.3V，必须加分压电阻！
 *   Pinout:   MQ-135 AO → 分压电路 → A0
 *
 * 首次配网: 手机连 P008-Env-Monitor 热点 → 192.168.4.1 配WiFi
 * 换WiFi:   按住 FLASH 按钮上电 → 进入配网模式
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#if USE_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#elif USE_MQ135
// MQ-135 直接用 ADC 读，不需要额外库
#else
#include <DHT.h>
#endif

#include "config.h"
#include "log.h"

// --------------- 全局变量 ---------------
WiFiClientSecure wifiClientSecure;
HTTPClient http;

#if USE_DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
#else
DHT dht(DHT_PIN, DHT_TYPE);
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

  char body[360];
#if USE_MQ135
  // MQ-135 上报：airQuality 综合分数 + 原始 ADC 值
  {
    int rawAdc = (int)val1;             // 原始 ADC 读数 (0~1024)
    float airQualityScore = val2;       // 校准后的综合分数 (0~100)
    snprintf(body, sizeof(body),
      "{\"airQuality\":%.1f,\"rawAdc\":%d,\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"sensor\":\"MQ-135\""
      "}}",
      airQualityScore, rawAdc, chipIdHex);
  }
#elif BATTERY_MODE
  snprintf(body, sizeof(body),
    "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":1,\"otherData\":{"
    "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"power\":\"battery\""
    "}}",
    val1, val2, chipIdHex);
#else
  // 插电版 body
  #if USE_DOOR_SENSOR
    bool doorOpen = (digitalRead(DOOR_PIN) == HIGH);
    _doorChanged = (doorOpen != _doorLastState);
    _doorLastState = doorOpen;
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":0,\"doorOpen\":%s,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\"%s"
      "}}",
      val1, val2,
      doorOpen ? "true" : "false",
      chipIdHex,
      _doorChanged ? ",\"event\":\"door_toggle\"" : "");
  #else
    #if USE_DS18B20
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"sensor\":\"DS18B20\",\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\",\"power\":\"plug\""
      "}}",
      val1, chipIdHex);
    #else
    snprintf(body, sizeof(body),
      "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":0,\"otherData\":{"
      "\"firmwareVer\":\"" FIRMWARE_VERSION "\",\"channel\":\"" FIRMWARE_CHANNEL "\",\"chipId\":\"%s\""
      "}}",
      val1, val2, chipIdHex);
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

  int code = http.POST(body);
  LOG_I("HTTP", "Code: %d", code);
  http.end();
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
  } else {
    LOG_D("Config", "GET failed: %d", code);
  }
  http.end();
}

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
  #else
    dht.begin();
    delay(200);

    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temp) || isnan(humidity)) {
      LOG_W("DHT22", "Read failed (temp=%f, hum=%f)", temp, humidity);
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
  LOG_I("Boot", "P008 Env Monitor v3.1 (定风版 loop)");
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
  #if USE_MQ135
    // MQ-135 不需要额外初始化，ADC 直接可用
    pinMode(MQ135_PIN, INPUT);
    LOG_I("MQ135", "Init OK (pin=%d)", MQ135_PIN);
  #elif USE_DS18B20
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
    connectWiFi();
  }

  // 到时间才上报
  unsigned long now = millis();
  unsigned long elapsed = (now >= _lastReport) ? (now - _lastReport) : (now + (0xFFFFFFFF - _lastReport));
  if (elapsed < _reportIntervalMs) {
    delay(100);
    return;
  }

  _lastReport = now;

  // 读传感器
  float val1 = 0;   // temp (DHT/DS18B20) 或 rawAdc (MQ-135)
  float val2 = 0;   // humidity (DHT)   或 airQualityScore (MQ-135)
  #if USE_MQ135
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
  } else {
    LOG_I("DHT22", "Temp=%.1fC, Humidity=%.1f%%", val1, val2);
  }
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
#if !BATTERY_MODE
    // 有缓存 → 补发缓存的旧数据（最多 5 条，避免卡太久）
    int resent = 0;
    while (cache.count > 0 && resent < 5) {
      int idx = cache.head;
      float ct = cache.data[idx * 2];
      float ch = cache.data[idx * 2 + 1];
      cache.head = (cache.head + 1) % CACHE_MAX;
      cache.count--;
      if (reportData(ct, ch) == 200) {
        resent++;
      } else {
        break;
      }
    }
    if (cache.count == 0) cache.head = 0;
#endif
  } else if (code > 0 && code != 404) {
#if !BATTERY_MODE && !USE_MQ135
    // 服务器其他错误（500 等）→ 缓存（MQ-135 不缓存）
    cachePush(val1, val2);
#endif
  } else if (code <= 0) {
#if !BATTERY_MODE && !USE_MQ135
    // 网络错误（-1, 超时等）→ 缓存（MQ-135 不缓存）
    cachePush(val1, val2);
#endif
  }

  // 拉取云端配置（更新下次上报间隔）
  if (code == 200) {
    fetchConfig();
  }
}
