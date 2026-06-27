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
// Config::loadFromFile stub
// (The real one lives in M4's config.cpp. Here we fake success and fill
//  reasonable defaults so we don't need config.txt to be present.)
// ---------------------------------------------------------------------------
bool Config::loadFromFile(const std::string& /*fileName*/) {
    // Try to read the real file first; fall back to hardcoded defaults.
    // For the harness, just set defaults and mark initialized.
    numCPU           = 4;
    scheduler        = "rr";
    quantumCycles    = 5;
    batchProcessFreq = 3;
    minIns           = 10;
    maxIns           = 50;
    delayPerExec     = 0;
    initialized      = true;
    std::cout << "[Config stub] Loaded hardcoded defaults (no config.txt needed).\n";
    return true;
}

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
