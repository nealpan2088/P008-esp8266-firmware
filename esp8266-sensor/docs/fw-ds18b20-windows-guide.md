# DS18B20 新 8266 烧录操作指南（Windows 版）

> 参考设计文档：`docs/fw-ds18b20-plug-design.md`
> 时间：2026-05-03

---

## 一、接线（桌面上操作）

新 NodeMCU 到手后，接三根线：

| DS18B20 模块 | → | NodeMCU |
|-------------|---|---------|
| 右（VCC） | → | 3V3 |
| 中（DATA） | → | **D3 (GPIO0)** |
| 左（GND） | → | GND |

> ⚠️ DATA 与 VCC 之间需要 4.7kΩ 上拉电阻，模块通常自带，如果不确定拍照发我看看

**烧录时 USB 线插电脑即可，不用额外供电。**

---

## 二、编译烧录

在 P008-env-monitor 项目目录下打开命令行（VS Code 终端或 CMD/PowerShell 进到 hardware/esp8266-sensor），执行：

```bash
pio run -e fw-ds18b20-plug -t upload
```

如果电脑上插了多个串口设备，需要指定端口（看设备管理器里是 COM几）：

```bash
pio run -e fw-ds18b20-plug -t upload --upload-port COM3
```

---

## 三、首次配网

烧录成功后打开串口监视器：

```bash
pio device monitor -b 115200
```

1. 8266 会创建一个 WiFi 热点 `P008-Env-Monitor`
2. 手机或电脑连上这个热点
3. 浏览器打开 `192.168.4.1`
4. 选你家的 WiFi 输密码
5. 配网成功后串口会打印 `chipId` 和自动生成的序列号

**把 chipId 发给我**，我在服务器端配好。

---

## 四、验证

配网后等 2 分钟，设备会上报数据，你可以在串口看到上报日志。我在后端查数据确认一切正常。
