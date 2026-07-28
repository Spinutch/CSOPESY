#pragma once
// ============================================================================
// scheduler.h  –  Member 2: Scheduler Engine
// Exposes EXACTLY the interface M1's console.cpp calls.
// ============================================================================
#include "mo1.h"
#include <string>
#include <memory>

// Forward-declare implementation to keep header clean
struct SchedulerImpl;

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    // Called by M1 console after initialize
    void start(const Config& cfg);

    // Batch generation toggle (scheduler-start / scheduler-stop)
    void schedulerStart();
    void schedulerStop();

    // Snapshot for screen -ls / report-util
    SchedulerSnapshot getSnapshot();

    // MO2: cumulative CPU-tick accounting for "vmstat" (Heather / Justine).
    // idleTicks + activeTicks == totalTicks == numCPU * (master ticks elapsed).
    struct CpuTickStats {
        uint64_t idleTicks = 0;
        uint64_t activeTicks = 0;
        uint64_t totalTicks = 0;
    };
    CpuTickStats getCpuTickStats();

    // Process lookup for screen -r / screen -s
    Process* findProcess(const std::string& name);
    // memSize: validated by the caller (see MemoryUtils::isValidMemorySize)
    // before this is invoked -- e.g. from "screen -s <name> <mem_size>".
    void     createNamedProcess(const std::string& name, const Config& cfg, uint64_t memSize = 0);
    // Create a single batch process (returns generated name)
    std::string createBatchProcess();

    // Clean shutdown (called by M4's main before exit)
    void shutdown();

private:
    std::unique_ptr<SchedulerImpl> impl_;
};