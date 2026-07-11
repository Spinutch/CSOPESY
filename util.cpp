// ============================================================================
// util.cpp  –  Member 1: Shared Utilities
// ============================================================================
#include "mo1.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <mutex>

// Returns wall-clock time as "MM/DD/YYYY HH:MM:SSam/pm"
// Example: "06/27/2026 02:45:30PM"
std::string formatTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm *tm = std::localtime(&now);

    char buf[32];
    // %I = 12-hr hour, %p = AM/PM (locale-dependent but standard on Linux/macOS)
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", tm);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// viewOf  –  Process -> ProcView conversion
// ---------------------------------------------------------------------------
// Declared in mo1.h but previously unimplemented anywhere in the codebase.
// M2's scheduler.cpp needs this to populate SchedulerSnapshot::running /
// ::finished, so it belongs alongside the other shared M1 utilities.
// Takes the process's own logMutex (if present) so the copy of `logs` is
// consistent even if M2's scheduler thread is appending to it concurrently.
