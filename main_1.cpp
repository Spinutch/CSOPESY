// ============================================================================
// main_1.cpp  –  Member 1: Standalone Test Harness
// ============================================================================
// Compiles with:
//   g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp -o mo1_test
//
// Exercises:
//   - ASCII banner
//   - 'initialize' gating
//   - 'screen -ls'  (stub snapshot)
//   - 'screen -s'   (stub process + attached loop)
//   - 'report-util' (writes csopesy-log.txt)
//   - 'scheduler-start' / 'scheduler-stop'
// ============================================================================

#include "mo1.h"      // Config, Process, ProcessState, SchedulerSnapshot, etc.
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

// ---------------------------------------------------------------------------
// g_cpuTick – M4 owns the real definition in clock.cpp.
// The harness defines it locally so main_1.cpp links standalone.
// ---------------------------------------------------------------------------
std::atomic<uint64_t> g_cpuTick{0};

// ---------------------------------------------------------------------------
// Config::loadFromFile stub
// (The real one lives in M4's config.cpp. Here we fake success and fill
//  reasonable defaults so we don't need config.txt to be present.)
// ---------------------------------------------------------------------------
bool Config::loadFromFile(const std::string& /*fileName*/) {
    // Try to read the real file first; fall back to hardcoded defaults.
    // For the harness, just set defaults and mark initialized.
    numCPU           = 4;
    scheduler        = "rr";
    quantumCycles    = 5;
    batchProcessFreq = 3;
    minIns           = 10;
    maxIns           = 50;
    delayPerExec     = 0;
    initialized      = true;
    std::cout << "[Config stub] Loaded hardcoded defaults (no config.txt needed).\n";
    return true;
}

// ---------------------------------------------------------------------------
// StubScheduler  –  feeds canned data to M1's rendering code
// ---------------------------------------------------------------------------
class StubScheduler {
public:
    // --- Process store ---
    std::map<std::string, Process> processes;
    std::mutex                     mu;
    std::atomic<bool>              batchRunning{false};
    std::thread                    heartbeatThread;
    std::atomic<bool>              alive{true};
    int                            numCores = 4;
    int                            nextId   = 1;

    StubScheduler() {
        // Seed a few fake processes
        _addProcess("p01", ProcessState::RUNNING,  20, 12, 0);
        _addProcess("p02", ProcessState::RUNNING,  35,  7, 1);
        _addProcess("p03", ProcessState::FINISHED, 15, 15, -1);
        _addProcess("p04", ProcessState::FINISHED, 50, 50, -1);

        // Background heartbeat
        heartbeatThread = std::thread([this]() {
            while (alive) {
                ++g_cpuTick;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    ~StubScheduler() {
        alive = false;
        if (heartbeatThread.joinable()) heartbeatThread.join();
    }

    void start(const Config& cfg) {
        numCores = cfg.numCPU;
        std::cout << "[StubScheduler] start() called. numCPU=" << cfg.numCPU
                  << "  algo=" << cfg.scheduler << "\n";
    }

    void schedulerStart() {
        batchRunning = true;
        std::cout << "[StubScheduler] schedulerStart(): batch generation ON.\n";
    }

    void schedulerStop() {
        batchRunning = false;
        std::cout << "[StubScheduler] schedulerStop(): batch generation OFF.\n";
    }

    // Return a snapshot of current state
    SchedulerSnapshot getSnapshot() {
        std::lock_guard<std::mutex> lk(mu);
        SchedulerSnapshot snap;
        snap.totalCores = numCores;

        for (auto& [name, p] : processes) {
            ProcView v;
            v.id                = p.id;
            v.name              = p.name;
            v.state             = p.state;
            v.totalCommands     = p.totalCommands;
            v.executedCommands  = p.executedCommands;
            v.creationTimestamp = p.creationTimestamp;
            v.coreId            = p.coreId;

            if (p.state == ProcessState::FINISHED) {
                snap.finished.push_back(v);
            } else {
                snap.running.push_back(v);
                if (p.coreId >= 0) ++snap.usedCores;
            }
        }
        return snap;
    }

    // Create a named process (screen -s)
    void createNamedProcess(const std::string& name, const Config& cfg) {
        std::lock_guard<std::mutex> lk(mu);
        if (processes.count(name)) {
            std::cout << "[StubScheduler] Process '" << name << "' already exists.\n";
            return;
        }
        int total = (int)((cfg.minIns + cfg.maxIns) / 2); // midpoint for stub
        _addProcessLocked(name, ProcessState::RUNNING, total, 0,
                          nextId % numCores);
        std::cout << "[StubScheduler] Created named process '" << name << "'.\n";
    }

    // Find a live process by name; nullptr if not found
    Process* findProcess(const std::string& name) {
        std::lock_guard<std::mutex> lk(mu);
        auto it = processes.find(name);
        return (it != processes.end()) ? &it->second : nullptr;
    }

private:
    void _addProcess(const std::string& name, ProcessState st,
                     int total, int done, int core) {
        std::lock_guard<std::mutex> lk(mu);
        _addProcessLocked(name, st, total, done, core);
    }

    void _addProcessLocked(const std::string& name, ProcessState st,
                            int total, int done, int core) {
        Process p;
        p.id                = nextId++;
        p.name              = name;
        p.state             = st;
        p.totalCommands     = total;
        p.executedCommands  = done;
        p.creationTimestamp = formatTimestamp();
        p.coreId            = core;
        processes[name]     = std::move(p);
    }
};

// ---------------------------------------------------------------------------
// Pull in template implementations AFTER StubScheduler is fully defined
// so the compiler can instantiate them.
// ---------------------------------------------------------------------------
#include "system_ui.cpp"
#include "console.cpp"

// Explicit instantiations for StubScheduler
template void renderSystemStatus<StubScheduler>(StubScheduler&, std::ostream&);
template void runConsole<StubScheduler>(StubScheduler&, Config&);

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    Config         cfg;
    StubScheduler  sched;

    try {
        runConsole(sched, cfg);
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "System exited gracefully.\n";
    return 0;
}
