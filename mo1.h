#pragma once
// ============================================================================
// mo1.h  –  Member 1: Shell & System UI  (Day-0 header)
// Declares everything M2/M3 need to call into M1's module.
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <iostream> 
#include "Process.h"
#include "Config.h"
#include "Clock.h"

// ---------------------------------------------------------------------------
// Forward-declare the two types M1 depends on so this header stays
// self-contained even before M2/M3 hand over their real headers.
// ---------------------------------------------------------------------------


/* COMMENTED OUT BCS OF DUPLICATE DEFINITION
// ---- Minimal Config contract (owned by M4, repeated here for M1 stubs) ----
#ifndef CONFIG_H
#define CONFIG_H
#include <string>
struct Config {
    int         numCPU          = 1;
    std::string scheduler       = "rr";
    uint64_t    quantumCycles   = 5;
    uint64_t    batchProcessFreq= 1;
    uint64_t    minIns          = 10;
    uint64_t    maxIns          = 100;
    uint64_t    delayPerExec    = 0;
    bool        initialized     = false;

    bool loadFromFile(const std::string& fileName);
};
#endif // CONFIG_H */

// COMMENTED OUT BCS OF DUPLICATE DEFINITION
/*  ---- Minimal Process contract (owned by M3) --------------------------------
#ifndef PROCESS_H
#define PROCESS_H
#include <iostream>
#include <queue>
struct Process {
    int          id;
    std::string  name;
    ProcessState state;
    std::queue<std::string> commands;
    int          totalCommands;
    int          executedCommands;
    std::string  creationTimestamp;
    int          coreId;           // -1 = no core
};
#endif // PROCESS_H */

// ---- Minimal Scheduler contract (owned by M2) ------------------------------
// M1 only needs the snapshot API. M2 will provide the real implementation.
#ifndef SCHEDULER_INTERFACE
#define SCHEDULER_INTERFACE
struct ProcView {
    int          id;
    std::string  name;
    ProcessState state;
    int          totalCommands;
    int          executedCommands;
    std::string  creationTimestamp;
    int          coreId;
};

struct SchedulerSnapshot {
    std::vector<ProcView> running;
    std::vector<ProcView> finished;
    int usedCores  = 0;
    int totalCores = 0;
};
#endif // SCHEDULER_INTERFACE
ProcView viewOf(const Process& p);
// ---------------------------------------------------------------------------
// util.cpp
// ---------------------------------------------------------------------------
// Returns current wall-clock time in "MM/DD/YYYY HH:MM:SSam/pm" format.
std::string formatTimestamp();

// ---------------------------------------------------------------------------
// system_ui.cpp
// ---------------------------------------------------------------------------
// Renders the screen -ls / report-util block to 'out' (stdout or a file).
// Accepts a snapshot so the render is pure/testable.
template<typename SchedT>
void renderSystemStatus(SchedT& sched, std::ostream& out);

// Non-template declaration for cases where the full scheduler type is known.
// (Implemented inline via the template above.)

// ---------------------------------------------------------------------------
// console.cpp  –  M3 entry point that M1 calls inside attached-process mode
// ---------------------------------------------------------------------------
// M3 owns renderProcessSmi; M1 just calls it.
void renderProcessSmi(const Process& p, std::ostream& out = std::cout);

// ---------------------------------------------------------------------------
// console.cpp
// ---------------------------------------------------------------------------
// Main REPL.  Sched must expose:
//   SchedulerSnapshot getSnapshot()
//   void schedulerStart()
//   void schedulerStop()
//   Process* findProcess(const std::string& name)   // nullptr = not found
template<typename SchedT>
void runConsole(SchedT& sched, Config& cfg);