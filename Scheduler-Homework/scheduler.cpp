#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <memory>

// ============================================================================
// CONFIGURATION FLAGS
// ============================================================================
// Set to false for final Machine Project submission 
bool ENABLE_LOGGING = true;
const int NUM_CORES = 4;
const int COMMANDS_PER_PROCESS = 100;

// ============================================================================
// DATA STRUCTURES
// ============================================================================
enum class ProcessState
{
    READY,
    RUNNING,
    FINISHED
};

struct Process
{
    int id;
    std::string name;
    ProcessState state;
    std::queue<std::string> commands; // The instruction queue
    int totalCommands;
    int executedCommands;
    std::string creationTimestamp;
    int coreId; // Which core is handling this process (-1 if none)
};

// ============================================================================
// GLOBAL STATE & SYNCHRONIZATION
// ============================================================================
std::vector<std::shared_ptr<Process>> allProcesses;
std::queue<std::shared_ptr<Process>> readyQueue;

std::mutex systemMutex;            // Protects queues and shared states
std::condition_variable cv;        // Signals worker threads when a process is ready
std::atomic<bool> isRunning(true); // Controls the main application lifecycle

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
// Generates a timestamp matching the (MM/DD/YYYY HH:MM:SSAM/PM) format
std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "(" << std::put_time(std::localtime(&in_time_t), "%m/%d/%Y %I:%M:%S%p") << ")";
    return ss.str();
}

// ============================================================================
// CPU WORKER THREAD LOGIC
// ============================================================================
void cpuWorker(int coreId)
{
    while (isRunning)
    {
        std::shared_ptr<Process> currentProcess = nullptr;

        {
            // Lock the mutex before checking/modifying the ready queue
            std::unique_lock<std::mutex> lock(systemMutex);

            // Wait until there is a process in the queue, OR the system is shutting down
            cv.wait(lock, []
                    { return !readyQueue.empty() || !isRunning; });

            if (!isRunning && readyQueue.empty())
            {
                return; // Exit thread gracefully
            }

            // FCFS: Pop the oldest process from the front of the queue
            currentProcess = readyQueue.front();
            readyQueue.pop();

            // Update process state
            currentProcess->state = ProcessState::RUNNING;
            currentProcess->coreId = coreId;
        }

        // --- EXECUTION PHASE (Outside the lock so other cores can pick up work) ---
        if (currentProcess)
        {
            std::ofstream logFile;
            if (ENABLE_LOGGING)
            {
                logFile.open(currentProcess->name + ".txt", std::ios::app);
            }

            while (!currentProcess->commands.empty())
            {
                std::string cmd = currentProcess->commands.front();
                currentProcess->commands.pop();

                std::string timestamp = getCurrentTimestamp();

                if (ENABLE_LOGGING && logFile.is_open())
                {
                    logFile << timestamp << " Core:" << coreId << " \"" << cmd << "\"\n";
                }

                currentProcess->executedCommands++;

                // Artificial delay so the process doesn't finish instantly.
                // This gives you time to type 'screen -ls' for your video!
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (ENABLE_LOGGING && logFile.is_open())
            {
                logFile.close();
            }

            // Lock again to safely update the finished state
            {
                std::lock_guard<std::mutex> lock(systemMutex);
                currentProcess->state = ProcessState::FINISHED;
            }
        }
    }
}

// ============================================================================
// SCHEDULER THREAD LOGIC
// ============================================================================
void schedulerThread()
{
    // In this FCFS implementation, the scheduler simply acts as an initializer.
    // It generates the 10 processes and feeds them into the Ready Queue.
    for (int i = 1; i <= 10; ++i)
    {
        auto p = std::make_shared<Process>();
        p->id = i;

        // Pad numbers < 10 with a leading zero (e.g., process_01)
        std::stringstream nameStream;
        nameStream << "process_" << std::setfill('0') << std::setw(2) << i;
        p->name = nameStream.str();

        p->state = ProcessState::READY;
        p->totalCommands = COMMANDS_PER_PROCESS;
        p->executedCommands = 0;
        p->creationTimestamp = getCurrentTimestamp();
        p->coreId = -1;

        for (int j = 1; j <= COMMANDS_PER_PROCESS; ++j)
        {
            p->commands.push("Hello world from " + p->name + "!");
        }

        {
            std::lock_guard<std::mutex> lock(systemMutex);
            allProcesses.push_back(p);
            readyQueue.push(p);
        }

        // Notify one of the sleeping worker threads that a process is ready
        cv.notify_one();
    }
}

// ============================================================================
// MAIN CLI THREAD
// ============================================================================
int main()
{
    std::cout << "Starting OS Emulator (FCFS Scheduler)...\n";
    std::cout << "Type 'screen -ls' to view processes, or 'exit' to quit.\n\n";

    // Start 4 CPU Worker Threads
    std::vector<std::thread> workers;
    for (int i = 0; i < NUM_CORES; ++i)
    {
        workers.emplace_back(cpuWorker, i);
    }

    // Start the Master Scheduler Thread
    std::thread scheduler(schedulerThread);

    // Main CLI Loop
    std::string command;
    while (isRunning)
    {
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command == "exit")
        {
            std::cout << "Initiating graceful shutdown...\n";
            isRunning = false;
            cv.notify_all(); // Wake up any sleeping cores so they can exit
            break;
        }
        else if (command == "screen -ls")
        {
            std::lock_guard<std::mutex> lock(systemMutex); // Lock to get a clean snapshot of states

            std::cout << "--------------------------------------------------\n";
            std::cout << "Running processes:\n";
            for (const auto &p : allProcesses)
            {
                if (p->state == ProcessState::RUNNING)
                {
                    std::cout << p->name << "   " << p->creationTimestamp
                              << "   Core: " << p->coreId << "    "
                              << p->executedCommands << " / " << p->totalCommands << "\n";
                }
            }

            std::cout << "\nFinished processes:\n";
            for (const auto &p : allProcesses)
            {
                if (p->state == ProcessState::FINISHED)
                {
                    std::cout << p->name << "   " << p->creationTimestamp
                              << "   Finished   "
                              << p->executedCommands << " / " << p->totalCommands << "\n";
                }
            }
            std::cout << "--------------------------------------------------\n";
        }
        else if (!command.empty())
        {
            std::cout << "Unknown command. Try 'screen -ls' or 'exit'.\n";
        }
    }

    // Join all threads before exiting to clean up memory safely
    scheduler.join();
    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    std::cout << "Emulator closed safely.\n";
    return 0;
}