# SCT-013-000 电流测试固件 CHANGELOG

## v1.0 (2026-05-05)

### 新增
- 🆕 初始版本：ADC 采样 2000 点计算 RMS 电流
- 🆕 P008 平台自动注册（序列号 SCT-TEST-{chipId}）
- 🆕 每 5 秒上报电流值（`otherData.currentA`）
- 🆕 启动自动校准偏置（1000 次采样取平均）
- 🆕 死区 0.1A 过滤空载噪声
- 🆕 配网 WiFiManager Portal（热点 P008-SCT-Test）

### 技术细节
- 硬件：NodeMCU v3 + SCT-013-000（10A 电压输出型）
- SCT-013-000 红线接 A0，黑线接 GND，零外围元件
- 量程 0-10A，输出 0-1V（0.1V/A）
- 序列号前缀 SCT-TEST- 需后端 ALLOWED_SERIAL_PREFIXES 支持

---

*P008 环境监测系统 · SCT-013-000 电流测试固件 v1.0*
