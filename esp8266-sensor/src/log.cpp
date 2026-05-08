/**
 * P008 日志系统实现
 * - 分级打印：E/W/I/D + [tag] 前缀
 * - 可选循环缓冲区：LOG_RING_BUF_SIZE > 0 时启用
 * - 格式化对齐（左对齐 tag，右对齐消息）
 */

#include "log.h"

void _logPrint(char level, const char* tag, const char* fmt, ...) {
  if (!Serial) return;  // 串口未就绪

  char prefix[20];
  snprintf(prefix, sizeof(prefix), "[%c] [%-8s] ", level, tag);
  Serial.print(prefix);

  va_list args;
  va_start(args, fmt);
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  Serial.println(buf);
}

// --------------- 循环日志缓冲区实现 ---------------

#if LOG_RING_BUF_SIZE > 0

#include <cstring>

static char logRing[LOG_RING_BUF_SIZE];
static volatile size_t logRingHead = 0;
static volatile size_t logRingTail = 0;
static bool logRingWrapped = false;

void logRingInit() {
  logRingHead = 0;
  logRingTail = 0;
  logRingWrapped = false;
  memset(logRing, 0, sizeof(logRing));
}

#endif // LOG_RING_BUF_SIZE > 0
