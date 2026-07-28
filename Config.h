#ifndef CSOPESY_EMULATOR_CONFIG_H
#define CSOPESY_EMULATOR_CONFIG_H

#pragma once
#include <string>
#include <cstdint>

struct Config {
    int numCPU = 4;                     // Range: [1, 128]
    std::string scheduler = "rr";       // "fcfs" or "rr"
    uint64_t quantumCycles = 5;         // Range: [1, 2^32]
    uint64_t batchProcessFreq = 1;      // Range: [1, 2^32]
    uint64_t minIns = 1000;             // Range: [1, 2^32]
    uint64_t maxIns = 2000;             // Range: [1, 2^32]
    uint64_t delayPerExec = 0;          // Range: [0, 2^32]

    // --- MO2: Memory manager parameters ---
    // All memory ranges are [2^6, 2^16] bytes and must be a power of 2.
    uint64_t maxOverallMem = 16384;      // Max memory available in bytes
    uint64_t memPerFrame = 16;           // Bytes per frame/page (total frames = maxOverallMem / memPerFrame)
    uint64_t minMemPerProc = 4096;       // Min memory rolled for scheduler-generated processes
    uint64_t maxMemPerProc = 4096;       // Max memory rolled for scheduler-generated processes

    bool initialized = false;
    bool seedProcesses = false;

    bool loadFromFile(const std::string& fileName);
};

#endif // CSOPESY_EMULATOR_CONFIG_H