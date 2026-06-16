// ============================================================
//  CSOPESY OS Emulator — FCFS CPU Scheduler
//  Multi-threaded First-Come-First-Serve scheduler with
//  4 CPU worker threads and a master scheduler thread.
//
//  Platform : macOS (Apple Silicon M1), C++17
//  Build    : g++ -std=c++17 -pthread -o scheduler scheduler.cpp
//
//  HOW IT WORKS (bird's-eye view):
//  ────────────────────────────────
//  1. main() generates 10 Process objects (each with 100
//     "print" instructions) and pushes them into a shared,
//     mutex-protected FCFS ready queue.
//  2. The Scheduler Thread does nothing fancy here — its job
//     is conceptually "dispatch in order", which the queue's
//     FIFO nature + the workers' pop-when-free behaviour
//     already guarantees. We still spin up a real thread for
//     it so the assignment's "master distributes work" model
//     is honoured, and so you can later extend it with
//     priority logic, RR time-slicing, etc.
//  3. Each of the 4 CPU Worker Threads loops forever:
//       - lock the queue
//       - wait (via condition_variable) until a process is
//         available OR shutdown is requested
//       - pop the front process (FCFS: whoever is at the
//         front was generated first)
//       - unlock, then execute all 100 instructions for that
//         process uninterrupted (run-to-completion, as FCFS
//         requires)
//       - mark process Finished, loop back for the next one
//  4. The main thread stays interactive, reading CLI commands
//     ("screen -ls", "exit") while the workers run in the
//     background.
// ============================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <sys/select.h>
#include <unistd.h> // STDIN_FILENO
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// ╔══════════════════════════════════════════════════════════╗
// ║                    CONFIGURATION                          ║
// ╚══════════════════════════════════════════════════════════╝

// Toggle this to false before your final submission / grading
// run if your professor wants to measure scheduler throughput
// without file I/O overhead. When false, NO text files are
// created or written to at all — the "print" commands are
// still "executed" (counted, timed) but nothing touches disk.
static bool ENABLE_LOGGING = true;

static const int NUM_PROCESSES = 10;             // processes generated at startup
static const int NUM_CORES = 4;                  // CPU worker threads
static const int INSTRUCTIONS_PER_PROCESS = 100; // "print" commands per process

// ╔══════════════════════════════════════════════════════════╗
// ║                    PROCESS  STRUCT                        ║
// ╚══════════════════════════════════════════════════════════╝

// Tracks the lifecycle of a process as it moves through the
// scheduler: Ready -> Running -> Finished.
enum class ProcessState
{
    Ready,   // sitting in the FCFS queue, not yet picked up
    Running, // a core has popped it and is executing instructions
    Finished // all instructions have been executed
};

struct Process
{
    int id;                               // 1-based process number (creation order)
    std::queue<std::string> instructions; // remaining "print" commands
    ProcessState state = ProcessState::Ready;
    int coreId = -1;           // which core ran/is running this (-1 = none yet)
    int totalInstructions = 0; // for progress reporting in screen -ls

    // Guards `instructions`. The owning CpuWorker pops from this
    // queue with no other worker ever touching it concurrently
    // (FCFS hands each process to exactly one core), but the
    // main thread's "screen -ls" handler reads instructions.size()
    // at the same time to report progress — that read-while-pop
    // is a real data race without this lock (confirmed via
    // ThreadSanitizer during testing).
    std::mutex instructionsMutex;

    Process(int processId, int instructionCount)
        : id(processId), totalInstructions(instructionCount)
    {
        // Pre-fill the instruction queue with dummy "print" commands.
        // Each command is unique (includes its own index) purely so
        // the log file shows clearly which line of the program ran.
        for (int i = 1; i <= instructionCount; ++i)
        {
            std::ostringstream cmd;
            cmd << "print(\"Hello from process " << id
                << ", command " << i << "\")";
            instructions.push(cmd.str());
        }
    }
};

// ╔══════════════════════════════════════════════════════════╗
// ║              SHARED SCHEDULER STATE                       ║
// ╚══════════════════════════════════════════════════════════╝
//
//  This is the ONLY data that multiple threads touch directly.
//  Every access goes through g_queueMutex (for the ready queue)
//  or g_processListMutex (for the master process list used by
//  "screen -ls"). Keeping these separate avoids a worker thread
//  blocking "screen -ls" reads (and vice versa) any longer than
//  strictly necessary.

// ── FCFS ready queue ─────────────────────────────────────────
// Holds raw pointers into g_allProcesses (the master list owns
// the actual Process objects; the queue just orders them).
std::queue<Process *> g_readyQueue;
std::mutex g_queueMutex;           // protects g_readyQueue
std::condition_variable g_queueCV; // wakes workers when work arrives

// ── Master process list (for status reporting) ──────────────
std::vector<Process *> g_allProcesses;
std::mutex g_processListMutex; // protects state/coreId fields

// ── Shutdown flag ─────────────────────────────────────────────
// Workers check this every time they wake up. Using atomic<bool>
// means we can read it from any thread without extra locking.
std::atomic<bool> g_shutdownRequested{false};

// ── Log-file write mutex ──────────────────────────────────────
// Each process writes to its OWN file (process_<id>.txt), so in
// theory two cores never touch the same file at once under this
// assignment's FCFS rule (one process = one core at a time).
// We still keep a single mutex around all file writes for two
// reasons: (1) std::ofstream is not guaranteed thread-safe even
// for *different* file handles on every platform/stdlib, and
// (2) it serializes console-visible side effects so your demo
// video doesn't show garbled interleaved output if you later
// add a "tail the logs live" feature. The lock is held only for
// the duration of one line write, so contention is negligible.
std::mutex g_logMutex;

// ╔══════════════════════════════════════════════════════════╗
// ║                    HELPER FUNCTIONS                       ║
// ╚══════════════════════════════════════════════════════════╝

// Dedicated mutex for the localtime() call only. This is
// SEPARATE from g_logMutex on purpose: WriteLogLine() holds
// g_logMutex for its entire body (to serialize file writes)
// and calls CurrentTimestamp() from inside that locked region.
// If CurrentTimestamp() tried to lock g_logMutex again, the
// SAME thread would block forever waiting on a mutex it
// already owns (std::mutex is not reentrant) — an instant
// self-deadlock. Using a separate mutex here avoids that.
std::mutex g_timeMutex;

// Returns a timestamp string formatted as: 2026-06-16 14:32:05
std::string CurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // localtime() is not guaranteed thread-safe on all platforms
    // (it may return a pointer to shared internal storage), so
    // we guard the conversion with its own small mutex.
    std::tm localTm{};
    {
        std::lock_guard<std::mutex> lock(g_timeMutex);
        localTm = *std::localtime(&t);
    }

    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Appends one log line to process_<id>.txt in the required
// format: [Timestamp] | [Core ID] | [Command String]
// No-ops entirely if ENABLE_LOGGING is false.
void WriteLogLine(int processId, int coreId, const std::string &command)
{
    if (!ENABLE_LOGGING)
        return;

    // Compute the timestamp BEFORE acquiring g_logMutex, so the
    // two mutexes (g_timeMutex and g_logMutex) are never held
    // at the same time by this thread — avoids any chance of a
    // lock-ordering deadlock even if the locking strategy changes
    // later.
    std::string timestamp = CurrentTimestamp();

    std::string filename = "process_" + std::to_string(processId) + ".txt";

    // Lock around the whole open->write->close so that even if
    // two threads somehow targeted the same file, lines never
    // interleave mid-write.
    std::lock_guard<std::mutex> lock(g_logMutex);

    std::ofstream out(filename, std::ios::app); // append mode
    if (!out.is_open())
    {
        std::cerr << "[WARN] Could not open " << filename << " for logging.\n";
        return;
    }

    out << "[" << timestamp << "] | "
        << "Core " << coreId << " | "
        << command << "\n";
    // ofstream's destructor flushes & closes automatically here.
}

// ╔══════════════════════════════════════════════════════════╗
// ║                  CPU WORKER THREAD                        ║
// ╚══════════════════════════════════════════════════════════╝
//
//  This is the function each of the 4 "CPU core" threads runs.
//  coreId is 0..3 so logs clearly show which core did the work.
//
//  THREAD-SAFETY WALKTHROUGH (see also written explanation
//  below the code):
//  ─────────────────────────────────────────────────────────
//  We use a classic "producer/consumer" pattern:
//    - g_queueMutex protects the queue itself.
//    - g_queueCV.wait(lock, predicate) atomically: (a) unlocks
//      the mutex and sleeps, THEN (b) re-locks the mutex and
//      re-checks the predicate when woken up. This is the key
//      mechanism that makes it impossible for two cores to
//      "race" to the front of the queue — only one thread can
//      hold the lock at the instant it calls queue.pop(), so
//      only one thread can ever retrieve any given Process*.
//
void CpuWorker(int coreId)
{
    while (true)
    {
        Process *myProcess = nullptr;

        // ── Step 1: Try to grab the next process (FCFS) ────
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);

            // Sleep until EITHER:
            //   (a) the queue is non-empty (work to do), OR
            //   (b) shutdown was requested (time to exit).
            // wait() releases the lock while sleeping and
            // re-acquires it automatically before returning,
            // so no other thread can sneak in undetected.
            g_queueCV.wait(lock, []
                           { return !g_readyQueue.empty() || g_shutdownRequested.load(); });

            // If we woke up ONLY because of shutdown and there's
            // no work left, this core is done — exit the loop.
            if (g_shutdownRequested.load() && g_readyQueue.empty())
            {
                break;
            }

            // At this point we are guaranteed to be the ONLY
            // thread holding the lock, so popping is 100% safe:
            // no other core can have grabbed this same pointer.
            myProcess = g_readyQueue.front();
            g_readyQueue.pop();
        } // ── lock released here; other cores can now compete
          //    for the NEXT process while we execute this one.

        // ── Step 2: Mark process Running (FCFS run-to-completion) ──
        {
            std::lock_guard<std::mutex> lock(g_processListMutex);
            myProcess->state = ProcessState::Running;
            myProcess->coreId = coreId;
        }

        // ── Step 3: Execute every instruction, in order ─────
        // FCFS means once a core picks up a process, it runs
        // it to completion before touching anything else —
        // there is no preemption here. We still lock
        // instructionsMutex around each pop because the main
        // thread's "screen -ls" handler reads
        // instructions.size() concurrently for progress display.
        while (true)
        {
            std::string cmd;
            {
                std::lock_guard<std::mutex> lock(myProcess->instructionsMutex);
                if (myProcess->instructions.empty())
                    break;
                cmd = myProcess->instructions.front();
                myProcess->instructions.pop();
            }

            // "Execute" the print command by logging it.
            WriteLogLine(myProcess->id, coreId, cmd);

            // Tiny artificial delay so you can actually SEE
            // processes running concurrently if you watch
            // `screen -ls` live, or watch the log files grow.
            // Remove or shrink this for raw throughput testing.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        // ── Step 4: Mark process Finished ───────────────────
        {
            std::lock_guard<std::mutex> lock(g_processListMutex);
            myProcess->state = ProcessState::Finished;
        }
    }
}

// ╔══════════════════════════════════════════════════════════╗
// ║                  SCHEDULER THREAD                          ║
// ╚══════════════════════════════════════════════════════════╝
//
//  In this simple FCFS scheme the "scheduling decision" is
//  trivial: always serve whoever is at the front of the queue.
//  The queue's FIFO order, combined with workers only popping
//  one element at a time under the mutex, already enforces
//  this. We still model the Scheduler as its own thread (rather
//  than just pushing processes from main()) because:
//    (a) the assignment explicitly asks for a master dispatcher
//        thread, and
//    (b) it gives you a clean seam to later add real scheduling
//        policy (priorities, SRTF, round robin, etc.) without
//        touching the worker or main-thread code at all.
//
//  Here, "distributing work" = pushing all generated processes
//  into the shared ready queue, in creation order, then
//  notifying the workers that something is available.
//
void SchedulerThread()
{
    std::unique_lock<std::mutex> lock(g_queueMutex);

    for (Process *p : g_allProcesses)
    {
        g_readyQueue.push(p);
    }

    // Wake up all 4 workers at once — they will then compete
    // (safely, via the mutex) for who gets to pop first. Since
    // the queue is FIFO, whichever worker wins the race always
    // gets process #1 first, then whoever wins next gets #2,
    // and so on — order of arrival into the queue is preserved
    // regardless of which physical thread executes it.
    lock.unlock();
    g_queueCV.notify_all();
}

// ╔══════════════════════════════════════════════════════════╗
// ║                  STATUS REPORTING (CLI)                   ║
// ╚══════════════════════════════════════════════════════════╝

// Builds the full status report as a single string (instead of
// printing directly) so the SAME logic can be reused for both:
//   - the one-shot "screen -ls" command, and
//   - the auto-refreshing "screen -ls -live" view below, which
//     needs to overwrite the previous frame in place.
std::string BuildProcessListReport()
{
    std::lock_guard<std::mutex> lock(g_processListMutex);
    std::ostringstream out;

    out << "----------------------------------------\n";
    out << "Running processes:\n";
    bool anyRunning = false;
    for (Process *p : g_allProcesses)
    {
        if (p->state == ProcessState::Running)
        {
            int remaining;
            {
                // Same mutex the worker uses when popping —
                // guarantees this read never races with pop().
                std::lock_guard<std::mutex> instrLock(p->instructionsMutex);
                remaining = (int)p->instructions.size();
            }
            int done = p->totalInstructions - remaining;
            out << "  process_" << p->id
                << "  [Core " << p->coreId << "]  "
                << done << "/" << p->totalInstructions
                << " instructions\n";
            anyRunning = true;
        }
    }
    if (!anyRunning)
        out << "  (none)\n";

    out << "\nFinished processes:\n";
    bool anyFinished = false;
    for (Process *p : g_allProcesses)
    {
        if (p->state == ProcessState::Finished)
        {
            out << "  process_" << p->id
                << "  [Core " << p->coreId << "]  "
                << "Finished\n";
            anyFinished = true;
        }
    }
    if (!anyFinished)
        out << "  (none)\n";

    out << "\nWaiting (Ready) processes:\n";
    bool anyReady = false;
    for (Process *p : g_allProcesses)
    {
        if (p->state == ProcessState::Ready)
        {
            out << "  process_" << p->id << "  Ready\n";
            anyReady = true;
        }
    }
    if (!anyReady)
        out << "  (none)\n";

    out << "----------------------------------------\n";
    return out.str();
}

// One-shot print: just dump the report once, like before.
void PrintProcessList()
{
    std::cout << "\n"
              << BuildProcessListReport() << "\n";
}

// ── Live auto-refreshing view ───────────────────────────────
//
//  Redraws the report in place every LIVE_REFRESH_MS using the
//  same ANSI "move cursor up N lines, clear, reprint" trick as
//  the Marquee Console project. This is what actually lets you
//  WATCH the "x/100 instructions" counters climb for each core
//  in real time instead of needing to retype "screen -ls" over
//  and over.
//
//  Exits back to the normal prompt as soon as any key is
//  pressed (uses the same kbhit()/getch() raw-mode pair you
//  already have in the marquee project — see the note below if
//  you want to wire that in instead of the simpler timeout used
//  here).
//
static const int LIVE_REFRESH_MS = 200;

void ShowLiveProcessList()
{
    std::cout << "\n[Live view — refreshing every "
              << LIVE_REFRESH_MS << " ms. Press ENTER to stop.]\n\n";

    int linesPrinted = 0;
    auto lastFrame = std::chrono::steady_clock::now();

    while (true)
    {
        // ── Check if the user pressed ENTER to stop ─────────
        // We do a quick non-blocking check via select() with a
        // zero timeout so this loop never blocks waiting on
        // input — it just peeks.
        timeval tv{0, 0};
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int ready = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &tv);
        if (ready > 0)
        {
            std::string discard;
            std::getline(std::cin, discard); // consume the ENTER
            break;
        }

        // ── Throttle to LIVE_REFRESH_MS ──────────────────────
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrame);
        if (elapsed.count() < LIVE_REFRESH_MS)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(LIVE_REFRESH_MS) - elapsed);
        }
        lastFrame = std::chrono::steady_clock::now();

        // ── Erase the previous frame ─────────────────────────
        // Move the cursor up by however many lines we printed
        // last time, then clear from there to the end of the
        // screen, so the new frame draws cleanly with no
        // leftover text below it (handles the report shrinking
        // as processes move from Running -> Finished).
        if (linesPrinted > 0)
        {
            std::cout << "\033[" << linesPrinted << "A"; // cursor up N lines
            std::cout << "\033[J";                       // clear to end of screen
        }

        std::string report = BuildProcessListReport();
        std::cout << report << std::flush;

        // Count how many lines we just printed so next frame
        // knows how far to rewind the cursor.
        linesPrinted = (int)std::count(report.begin(), report.end(), '\n');

        // Stop automatically once every process has finished —
        // no point refreshing a static screen forever.
        {
            std::lock_guard<std::mutex> lock(g_processListMutex);
            bool allDone = true;
            for (Process *p : g_allProcesses)
            {
                if (p->state != ProcessState::Finished)
                {
                    allDone = false;
                    break;
                }
            }
            if (allDone)
            {
                std::cout << "\n[All processes finished — live view stopped automatically.]\n";
                break;
            }
        }
    }
}

// ╔══════════════════════════════════════════════════════════╗
// ║                          MAIN                              ║
// ╚══════════════════════════════════════════════════════════╝

int main()
{
    std::cout << "==========================================\n";
    std::cout << " CSOPESY OS Emulator - FCFS CPU Scheduler\n";
    std::cout << "==========================================\n";
    std::cout << "Logging to text files: "
              << (ENABLE_LOGGING ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Generating " << NUM_PROCESSES << " processes, "
              << INSTRUCTIONS_PER_PROCESS << " instructions each...\n";
    std::cout << "Spinning up " << NUM_CORES << " CPU worker threads...\n";
    std::cout << "Type 'screen -ls' for a one-time status check,\n";
    std::cout << "     'screen -ls -live' to watch counts climb in real time,\n";
    std::cout << "     'exit' to quit.\n\n";

    // ── 1. Generate the 10 processes ───────────────────────
    // These are created BEFORE any worker threads start, so
    // there is no race on g_allProcesses during this loop.
    for (int i = 1; i <= NUM_PROCESSES; ++i)
    {
        g_allProcesses.push_back(new Process(i, INSTRUCTIONS_PER_PROCESS));
    }

    // ── 2. Launch the 4 CPU worker threads ─────────────────
    std::vector<std::thread> coreThreads;
    for (int coreId = 0; coreId < NUM_CORES; ++coreId)
    {
        coreThreads.emplace_back(CpuWorker, coreId);
    }

    // ── 3. Launch the Scheduler thread ─────────────────────
    // It does its one job (push all processes + notify) and
    // then returns; we join it immediately since it has no
    // further work in this simple FCFS model.
    std::thread scheduler(SchedulerThread);
    scheduler.join();

    // ── 4. Interactive CLI on the main thread ──────────────
    std::string line;
    while (true)
    {
        std::cout << "Enter a command (screen -ls / screen -ls -live / exit): ";
        if (!std::getline(std::cin, line))
            break; // EOF safety

        if (line == "screen -ls")
        {
            PrintProcessList();
        }
        else if (line == "screen -ls -live")
        {
            ShowLiveProcessList();
        }
        else if (line == "exit")
        {
            std::cout << "Shutting down scheduler...\n";
            break;
        }
        else if (!line.empty())
        {
            std::cout << "Unknown command: \"" << line << "\"\n";
        }
    }

    // ── 5. Graceful shutdown sequence ──────────────────────
    //
    //  Setting the atomic flag and notifying ALL waiting
    //  workers ensures none of them are left sleeping forever
    //  on the condition_variable. Each worker re-checks the
    //  predicate, sees shutdown==true AND queue empty (assuming
    //  all 10 processes have finished by now), and exits its
    //  while(true) loop on its own.
    //
    //  NOTE: if you call "exit" before all 10 processes have
    //  finished, the workers will keep draining the remaining
    //  queued processes to completion first (the predicate is
    //  "queue not empty OR shutdown" — shutdown alone doesn't
    //  abandon in-flight work, it only stops new waits once the
    //  queue is empty). This matches "graceful" termination.
    //
    g_shutdownRequested.store(true);
    g_queueCV.notify_all();

    for (std::thread &t : coreThreads)
    {
        t.join();
    }

    // ── 6. Clean up heap-allocated Process objects ─────────
    for (Process *p : g_allProcesses)
    {
        delete p;
    }

    std::cout << "All threads joined. Goodbye!\n";
    return 0;
}