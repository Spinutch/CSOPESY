// ============================================================================
// console.cpp  –  Member 1: Shell / REPL
// ============================================================================
// Owns: ASCII banner, command gating, main-menu loop, attached-process loop,
//       routing to renderSystemStatus / renderProcessSmi / sched API.
// ============================================================================
#include "mo1.h"
#include "scheduler.h"
#include "MemoryUtils.h"
#include "MemoryManager.h"
#include "MemoryStats.h"
#include "BackingStore.h"
#include "InstructionParser.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <memory>

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
void runConsole(SchedT &sched, Config &cfg, BackingStore &backingStore)
{
    printBanner();

    // MO2: MemoryManager is created after config loads (needs frame size info).
    std::unique_ptr<MemoryManager> memMgr;

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

                // MO2: Create the memory manager now that config is loaded.
                memMgr = std::make_unique<MemoryManager>(cfg, backingStore);
                sched.start(cfg, memMgr.get());
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

        // ---- screen -s <name> <mem_size>  (create & attach) ----
        // MO2: a memory size is now required. Range: power of 2 in [64, 65536]
        // bytes. Out-of-range/non-power-of-2 values are an "invalid memory
        // allocation" and the process is NOT created.
        if (line.rfind("screen -s ", 0) == 0)
        {
            std::string rest = trim(line.substr(10));
            std::istringstream iss(rest);
            std::string name, sizeToken;
            iss >> name >> sizeToken;

            if (name.empty() || sizeToken.empty())
            {
                std::cout << "[console] Usage: screen -s <process_name> <process_memory_size>\n";
                continue;
            }

            uint64_t memSize = 0;
            try
            {
                memSize = std::stoull(sizeToken);
            }
            catch (const std::exception &)
            {
                std::cout << "[console] " << MemoryUtils::invalidMemoryMessage(0) << "\n";
                continue;
            }

            if (!MemoryUtils::isValidMemorySize(memSize))
            {
                std::cout << "[console] " << MemoryUtils::invalidMemoryMessage(memSize) << "\n";
                continue;
            }

            std::cout << "[console] >> Routing: 'screen -s' → create process '"
                      << name << "' (" << memSize << " bytes) then attach\n";
            sched.createNamedProcess(name, cfg, memSize);
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
            // MO2 (Danika): a process that crashed on a memory access
            // violation can no longer be re-attached to; report the
            // shutdown instead, per the spec's updated "screen -r" behavior.
            if (p->crashed)
            {
                std::ostringstream hexAddr;
                hexAddr << "0x" << std::hex << std::uppercase << p->violationAddress;
                std::cout << "Process " << name
                          << " shut down due to memory access violation error that occurred at "
                          << p->violationTimestamp << ". " << hexAddr.str() << " invalid.\n";
                continue;
            }
            attachedProcessLoop(sched, name);
            continue;
        }

        // ---- screen -c <name> <mem_size> "<instructions>"  (user-defined) ----
        // MO2 (Danika): create & attach to a process whose instructions are
        // exactly the 1-50 semicolon-separated ones the user supplied,
        // instead of the scheduler's randomly-generated program.
        if (line.rfind("screen -c ", 0) == 0)
        {
            std::string rest = trim(line.substr(10));

            size_t firstQuote = rest.find('"');
            if (firstQuote == std::string::npos || rest.empty() || rest.back() != '"' ||
                firstQuote == rest.size() - 1)
            {
                std::cout << "[console] Usage: screen -c <process_name> <process_memory_size> "
                             "\"<instructions>\"\n";
                continue;
            }

            std::string header = trim(rest.substr(0, firstQuote));
            std::string instrBody = rest.substr(firstQuote + 1, rest.size() - firstQuote - 2);

            std::istringstream hiss(header);
            std::string name, sizeToken;
            hiss >> name >> sizeToken;
            if (name.empty() || sizeToken.empty())
            {
                std::cout << "[console] Usage: screen -c <process_name> <process_memory_size> "
                             "\"<instructions>\"\n";
                continue;
            }

            uint64_t memSize = 0;
            try
            {
                memSize = std::stoull(sizeToken);
            }
            catch (const std::exception &)
            {
                std::cout << "[console] " << MemoryUtils::invalidMemoryMessage(0) << "\n";
                continue;
            }

            if (!MemoryUtils::isValidMemorySize(memSize))
            {
                std::cout << "[console] " << MemoryUtils::invalidMemoryMessage(memSize) << "\n";
                continue;
            }

            if (sched.findProcess(name))
            {
                std::cout << "[console] Process '" << name << "' already exists.\n";
                continue;
            }

            InstructionParser::ParseResult parsed = InstructionParser::parseUserInstructions(instrBody);
            if (!parsed.ok)
            {
                std::cout << "invalid command\n";
                continue;
            }

            std::cout << "[console] >> Routing: 'screen -c' → create user-defined process '"
                      << name << "' (" << memSize << " bytes, " << parsed.instructions.size()
                      << " instruction(s)) then attach\n";
            sched.createUserDefinedProcess(name, memSize, parsed.instructions);
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

        // ---- process-smi (system-wide overview, nvidia-smi style) ----
        if (line == "process-smi")
        {
            std::cout << "[console] >> Routing: 'process-smi' → buildMemoryStats + getSnapshot\n";

            SchedulerSnapshot snap = sched.getSnapshot();
            double cpuUtil = (snap.totalCores > 0)
                                 ? (100.0 * snap.usedCores / snap.totalCores)
                                 : 0.0;

            uint64_t totalMem = cfg.maxOverallMem;
            uint64_t usedMem = memMgr ? memMgr->getUsedMemoryBytes() : 0;
            double memUtil = (totalMem > 0) ? (100.0 * usedMem / totalMem) : 0.0;

            std::cout << "--------------------------------------------------------------------------------\n";
            std::cout << "| PROCESS-SMI V01.00   Driver Version: 01.00 |\n";
            std::cout << "--------------------------------------------------------------------------------\n";
            std::cout << "CPU-Util: " << std::fixed << std::setprecision(0) << cpuUtil << "%\n";
            std::cout << "Memory Usage: " << usedMem << "B / " << totalMem << "B\n";
            std::cout << "Memory Util: " << std::fixed << std::setprecision(0) << memUtil << "%\n";
            std::cout << "--------------------------------------------------------------------------------\n";
            std::cout << "Running processes and memory usage:\n";
            if (snap.running.empty())
            {
                std::cout << "  (none)\n";
            }
            else
            {
                for (const auto &p : snap.running)
                {
                    Process *pp = sched.findProcess(p.name);
                    uint64_t memSize = pp ? pp->memorySize : 0;
                    std::cout << p.name << " " << memSize << "B\n";
                }
            }
            std::cout << "--------------------------------------------------------------------------------\n";
            continue;
        }

        // ---- vmstat ----
        if (line == "vmstat")
        {
            std::cout << "[console] >> Routing: 'vmstat' → buildMemoryStats\n";
            int64_t usedBytes = memMgr ? static_cast<int64_t>(memMgr->getUsedMemoryBytes()) : -1;
            MemoryStats stats = buildMemoryStats(cfg, sched, backingStore, usedBytes);

            std::cout << "================================================================================\n";
            std::cout << "  VMSTAT\n";
            std::cout << "================================================================================\n";
            std::cout << "  Total memory     : " << stats.totalMemory << " bytes\n";
            if (stats.usedMemoryKnown)
            {
                std::cout << "  Used memory      : " << stats.usedMemory << " bytes\n";
                std::cout << "  Free memory      : " << stats.freeMemory << " bytes\n";
            }
            else
            {
                std::cout << "  Used memory      : N/A\n";
                std::cout << "  Free memory      : N/A\n";
            }
            std::cout << "--------------------------------------------------------------------------------\n";
            std::cout << "  Idle CPU ticks   : " << stats.idleCpuTicks << "\n";
            std::cout << "  Active CPU ticks : " << stats.activeCpuTicks << "\n";
            std::cout << "  Total CPU ticks  : " << stats.totalCpuTicks << "\n";
            std::cout << "--------------------------------------------------------------------------------\n";
            std::cout << "  Num paged in     : " << stats.numPagedIn << "\n";
            std::cout << "  Num paged out    : " << stats.numPagedOut << "\n";
            std::cout << "================================================================================\n";
            continue;
        }

        // ---- Unknown ----
        std::cout << "[console] Unknown command: '" << line << "'\n";
        std::cout << "  Valid commands: initialize, exit, screen -ls, "
                     "screen -s <name> <mem_size>, screen -r <name>,\n"
                     "                 screen -c <name> <mem_size> \"<instructions>\",\n"
                     "                 scheduler-start, scheduler-stop, report-util, process-smi, vmstat\n";
    }

    // CRITICAL: stop and join every scheduler/worker/batch thread *before*
    // this function returns. memMgr is a local variable -- the instant
    // runConsole() returns, its destructor runs and frees the MemoryManager.
    // The scheduler's background threads hold a raw MemoryManager* and keep
    // calling into it (allocateProcess/contextSwitch/etc.) until they are
    // actually joined. Without this, there's a window where those threads
    // dereference an already-freed MemoryManager (use-after-free / data race,
    // observed as sporadic segfaults/heap corruption under load). Calling
    // shutdown() here (it's idempotent) guarantees all such threads have
    // stopped before memMgr is torn down. main.cpp's own shutdown() call
    // afterward becomes a harmless no-op.
    sched.shutdown();
}

// ---------------------------------------------------------------------------
// Explicit template instantiation
// ---------------------------------------------------------------------------
// runConsole is only ever instantiated against the real Scheduler now:
// buildMemoryStats() (called from the process-smi / vmstat handlers above)
// takes a concrete Scheduler&, not a template parameter, so a stand-in
// scheduler type can no longer satisfy this template. Standalone testing
// without scheduler.cpp/MemoryManager.cpp/BackingStore.cpp is therefore not
// possible for this file anymore — see main_1.cpp for the integration test
// this now requires instead.
#include "scheduler.h"
template void runConsole<Scheduler>(Scheduler &, Config &, BackingStore &);