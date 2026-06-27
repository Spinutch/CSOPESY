#include <iostream>
#include "Config.h"
#include "Clock.h"
#include "mo1.h"
#include "scheduler.h"

extern std::atomic<uint64_t> g_cpuTick;

int main() {
    Config systemConfig;
    Scheduler systemScheduler;

    try {
        runConsole(systemScheduler, systemConfig);
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Exception]: " << e.what() << "\n";
    }

    systemScheduler.shutdown();
    std::cout << "System exited gracefully.\n";
    return 0;
}