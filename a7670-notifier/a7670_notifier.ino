/**
 * P008 A7670C 4G 短信通知模块 Demo
 * 
 * 功能：接收服务器指令，通过 A7670 模块发送短信通知客户
 * 用途：微信推送失败的保底方案，电话/短信一定能触达客户
 * 
 * 接线：A7670 → Arduino/ESP8266
 *       TX   →  RX (GPIO4/D2)
 *       RX   →  TX (GPIO5/D1)
 *       VCC  →  5V (注意供电，建议独立电源)
 *       GND  →  GND
 * 
 * 平台：Hardware Demo - 供后续开发参考
 */

#include <SoftwareSerial.h>

// A7670 串口（如果不是用硬件串口）
SoftwareSerial a7670(4, 5); // RX=D2, TX=D1

// ===== 服务器 API 配置（同 P008 后端） =====
const char* serverHost = "zghj.openyun.xin";
const int serverPort = 443;  // HTTPS

void setup() {
  Serial.begin(115200);
  a7670.begin(115200);
  
  Serial.println("=== A7670C 短信通知模块 ===");
  Serial.println("等待模块启动...");
  delay(3000);
  
  // 初始化模块
  initModule();
}

void loop() {
  // 轮询服务器，检查是否有需要发送的短信指令
  checkServerCommands();
  delay(30000);  // 每30秒检查一次
}

/**
 * 初始化 A7670 模块
 */
void initModule() {
  // 检查模块响应
  sendAT("AT", 2000, "检查模块响应");
  
  // 关闭回显
  sendAT("ATE0", 1000, "关闭回显");
  
  // 检查 SIM 卡
  sendAT("AT+CPIN?", 2000, "检查 SIM 卡");
  
  // 检查信号质量
  sendAT("AT+CSQ", 1000, "信号质量");
  
  // 注册网络
  sendAT("AT+CREG?", 2000, "网络注册状态");
  
  // 设置短信为文本模式
  sendAT("AT+CMGF=1", 1000, "短信文本模式");
  
  Serial.println("=== 模块初始化完成 ===");
}

/**
 * 轮询服务器，检查是否有待发送的短信通知
 */
void checkServerCommands() {
  Serial.println("检查服务器是否有短信任务...");
  
  // 通过 HTTP GET 获取待发送的通知队列
  // 后端需要一个接口：GET /api/v1/notify/pending?device=xxx
  // 返回 JSON：{"phone":"138xxxx","message":"冷库温度偏高"}
  
  // TODO: 使用 AT+HTTPGET 或 AT+QHTTPURL 实现
  // 或者通过 ESP8266 WiFi 模块 + 本模块串口通信获取
}

/**
 * 发送短信
 * @param number 手机号
 * @param text 短信内容
 */
bool sendSMS(String number, String text) {
  Serial.print("发送短信到 ");
  Serial.print(number);
  Serial.print(": ");
  Serial.println(text);
  
  // AT+CMGS 指令
  a7670.println("AT+CMGS=\"" + number + "\"");
  
  // 等待 ">" 提示符
  if (waitForResponse(">", 5000)) {
    a7670.print(text);
    delay(200);
    a7670.write(26);  // Ctrl+Z 发送
    return waitForResponse("OK", 10000);
  }
  
  Serial.println("短信发送失败");
  return false;
}

/**
 * 拨打电话（TTS 语音通知 - 需要外接喇叭功放）
 */
bool makeCall(String number) {
  Serial.print("拨打电话: ");
  Serial.println(number);
  
  // ATD 拨号
  a7670.println("ATD" + number + ";");
  return waitForResponse("OK", 10000);
}

/**
 * 挂断电话
 */
void hangUp() {
  a7670.println("ATH");
  delay(500);
}

/**
 * 发送 AT 指令并等待响应
 */
bool sendAT(String cmd, int timeout, String desc) {
  Serial.print("[AT] ");
  Serial.print(desc);
  Serial.print(": ");
  Serial.println(cmd);
  
  a7670.println(cmd);
  bool ok = waitForResponse("OK", timeout);
  Serial.println(ok ? "  ✓" : "  ✗");
  return ok;
}

/**
 * 等待指定响应
 */
bool waitForResponse(String expected, int timeout) {
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    if (a7670.available()) {
      char c = a7670.read();
      response += c;
      Serial.print(c);
      
      if (response.indexOf(expected) != -1) {
        return true;
      }
    }
  }
  
  return response.indexOf(expected) != -1;
}
