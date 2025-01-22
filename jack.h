#pragma once

#include <stdbool.h>

#include <jack/jack.h>

#define sampletime_t jack_time_t

sampletime_t jack_start_time(double when, double epochOffset);

void jack_init(bool autoconnect);
