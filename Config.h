#ifndef CSOPESY_EMULATOR_CONFIG_H
#define CSOPESY_EMULATOR_CONFIG_H
#include <cstdint>

#endif //CSOPESY_EMULATOR_CONFIG_H

#pragma once
#include <string>

struct Config {
    int numCPU = 4;
    std::string scheduler = "rr";
    uint32_t quantumCycles = 5;
    uint32_t batchProcessFreq = 1;
    uint32_t minIns = 1000;
    uint32_t maxIns = 2000;
    uint32_t delayPerExec = 0;

    bool loadFromFile(const std::string& fileName);
};