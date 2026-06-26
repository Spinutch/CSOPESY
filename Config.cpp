#include "Config.h"
#include <fstream>
#include <sstream>

bool Config::loadFromFile(const std::string& fileName) {
    std::ifstream ifs(fileName);
    if (!ifs.is_open()) return false;

    std::string key;
    while (file >> key) {
        if      (key == "num-cpu")              file >> numCPU;
        else if (key == "scheduler")            file >> scheduler;
        else if (key == "quantum-cycles")       file >> quantumCycles;
        else if (key == "batch-process-freq")   file >> batchProcessFreq;
        else if (key == "min-ins")              file >> minIns;
        else if (key == "max-ins")              file >> maxIns;
        else if (key == "delay-per-exec")       file >> delayPerExec;
    }
    return true;
};
