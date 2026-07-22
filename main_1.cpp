// ============================================================================
// main_1.cpp  –  Member 1: Standalone Test Harness
// ============================================================================
// Two ways to build this file:
//
//   A) TRUE STANDALONE (no scheduler.cpp / MemoryManager.cpp needed at all):
//        g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp \
//            -DMO1_STANDALONE_TEST -o mo1_test
//      Uses StubScheduler (stub_scheduler.h) so console.cpp / system_ui.cpp
//      link without M2's or M3's files existing yet. Runs the automated
//      concurrency test, then (unless --auto-only is passed) drops into the
//      same interactive demo as production main.cpp.
//
//   B) AGAINST THE REAL SCHEDULER (once scheduler.cpp exists):
//        g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp \
//            scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp \
//            -o mo1_test
//      Uses the real Scheduler; skips the automated test (it's
//      StubScheduler-specific) and goes straight to the interactive demo.
//
// Exercises:
//   - ASCII banner
//   - 'initialize' gating
//   - 'screen -ls'          (renderSystemStatus)
//   - 'screen -s' / 'screen -r' (attached loop + renderProcessSmi)
//   - 'report-util'         (writes csopesy-log.txt)
//   - 'scheduler-start' / 'scheduler-stop'
//   - [MO1_STANDALONE_TEST only] console responsiveness while a background
//     thread hammers disk with memory_stamp_*.txt writes (5ms cadence —
//     far faster than the real system's once-per-quantum cadence), which is
//     the specific regression M1 is on the hook to guard against once the
//     memory manager lands.
// ============================================================================

#include "mo1.h" // Config, Process, ProcessState, SchedulerSnapshot, etc.
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>

// ---------------------------------------------------------------------------
// g_cpuTick – M4 owns the real definition in Clock.cpp.
// The harness defines it locally so main_1.cpp can link standalone even
// against the real scheduler.cpp (build option B above).
// ---------------------------------------------------------------------------
std::atomic<uint64_t> g_cpuTick{0};

#ifdef MO1_STANDALONE_TEST
#include "stub_scheduler.h"
#include <filesystem>
using SchedT = StubScheduler;
#else
#include "scheduler.h"
using SchedT = Scheduler;
#endif

#ifdef MO1_STANDALONE_TEST
// ---------------------------------------------------------------------------
// Automated test: console must stay responsive while a background thread
// writes memory_stamp_*.txt files.
//
// What "responsive" means here, concretely:
//   1. Calls into M1's own rendering code (renderSystemStatus, formatTimestamp,
//      renderProcessSmi) must not block for anywhere close to human-perceptible
//      time (we assert < 50ms; the background writer alone never sleeps more
//      than 5ms so 50ms is a generous margin) while the writer thread is
//      actively hammering disk I/O concurrently.
//   2. The writer thread must genuinely be producing output concurrently
//      (not just idle) — verified by checking new memory_stamp files
//      actually appeared on disk during the measurement window.
//   3. No exception / crash from data races (run under a race-detector
//      build, e.g. `g++ -fsanitize=thread`, for a stronger guarantee than
//      this test alone can offer).
// ---------------------------------------------------------------------------
static bool test_console_responsive_during_bg_writes()
{
    std::cout << "\n=== M1 TEST: console responsiveness under background memory-stamp I/O ===\n";

    namespace fs = std::filesystem;
    fs::path originalCwd = fs::current_path();
    fs::path testDir = fs::temp_directory_path() / "mo1_stamp_test";
    fs::create_directories(testDir);
    fs::current_path(testDir);

    // Clean slate
    for (auto &entry : fs::directory_iterator(testDir))
    {
        if (entry.path().filename().string().rfind("memory_stamp_", 0) == 0)
        {
            fs::remove(entry.path());
        }
    }

    Config cfg;
    cfg.numCPU = 2;
    cfg.scheduler = "rr";
    cfg.quantumCycles = 4;
    cfg.minIns = 5;
    cfg.maxIns = 10;

    StubScheduler sched;
    sched.start(cfg); // spins up the memory-stamp writer thread
    sched.createNamedProcess("alpha", cfg);
    sched.createNamedProcess("beta", cfg);

    std::ostringstream captured; // where renderSystemStatus writes in this test
    double maxLatencyMs = 0.0;
    double totalLatencyMs = 0.0;
    int calls = 0;

    auto testDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < testDeadline)
    {
        auto t0 = std::chrono::steady_clock::now();

        renderSystemStatus(sched, captured);
        std::string ts = formatTimestamp();
        Process *p = sched.findProcess("alpha");
        if (p)
            renderProcessSmi(*p, captured);

        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        maxLatencyMs = std::max(maxLatencyMs, ms);
        totalLatencyMs += ms;
        ++calls;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int stampFileCount = 0;
    for (auto &entry : fs::directory_iterator(testDir))
    {
        if (entry.path().filename().string().rfind("memory_stamp_", 0) == 0)
        {
            ++stampFileCount;
        }
    }

    sched.shutdown();

    double avgLatencyMs = calls > 0 ? (totalLatencyMs / calls) : 0.0;
    std::cout << "  Render calls made       : " << calls << "\n";
    std::cout << "  Max single-call latency : " << maxLatencyMs << " ms\n";
    std::cout << "  Avg single-call latency : " << avgLatencyMs << " ms\n";
    std::cout << "  memory_stamp_*.txt seen : " << stampFileCount << "\n";

    bool ok = true;
    if (calls < 50)
    {
        std::cout << "  FAIL: main thread starved — expected ~200 render calls in 2s, got "
                  << calls << "\n";
        ok = false;
    }
    if (maxLatencyMs >= 50.0)
    {
        std::cout << "  FAIL: a render call blocked for " << maxLatencyMs
                  << "ms — console would visibly stutter.\n";
        ok = false;
    }
    if (stampFileCount == 0)
    {
        std::cout << "  FAIL: background writer produced no files — test wasn't exercising "
                     "concurrent I/O at all.\n";
        ok = false;
    }

    // Clean up test artifacts
    for (auto &entry : fs::directory_iterator(testDir))
    {
        if (entry.path().filename().string().rfind("memory_stamp_", 0) == 0)
        {
            fs::remove(entry.path());
        }
    }

    std::cout << (ok ? "  PASSED\n" : "  FAILED\n");

    fs::current_path(originalCwd); // don't leak cwd change into the demo that follows
    return ok;
}
#endif // MO1_STANDALONE_TEST

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    bool autoOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--auto-only")
            autoOnly = true;
    }

#ifdef MO1_STANDALONE_TEST
    bool allPassed = test_console_responsive_during_bg_writes();
    if (!allPassed)
    {
        std::cerr << "\n[main_1] One or more automated tests FAILED.\n";
        return 1;
    }
    std::cout << "\n[main_1] All automated tests passed.\n";
    if (autoOnly)
        return 0;
    std::cout << "[main_1] Dropping into interactive demo (Ctrl-D / 'exit' to quit)...\n\n";
#else
    (void)autoOnly;
#endif

    Config cfg;
    SchedT sched;

    try
    {
        runConsole(sched, cfg);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "System exited gracefully.\n";
    return 0;
}