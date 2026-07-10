#pragma once
// ============================================================================
// MemoryManager.h  –  Member 3: First-Fit Flat Memory Allocator
// ============================================================================
// (agreed by M2 + M3 + M4, do not change signatures without a sync):
//   - Member 2 (scheduler.cpp) calls allocate()/deallocate() from inside its
//     dispatch/finish logic. It must NOT reach into MemoryManager internals.
//   - Member 3 (MemoryManager.cpp) owns the block list, first-fit search,
//     coalescing, and the memory_stamp_<qq>.txt writer.
//   - Member 4 (main.cpp) wires configure() once, right after Config::loadFromFile().
//
// THREAD SAFETY: All public methods must take their own internal lock
// (see mu_ below). Scheduler code calls these while already holding its own
// storeMu — MemoryManager must NEVER call back into Scheduler to avoid deadlock.
// ============================================================================

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

class MemoryManager {
public:
    MemoryManager() = default;

    // Called once by M4 in main.cpp, right after Config::loadFromFile().
    // maxOverallMem/memPerFrame/memPerProc come straight from Config.
    void configure(uint64_t maxOverallMem, uint64_t memPerFrame, uint64_t memPerProc);

    // First-fit allocate a fixed memPerProc-sized block for this process.
    // Returns false if no free block is large enough (memory is full) —
    // caller (M2) must then requeue the process at the tail of the ready
    // queue and NOT dispatch it this cycle.
    bool allocate(int pid, const std::string& procName);

    // Releases the block owned by pid and coalesces adjacent free blocks.
    // Called by M2 exactly once, when a process transitions to FINISHED.
    // Returns false if pid was not found (already freed / never allocated).
    bool deallocate(int pid);

    // Number of processes currently resident in memory (for the snapshot header).
    int getProcessCountInMemory() const;

    // Total free bytes that exist between/around occupied blocks.
    // Used for the "Total external fragmentation in KB" line
    // (report bytes / 1024).
    uint64_t getExternalFragmentationBytes() const;

    // Writes "memory_stamp_<qq>.txt" (qq zero-padded to 2 digits, e.g. "03")
    // to the current working directory, in the exact format of the
    // assignment's mockup: timestamp, process count, fragmentation in KB,
    // then the ASCII memory map from maxOverallMem down to 0.
    void writeSnapshot(uint64_t quantumCycle) const;

private:
    struct Block {
        uint64_t start;
        uint64_t size;
        bool free;
        int pid;              // -1 if free
        std::string procName; // empty if free
    };

    mutable std::mutex mu_;
    std::vector<Block> blocks_;

    uint64_t maxOverallMem_ = 16384;
    uint64_t memPerFrame_ = 16;
    uint64_t memPerProc_ = 4096;
};