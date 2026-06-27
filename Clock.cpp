#include "Clock.h"

// Initialize the global atomic clock counter to 0 at application startup
std::atomic<uint64_t> g_cpuTick{0};