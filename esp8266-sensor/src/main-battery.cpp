/**
 * P008 Environment Monitor - ESP8266 Firmware v3.2 定风电池版
 * -----------------------------------------------------------
 * Hardware: NodeMCU V3 (ESP8266) + DHT22
 * 供电: 锂电池 + 18650 充电模块
 *
 * 工作原理:
 *   setup:    唤醒 → 读DHT22 → WiFi连网 → HTTPS上报 → deepSleep
 *   loop:    不用（每次启动唤醒一次，上报完就睡）
 *
 * 条件:
 *   ⚠️ GPIO16 (D0) 必须接到 RST 引脚——否则 deepSleep 永远醒不来
 *   ⚠️ USB 供电也可以用，但会反复重启（每 N 秒重启一次）
 *
 * 功耗:
 *   - 上报时: ~170mA（WiFi 发射峰值）
 *   - 上报耗时: ~3-5 秒
 *   - 休眠时: ~20μA（ESP.deepSleep 模式）
 *   - 平均: 18650（3000mAh）→ 约 300 天
 *
 * Pinout:
 *   D0(GPIO16): 接 RST（deepSleep 唤醒必须）
 *   D1(GPIO5):  DHT22 DATA
 *   D4(GPIO2):  LED_BUILTIN（不接，低功耗）
 *
 * 首次配网: 手机连 P008-Env-Monitor 热点 → 192.168.4.1 配WiFi
 * 换WiFi:   按住 FLASH 按钮上电 → 进入配网模式
 *
 * 已知限制:
 *   - 不支持 404 检测（每次苏醒时间很短，无法累计）
 *   - 不支持云端 reportInterval 动态拉取（deepSleep 后状态丢失）
 *   - 不支持缓存补发（同上）
 *   - 配网时功耗 ~200mA，配网超时 2 分钟
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <DHT.h>

#include "config.h"
#include "log.h"

// --------------- 全局变量 ---------------
WiFiClientSecure wifiClientSecure;
HTTPClient http;
DHT dht(DHT_PIN, DHT_TYPE);

char deviceSerial[32] = "";
char deviceKey[64]   = "";
char chipIdHex[16]   = "";

unsigned long _bootStart = 0;
#define WIFI_TIMEOUT_MS 10000   // WiFi 最久等 10 秒
#define HTTP_TIMEOUT_MS 8000    // HTTP 最久等 8 秒（电池版多留点余量）
#define TOTAL_TIMEOUT_MS 20000  // 从头到尾最久 20 秒，超时直接睡

// --------------- SHA256 设备密钥 ---------------
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
  snprintf(deviceSerial, sizeof(deviceSerial), "DHT22-%08X", chipId);
  generateDeviceKey(deviceSerial, deviceKey, sizeof(deviceKey));
  LOG_I("Identity", "Serial: %s", deviceSerial);
  LOG_I("Identity", "Key: %s", deviceKey);
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
  }

  LOG_I("WiFi", "Connected, IP: %s", WiFi.localIP().toString().c_str());
  return true;
}

// --------------- 配网 Portal ---------------
void startConfigPortal() {
  LOG_I("WiFiManager", "Starting config portal...");
  // 配网模式下不 sleep，防止配网超时进入 deepSleep
  WiFiManager wm;
  wm.setConfigPortalTimeout(120);
  wm.startConfigPortal(AP_NAME);
  LOG_I("WiFiManager", "Portal done, restarting...");
  delay(100);
  ESP.restart();
}

// --------------- 上报 ---------------
int reportData(float temp, float humidity) {
  char url[256];
  snprintf(url, sizeof(url), "%s/devices/%s/data", API_BASE_URL, deviceSerial);

  char body[256];
  snprintf(body, sizeof(body),
    "{\"temp\":%.1f,\"humidity\":%.1f,\"battery\":1,\"otherData\":{"
    "\"firmwareVer\":\"3.2\",\"chipId\":\"%s\",\"power\":\"battery\""
    "}}",
    temp, humidity, chipIdHex);

  wifiClientSecure.setInsecure();
  http.begin(wifiClientSecure, url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  int code = http.POST(body);
  LOG_I("HTTP", "Code: %d", code);
  http.end();
  return code;
}

// --------------- setup （也是 loop：一次启动→上报→deepSleep） ---------------
void setup() {
  _bootStart = millis();
  Serial.begin(SERIAL_BAUD);
  delay(100);

  // 第一次启动日志
  static uint32_t bootCount = 0;
  bootCount++;
  Serial.println("\n========================================\n");
  LOG_I("Boot", "P008 Env Monitor v3.2 (定风电池版)");
  LOG_I("Boot", "Boot #%d", bootCount);

  generateIdentity();

  // Flash 按钮按 3 秒 → 配网（不进 deepSleep）
  pinMode(0, INPUT_PULLUP);
  int holdMs = 0;
  while (digitalRead(0) == LOW && holdMs < 3000 && (millis() - _bootStart) < 5000) {
    delay(10);
    holdMs += 10;
  }
  if (holdMs >= 3000) {
    LOG_I("FlashBtn", "3s hold → config portal");
    startConfigPortal();
    return;  // 配网不会走回来
  }

  // WiFi
  bool wifiOk = connectWiFi();
  if (!wifiOk) {
    LOG_W("Main", "WiFi failed, entering config portal...");
    startConfigPortal();
    return;
  }

  // 传感器
  dht.begin();
  delay(200);

  // 读传感器
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temp) || isnan(humidity)) {
    LOG_W("DHT22", "Read failed (temp=%f, hum=%f)", temp, humidity);
  } else {
    LOG_I("DHT22", "Temp=%.1fC, Humidity=%.1f%%", temp, humidity);
  }

  // 上报（允许 2 次重试）
  int code = -1;
  for (int retry = 0; retry < 2 && code != 200 && (millis() - _bootStart) < TOTAL_TIMEOUT_MS; retry++) {
    if (retry > 0) {
      LOG_W("Report", "Retry #%d...", retry);
      delay(500);
    }
    code = reportData(temp, humidity);
  }

  float uptime = (millis() - _bootStart) / 1000.0;
  LOG_I("Sleep", "Uptime=%.1fs, going to sleep for %llus", uptime, DEEP_SLEEP_US / 1000000ULL);

  // deepSleep 前关闭 WiFi 和串口
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.flush();
  Serial.end();

  ESP.deepSleep(DEEP_SLEEP_US);
}

// --------------- loop（几乎不用） ---------------
void loop() {
  // deepSleep 唤醒后实际上重启了，不会走到这里
  // 但如果 GPIO16 没接 RST，会卡在这里不断重启
  delay(60000);
  ESP.restart();
}
