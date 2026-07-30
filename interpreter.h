#pragma once
#include <cstdint>
#include "Process.h"

struct StepResult {
    enum Type { RAN, SLEEPING, FINISHED, CRASHED } type;
    uint64_t sleepTicks = 0;
};

// Execute a single instruction for process `p` on core `coreId` at tick `tick`.
// Returns RAN when an instruction executed and the core remains; SLEEPING when
// the process requested sleep (core should relinquish for sleepTicks); and
// FINISHED when the process completed.
StepResult stepProcess(Process &p, int coreId, uint64_t tick);
