# SCT-013 雷电版 — RMS 有效值采样规格

## 概述
SCT-013（雷电版）从 v1.0.1 起采用 RMS 有效值采样方案，
替代旧版的"每1秒1次ADC碰运气"方式。

## 采样参数

| 参数 | 值 | 说明 |
|------|-----|------|
| RMS 采样点数 | 200 | 每次 RMS 计算采 200 个 ADC 点 |
| 采样间隔 | 100μs | 10kHz 采样率 |
| 采样周期 | 3 秒 | 每 3 秒计算一次 RMS |
| 覆盖周波 | 2 个 | 200点 × 100μs = 20ms，覆盖完整 50Hz |
| 基线校准 | 30 点 | 启动时采 30 个点得零漂基线 |
| 换算系数 | 30.0 | SCT-013-30 型号（30A:1V）|

## 算法流程

```
启动校准：30 点 ADC 平均 → 零漂基线

每 3 秒循环：
  1. 密集采 200 个 ADC 点（10kHz）
  2. 每个点减去基线，平方累加
  3. RMS = sqrt(平方和 / 有效采样数)
  4. 电流 = RMS × (3.3/1024) × 30.0
  5. 更新自适应噪声窗口
  6. 如果 |当前RMS - 上次上报值| > 阈值 → 上报
```

## 自适应变化阈值

### 优先级
1. **云端配置** — 管理员设置 `sct013ChangeThreshold`，固件拉取后覆盖
2. **自学习** — 无云端配置时，每 7 次 RMS（约 21 秒）学习一次
3. **默认值** — 学习期间使用 0.1A 保底

### 算法
```
每 7 次 RMS 计算：
  noiseFloor = _noiseMax - _noiseMin
  rawTh = noiseFloor × 3.0（范围 0.1A ~ 2.0A）
  首次：newTh = rawTh
  后续：newTh = oldTh × 0.85 + rawTh × 0.15（EMA 平滑）
  
  重置窗口：±0.1A（保底）
```

### 日志示例
```
[SCT013] RMS=0.92A (changed=YES, interval=300s, th=0.10A/default)
[SCT013] Threshold=0.89A (adaptive, noiseFloor=0.30A)
[SCT013] Threshold=0.84A (adaptive, noiseFloor=0.20A)
[SCT013] RMS=1.20A (changed=YES, interval=300s, th=0.84A/adaptive)
```

## 与旧版对比

| 维度 | v1.0.0（旧） | v1.0.1（RMS） |
|------|------------|--------------|
| 采样方式 | 每1秒1个ADC点 | 每3秒200个ADC点 |
| 覆盖周波 | 0（完全随机） | 2个完整50Hz周波 |
| 电流算法 | 瞬时值×系数 | 真RMS均方根 |
| 去噪方式 | 10点滑动窗口均值 | 一次RMS计算 |
| 结果稳定性 | 跳跃大（0~1.4A随机） | 稳定（反映真实有效值） |
| 变化触发 | 频繁误报 | 只报真实变化 |
| 后端限流 | 经常429 | 几乎不会触发 |

## 限流兼容设计

固件收到 429 时：
- 强制 `_reportIntervalMs = 3000`（3秒最小间隔）
- 等待限流窗口过去再上报
- 日志：`Rate limited (th=0.84A), waiting...`

后端 CURRENT_METER 限流参数：5 秒（2026-05-11 从 3 秒放宽）

## 配置参数（云端可控）

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `sct013ChangeThreshold` | float | 自适应 | 变化阈值（A） |
| `sct013HeartbeatLowSec` | int | 1800 | 空载心跳（秒） |
| `sct013HeartbeatMedSec` | int | 300 | 轻载心跳（秒） |
| `sct013HeartbeatHighSec` | int | 60 | 重载心跳（秒） |
| `sct013LoadLowA` | float | 0.5 | 空载/轻载分界（A） |
| `sct013LoadHighA` | float | 5.0 | 轻载/重载分界（A） |
