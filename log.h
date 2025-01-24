#pragma once

typedef enum { LOG_OUT, LOG_ERR } LOG;

extern void log_printf(LOG target, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
