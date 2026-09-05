#pragma once
#include <cstdint>
#ifdef RECOMP_SCHEDULER_DIAGNOSTICS
void debug_queue_event(char operation, int32_t queue, int32_t thread, int32_t next);
#else
inline void debug_queue_event(char, int32_t, int32_t, int32_t) {}
#endif
