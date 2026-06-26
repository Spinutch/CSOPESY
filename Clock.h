/* #pragma once
#include "imgui/imgui.h"

class Clock {
public:
    Clock() = default;
    ~Clock() = default;

    // Evaluates time and draws the overlay at the top-right
    void Render(const ImVec2& screen_max_p, float margin = 20.0f);
}; */

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