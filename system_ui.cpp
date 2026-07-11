// ============================================================================
// system_ui.cpp  –  Member 1: System Status Renderer
// ============================================================================
#include "mo1.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
static const std::string DIV = "--------------------------------------------------------------------------------";
static const std::string DIV2 = "================================================================================";

static std::string stateLabel(ProcessState s)
{
    switch (s)
    {
    case ProcessState::RUNNING:
        return "Running";
    case ProcessState::READY:
        return "Ready";
    case ProcessState::FINISHED:
        return "Finished";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// renderSystemStatus
// ---------------------------------------------------------------------------
template <typename SchedT>
void renderSystemStatus(SchedT &sched, std::ostream &out)
{
    SchedulerSnapshot snap = sched.getSnapshot();

    // ---- Header block ----
    int total = snap.totalCores;
    int used = snap.usedCores;
    int avail = total - used;
    double util = (total > 0) ? (100.0 * used / total) : 0.0;

    out << DIV2 << "\n";
    out << "  CPU Utilization : " << std::fixed << std::setprecision(2) << util << "%\n";
    out << "  Cores used      : " << used << "\n";
    out << "  Cores available : " << avail << "\n";
    out << DIV2 << "\n";

    // ---- Running processes ----
    out << "\nRunning processes:\n";
    out << DIV << "\n";

    if (snap.running.empty())
    {
        out << "  (none)\n";
    }
    else
    {
        for (const auto &p : snap.running)
        {
            // Tabular alignment using iomanip
            out << "  " << std::left << std::setw(15) << p.name
                << std::left << std::setw(28) << ("(" + p.creationTimestamp + ")")
                << std::left << std::setw(15) << ("Core: " + std::to_string(p.coreId))
                << std::right << std::setw(18) << (std::to_string(p.executedCommands) + " / " + std::to_string(p.totalCommands))
                << "\n";
        }
    }

    out << DIV << "\n";

    // ---- Finished processes ----
    out << "\nFinished processes:\n";
    out << DIV << "\n";

    if (snap.finished.empty())
    {
        out << "  (none)\n";
    }
    else
    {
        for (const auto &p : snap.finished)
        {
            out << "  " << std::left << std::setw(15) << p.name
                << std::left << std::setw(28) << ("(" + p.creationTimestamp + ")")
                << std::left << std::setw(15) << "Finished"
                << std::right << std::setw(18) << (std::to_string(p.executedCommands) + " / " + std::to_string(p.totalCommands))
                << "\n";
        }
    }

    out << DIV << "\n\n";
}

// ---------------------------------------------------------------------------
// renderProcessSmi (M3 implementation)
// ---------------------------------------------------------------------------
void renderProcessSmi(const Process &p, std::ostream &out)
{
    out << DIV << "\n";
    out << "Process: " << p.name << "  |  PID: " << p.id << "\n";
    out << DIV << "\n";

    // Current instruction line (1-based) and total lines
    int curLine = 0;
    if (p.totalCommands <= 0)
    {
        curLine = 0;
    }
    else if (p.state == ProcessState::FINISHED)
    {
        curLine = p.totalCommands;
    }
    else
    {
        curLine = std::min(p.totalCommands, p.executedCommands + 1);
    }

    out << "  Current instruction : " << curLine << " / " << p.totalCommands << "\n";
    out << "  State               : " << stateLabel(p.state) << "\n";
    out << "  Core                : " << p.coreId << "\n";
    out << "\n";

    // Render recent logs (thread-safe)
    out << "  Recent logs:\n";
    const size_t maxLines = 50;
    if (p.logMutex)
    {
        std::lock_guard<std::mutex> lk(*p.logMutex);
        size_t start = (p.logs.size() > maxLines) ? (p.logs.size() - maxLines) : 0;
        if (p.logs.empty())
        {
            out << "    (no logs)\n";
        }
        else
        {
            for (size_t i = start; i < p.logs.size(); ++i)
            {
                out << "    " << p.logs[i] << "\n";
            }
        }
    }
    else
    {
        // If no mutex, just print what's available
        size_t start = (p.logs.size() > maxLines) ? (p.logs.size() - maxLines) : 0;
        if (p.logs.empty())
        {
            out << "    (no logs)\n";
        }
        else
        {
            for (size_t i = start; i < p.logs.size(); ++i)
            {
                out << "    " << p.logs[i] << "\n";
            }
        }
    }

    if (p.state == ProcessState::FINISHED)
    {
        out << "\nFinished!\n";
    }

    out << DIV << "\n";
}

#include "scheduler.h"
template void renderSystemStatus<Scheduler>(Scheduler &, std::ostream &);