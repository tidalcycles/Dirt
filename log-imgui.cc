#include <pthread.h>
#include <stdarg.h>
#include <imgui.h>

#include "log-imgui.h"

ImGuiTextBuffer log_buffer;
pthread_mutex_t log_lock;

extern "C"
__attribute__((format(printf, 2, 3)))
void log_printf(LOG target, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  pthread_mutex_lock(&log_lock);
  log_buffer.appendfv(fmt, args);
  pthread_mutex_unlock(&log_lock);
  va_end(args);
}

void log_clear(void)
{
  pthread_mutex_lock(&log_lock);
  log_buffer.clear();
  pthread_mutex_unlock(&log_lock);
}

void log_display(void)
{
  pthread_mutex_lock(&log_lock);
  ImGui::TextUnformatted(log_buffer.begin(), log_buffer.end());
  pthread_mutex_unlock(&log_lock); 
}

void log_init(void)
{
  pthread_mutex_init(&log_lock, NULL);
}

void log_destroy(void)
{
  log_clear();
  pthread_mutex_destroy(&log_lock);
}
