#pragma once
// ============================================================================
// stub_scheduler.h  –  Member 1: TEST-ONLY double for the real Scheduler
// ============================================================================
// Used exclusively by main_1.cpp when built with -DMO1_STANDALONE_TEST.
// Never included by production files (console.cpp / system_ui.cpp only pull
// this in under that same macro guard) and never linked into the real
// csopesy executable.
//
// Purpose: let M1's console.cpp / system_ui.cpp / util.cpp be built and
// tested completely standalone, without requiring scheduler.cpp (M2) or
// MemoryManager.cpp (M3) to exist or compile. This matches the documented
// harness build command:
//   g++ -std=c++17 -pthread main_1.cpp util.cpp system_ui.cpp console.cpp \
//       -DMO1_STANDALONE_TEST -o mo1_test
//
// It exposes the exact same public surface M1's templates call:
//   start(cfg), schedulerStart(), schedulerStop(), getSnapshot(),
//   findProcess(name), createNamedProcess(name,cfg), createBatchProcess(),
//   shutdown()
//
// It also simulates the one piece of M2/M3 behavior that matters for M1's
// required concurrency test: a background thread that periodically writes
// memory_stamp_<NN>.txt files to disk, the same way the real scheduler will
// once MemoryManager::writeSnapshot() is wired in. This lets M1 prove their
// CLI stays responsive (no blocking, no stdout corruption) while that I/O
// happens on another thread — independently of whether M2/M3 are done yet.
// ============================================================================

#include "mo1.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class StubScheduler {
public:
    StubScheduler() = default;
    ~StubScheduler() { shutdown(); }

    // ---- Called by runConsole() on 'initialize' -----------------------
    void start(const Config& cfg) {
        if (alive_.load()) return;
        totalCores_ = cfg.numCPU > 0 ? cfg.numCPU : 1;
        alive_.store(true);

        // Background "memory stamp writer" thread — stands in for M2/M3's
        // future scheduler-thread + MemoryManager::writeSnapshot() calls.
        // Writes a file every ~5ms so it hammers disk I/O far harder than
        // the real system will (real cadence is once per quantum), giving
        // a worst-case stress test for console responsiveness.
        stampThread_ = std::thread([this] { stampWriterLoop(); });
    }

    // ---- scheduler-start / scheduler-stop ------------------------------
    void schedulerStart() {
        batchRunning_.store(true);
        if (!batchThread_.joinable()) {
            batchThread_ = std::thread([this] { batchLoop(); });
        }
    }

    void schedulerStop() {
        batchRunning_.store(false);
    }

    // ---- screen -ls / report-util ---------------------------------------
    SchedulerSnapshot getSnapshot() {
        std::lock_guard<std::mutex> lk(mu_);
        SchedulerSnapshot snap;
        snap.totalCores = totalCores_;
        int used = 0;
        for (auto& p : procs_) {
            ProcView v;
            v.id = p.id;
            v.name = p.name;
            v.state = p.state;
            v.totalCommands = p.totalCommands;
            v.executedCommands = p.executedCommands;
            v.creationTimestamp = p.creationTimestamp;
            v.coreId = p.coreId;
            v.logs = p.logs;
            if (p.state == ProcessState::FINISHED) {
                snap.finished.push_back(v);
            } else {
                snap.running.push_back(v);
                if (p.state == ProcessState::RUNNING) ++used;
            }
        }
        snap.usedCores = std::min(used, totalCores_);
        return snap;
    }

    // ---- screen -r / screen -s -------------------------------------------
    Process* findProcess(const std::string& name) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& p : procs_) {
            if (p.name == name) {
                lastLookup_ = p; // stable copy for the caller (display-only)
                return &lastLookup_;
            }
        }
        return nullptr;
    }

    void createNamedProcess(const std::string& name, const Config& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& p : procs_) {
            if (p.name == name) return; // already exists
        }
        procs_.push_back(makeFakeProcess(name, static_cast<int>((cfg.minIns + cfg.maxIns) / 2)));
    }

    std::string createBatchProcess() {
        std::lock_guard<std::mutex> lk(mu_);
        ++batchCounter_;
        std::string name = std::string("p") + (batchCounter_ < 10 ? "0" : "") + std::to_string(batchCounter_);
        procs_.push_back(makeFakeProcess(name, 10));
        return name;
    }

    // ---- shutdown -----------------------------------------------------
    void shutdown() {
        if (!alive_.exchange(false)) return;
        batchRunning_.store(false);
        if (stampThread_.joinable()) stampThread_.join();
        if (batchThread_.joinable()) batchThread_.join();
    }

private:
    Process makeFakeProcess(const std::string& name, int totalCmds) {
        Process p{};
        p.id = nextId_++;
        p.name = name;
        p.state = ProcessState::READY;
        p.totalCommands = totalCmds;
        p.executedCommands = 0;
        p.creationTimestamp = formatTimestamp();
        p.coreId = -1;
        return p;
    }

    // Simulates each ready process advancing a bit every tick, and randomly
    // finishing — just enough liveliness for screen -ls / report-util to
    // show something changing during the demo.
    void advanceFakeProcesses() {
        std::lock_guard<std::mutex> lk(mu_);
        int coreId = 0;
        for (auto& p : procs_) {
            if (p.state == ProcessState::FINISHED) continue;
            p.state = ProcessState::RUNNING;
            p.coreId = coreId++ % std::max(1, totalCores_);
            if (p.executedCommands < p.totalCommands) p.executedCommands++;
            if (p.executedCommands >= p.totalCommands) {
                p.state = ProcessState::FINISHED;
                p.coreId = -1;
            }
        }
    }

    // ---- Background thread: periodic memory_stamp_<NN>.txt writer -----
    void stampWriterLoop() {
        int seq = 0;
        while (alive_.load()) {
            std::string fname = "memory_stamp_" + std::to_string(seq++) + ".txt";
            {
                std::ofstream f(fname, std::ios::trunc);
                if (f.is_open()) {
                    f << "Timestamp: (" << formatTimestamp() << ")\n";
                    f << "Number of processes in memory: 0\n";
                    f << "Total external fragmentation in KB: 0\n";
                }
            } // file closed/flushed here, off the console thread
            advanceFakeProcesses();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void batchLoop() {
        while (alive_.load()) {
            if (batchRunning_.load()) {
                createBatchProcess();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    std::mutex mu_;
    std::vector<Process> procs_;
    Process lastLookup_{};
    int totalCores_ = 1;
    int nextId_ = 1;
    int batchCounter_ = 0;
    std::atomic<bool> alive_{false};
    std::atomic<bool> batchRunning_{false};
    std::thread stampThread_;
    std::thread batchThread_;
};
