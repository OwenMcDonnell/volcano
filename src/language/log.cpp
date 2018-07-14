/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
 */
#include "language.h"

void logV(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('V', fmt, ap);
  va_end(ap);
}
void logD(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('D', fmt, ap);
  va_end(ap);
}
void logI(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('I', fmt, ap);
  va_end(ap);
}
void logW(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('W', fmt, ap);
  va_end(ap);
}
void logE(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('E', fmt, ap);
  va_end(ap);
}
void logF(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  logVolcano('F', fmt, ap);
  va_end(ap);
}

#ifdef __ANDROID__
#include <android/log.h>
static const char logTag[] = "volcano";
void logVolcano(char level, const char* fmt, va_list ap) {
  android_LogPriority prio = ANDROID_LOG_UNKNOWN;
  switch (level) {
    case 'V':
      prio = ANDROID_LOG_VERBOSE;
      break;
    case 'D':
      prio = ANDROID_LOG_DEBUG;
      break;
    case 'I':
      prio = ANDROID_LOG_INFO;
      break;
    case 'W':
      prio = ANDROID_LOG_WARN;
      break;
    case 'E':
      prio = ANDROID_LOG_ERROR;
      break;
    case 'F':
      prio = ANDROID_LOG_FATAL;
      break;
    default:
      break;
  }
  __android_log_vprint(prio, logTag, fmt, ap);
  if (prio == ANDROID_LOG_FATAL) {
    __android_log_assert("call to logF()", logTag, "printing backtrace:");
  }
}

#elif defined(_WIN32) /* !defined(__ANDROID__) */
#include <time.h>
static FILE* errorLog = nullptr;
void logVolcano(char level, const char* fmt, va_list ap) {
  if (!errorLog) {
    errorLog = fopen("volcano.log", "a");
  }
  time_t raw;
  time(&raw);
  struct tm tm_results;
  struct tm* t = &tm_results;
#ifdef _WIN32
  if (localtime_s(&tm_results, &raw)) {
    t = 0;
  }
#else
  // Yes, this function is only for _WIN32 already. Here is the posix line FYI:
  t = localtime_r(&raw, &tm_result);
#endif
  char timestamp[256];
  if (t) {
    strftime(timestamp, sizeof(timestamp), "%Y.%m.%d %H:%M:%S", t);
  } else {
    snprintf(timestamp, sizeof(timestamp), "?%llu?", (unsigned long long)raw);
  }
  if (errorLog) {
    fprintf(errorLog, "%s %c ", timestamp, level);
    vfprintf(errorLog, fmt, ap);
    fflush(errorLog);
  }
  OutputDebugString(timestamp);
  snprintf(timestamp, sizeof(timestamp), " %c ", level);
  OutputDebugString(timestamp);
  vsnprintf(timestamp, sizeof(timestamp), fmt, ap);
  OutputDebugString(timestamp);

  if (level == 'F') {
    exit(1);
  }
}

#else /* !defined(_WIN32) && !defined(__ANDROID__) */

void logVolcano(char level, const char* fmt, va_list ap)
    __attribute__((format(printf, 2, 0)));
void logVolcano(char level, const char* fmt, va_list ap) {
  fprintf(stderr, "%c ", level);
  vfprintf(stderr, fmt, ap);
  if (level == 'F') {
    exit(1);
  }
}

#endif
