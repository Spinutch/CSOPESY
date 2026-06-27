#ifndef CLOCK_H
#define CLOCK_H

#include <atomic>
#include <cstdint>

// ============================================================================
// SPECIFICATION: The Global Heartbeat Clock
// ============================================================================
// This is the variable Member 2 (Scheduler) will declare as 'extern' 
// and increment (g_cpuTick++) once per execution loop cycle.
extern std::atomic<uint64_t> g_cpuTick;

#endif // CLOCK_H