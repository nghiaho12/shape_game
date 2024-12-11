#pragma once

#ifdef __ANDROID__
#include <android/log.h>
#include <stdarg.h>
void debug(const char *fmt, ...) {
   va_list args;
   va_start(args, fmt);
   __android_log_vprint(ANDROID_LOG_INFO, "SDL", fmt, args);
   va_end(args);
}
#else
#define debug(...) SDL_Log(__VA_ARGS__)
#endif

