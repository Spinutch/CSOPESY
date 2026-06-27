// ============================================================================
// main_1.cpp  –  Member 1: Standalone Test Harness
// ============================================================================
// Compiles with:
//   g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp -o mo1_test
//
// Exercises:
//   - ASCII banner
//   - 'initialize' gating
//   - 'screen -ls'  (stub snapshot)
//   - 'screen -s'   (stub process + attached loop)
//   - 'report-util' (writes csopesy-log.txt)
//   - 'scheduler-start' / 'scheduler-stop'
// ============================================================================

#include "mo1.h"      // Config, Process, ProcessState, SchedulerSnapshot, etc.
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

// ---------------------------------------------------------------------------
// g_cpuTick – M4 owns the real definition in clock.cpp.
// The harness defines it locally so main_1.cpp links standalone.
// ---------------------------------------------------------------------------
std::atomic<uint64_t> g_cpuTick{0};


// ---------------------------------------------------------------------------
// StubScheduler  –  feeds canned data to M1's rendering code
// ---------------------------------------------------------------------------
#include "scheduler.h"



// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    Config         cfg;
    Scheduler  sched;

    try {
        runConsole(sched, cfg);
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "System exited gracefully.\n";
    return 0;
}
