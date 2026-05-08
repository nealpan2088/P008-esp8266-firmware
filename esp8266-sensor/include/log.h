/**
 * P008 日志系统 — 轻量级日志分级 + 可选循环缓冲区
 *
 * 用法：
 *   LOG_I("WiFi", "Connected, IP: %s", ip);   // INFO
 *   LOG_W("WiFi", "Retry #%d failed", n);       // WARN
 *   LOG_E("HTTP", "POST failed: code=%d", c);   // ERROR
 *   LOG_D("DHT22", "Raw value=%d", raw);        // DEBUG
 *
 * 日志级别（config.h LOG_LEVEL）：
 *   0=静默  1=ERROR  2=WARN  3=INFO  4=DEBUG
 *
 * RAM 占用：
 *   LOG_RING_BUF_SIZE=0（量产）：约 0 字节额外 RAM
 *   LOG_RING_BUF_SIZE=512（调试）：约 512 字节循环缓冲区
 */

#ifndef P008_LOG_H
#define P008_LOG_H

#include <Arduino.h>
#include "config.h"

// --------------- 日志级别枚举 ---------------
#define LVL_NONE  0
#define LVL_ERROR 1
#define LVL_WARN  2
#define LVL_INFO  3
#define LVL_DEBUG 4

// --------------- 日志宏 ---------------
// 只在级别 >= LOG_LEVEL 时输出，编译时保留字符串（但运行时跳过）
// 这样即使量产开 INFO，调试时改到 DEBUG 重新烧录即可看全量日志

#if LOG_LEVEL >= LVL_ERROR
#define LOG_E(tag, fmt, ...)  _logPrint('E', tag, fmt, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...)  do {} while(0)
#endif

#if LOG_LEVEL >= LVL_WARN
#define LOG_W(tag, fmt, ...)  _logPrint('W', tag, fmt, ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...)  do {} while(0)
#endif

#if LOG_LEVEL >= LVL_INFO
#define LOG_I(tag, fmt, ...)  _logPrint('I', tag, fmt, ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...)  do {} while(0)
#endif

#if LOG_LEVEL >= LVL_DEBUG
#define LOG_D(tag, fmt, ...)  _logPrint('D', tag, fmt, ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...)  do {} while(0)
#endif

// --------------- 兼容旧 Serial.printf 用法 ---------------
// 保留一条迁移路径，旧代码直接改 LOG_X 即可
// 向后兼容宏：打印全部日志（无条件），建议新代码用 LOG_I
#define LOG_RAW(fmt, ...)     Serial.printf(fmt, ##__VA_ARGS__)

// --------------- 循环日志缓冲区（可选） ---------------
// 量产时 LOG_RING_BUF_SIZE=0 不占 RAM
// 调试时设 512~1024，可通过 logDump() 一次性输出

#if LOG_RING_BUF_SIZE > 0

// 初始化环形缓冲区（需在 setup() 中调用一次）
void logRingInit();

// 转储环形缓冲区内容到串口
void logDump();

#else

// 禁用时为空实现，不占代码空间
#define logRingInit() do {} while(0)
#define logDump()     do {} while(0)

#endif // LOG_RING_BUF_SIZE > 0

// --------------- 内部函数（不直接调用） ---------------
void _logPrint(char level, const char* tag, const char* fmt, ...);

#endif // P008_LOG_H
