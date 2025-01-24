#include <stdarg.h>
#include <stdio.h>

#include "log.h"

__attribute__((format(printf, 2, 3)))
void log_printf(LOG target, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(target == LOG_OUT ? stdout : stderr, fmt, args);
  va_end(args);
}
