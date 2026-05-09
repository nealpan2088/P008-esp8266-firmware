/**
 * JW01-CO2 模块验证测试
 * 
 * 接线：
 *   JW01-CO2 B(TX)  → NodeMCU D6 (GPIO12) = Serial1 RX
 *   JW01-CO2 A(RX)  → NodeMCU D7 (GPIO13) = Serial1 TX（可不接）
 *   JW01-CO2 +5     → NodeMCU VU (5V)
 *   JW01-CO2 G      → NodeMCU GND
 *
 * 打开串口监视器 (115200 baud)，看是否收到 CO2 数据
 */

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n========================================");
  Serial.println("JW01-CO2 Test");
  Serial.println("========================================");

  // JW01-CO2 默认波特率 9600
  Serial1.begin(9600);
  Serial.println("Serial1 started at 9600 baud");
  Serial.println("Waiting for data...\n");
}

void loop() {
  while (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      Serial.print("[RAW] ");
      Serial.println(line);

      int co2 = 0;
      float temp = 0;
      float hum = 0;

      int ppmIdx = line.indexOf("ppm");
      if (ppmIdx > 0) {
        int start = ppmIdx - 1;
        while (start >= 0 && (line[start] >= '0' && line[start] <= '9')) start--;
        co2 = line.substring(start + 1, ppmIdx).toInt();

        int cIdx = line.indexOf("C,");
        if (cIdx < 0) cIdx = line.indexOf("C ");
        if (cIdx > 0) {
          start = cIdx - 1;
          while (start >= 0 && ((line[start] >= '0' && line[start] <= '9') || line[start] == '.')) start--;
          temp = line.substring(start + 1, cIdx).toFloat();
        }

        int hIdx = line.indexOf("%,");
        if (hIdx < 0) hIdx = line.indexOf("% ");
        if (hIdx > 0) {
          start = hIdx - 1;
          while (start >= 0 && ((line[start] >= '0' && line[start] <= '9') || line[start] == '.')) start--;
          hum = line.substring(start + 1, hIdx).toFloat();
        }
      }

      if (co2 > 0) {
        Serial.print("[PARSED] CO2=");
        Serial.print(co2);
        Serial.print("ppm");
        if (temp > 0) { Serial.print(" Temp="); Serial.print(temp); Serial.print("C"); }
        if (hum > 0) { Serial.print(" Hum="); Serial.print(hum); Serial.print("%"); }
        Serial.println();
      }
    }
  }
  delay(10);
}
