#pragma once
// ============================================================================
// MemoryStats.h  -  Memory Visualization & Backing Store Access
// ============================================================================
// Shared data contract for the "vmstat" and "process-smi" commands (rendered
// by Justine's CLI code). This struct collects every field the MO2 spec's
// vmstat table asks for, sourced from whichever module actually owns that
// data:
//
//   totalMemory   <- Config::maxOverallMem                    [available now]
//   usedMemory    <- Rai's Memory Manager frame table         [[TBD until Rai's
//   freeMemory    <- totalMemory - usedMemory                  MemoryManager
//                                                               lands]]
//   idleCpuTicks, activeCpuTicks, totalCpuTicks
//                 <- Scheduler::getCpuTickStats()              [available now]
//   numPagedIn, numPagedOut
//                 <- BackingStore::getStats()                  [available now]
//
// Call buildMemoryStats() to assemble what's available today. Pass the real
// usedMemoryBytes once Rai's Memory Manager exposes it; until then it
// defaults to 0 (meaning "unknown / not yet wired in", NOT "zero memory
// used" -- callers rendering this should say "N/A" rather than "0" if
// usedMemoryKnown is false).
// ============================================================================
#include <cstdint>
#include "Config.h"
#include "scheduler.h"
#include "BackingStore.h"

struct MemoryStats {
    uint64_t totalMemory = 0;
    uint64_t usedMemory = 0;
    uint64_t freeMemory = 0;
    bool     usedMemoryKnown = false; // false until the real frame table is wired in

    uint64_t idleCpuTicks = 0;
    uint64_t activeCpuTicks = 0;
    uint64_t totalCpuTicks = 0;

    uint64_t numPagedIn = 0;
    uint64_t numPagedOut = 0;
};

// Assembles a MemoryStats snapshot from the sources available today.
// usedMemoryBytes: pass the real resident-frame total once Rai's Memory
// Manager can report it; leave as -1 (default) while it's still unavailable.
inline MemoryStats buildMemoryStats(const Config& cfg, Scheduler& sched,
                                     const BackingStore& backingStore,
                                     int64_t usedMemoryBytes = -1) {
    MemoryStats stats;
    stats.totalMemory = cfg.maxOverallMem;

    if (usedMemoryBytes >= 0) {
        stats.usedMemory = static_cast<uint64_t>(usedMemoryBytes);
        stats.freeMemory = (stats.totalMemory > stats.usedMemory)
                               ? (stats.totalMemory - stats.usedMemory)
                               : 0;
        stats.usedMemoryKnown = true;
    }

    Scheduler::CpuTickStats ticks = sched.getCpuTickStats();
    stats.idleCpuTicks = ticks.idleTicks;
    stats.activeCpuTicks = ticks.activeTicks;
    stats.totalCpuTicks = ticks.totalTicks;

    BackingStore::Stats bsStats = backingStore.getStats();
    stats.numPagedIn = bsStats.pagedIn;
    stats.numPagedOut = bsStats.pagedOut;

    return stats;
}
