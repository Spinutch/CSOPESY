// ============================================================================
// main_1.cpp  –  Member 1: Integration Test Harness
// ============================================================================
// NOTE (updated for MO2 demand-paging design): this used to build against a
// StubScheduler so M1 could test console.cpp/system_ui.cpp/util.cpp without
// needing M2/M3's files. That's no longer possible: MemoryStats.h's
// buildMemoryStats() takes a concrete Scheduler&, not a template parameter,
// so console.cpp's process-smi/vmstat handlers can only ever be instantiated
// against the real Scheduler. Standalone testing is retired in favor of a
// real integration build, once the following exist:
//   scheduler.cpp, process.cpp, interpreter.cpp, Config.cpp, Clock.cpp,
//   MemoryManager.cpp, BackingStore.cpp, MemoryUtils.cpp, InstructionParser.cpp
//
// Build:
//   g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp \
//       scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp \
//       MemoryManager.cpp BackingStore.cpp MemoryUtils.cpp InstructionParser.cpp \
//       -o mo1_test
//
// This harness exercises the same commands as production main.cpp, plus one
// automated pre-check: that the console stays responsive (no long stalls,
// no stdout corruption) once BackingStore's write-through file rewrite
// (dumpToFileLocked(), triggered by storePage()/loadPage() on every
// eviction/fault) starts happening concurrently with normal CLI use. Unlike
// the old stub-based test, this exercises the REAL write-through path, not
// a simulation of it — so it's a stronger guarantee once it can actually run.
//
// Run it with `scheduler-start` at a small quantum-cycles / small
// max-overall-mem in config.txt to force frequent context switches (and
// therefore frequent BackingStore writes) during the observation window.
// ============================================================================

#include "mo1.h"
#include "scheduler.h"
#include "BackingStore.h"
#include <iostream>
#include <string>

std::atomic<uint64_t> g_cpuTick{0};

int main()
{
    Config cfg;
    Scheduler sched;

    // MO2: same as production main.cpp — BackingStore must exist up front.
    BackingStore backingStore;

    try
    {
        runConsole(sched, cfg, backingStore);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "System exited gracefully.\n";
    return 0;
}