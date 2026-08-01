#pragma once
#include <cstdint>
#include "Process.h"

class MemoryManager;

struct StepResult {
    enum Type { RAN, SLEEPING, FINISHED, CRASHED } type;
    uint64_t sleepTicks = 0;
};

// Execute a single instruction for process `p` on core `coreId` at tick `tick`.
// `memMgr` (may be null) is consulted before any instruction that touches the
// symbol table or a memory address, so the page it needs is faulted in
// on demand (MemoryManager::handlePageFault) before the access happens.
// Returns RAN when an instruction executed and the core remains; SLEEPING when
// the process requested sleep (core should relinquish for sleepTicks); and
// FINISHED when the process completed.
StepResult stepProcess(Process &p, int coreId, uint64_t tick, MemoryManager *memMgr = nullptr);
