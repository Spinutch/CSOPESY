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
class BackingStore;
class SessionLogger;

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

    // Recent logs (M3 provides these)
    std::vector<std::string> logs;
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
void runConsole(SchedT& sched, Config& cfg, BackingStore& backingStore, SessionLogger& logger);