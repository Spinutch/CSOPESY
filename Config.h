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

    bool initialized = false;
    bool seedProcesses = false;

    bool loadFromFile(const std::string& fileName);
};

#endif // CSOPESY_EMULATOR_CONFIG_H