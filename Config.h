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

    // ---- Memory Manager parameters (M3's MemoryManager reads these) ---- //
    uint64_t maxOverallMem = 16384;      // Range: [1, 2^32] — total main memory in bytes
    uint64_t memPerFrame = 16;           // Range: [1, 2^32] — allocation granularity in bytes
    uint64_t memPerProc = 4096;          // Range: [1, 2^32] — fixed memory required per process

    bool initialized = false;           
    
    // If true, every created process is seeded with x,y,z=0 and the fixed FOR program
    bool seedProcesses = false;

    bool loadFromFile(const std::string& fileName);
};

#endif // CSOPESY_EMULATOR_CONFIG_H