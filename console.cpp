// ============================================================================
// console.cpp  –  Member 1: Shell / REPL
// ============================================================================
// Owns: ASCII banner, command gating, main-menu loop, attached-process loop,
//       routing to renderSystemStatus / renderProcessSmi / sched API.
// ============================================================================
#include "mo1.h"
#include "scheduler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

// ---------------------------------------------------------------------------
// Internal: clear terminal (POSIX + Windows fallback)
// ---------------------------------------------------------------------------
static void clearScreen()
{
    // ANSI escape: move cursor home + erase display
    std::cout << "\033[2J\033[H" << std::flush;
}

// ---------------------------------------------------------------------------
// Internal: trim leading/trailing whitespace
// ---------------------------------------------------------------------------
static std::string trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// ASCII banner  –  printed once on startup
// ---------------------------------------------------------------------------
static void printBanner()
{
    std::cout << "\n";
    std::cout << R"(   ___________ ____  ____  ___________  __ )" << "\n";
    std::cout << R"(  / ____/ ___// __ \/ __ \/ ____/ ___/\ \/ / )" << "\n";
    std::cout << R"( / /    \__ \/ / / / /_/ / __/  \__ \  \  /  )" << "\n";
    std::cout << R"( / /___ ___/ / /_/ / ____/ /___ ___/ /  / /  )" << "\n";
    std::cout << R"( \____//____/\____/_/   /_____//____/  /_/   )" << "\n";
    std::cout << "\n";
    std::cout << "  OS Emulator  –  Process Scheduler & CLI\n";
    std::cout << "  Build: C++17  |  CSOPESY  |  Group Output 1\n";
    std::cout << "  " << formatTimestamp() << "\n";
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  Type 'initialize' to boot the system.\n";
    std::cout << "  Type 'exit' to quit.\n";
    std::cout << "================================================================================\n\n";
}

// ---------------------------------------------------------------------------
// Attached-process loop  (screen -s / screen -r)
// ---------------------------------------------------------------------------
template <typename SchedT>
static void attachedProcessLoop(SchedT &sched, const std::string &procName)
{
    Process *proc = sched.findProcess(procName);

    if (!proc)
    {
        std::cout << "[screen] Process '" << procName << "' not found.\n";
        return;
    }

    clearScreen();
    std::cout << "================================================================================\n";
    std::cout << "  Attached to process: " << procName << "\n";
    std::cout << "  Type 'process-smi' to view status.\n";
    std::cout << "  Type 'exit' to return to main menu.\n";
    std::cout << "================================================================================\n\n";

    std::string cmd;
    while (true)
    {
        std::cout << procName << "> ";
        if (!std::getline(std::cin, cmd))
        {
            std::cout << "[DEBUG] stdin closed; returning to menu.\n";
            break;
        }
        cmd = trim(cmd);

        if (cmd == "exit")
        {
            std::cout << "[screen] Detaching from '" << procName << "'.\n";
            break;
        }
        else if (cmd == "process-smi")
        {
            // Refresh the pointer (process may have advanced state)
            proc = sched.findProcess(procName);
            if (!proc)
            {
                std::cout << "[screen] Process has been released.\n";
                break;
            }
            renderProcessSmi(*proc, std::cout);
        }
        else if (cmd.empty())
        {
            // ignore blank lines
        }
        else
        {
            std::cout << "[screen] Unknown command: '" << cmd
                      << "'. Valid: process-smi, exit\n";
        }
    }

    clearScreen();
}

// ---------------------------------------------------------------------------
// runConsole  –  main REPL
// ---------------------------------------------------------------------------
template <typename SchedT>
void runConsole(SchedT &sched, Config &cfg)
{
    printBanner();

    std::string line;
    while (true)
    {
        std::cout << "root:\\> ";
        if (!std::getline(std::cin, line))
        {
            std::cout << "\n[console] stdin EOF — exiting.\n";
            break;
        }
        line = trim(line);

        // ---- Always-available commands (even before initialize) ----
        if (line == "exit")
        {
            std::cout << "[console] Goodbye.\n";
            break;
        }

        if (line == "initialize")
        {
            if (cfg.initialized)
            {
                std::cout << "[console] Already initialized. Skipping.\n";
                continue;
            }
            std::cout << "[console] >> Routing: 'initialize' → Config::loadFromFile\n";
            if (cfg.loadFromFile("config.txt"))
            {
                std::cout << "[console] System initialized successfully.\n";
                std::cout << "  num-cpu          : " << cfg.numCPU << "\n";
                std::cout << "  scheduler        : " << cfg.scheduler << "\n";
                std::cout << "  quantum-cycles   : " << cfg.quantumCycles << "\n";
                std::cout << "  batch-proc-freq  : " << cfg.batchProcessFreq << "\n";
                std::cout << "  min-ins          : " << cfg.minIns << "\n";
                std::cout << "  max-ins          : " << cfg.maxIns << "\n";
                std::cout << "  delay-per-exec   : " << cfg.delayPerExec << "\n";
                sched.start(cfg);
            }
            else
            {
                std::cerr << "[console] ERROR: Failed to read 'config.txt'.\n";
            }
            continue;
        }

        // ---- Gating: everything below requires initialize ----
        if (!cfg.initialized)
        {
            std::cout << "[console] ERROR: You must run 'initialize' first "
                         "before using any other commands.\n";
            continue;
        }

        // ---- Blank line ----
        if (line.empty())
            continue;

        // ---- screen -ls ----
        if (line == "screen -ls")
        {
            std::cout << "[console] >> Routing: 'screen -ls' → renderSystemStatus\n";
            renderSystemStatus(sched, std::cout);
            continue;
        }

        // ---- screen -s <name>  (create & attach) ----
        if (line.rfind("screen -s ", 0) == 0)
        {
            std::string name = trim(line.substr(10));
            if (name.empty())
            {
                std::cout << "[console] Usage: screen -s <process_name>\n";
                continue;
            }
            std::cout << "[console] >> Routing: 'screen -s' → create process '"
                      << name << "' then attach\n";
            sched.createNamedProcess(name, cfg);
            attachedProcessLoop(sched, name);
            continue;
        }

        // ---- screen -r <name>  (re-attach) ----
        if (line.rfind("screen -r ", 0) == 0)
        {
            std::string name = trim(line.substr(10));
            if (name.empty())
            {
                std::cout << "[console] Usage: screen -r <process_name>\n";
                continue;
            }
            std::cout << "[console] >> Routing: 'screen -r' → re-attach '"
                      << name << "'\n";
            Process *p = sched.findProcess(name);
            if (!p)
            {
                std::cout << "Process " << name << " not found.\n";
                continue;
            }
            attachedProcessLoop(sched, name);
            continue;
        }

        // ---- scheduler-start ----
        if (line == "scheduler-start")
        {
            std::cout << "[console] >> Routing: 'scheduler-start' → sched.schedulerStart()\n";
            sched.schedulerStart();
            std::cout << "[console] Batch process generation started.\n";
            continue;
        }

        // Spawn a single batch process on demand
        if (line == "spawn-batch")
        {
            std::cout << "[console] >> Routing: 'spawn-batch' → sched.createBatchProcess()\n";
            std::string name = sched.createBatchProcess();
            std::cout << "[console] Spawned batch process '" << name << "'\n";
            continue;
        }

        // ---- scheduler-stop ----
        if (line == "scheduler-stop")
        {
            std::cout << "[console] >> Routing: 'scheduler-stop' → sched.schedulerStop()\n";
            sched.schedulerStop();
            std::cout << "[console] Batch process generation stopped.\n";
            continue;
        }

        // ---- report-util ----
        if (line == "report-util")
        {
            std::cout << "[console] >> Routing: 'report-util' → "
                         "renderSystemStatus → csopesy-log.txt\n";
            std::ofstream logFile("csopesy-log.txt", std::ios::trunc);
            if (!logFile.is_open())
            {
                std::cerr << "[console] ERROR: Cannot open csopesy-log.txt for writing.\n";
                continue;
            }
            std::string ts = formatTimestamp();
            logFile << "CSOPESY Utilization Report\n";
            logFile << "Generated: " << ts << "\n\n";
            renderSystemStatus(sched, logFile);
            logFile.close();
            std::cout << "Report generated at " << ts << " → csopesy-log.txt\n";
            continue;
        }

        // ---- Unknown ----
        std::cout << "[console] Unknown command: '" << line << "'\n";
        std::cout << "  Valid commands: initialize, exit, screen -ls, "
                     "screen -s <name>, screen -r <name>,\n"
                     "                 scheduler-start, scheduler-stop, report-util\n";
    }
}

#include "scheduler.h"
template void runConsole<Scheduler>(Scheduler&, Config&);

