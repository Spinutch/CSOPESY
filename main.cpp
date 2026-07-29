#include <iostream>
#include "Config.h"
#include "Clock.h"
#include "mo1.h"
#include "scheduler.h"
#include "BackingStore.h"
#include "MemoryManager.h"

extern std::atomic<uint64_t> g_cpuTick;

int main() {
    Config systemConfig;
    Scheduler systemScheduler;

    // MO2: the backing store is a text file that must be
    // accessible at any given time -- construct it up front so
    // "csopesy-backing-store.txt" exists (empty, valid) from process start,
    // ready for the Memory Manager to store/load pages into.
    BackingStore systemBackingStore;

    try {
        runConsole(systemScheduler, systemConfig, systemBackingStore);
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal Exception]: " << e.what() << "\n";
    }

    systemScheduler.shutdown();
    std::cout << "System exited gracefully.\n";
    return 0;
}