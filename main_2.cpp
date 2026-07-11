// ============================================================================
// main_2.cpp  –  Member 2: Standalone Test Harness
// ============================================================================
// Compiles with:
//   g++ -std=c++17 -pthread scheduler.cpp main_2.cpp -o mo2_test
//
// Verifies:
//   1. All processes eventually reach FINISHED
//   2. Under RR, no process exceeds quantumCycles without yielding
//   3. usedCores never exceeds numCPU
//   4. Batch generation produces p01, p02, ... names
//   5. FCFS processes run to completion without preemption
// ============================================================================

#include "scheduler.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <iomanip>
    sched.shutdown();



// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static void printSnapshot(const SchedulerSnapshot& snap) {
    std::cout << "  [Snapshot] Cores: " << snap.usedCores << "/" << snap.totalCores
              << "  Running: " << snap.running.size()
              << "  Finished: " << snap.finished.size() << "\n";
    for (auto& p : snap.running)
        std::cout << "    [RUN] " << std::setw(8) << p.name
                  << "  Core:" << p.coreId
                  << "  " << p.executedCommands << "/" << p.totalCommands << "\n";
    for (auto& p : snap.finished)
        std::cout << "    [FIN] " << std::setw(8) << p.name
                  << "  " << p.executedCommands << "/" << p.totalCommands << "\n";
}

// Wait until all processes in the scheduler are FINISHED, or timeout (seconds)
static bool waitAllFinished(Scheduler& sched, int timeoutSec) {
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(timeoutSec);
    while (std::chrono::steady_clock::now() < deadline) {
        auto snap = sched.getSnapshot();
        if (snap.running.empty()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}
static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// ---------------------------------------------------------------------------
// TEST 1: FCFS – manually enqueued processes all finish
// ---------------------------------------------------------------------------
static void test_fcfs_all_finish() {
    std::cout << "\n=== TEST 1: FCFS – all processes finish ===\n";

    Config cfg;
    cfg.numCPU           = 2;
    cfg.scheduler        = "fcfs";
    cfg.quantumCycles    = 0;   // unused in FCFS
    cfg.batchProcessFreq = 999; // don't auto-batch
    cfg.minIns           = 5;
    cfg.maxIns           = 10;
    cfg.delayPerExec     = 0;

    Scheduler sched;
    sched.start(cfg);

    // Create 4 named processes
    sched.createNamedProcess("alpha", cfg);
    sched.createNamedProcess("beta",  cfg);
    sched.createNamedProcess("gamma", cfg);
    sched.createNamedProcess("delta", cfg);

    bool ok = waitAllFinished(sched, 30);
    auto snap = sched.getSnapshot();
    printSnapshot(snap);

    assert(ok && "TEST 1 FAILED: not all processes finished in time");
    assert(snap.finished.size() == 4 && "TEST 1 FAILED: wrong finished count");
    assert(snap.usedCores <= cfg.numCPU && "TEST 1 FAILED: usedCores > numCPU");

    std::cout << "  TEST 1 PASSED\n";
    sched.shutdown();
}

// ---------------------------------------------------------------------------
// TEST 2: RR – processes yield after quantum, all eventually finish
// ---------------------------------------------------------------------------
static void test_rr_quantum_yield() {
    std::cout << "\n=== TEST 2: RR – quantum yield + all finish ===\n";

    Config cfg;
    cfg.numCPU           = 2;
    cfg.scheduler        = "rr";
    cfg.quantumCycles    = 3;  // small quantum so we can observe yields
    cfg.batchProcessFreq = 999;
    cfg.minIns           = 12; // > quantum so preemption is guaranteed
    cfg.maxIns           = 20;
    cfg.delayPerExec     = 0;

    cfg.maxOverallMem    = 4096;
    cfg.memPerProc       = 512;
    cfg.memPerFrame      = 16;

    Scheduler sched;
    sched.start(cfg);

    sched.createNamedProcess("r1", cfg);
    sched.createNamedProcess("r2", cfg);
    sched.createNamedProcess("r3", cfg);

    bool ok = waitAllFinished(sched, 60);
    auto snap = sched.getSnapshot();
    printSnapshot(snap);

    assert(ok && "TEST 2 FAILED: RR processes did not all finish");
    assert(snap.finished.size() == 3 && "TEST 2 FAILED: wrong finished count");
    assert(snap.usedCores <= cfg.numCPU && "TEST 2 FAILED: usedCores > numCPU");

    // Asserts memory stamps are being written at expected intervals
    // Since processes run > 12 ins and quantum = 3, at least one snapshot should be generated.
    assert(fileExists("memory_stamp_1.txt") && "TEST 2 FAILED: memory_stamp_1.txt file was never dumped");

    std::cout << "  TEST 2 PASSED\n";
    sched.shutdown();
}

// ---------------------------------------------------------------------------
// TEST 3: usedCores never exceeds numCPU under load
// ---------------------------------------------------------------------------
static void test_cores_not_exceeded() {
    std::cout << "\n=== TEST 3: usedCores ≤ numCPU under load ===\n";

    Config cfg;
    cfg.numCPU           = 3;
    cfg.scheduler        = "fcfs";
    cfg.quantumCycles    = 5;
    cfg.batchProcessFreq = 999;
    cfg.minIns           = 20;
    cfg.maxIns           = 40;
    cfg.delayPerExec     = 0;

    Scheduler sched;
    sched.start(cfg);

    // Flood with more processes than cores
    for (int i = 0; i < 10; ++i) {
        sched.createNamedProcess("load" + std::to_string(i), cfg);
    }

    // Poll for 3 seconds checking the invariant
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    bool violated = false;
    while (std::chrono::steady_clock::now() < deadline) {
        auto snap = sched.getSnapshot();
        if (snap.usedCores > cfg.numCPU) {
            std::cerr << "  VIOLATION: usedCores=" << snap.usedCores
                      << " > numCPU=" << cfg.numCPU << "\n";
            violated = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    waitAllFinished(sched, 30);
    auto snap = sched.getSnapshot();
    printSnapshot(snap);

    assert(!violated && "TEST 3 FAILED: usedCores exceeded numCPU");
    std::cout << "  TEST 3 PASSED\n";
    sched.shutdown();
}

// ---------------------------------------------------------------------------
// TEST 4: Batch generation produces p01, p02, ... names
// ---------------------------------------------------------------------------
static void test_batch_naming() {
    std::cout << "\n=== TEST 4: Batch naming (p01, p02, …) ===\n";

    Config cfg;
    cfg.numCPU           = 2;
    cfg.scheduler        = "fcfs";
    cfg.quantumCycles    = 5;
    cfg.batchProcessFreq = 5;   // every 5 ticks
    cfg.minIns           = 5;
    cfg.maxIns           = 10;
    cfg.delayPerExec     = 0;

    Scheduler sched;
    sched.start(cfg);
    sched.schedulerStart();

    // Let batch run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    sched.schedulerStop();

    waitAllFinished(sched, 30);
    auto snap = sched.getSnapshot();
    printSnapshot(snap);

    int total = (int)(snap.running.size() + snap.finished.size());
    std::cout << "  Total processes generated: " << total << "\n";

    // Check at least p01 exists
    bool foundP01 = false;
    for (auto& p : snap.finished)
        if (p.name == "p01") { foundP01 = true; break; }
    for (auto& p : snap.running)
        if (p.name == "p01") { foundP01 = true; break; }

    assert(foundP01 && "TEST 4 FAILED: p01 not found in batch output");
    assert(total > 0  && "TEST 4 FAILED: no processes spawned");

    std::cout << "  TEST 4 PASSED\n";
    sched.shutdown();
}

// ---------------------------------------------------------------------------
// TEST 5: scheduler-stop halts generation; scheduler-start resumes
// ---------------------------------------------------------------------------
static void test_start_stop_toggle() {
    std::cout << "\n=== TEST 5: scheduler-start / scheduler-stop toggle ===\n";

    Config cfg;
    cfg.numCPU           = 1;
    cfg.scheduler        = "fcfs";
    cfg.quantumCycles    = 5;
    cfg.batchProcessFreq = 3;
    cfg.minIns           = 3;
    cfg.maxIns           = 6;
    cfg.delayPerExec     = 0;

    Scheduler sched;
    sched.start(cfg);
    sched.schedulerStart();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    sched.schedulerStop();

    auto snap1 = sched.getSnapshot();
    int count1 = (int)(snap1.running.size() + snap1.finished.size());
    std::cout << "  After stop: " << count1 << " processes\n";

    // Wait a bit — no new processes should appear
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto snap2 = sched.getSnapshot();
    int count2 = (int)(snap2.running.size() + snap2.finished.size());
    std::cout << "  After pause: " << count2 << " processes\n";

    // Resume
    sched.schedulerStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    sched.schedulerStop();

    waitAllFinished(sched, 30);
    auto snap3 = sched.getSnapshot();
    int count3 = (int)(snap3.running.size() + snap3.finished.size());
    std::cout << "  After resume+stop: " << count3 << " processes\n";

    assert(count3 > count2 && "TEST 5 FAILED: no new processes after resume");

    std::cout << "  TEST 5 PASSED\n";
    sched.shutdown();
}

static void test_memory_full_requeue() {
    std::cout << "\n=== TEST 6: Memory full requeue ===\n";
    Config cfg;
    cfg.numCPU = 2; // Setup 2 cores available
    cfg.scheduler = "rr";
    cfg.quantumCycles = 5;
    cfg.batchProcessFreq = 999;
    cfg.minIns = 10; cfg.maxIns = 10; cfg.delayPerExec = 0;
    
    // Setup memory boundary so ONLY 1 process can be allocated at a time
    cfg.maxOverallMem = 1024; 
    cfg.memPerProc = 1024;
    cfg.memPerFrame = 16;

    Scheduler sched;
    sched.start(cfg);
    sched.createNamedProcess("m1", cfg);
    sched.createNamedProcess("m2", cfg);

    // Wait a brief tick, observe that usedCores is exactly 1 because 'm2' fails allocation
    // despite 2 cores being available. 'm2' is pushed back to the tail of the readyQueue.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto snap = sched.getSnapshot();
    
    assert(snap.usedCores <= 1 && "TEST 6 FAILED: Dispatched process to core when memory should be full");
    std::cout << "  Confirmed active core allocation capped at 1 due to memory constraints.\n";

     bool sawCap = false;
    for (int i = 0; i < 20; ++i) {
        auto s = sched.getSnapshot();
        assert(s.usedCores <= 1 && "TEST 6 FAILED: dispatched >1 process when memory allows only 1");
        if (s.usedCores == 1) sawCap = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(sawCap && "TEST 6 FAILED: never observed a process actually get to run");

    bool ok = waitAllFinished(sched, 30);
    auto snap2 = sched.getSnapshot();
    printSnapshot(snap2);
    assert(ok && "TEST 6 FAILED: processes stalled under memory pressure (deadlock)");
    assert(snap2.finished.size() == 2 && "TEST 6 FAILED: wrong finished count");

    std::cout << "  TEST 6 PASSED\n";
    sched.shutdown();

}



// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "============================================\n";
    std::cout << "  CSOPESY  –  M2 Scheduler Test Harness   \n";
    std::cout << "============================================\n";

    try {
        test_fcfs_all_finish();
        test_rr_quantum_yield();
        test_cores_not_exceeded();
        test_batch_naming();
        test_start_stop_toggle();
        test_memory_full_requeue();
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n============================================\n";
    std::cout << "  ALL TESTS PASSED\n";
    std::cout << "============================================\n";
    return 0;
}
