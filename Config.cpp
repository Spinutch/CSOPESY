#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>

static std::string cleanQuotes(std::string str) {
    str.erase(0, str.find_first_not_of(" \t\r\n"));
    str.erase(str.find_last_not_of(" \t\r\n") + 1);
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        str = str.substr(1, str.size() - 2);
    }
    return str;
}

template <typename T>
static T clampVal(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

bool Config::loadFromFile(const std::string& fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) return false;

    std::string key;
    std::string valueStr;

    // Read key-value pairs reliably
    while (file >> key >> valueStr) {
        try {
            if (key == "num-cpu") {
                long long val = std::stoll(valueStr);
                numCPU = static_cast<int>(clampVal(val, 1LL, 128LL));
            }
            else if (key == "scheduler") {
                std::string cleaned = cleanQuotes(valueStr);
                if (cleaned == "fcfs" || cleaned == "rr") {
                    scheduler = cleaned;
                } else {
                    scheduler = "rr"; // Fallback default
                }
            }
            else if (key == "quantum-cycles") {
                unsigned long long val = std::stoull(valueStr);
                quantumCycles = clampVal(val, 1ULL, 4294967296ULL);
            }
            else if (key == "batch-process-freq") {
                unsigned long long val = std::stoull(valueStr);
                batchProcessFreq = clampVal(val, 1ULL, 4294967296ULL);
            }
            else if (key == "min-ins") {
                unsigned long long val = std::stoull(valueStr);
                minIns = clampVal(val, 1ULL, 4294967296ULL);
            }
            else if (key == "max-ins") {
                unsigned long long val = std::stoull(valueStr);
                maxIns = clampVal(val, 1ULL, 4294967296ULL);
            }
            else if (key == "delay-per-exec") {
                unsigned long long val = std::stoull(valueStr);
                delayPerExec = clampVal(val, 0ULL, 4294967296ULL);
            }
        } catch (const std::exception& e) {
            // Protects system from crashing if invalid character inputs are added to config.txt
            std::cerr << "[Config Warning] Field error for " << key << ": " << e.what() << "\n";
        }
    }

    if (minIns > maxIns) {
        std::swap(minIns, maxIns);
    }

    initialized = true;
    return true;
}