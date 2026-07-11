// ============================================================================
// scheduler.cpp  –  Member 2: Scheduler Engine (FCFS + Round-Robin)
// ============================================================================
// Architecture:
//   1 scheduler thread  – owns the ready queue, dispatches to worker cores
//   N worker threads    – one per CPU core; executes one instruction per tick
//   1 batch thread      – generates processes every batchProcessFreq ticks
//
// Heartbeat: g_cpuTick incremented once per scheduler cycle (master tick).
//
// FCFS: process runs on a core until ALL instructions done (no preemption).
// RR:   process runs for quantumCycles ticks then requeued at tail.
//
// delay-per-exec: after each instruction, core busy-waits delayPerExec ticks
//                 (process stays on CPU during delay — not requeued).
// ============================================================================

#include "scheduler.h"
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include "Clock.h"
#include "Process.h"
#include "interpreter.h"
#include <set>
#include "MemoryManager.h"

using ReadyQueue = std::deque<std::string>;

// ---------------------------------------------------------------------------
// Internal process record (richer than the public Process struct)
// ---------------------------------------------------------------------------
struct CoreProcess
{
    // The rich Process owned by M3 (instructions, vars, logs, etc.)
    Process proc;

    // --- Scheduler internals ---
    uint64_t ticksOnCore = 0;    // ticks spent on current core burst
    uint64_t sleepUntilTick = 0; // for SLEEP instruction (M3 sets this)
    bool sleeping = false;
    bool inMemory = false;      // true once memMgr.allocate() has succeeded for this process;
                                 // stays true across RR requeues (process keeps its memory
                                 // block until it actually finishes) so it isn't re-allocated
                                 // on every subsequent quantum burst.

    // Converts to the public Process view (for M1 / M3 calls)
    Process toProcess() const
    {
        return proc; // Process is copyable (logs use shared_ptr mutex)
    }
};

// ---------------------------------------------------------------------------
// SchedulerImpl – the real engine
// ---------------------------------------------------------------------------
struct SchedulerImpl
{
    // --- Config (set in start()) ---
    int numCPU = 1;
    bool isRR = false;
    uint64_t quantumCycles = 5;
    uint64_t batchProcessFreq = 1;
    uint64_t minIns = 10;
    uint64_t maxIns = 100;
    uint64_t delayPerExec = 0;
    bool seedProcesses = false; // if true, seed every process with x,y,z and fixed FOR program

    MemoryManager memMgr;
    uint64_t quantumCycleCounter = 0;

    // --- Process store ---
    std::mutex storeMu;
    std::map<std::string, CoreProcess> processMap; // name → process
    ReadyQueue readyQueue;                         // ordered names (deque for O(1) pop_front)
    std::vector<std::string> coreSlots;            // index=coreId, value=procName or ""
    int nextId = 1;
    int batchCounter = 0; // for p01, p02…

    // --- Thread control ---
    std::atomic<bool> alive{false};
    std::atomic<bool> batchRunning{false};
    std::thread schedulerThread;
    std::vector<std::thread> workerThreads;
    std::thread batchThread;

    // --- Per-core work signals ---
    // atomic/mutex/cv are not copyable; use raw unique_ptr arrays sized in start()
    std::unique_ptr<std::atomic<bool>[]> workerReady;
    std::unique_ptr<std::mutex[]> workerMu;
    std::unique_ptr<std::condition_variable[]> workerCv;

    // --- Recent utilization window for smoothing ---
    std::deque<int> recentUsedCores; // sliding window of used core counts
    size_t smoothingWindow = 50;     // default window size (ticks)

    // RNG for batch generation
    std::mt19937 rng{std::random_device{}()};

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    std::string makeBatchName()
    {
        // p01, p02, … p1240
        std::ostringstream ss;
        ss << "p" << std::setfill('0') << std::setw(2) << (++batchCounter);
        return ss.str();
    }

    int randomInstructionCount()
    {
        std::uniform_int_distribution<int> dist(
            static_cast<int>(minIns),
            static_cast<int>(maxIns));
        return dist(rng);
    }

    std::string nowTimestamp()
    {
        std::time_t t = std::time(nullptr);
        std::tm *tm = std::localtime(&t);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", tm);
        return buf;
    }

    // Enqueue a fresh process into the ready queue (lock must NOT be held)
    void enqueue(const std::string &name, int totalCmds)
    {
        std::lock_guard<std::mutex> lk(storeMu);
        CoreProcess cp;
        cp.proc.id = nextId++;
        cp.proc.name = name;
        cp.proc.state = ProcessState::READY;
        cp.proc.totalCommands = totalCmds;
        cp.proc.executedCommands = 0;
        cp.proc.creationTimestamp = nowTimestamp();
        cp.proc.coreId = -1;
        cp.ticksOnCore = 0;
        cp.sleepUntilTick = 0;
        cp.sleeping = false;

        // Populate instructions
        cp.proc.instructions.clear();

        bool treatAsBatch = (!name.empty() && name[0] == 'p') && !seedProcesses;
        if (treatAsBatch) {
            // Batch process: generate a linear program with totalCmds instructions
            // Start with DECLARE x 0
            Instruction decl;
            decl.type = InstructionType::DECLARE;
            decl.varName = "x";
            decl.varValue = 0;
            cp.proc.instructions.push_back(decl);

            // Fill remaining instructions alternating ADD and PRINT
            int remaining = std::max(0, totalCmds - 1);
            for (int i = 0; i < remaining; ++i) {
                if ((i % 2) == 0) {
                    Instruction add;
                    add.type = InstructionType::ADD;
                    add.dest = "x";
                    add.src1 = "x";
                    add.src1IsVar = true;
                    add.src2IsVar = false;
                    add.src2Val = 1;
                    cp.proc.instructions.push_back(add);
                } else {
                    Instruction pr;
                    pr.type = InstructionType::PRINT;
                    pr.msg = "Batch value: ";
                    pr.src1IsVar = true;
                    pr.src1 = "x";
                    cp.proc.instructions.push_back(pr);
                }
            }
        } else {
            // Seed every process with x,y,z=0 and a FOR loop of 100 reps
            // Seed DECLARE x,y,z
            for (const auto &v : {"x", "y", "z"}) {
                Instruction d;
                d.type = InstructionType::DECLARE;
                d.varName = v;
                d.varValue = 0;
                cp.proc.instructions.push_back(d);
                // Initialize variable table as well
                cp.proc.writeVar(v, 0);
            }

            // Build FOR body: [ADD x,x,1 ; PRINT "Value from: " + x]
            Instruction add;
            add.type = InstructionType::ADD;
            add.dest = "x";
            add.src1 = "x";
            add.src1IsVar = true;
            add.src2IsVar = false;
            add.src2Val = 1;

            Instruction pr;
            pr.type = InstructionType::PRINT;
            pr.msg = "Value from: ";
            pr.src1IsVar = true;
            pr.src1 = "x";

            Instruction forIns;
            forIns.type = InstructionType::FOR;
            forIns.repeatCount = 100;
            forIns.forBody = {add, pr};

            cp.proc.instructions.push_back(forIns);

            // Update totalCommands to the flattened count (3 declares + 2*100)
            cp.proc.totalCommands = static_cast<int>(3 + 2 * forIns.repeatCount);
        }

        // Ensure flatten cache is invalidated
        cp.proc.invalidateFlatten();

        processMap[name] = cp;
        readyQueue.push_back(name);
    }

    // Create a single batch process and return its generated name
    std::string createBatchProcess()
    {
        std::string name = makeBatchName();
        int cmds = randomInstructionCount();
        enqueue(name, cmds);
        return name;
    }

    // -----------------------------------------------------------------------
    // Scheduler loop (master thread)
    // Advances g_cpuTick, dispatches ready queue to idle cores.
    // -----------------------------------------------------------------------
    void schedulerLoop()
    {
        while (alive)
        {
            uint64_t tick = ++g_cpuTick;

            // Quantum snapshot logic
            // Increments the quantumCycleCounter every quantumCycles tick and writes snapshot.
            // Only write while there's actually something resident in memory — otherwise the
            // master clock (which free-runs from 'initialize' to 'exit') spams empty
            // memory_stamp_*.txt files the entire time the emulator sits idle.
            if (quantumCycles > 0 && (g_cpuTick % quantumCycles) == 0 && memMgr.getProcessCountInMemory() > 0)
            {
                quantumCycleCounter++;
                memMgr.writeSnapshot(quantumCycleCounter);
            }

            {
                std::lock_guard<std::mutex> lk(storeMu);

                // Wake sleeping processes whose timer expired
                for (auto &[name, cp] : processMap)
                {
                    if (cp.sleeping && tick >= cp.sleepUntilTick)
                    {
                        cp.sleeping = false;
                        cp.proc.state = ProcessState::READY;
                        cp.proc.coreId = -1;
                        readyQueue.push_back(name);
                        // std::cout << "[Scheduler] Process '" << name
                        //          << "' woke at tick " << tick << "\n";
                    }
                }

                // Dispatch: assign a ready process to each idle core.
                //
                // IMPORTANT: we do NOT just peek the front of readyQueue and give up on
                // this core if that one entry can't get memory. With batch-process-freq
                // spawning a brand-new (never-yet-resident) process almost every tick,
                // the queue quickly fills with far more arrivals than the 4 memory slots
                // can admit. If we only ever looked at the front, an already-resident
                // process (just quantum-preempted, needs zero allocation — it already
                // owns its block) could sit stuck behind a huge pile of new arrivals that
                // keep failing to allocate, each cycle only shuffling 1-2 entries to the
                // tail per core per tick. That starved already-running processes for
                // seconds at a time even though a core was sitting idle and a dispatchable
                // (already-resident) process existed a few slots back.
                //
                // Fix: for each idle core, scan forward through the queue (at most once
                // through its current length) looking for the first candidate that is
                // EITHER already resident (instant dispatch, no allocation needed) OR can
                // successfully allocate a fresh block. Anything we skip over still gets
                // sent to the tail, preserving the spec's "revert to tail on alloc failure"
                // rule — we just don't stop looking after a single miss.
                for (int c = 0; c < numCPU; ++c)
                {
                    if (!workerReady[c].load())
                        continue; // core busy

                    size_t attemptsLeft = readyQueue.size();
                    bool dispatchedThisCore = false;

                    while (attemptsLeft > 0 && !readyQueue.empty())
                    {
                        --attemptsLeft;

                        std::string pname = readyQueue.front();
                        readyQueue.pop_front();

                        auto it = processMap.find(pname);
                        if (it == processMap.end())
                            continue; // stale entry — drop it, try next candidate

                        CoreProcess &cp = it->second;
                        if (cp.proc.state == ProcessState::FINISHED)
                            continue; // drop it, try next candidate
                        if (cp.sleeping)
                        {
                            readyQueue.push_back(pname); // shouldn't normally happen, but be safe
                            continue;
                        }

                        // Only allocate a new block the FIRST time this process enters memory.
                        // A process keeps its block across RR requeues (it stays resident until
                        // it finishes), so re-dispatching it for a later quantum burst must NOT
                        // ask the memory manager for another block — doing so would (a) waste
                        // memory the process already owns and (b) deadlock the whole system once
                        // memory fills up, since already-resident processes would then fail their
                        // own re-allocation forever.
                        if (!cp.inMemory)
                        {
                            if (!memMgr.allocate(cp.proc.id, pname))
                            {
                                // If allocation fails (memory full) -> revert to tail, try next candidate
                                readyQueue.push_back(pname);
                                continue;
                            }
                            cp.inMemory = true;
                        }

                        cp.proc.state = ProcessState::RUNNING;
                        cp.proc.coreId = c;
                        cp.ticksOnCore = 0;
                        coreSlots[c] = pname;

                        workerReady[c].store(false);
                        workerCv[c].notify_one(); // wake the worker
                        //    std::cout << "[Scheduler] Dispatched '" << pname
                        //;            << "' → Core " << c
                        //         << " at tick " << tick << "\n";
                        dispatchedThisCore = true;
                        break;
                    }

                    (void)dispatchedThisCore; // core stays idle this tick if nothing was dispatchable
                }
            }

            // Record current used cores into recent window (for smoothing)
            {
                int curUsed = 0;
                for (int i = 0; i < numCPU; ++i) {
                    if (!workerReady[i].load()) ++curUsed;
                }
                recentUsedCores.push_back(curUsed);
                if (recentUsedCores.size() > smoothingWindow) recentUsedCores.pop_front();
            }

            // Cycle sleep between master ticks.
            // NOTE: this used to be 1ms, which combined with batch-process-freq=1
            // spawned a new process every ~1ms (~1000/sec). With only 2 cores and
            // 4 memory slots, arrival rate massively outpaced service rate, so the
            // ready queue grew unbounded, almost nothing ever finished in any
            // observable test window, and the quantum-cycle memory snapshot (every
            // 4 ticks) fired ~250x/sec, which is how a few seconds of runtime
            // produced thousands of memory_stamp_*.txt files. 100ms gives a demo
            // pace where a 5-second scheduler-start window spawns ~50 processes,
            // matching the assignment's expectation that most/all processes finish
            // within the test window even though only 4 fit in memory at once.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // -----------------------------------------------------------------------
    // Worker loop (one per core)
    // Executes one instruction per tick; respects delay-per-exec and quantum.
    // -----------------------------------------------------------------------
    void workerLoop(int coreId)
    {
        while (alive)
        {
            // Wait until dispatcher signals this core
            {
                std::unique_lock<std::mutex> lk(workerMu[coreId]);
                workerCv[coreId].wait(lk, [&]
                                      { return !workerReady[coreId].load() || !alive.load(); });
            }
            if (!alive)
                break;

            // Get the assigned process name
            std::string pname;
            {
                std::lock_guard<std::mutex> lk(storeMu);
                pname = coreSlots[coreId];
            }
            if (pname.empty())
            {
                workerReady[coreId].store(true);
                continue;
            }

            // Execute instructions until: finished / quantum expired / sleep
            while (alive)
            {
                uint64_t curTick = g_cpuTick.load();

                std::string procName;
                int execDone, totalCmds;
                bool isFinished, isSleeping;
                uint64_t ticksOnCore;

                {
                    std::lock_guard<std::mutex> lk(storeMu);
                    auto it = processMap.find(pname);
                    if (it == processMap.end())
                        break;

                    CoreProcess &cp = it->second;
                    execDone = cp.proc.executedCommands;
                    totalCmds = cp.proc.totalCommands;
                    isFinished = (cp.proc.state == ProcessState::FINISHED);
                    isSleeping = cp.sleeping;
                    ticksOnCore = cp.ticksOnCore;
                    procName = cp.proc.name;
                }

                if (isFinished || isSleeping)
                    break;

                // ---- Execute one "instruction" ----
                {
                    std::lock_guard<std::mutex> lk(storeMu);
                    auto it = processMap.find(pname);
                    if (it == processMap.end())
                        break;
                    CoreProcess &cp = it->second;

                    // Call M3 interpreter to perform one step
                    StepResult res = stepProcess(cp.proc, coreId, curTick);

                    if (res.type == StepResult::RAN) {
                        cp.ticksOnCore++;
                        // If process completed as a result of this instruction
                        if (cp.proc.executedCommands >= cp.proc.totalCommands) {
                            cp.proc.state = ProcessState::FINISHED;
                            cp.proc.coreId = -1;
                            coreSlots[coreId] = "";
                            workerReady[coreId].store(true);
                            memMgr.deallocate(cp.proc.id);
                            cp.inMemory = false;
                            goto next_dispatch;
                        }
                    }
                    else if (res.type == StepResult::SLEEPING) {
                        // Put process to sleep until tick + sleepTicks
                        cp.sleeping = true;
                        cp.sleepUntilTick = curTick + res.sleepTicks;
                        cp.proc.state = ProcessState::READY;
                        cp.proc.coreId = -1;
                        coreSlots[coreId] = "";
                        workerReady[coreId].store(true);
                        goto next_dispatch;
                    }
                    else if (res.type == StepResult::FINISHED) {
                        memMgr.deallocate(cp.proc.id);
                        cp.inMemory = false;
                        cp.proc.state = ProcessState::FINISHED;
                        cp.proc.coreId = -1;
                        coreSlots[coreId] = "";
                        workerReady[coreId].store(true);
                        goto next_dispatch;
                    }

                    ticksOnCore = cp.ticksOnCore;
                }

                // ---- delay-per-exec: busy-wait on this core ----
                if (delayPerExec > 0)
                {
                    uint64_t waitUntil = g_cpuTick.load() + delayPerExec;
                    while (g_cpuTick.load() < waitUntil && alive)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                else
                {
                    // One instruction per tick: wait for next tick
                    uint64_t nextTick = g_cpuTick.load() + 1;
                    while (g_cpuTick.load() < nextTick && alive)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }

                // ---- RR quantum check ----
                if (isRR && ticksOnCore >= quantumCycles)
                {
                    std::lock_guard<std::mutex> lk(storeMu);
                    auto it = processMap.find(pname);
                    if (it != processMap.end())
                    {
                        CoreProcess &cp = it->second;
                        if (cp.proc.state == ProcessState::RUNNING)
                        {
                            cp.proc.state = ProcessState::READY;
                            cp.proc.coreId = -1;
                            cp.ticksOnCore = 0;
                            coreSlots[coreId] = "";
                            readyQueue.push_back(pname); // requeue at tail
                            // std::cout << "[Core " << coreId << "] RR quantum expired for '"
                            //           << pname << "' at tick " << g_cpuTick.load()
                            //           << " → requeued\n";
                        }
                    }
                    workerReady[coreId].store(true);
                    goto next_dispatch;
                }
            }

        next_dispatch:
            workerReady[coreId].store(true);
        }
    }

    // -----------------------------------------------------------------------
    // Batch generation thread
    // Spawns a new process every batchProcessFreq ticks while batchRunning.
    // -----------------------------------------------------------------------
    void batchLoop()
    {
        uint64_t lastSpawnTick = g_cpuTick.load();

        while (alive)
        {
            if (!batchRunning)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                lastSpawnTick = g_cpuTick.load(); // reset so no burst on restart
                continue;
            }

            uint64_t now = g_cpuTick.load();
            if (now - lastSpawnTick >= batchProcessFreq)
            {
                lastSpawnTick = now;
                std::string name = makeBatchName();
                int cmds = randomInstructionCount();
                enqueue(name, cmds);
                //  std::cout << "[Batch] Spawned '" << name
                //           << "' (" << cmds << " instructions) at tick "
                //           << now << "\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

// ============================================================================
// Scheduler public API
// ============================================================================

Scheduler::Scheduler() : impl_(std::make_unique<SchedulerImpl>()) {}
Scheduler::~Scheduler() { shutdown(); }

void Scheduler::start(const Config &cfg)
{
    auto &I = *impl_;
    if (I.alive)
        return; // already started

    I.numCPU = cfg.numCPU;
    I.isRR = (cfg.scheduler == "rr");
    I.quantumCycles = cfg.quantumCycles;
    I.batchProcessFreq = cfg.batchProcessFreq;
    I.minIns = cfg.minIns;
    I.maxIns = cfg.maxIns;
    I.delayPerExec = cfg.delayPerExec;
    I.seedProcesses = cfg.seedProcesses;

    // Wire up the memory manager with the config's memory parameters.
    // Without this call, MemoryManager's block list (blocks_) is never
    // initialized (stays empty), so allocate() can never find a free block
    // and every dispatch attempt fails silently — processes get requeued
    // forever, coreId stays -1, and nothing ever finishes.
    I.memMgr.configure(cfg.maxOverallMem, cfg.memPerFrame, cfg.memPerProc);

    // Size per-core arrays (atomic/mutex/cv are not copyable; use unique_ptr arrays)
    I.coreSlots.assign(cfg.numCPU, "");
    I.workerReady = std::make_unique<std::atomic<bool>[]>(cfg.numCPU);
    I.workerMu = std::make_unique<std::mutex[]>(cfg.numCPU);
    I.workerCv = std::make_unique<std::condition_variable[]>(cfg.numCPU);
    for (int i = 0; i < cfg.numCPU; ++i)
        I.workerReady[i].store(true);

    I.alive = true;

    // Launch worker threads
    I.workerThreads.reserve(cfg.numCPU);
    for (int c = 0; c < cfg.numCPU; ++c)
    {
        I.workerThreads.emplace_back([&I, c]()
                                     { I.workerLoop(c); });
    }

    // Launch scheduler (master) thread
    I.schedulerThread = std::thread([&I]()
                                    { I.schedulerLoop(); });

    // Launch batch thread (idle until schedulerStart())
    I.batchThread = std::thread([&I]()
                                { I.batchLoop(); });

    std::cout << "[Scheduler] Started: " << cfg.numCPU << " cores, algo="
              << cfg.scheduler << ", quantum=" << cfg.quantumCycles << "\n";
}

void Scheduler::schedulerStart()
{
    impl_->batchRunning = true;
}

void Scheduler::schedulerStop()
{
    impl_->batchRunning = false;
}

SchedulerSnapshot Scheduler::getSnapshot()
{
    auto &I = *impl_;
    std::lock_guard<std::mutex> lk(I.storeMu);

    SchedulerSnapshot snap;
    snap.totalCores = I.numCPU;
    snap.usedCores = 0;

    for (auto &[name, cp] : I.processMap)
    {
        ProcView v;
        v.id = cp.proc.id;
        v.name = cp.proc.name;
        v.state = cp.proc.state;
        v.totalCommands = cp.proc.totalCommands;
        v.executedCommands = cp.proc.executedCommands;
        v.creationTimestamp = cp.proc.creationTimestamp;
        v.coreId = cp.proc.coreId;

        // Copy recent logs into ProcView
        const size_t maxLines = 50;
        if (cp.proc.logMutex) {
            std::lock_guard<std::mutex> lk(*cp.proc.logMutex);
            size_t start = (cp.proc.logs.size() > maxLines) ? (cp.proc.logs.size() - maxLines) : 0;
            for (size_t i = start; i < cp.proc.logs.size(); ++i)
                v.logs.push_back(cp.proc.logs[i]);
        } else {
            size_t start = (cp.proc.logs.size() > maxLines) ? (cp.proc.logs.size() - maxLines) : 0;
            for (size_t i = start; i < cp.proc.logs.size(); ++i)
                v.logs.push_back(cp.proc.logs[i]);
        }

        if (cp.proc.state == ProcessState::FINISHED)
        {
            snap.finished.push_back(v);
        }
        else if (cp.proc.state == ProcessState::RUNNING)
        {
            snap.running.push_back(v);
        }
        else
        {
            // READY: created/preempted but not currently on a core.
            snap.waiting.push_back(v);
        }
    }

    // Option A: Recompute usedCores by inspecting workerReady flags (false => core busy)
    // Option B: Count assigned core IDs
    std::set<int> assignedCores;
    for (auto &entry : I.processMap) {
        if (entry.second.proc.coreId >= 0)
            assignedCores.insert(entry.second.proc.coreId);
    }
    // If smoothing window has data, use averaged used core count
    if (!I.recentUsedCores.empty()) {
        long sum = 0;
        for (int v : I.recentUsedCores) sum += v;
        double avg = static_cast<double>(sum) / static_cast<double>(I.recentUsedCores.size());
        snap.usedCores = static_cast<int>(std::round(avg));
    }
    else if (!assignedCores.empty()) {
        snap.usedCores = static_cast<int>(assignedCores.size());
    } else {
        // Fallback to workerReady flags
        snap.usedCores = 0;
        for (int i = 0; i < I.numCPU; ++i) {
            if (!I.workerReady[i].load()) ++snap.usedCores;
        }
    }
    return snap;
}

Process *Scheduler::findProcess(const std::string &name)
{
    // NOTE: Returns pointer into a thread-shared map.
    // Caller must treat as a snapshot read only (M1 uses it for process-smi display).
    // We keep a small per-lookup Process cache to avoid dangling pointer.
    // Thread-safety: storeMu held briefly to copy; caller gets stable heap object.
    auto &I = *impl_;

    // We store a copy in a thread_local to give M1 a stable pointer per call.
    // (Simple and safe for the display-only use case.)
    static thread_local Process tlsProc;

    std::lock_guard<std::mutex> lk(I.storeMu);
    auto it = I.processMap.find(name);
    if (it == I.processMap.end())
        return nullptr;
    tlsProc = it->second.toProcess();
    return &tlsProc;
}

void Scheduler::createNamedProcess(const std::string &name, const Config &cfg)
{
    auto &I = *impl_;
    {
        std::lock_guard<std::mutex> lk(I.storeMu);
        if (I.processMap.count(name))
        {
            std::cout << "[Scheduler] Process '" << name << "' already exists.\n";
            return;
        }
    }
    // Use midpoint of min/max for named processes (M3 can override with real instruction list)
    int cmds = static_cast<int>((cfg.minIns + cfg.maxIns) / 2);
    I.enqueue(name, cmds);
    std::cout << "[Scheduler] Created named process '" << name
              << "' with " << cmds << " instructions.\n";
}

std::string Scheduler::createBatchProcess()
{
    auto &I = *impl_;
    std::string name = I.createBatchProcess();
    std::cout << "[Scheduler] Created batch process '" << name << "'.\n";
    return name;
}

void Scheduler::shutdown()
{
    auto &I = *impl_;
    if (!I.alive)
        return;

    std::cout << "[Scheduler] Shutting down...\n";
    I.alive = false;
    I.batchRunning = false;

    // Wake all workers so they can exit their wait
    for (int c = 0; c < I.numCPU; ++c)
    {
        I.workerReady[c].store(true);
        I.workerCv[c].notify_all();
    }

    if (I.schedulerThread.joinable())
        I.schedulerThread.join();
    if (I.batchThread.joinable())
        I.batchThread.join();
    for (auto &t : I.workerThreads)
        if (t.joinable())
            t.join();

    std::cout << "[Scheduler] All threads joined. Clean exit.\n";
}
