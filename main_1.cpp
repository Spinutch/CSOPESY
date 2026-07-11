// ============================================================================
// main_1.cpp  –  Member 1: Standalone Test Harness
// ============================================================================
// Compiles with:
//   g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp \
//       scheduler.cpp process.cpp interpreter.cpp MemoryManager.cpp \
//       Config.cpp Clock.cpp -o mo1_test

#include "mo1.h" // Config, Process, ProcessState, SchedulerSnapshot, etc.
#include "scheduler.h"
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// g_cpuTick – M4 owns the real definition in Clock.cpp.
// The harness defines it locally so main_1.cpp links standalone if Clock.cpp
// isn't linked in; if Clock.cpp IS linked, remove this line to avoid a
// duplicate-definition error.
// ---------------------------------------------------------------------------
std::atomic<uint64_t> g_cpuTick{0};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    Config cfg;
    Scheduler sched;

    try
    {
        runConsole(sched, cfg);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    sched.shutdown();
    std::cout << "System exited gracefully.\n";
    return 0;
}