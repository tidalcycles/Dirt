#pragma once

extern "C"
{
#include "log.h"
};

void log_clear(void);
void log_display(void);
void log_init(void);
void log_destroy(void);
