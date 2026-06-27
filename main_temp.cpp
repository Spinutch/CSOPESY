// main_temp.cpp JUST TEMPORARY SO I CAN COMPILE AND TEST M2 SCHEDULER 
#include "mo1.h"
#include "scheduler.h"
#include "system_ui.cpp"
#include "console.cpp"

template void renderSystemStatus<Scheduler>(Scheduler &, std::ostream &);
template void runConsole<Scheduler>(Scheduler &, Config &);

bool Config::loadFromFile(const std::string &f)
{
    // Try real file first
    std::ifstream file(f);
    if (!file.is_open())
    {
        // fallback defaults
        numCPU = 4;
        scheduler = "rr";
        quantumCycles = 5;
        batchProcessFreq = 3;
        minIns = 10;
        maxIns = 50;
        delayPerExec = 0;
        initialized = true;
        return true;
    }
    // hand off to M4's real parser — for now just use defaults
    numCPU = 4;
    scheduler = "rr";
    quantumCycles = 5;
    batchProcessFreq = 3;
    minIns = 10;
    maxIns = 50;
    delayPerExec = 0;
    initialized = true;
    return true;
}

int main()
{
    Config cfg;
    Scheduler sched;
    try
    {
        runConsole(sched, cfg);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[FATAL] " << e.what() << "\n";
    }
    sched.shutdown();
    return 0;
}