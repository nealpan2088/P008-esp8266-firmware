# ESP8266 固件开发规范

## 一、环境命名规则

### 格式

```
fw-{传感器}-{功能}-{供电方式}
       │        │        │
       │        │        └─ plug（USB供电） / battery（deepSleep）
       │        └─ 可选：door / relay / current 等
       └─ dht22 / door / ds18b20 / sht30 等
```

### 现有变体

| 代号 | platformio.ini env 名 | 描述 |
|------|----------------------|------|
| DHT-插电 | `fw-dht22-plug` | DHT22 + USB 供电 |
| DHT门磁-插电 | `fw-dht22-door-plug` | DHT22 + 门磁 + USB 供电 |
| DHT-电池 | `fw-dht22-battery` | DHT22 + deepSleep |

**编译时只用 `-e <env名>` 指定，不用记引脚/宏。**

```bash
# 示例
pio run -e fw-dht22-plug -t upload           # 纯 DHT22 插电
pio run -e fw-dht22-door-plug -t upload       # DHT22+门磁插电
pio run -e fw-dht22-battery -t upload         # 电池版
```

---

## 二、`config.h` 常量定义规范

### 2.1 所有硬件参数走宏，不走硬编码

```c
// ✅ 正确
int pin = DHT_PIN;         // 引用 config.h 的宏
int pin = DOOR_PIN;

// ❌ 错误
int pin = 5;               // 硬编码引脚号
int pin = 14;
```

### 2.2 宏命名规则

| 前缀 | 含义 | 示例 |
|------|------|------|
| `USE_` | 功能开关（0/1） | `USE_DOOR_SENSOR`, `USE_DS18B20` |
| `SERIAL_` | 串口 | `SERIAL_BAUD` |
| `DHT_`, `DOOR_`, `CURRENT_` | 硬件引脚 | `DHT_PIN`, `DOOR_PIN` |
| `LED_BUILTIN` | 板载 LED 引脚 | `LED_BUILTIN` |
| `DEEP_SLEEP_` | deepSleep 相关 | `DEEP_SLEEP_US` |
| `LOG_` | 日志 | `LOG_LEVEL` |
| `WIFI_` | 网络 | `WIFI_SSID`, `WIFI_PASSWORD` |

### 2.3 新加宏时遵循 `#ifndef` 守卫模式

```c
// ✅ 正确：可被 build_flags 覆盖
#ifndef USE_DOOR_SENSOR
#define USE_DOOR_SENSOR 0
#endif

// ❌ 错误：不可被覆盖
#define USE_DOOR_SENSOR 0
```

---

## 三、`main.cpp` 代码组织规范

### 3.1 变体代码用 `#if` 宏隔离，不在运行时判断

```c
// ✅ 正确：编译时确定
#if USE_DOOR_SENSOR
  pinMode(DOOR_PIN, INPUT_PULLUP);
#endif

// ❌ 错误：运行时判断，两个分支都编译进固件
if (USE_DOOR_SENSOR) {
  pinMode(DOOR_PIN, INPUT_PULLUP);
}
```

### 3.2 电池版代码统一用 `#if BATTERY_MODE` 包裹，集中放在 `#else` 分支后

```c
#if BATTERY_MODE
  // setup 里一次跑完→上报→deepSleep
  ...
#else
  // setup + loop 模式，WiFi 常连
  ...
#endif
```

### 3.3 `reportData()` 中的 JSON body 构建也要用宏隔离

```c
#if USE_DOOR_SENSOR
  // 拼接 doorOpen 字段
#else
  // 不加 doorOpen 字段
#endif
```

---

## 四、文档记录规范

### 4.1 每个固件变体必须记录在 `firmware-variants.md`

```markdown
## fw-dht22-door-plug（DHT门磁-插电）

- 传感器: DHT22 + 门磁
- 供电: USB
- 引脚: DHT22→D1(GPIO5), 门磁→D5(GPIO14)
- 上报: 每 60 秒，含 doorOpen 字段
- 创建日期: 2026-05-02
```

### 4.2 `CHANGELOG.md` 记录每次版本变更

```markdown
## v3.3 (2026-05-02)
- 新增: 门磁传感器支持（`#if USE_DOOR_SENSOR`）
- 新增: `fw-dht22-door-plug` 固件变体
- 规范: platformio.ini env 命名规范化
```

### 4.3 接线图写在 `platformio.ini` 每个 env 的注释头里

```ini
; 接线:
;   DHT22 DATA → D1 (GPIO5)
;   门磁 DATA  → D5 (GPIO14)
;   门磁 GND   → 任意 G
```

---

## 五、版本号规则

| 类型 | 版本号 | 说明 |
|------|--------|------|
| 项目版本 | v0.6.x | 全栈项目版本 |
| 固件版本 | v3.x | 固件架构版本（v3=当前） |
| 变体版本 | 跟随固件版本 | 所有变体共享同一固件版本号 |

**固件版本号在 `main.cpp` 的 JSON body 中定义：**

```c
"firmwareVer":"3.1"  // 插电版
"firmwareVer":"3.2"  // 电池版
```

每次架构变更递增主版本号，小修小改递增子版本号。

---

## 六、序列号命名规范（设备接入后端的唯一身份）

### 6.1 格式

```
{设备类型}-{供电方式}-{芯片ID}
    │         │         │
    │         │         └─ 8位大写十六进制（ESP8266 chipId，固件自动生成）
    │         └─ PL=插电版（USB供电、WiFi常连）
    │            BT=电池版（deepSleep、省电）
    └─ DHT22 = 温湿度传感器
       DOOR  = 门磁传感器
       CURRENT = 电流互感器
       COLD  = 冷库/冷链传感器
```

### 6.2 示例

| 设备 | 序列号 | 含义 |
|------|--------|------|
| 插电版 DHT22 | `DHT22-PL-002B6350` | DHT22 + USB 供电 |
| 电池版 DHT22 | `DHT22-BT-00C6264D` | DHT22 + deepSleep |
| 插电版门磁 | `DOOR-PL-00A1B2C3` | 门磁 + USB 供电 |
| 电池版门磁 | `DOOR-BT-00D4E5F6` | 门磁 + deepSleep |

### 6.3 自动生成机制

固件 `config.h` 中通过宏定义前缀：

```c
#ifndef DEVICE_SERIAL_PREFIX
#define DEVICE_SERIAL_PREFIX "DHT22-PL-"    // 由 platformio.ini 的 build_flags 指定
#endif
```

`setup()` 中自动拼接：
```c
snprintf(_autoSerial, sizeof(_autoSerial), "%s%s", DEVICE_SERIAL_PREFIX, chipIdHex);
```

**新设备不需要手动配序列号，烧录即用。**

### 6.4 后端验证规则

```js
// constants.js
ALLOWED_SERIAL_PREFIXES: [
  'DHT22-', 'DOOR-', 'CURRENT-', 'COLD-', 'SENSOR-'
]
```

**注意**：以后端 `ALLOWED_SERIAL_PREFIXES` 数组为最终准入名单。新设备类型必须先加到这个数组，否则自动注册会失败。

### 6.5 现有设备兼容

当前两台设备（`DHT22-002B6350`、`DHT22-00C6264D`）没有供电方式标识（缺了 `-PL-` / `-BT-`），属于旧格式，继续使用不修改。新设备一律按新格式命名。

### ⚠️ 红线

**新设备上线流程：**
1. 确认 `platformio.ini` 的 `build_flags` 中 `DEVICE_SERIAL_PREFIX` 已配好
2. 确认后端 `constants.js` 的 `ALLOWED_SERIAL_PREFIXES` 已包含设备类型前缀
3. 烧录前先在 `firmware-variants.md` 记录新变体的接线和序列号格式
4. 烧录后验证后端数据库 `devices` 表自动注册成功

---

## 七、固件自适应后端场景规范

### 7.1 核心机制

固件的 **序列号前缀** 是设备与后端之间的"握手协议"。后端自动注册时通过序列号前缀推断：

```
固件命名 (DEVICE_SERIAL_PREFIX)
  → 序列号前缀 (serial.startsWith)
    → 后端 SERIAL_TO_SCENE 映射表
      → 场景类型 (sceneType)
        → reportInterval (上报间隔)
          → alertMode (报警模式)
            → thresholds (阈值)
```

**不需要人工配置，烧录即用。**

### 7.2 三层匹配策略

后端 `constants.js` 的 `SERIAL_TO_SCENE` 映射表按**精确到通用**排序：

```js
SERIAL_TO_SCENE: [
  // 第一层：精确匹配 → 细分场景（功能专一的专用固件）
  // 例：纯门磁插电 → 门禁场景（60s 上报，实时报警）
  { prefix: 'DOOR-PL-',   scene: 'DOOR' },

  // 第二层：中间态 → 通用场景（功能不够专一）
  // 例：DHT22+门磁 → 通用场景（60s 上报，用户手工配阈值）
  { prefix: 'DHT22-PL-',  scene: 'GENERAL' },

  // 第三层：兜底 → 通用场景（哪都能装的通用传感器）
  // 例：纯 DHT22 → 通用场景（60s 上报，用户手工配阈值）
  { prefix: 'DHT22-',     scene: 'GENERAL' },
]
```

**匹配规则：** 从前往后遍历，匹配到第一个前缀就停止。所以精确前缀必须放前面。

### 7.3 各场景对应的固件前缀

| 场景类型 | 场景预设 | 默认上报间隔 | 适配的固件前缀 | 匹配层级 |
|----------|---------|:----------:|---------------|:--------:|
| 通用监测 | **GENERAL** | **60s** | `DHT22-PL-`, `DHT22-`, `SENSOR-` | 第二三层 |
| 冷库/冷柜 | COLD_STORAGE | 120s | `COLD-`, `COLD-PL-`, `COLD-BT-` | 第一层 |
| 门禁监控 | DOOR | 60s | `DOOR-PL-` | 第一层 |
| 仓库 | WAREHOUSE | 300s | `DHT22-BT-` | 第一层 |
| 厨房 | KITCHEN | 180s | `KITCHEN-` | 第一层 |
| 机房 | SERVER_ROOM | 60s | `SERVER-` | 第一层 |
| 用电监测 | POWER_MONITOR | 300s | `CURRENT-`, `POWER-` | 第一层 |

**注意：** `DHT22-PL-` 和 `DHT22-` 这些通用/中间态前缀走的是 `GENERAL` 通用场景，不会跳到 DOOR 或 WAREHOUSE。
用户注册后可以在后台手动切换到更合适的场景。

### 7.4 添加新固件变体时的标准流程

```mermaid
flowchart LR
  A[确定场景需求] --> B[选序列号前缀]
  B --> C[固件 build_flags 加\nDEVICE_SERIAL_PREFIX]
  C --> D[后端 ALLOWED_SERIAL_PREFIXES\n加前缀准入]
  D --> E[后端 SERIAL_TO_SCENE\n加映射行]
  E --> F[烧录即用\n自动注册+自动配置]
```

**三步规范：**

1. **定前缀** — 必须是 `{类型}-{供电}-` 格式
2. **通后端** — `ALLOWED_SERIAL_PREFIXES` + `SERIAL_TO_SCENE` 各加一行
3. **落地文档** — `firmware-variants.md` 记录新变体

### 7.5 现有设备兼容

| 前缀格式 | 是否已注册 | 后续新设备 |
|---------|:---------:|:---------:|
| `DHT22-`（旧格式，缺供电标识） | ✅ 已注册的两台 | ❌ 不再生成 |
| `DHT22-PL-`（新格式） | ✅ DHT22-PL-00FE7390 | ✅ GENERAL 场景 60s |
| `DHT22-BT-`（新格式） | ❌ 尚无 | ✅ GENERAL 场景 300s |

旧格式设备不受影响，保持现有配置不变。

---

## 八、自动化约束（防呆机制）

> 规范不靠人记，靠工具卡死。以下约束在编译/启动/提交时自动触发。

### 8.1 固件编译检查（config.h）

`config.h` 底部加入守卫，**忘记设 `DEVICE_SERIAL_PREFIX` 直接编译失败**：

```c
// ========== 自动化约束（请勿删除） ==========
#ifndef DEVICE_SERIAL_PREFIX
  #error "❌ DEVICE_SERIAL_PREFIX 未定义！新设备必须设置正确的序列号前缀，例：-D DEVICE_SERIAL_PREFIX=\\"DHT22-PL-\\""
#elif (defined(DEVICE_SERIAL_PREFIX) && DEVICE_SERIAL_PREFIX[0] == '\0')
  #pragma message "⚠️ DEVICE_SERIAL_PREFIX 为空，旧设备兼容模式"
#endif
```

**效果**：
- 忘记加 `build_flags = -D DEVICE_SERIAL_PREFIX="xxx"` → ❌ 编译失败
- 加了但值为空 → ⚠️ 编译警告（旧设备兼容模式）
- 值正确（如 `"DHT22-PL-"`）→ ✅ 编译通过

### 8.2 后端启动自检（server.js / app.js）

后端启动时自动检查 `SERIAL_TO_SCENE` 映射表与 `ALLOWED_SERIAL_PREFIXES` 的一致性：

```js
// 放在 Fastify 启动前的初始化阶段
function checkSerialMappings() {
  const prefixes = BUSINESS.ALLOWED_SERIAL_PREFIXES;
  const mappings = BUSINESS.SERIAL_TO_SCENE.map(m => m.prefix);
  
  for (const m of mappings) {
    if (!prefixes.some(p => m.startsWith(p))) {
      console.warn(`⚠️ [AutoCheck] SERIAL_TO_SCENE 前缀 "${m}" 不在 ALLOWED_SERIAL_PREFIXES 中`);
      console.warn(`   → 设备自动注册会被拒绝，请将 "${m.split('-')[0] + '-'}" 加入 ALLOWED_SERIAL_PREFIXES`);
    }
  }
  
  // 检查每个场景预设是否存在
  for (const m of BUSINESS.SERIAL_TO_SCENE) {
    const scene = m.scene;
    if (scene !== 'GENERAL' && !BUSINESS.SCENE_PRESETS[scene]) {
      console.error(`❌ [AutoCheck] SERIAL_TO_SCENE 引用了不存在的场景 "${scene}"`);
      console.error(`   → 请先在 SCENE_PRESETS 中定义场景 "${scene}"`);
    }
  }
  
  console.log(`✅ [AutoCheck] SERIAL_TO_SCENE 检查通过 (${mappings.length} 条映射)`);
}
```

### 8.3 Git pre-commit hook

在 `.githooks/pre-commit` 放置以下脚本，`git commit` 时自动提示固件+后端一致性：

```bash
#!/bin/bash
# .githooks/pre-commit — 固件规范一致性检查
# 安装：git config core.hooksPath .githooks

CHANGED=$(git diff --cached --name-only)

# 检查固件配置变更
if echo "$CHANGED" | grep -q "platformio.ini"; then
  echo "🔔 [PreCommit] platformio.ini 已变更"
  # 提取新增的 env 名
  NEW_ENVS=$(git diff --cached --unified=0 platformio.ini | grep "^\+\[env:" | sed 's/^\+\[env:/  /' | sed 's/\]//')
  if [ -n "$NEW_ENVS" ]; then
    echo "  新 env:$NEW_ENVS"
    echo "  ⚠️  请确认："
    echo "    1. build_flags 包含 DEVICE_SERIAL_PREFIX"
    echo "    2. 后端 ALLOWED_SERIAL_PREFIXES 已添加"
    echo "    3. 后端 SERIAL_TO_SCENE 已添加映射"
    echo "    4. firmware-variants.md 已记录新变体"
  fi
fi

# 检查后端 constants.js 变更
if echo "$CHANGED" | grep -q "constants.js"; then
  echo "🔔 [PreCommit] constants.js 已变更"
  echo "  ⚠️  确认：SERIAL_TO_SCENE / ALLOWED_SERIAL_PREFIXES 是否同步更新？"
fi
```

### 8.4 变更检查清单（人工兜底）

当自动化工具未覆盖时（如误跳过 hook），以下清单可作为兜底。**新设备上线前手动过一遍**：

| # | 检查项 | 谁负责 |
|:-:|--------|:------:|
| 1 | `platformio.ini` 的 `build_flags` 中有 `DEVICE_SERIAL_PREFIX` | 固件开发者 |
| 2 | 后端 `ALLOWED_SERIAL_PREFIXES` 包含设备类型前缀 | 后端开发者 |
| 3 | 后端 `SERIAL_TO_SCENE` 已添加映射（精确前缀放前） | 后端开发者 |
| 4 | `firmware-variants.md` 已记录接线图和数据格式 | 文档 |
| 5 | 烧录后验证 `devices` 表 auto-register 成功 | 测试 |
| 6 | 验证 `sceneType` 和 `reportInterval` 正确匹配 | 测试 |

### 8.5 核心设计原则（云端优先）

> 8266 是傻执行者，后端是大脑。8266 只做一件事：听服务器的话。

#### 原则 1：不要硬编码任何运行时常量

❌ **错误做法**：
```cpp
// loop() 上报成功后硬设回 60 秒，覆盖了云端配置
_reportIntervalMs = 60000;
```

✅ **正确做法**：
```cpp
// 初值从云端取，或设一个合理的兜底值
_reportIntervalMs = 300000;  // 300 秒兜底

// 每次上报的响应中解析真正的配置
void fetchConfig() {
  // GET /devices/{serial}/config → 拿到 reportInterval
  // 更新 _reportIntervalMs
}
```

**规则**：任何 `_xxx = 常量` 赋值都必须有理由说明"为什么这个值不需要云端覆盖"。默认常量只允许两种：
- 硬件限制值（如引脚号、波特率、看门狗超时）
- WiFi/HTTP 超时值（网络层参数，跟业务无关）

#### 原则 2：云端配置优先

固件的所有运行参数必须按以下优先级（云端 > 硬件兜底）：

```
网页/后端改配置 → 数据库更新
  → 设备下次上报响应中返回 config.reportInterval
  → 或设备调用 GET /devices/{serial}/config 拉取
    → 8266 无条件遵守，不自作聪明
```

❌ **错误设计**：
```cpp
if (httpCode == 200) {
  _reportIntervalMs = 60000;  // "恢复默认" —— 但默认=60s 是硬编码的
  fetchConfig();              // 后面再拉云端，但被上面那行覆盖了
}
```

✅ **正确设计**：
```cpp
// setup() 时：从云端取
// 如果失败，给一个合理兜底值（300s 而不是 60s）
if (fetchConfig() != 200) {
  _reportIntervalMs = 300000;  // 兜底 5 分钟
}

// loop() 上报后：拉云端配置
if (httpCode == 200) {
  fetchConfig();  // 直接更新 _reportIntervalMs，不覆盖
}
```

#### 原则 3：远程指令（pendingCommands）同理

```cpp
// ❌ 固件自己决定不做
if (command.type == "SET_INTERVAL") {
  // 忽略，我觉得云端说的不对
}

// ✅ 无条件执行
if (command.type == "SET_INTERVAL") {
  _reportIntervalMs = command.interval * 1000;
  LOG_I("Command", "Interval updated to %ds by remote", command.interval);
}
```

#### 原则 4：跑偏就报错，不要静默修正

如果发现硬编码值被云端配置覆盖了，**打日志输出新旧值**，方便烧录后串口排查：

```cpp
// ✅ 每次云端配置更新时输出对比
LOG_I("Config", "reportInterval=%lds (from cloud, was %lds)",
      intervalSec, _reportIntervalMs / 1000);
```

#### 原则 5：新固件（Air780E 等）也必须遵守

所有未来的设备固件变体必须：
1. 启动时先拉云端配置
2. 每次上报后从响应中解析新配置
3. 不设任何业务相关的硬编码默认值
4. 参数变更必须输出日志

#### 原则 6：发布渠道追踪（FIRMWARE_CHANNEL）

所有固件变体必须在 `config.h` 中定义 `FIRMWARE_CHANNEL` 宏：

| 值 | 用途 | 说明 |
|----|------|------|
| `"self"` | 开发调试 | 默认值，自己编译烧录用 |
| `"official"` | 正式发布 | 发布 bin 文件前改为此值 |

**上报数据**中 `otherData.channel` 字段自动携带此值，后端可据此统计：
- 官方固件注册设备数 vs 自编译固件注册设备数
- 追踪固件分发渠道效果

**发布流程：**
```
1. config.h 中设 FIRMWARE_CHANNEL = "official"
2. 编译所有变体 → 收集 .bin 文件
3. 打包发布到 GitHub Release
4. 改回 FIRMWARE_CHANNEL = "self"（继续开发）
```

**在 platformio.ini 中覆盖（调试时）：**
```ini
build_flags =
    ${env:fw-dht22-plug.build_flags}
    -D FIRMWARE_CHANNEL=\"self\"
```


## 九、约束层级总结

```
编译失败 (#error)   ← 最硬：直接不让过
  ↓
编译警告 (#pragma)  ← 硬：可跳过但会被看到
  ↓
启动警告 (console.warn)  ← 中：每次重启都会提醒
  ↓
提交提示 (pre-commit)    ← 软：可 --no-verify 绕过
  ↓
文档检查清单 (手动)      ← 最软：兜底

原则：越容易被遗忘的检查，放在越靠前的层级
      编译时失败 < 启动时警告 < 提交时提示 < 手动检查
