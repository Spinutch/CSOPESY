#include "mo1.h"

#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

std::string formatTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", tm);

    return std::string(buf);
}

<<<<<<< HEAD
ProcView viewOf(const Process& p)
{
    ProcView v;

    v.id = p.id;
    v.name = p.name;
    v.state = p.state;
    v.totalCommands = p.totalCommands;
    v.executedCommands = p.executedCommands;
    v.creationTimestamp = p.creationTimestamp;
    v.coreId = p.coreId;

    if (p.logMutex)
    {
        std::lock_guard<std::mutex> lk(*p.logMutex);
        v.logs = p.logs;
    }
    else
    {
        v.logs = p.logs;
    }

    return v;
}
=======
// ---------------------------------------------------------------------------
// viewOf  –  Process -> ProcView conversion
// ---------------------------------------------------------------------------
// Declared in mo1.h but previously unimplemented anywhere in the codebase.
// M2's scheduler.cpp needs this to populate SchedulerSnapshot::running /
// ::finished, so it belongs alongside the other shared M1 utilities.
// Takes the process's own logMutex (if present) so the copy of `logs` is
// consistent even if M2's scheduler thread is appending to it concurrently.
>>>>>>> e95fc867901b5c3764a9fd1145a63e9b3574976c
