// Afterglow engine — logging through raylib's TraceLog (one sink for engine + raylib messages).
#pragma once
#include "raylib.h"

#define AG_INFO(...) TraceLog(LOG_INFO, __VA_ARGS__)
#define AG_WARN(...) TraceLog(LOG_WARNING, __VA_ARGS__)
#define AG_ERROR(...) TraceLog(LOG_ERROR, __VA_ARGS__)
