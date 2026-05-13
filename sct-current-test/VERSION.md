# SCT-013-000 电流测试固件 — 版本管理

> 最后更新：2026-05-10

## 概览

**位置**：`hardware/sct-current-test/`
**编译平台**：PlatformIO（`platformio.ini`，env: `nodemcuv3`）
**当前版本**：v1.0
**硬件**：NodeMCU V3 + SCT-013-000（10A 电压输出型开合式电流互感器）
**序列号前缀**：`SCT-TEST-`
**上报字段**：`otherData.current`（标准字段）
**配网方式**：WiFiManager Portal（AP 热点 `P008-SCT-Test`）

## 接线

| SCT-013-000 | NodeMCU |
|-------------|---------|
| 红线（信号） | A0 |
| 黑线（GND） | GND |

- **零外围元件**，直接接线即可
- 钳口夹在**单根**电源线上（不能同时夹零火线）
- 量程 0-10A，输出 0-1V（0.1V/A）

## 功能

- ADC 采样 2000 点计算 RMS 电流
- 启动自动校准偏置（空载采样 1000 次取平均）
- 死区 0.1A 过滤空载噪声
- 自动注册到 P008 平台
- 每 5 秒上报电流值

## 版本历史

| 版本 | 日期 | 关键变更 |
|------|------|---------|
| v1.0 | 2026-05-05 | 初始版本，基础电流测试 |

详细变更见 [`CHANGELOG.md`](./CHANGELOG.md)。

## 相关后端配置

`apps/backend/src/config/constants.js` 中需包含：
- `ALLOWED_SERIAL_PREFIXES: ['SCT-TEST-', ...]`
- `SERIAL_TO_SCENE: { 'SCT-TEST-': 'CURRENT_METER', ... }`

## 固件字段铁律

- 字段名一次定死，不因任何原因改动
- 定稿字段：`current`（标准），历史遗留兼容 `currentA` 由后端归一化处理
