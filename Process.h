#pragma once
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <memory>
#include "Instructions.h"

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

    // --- MO2: Required Memory per Process ---
    // Requested via "screen -s <name> <mem_size>"; validated as a power of 2
    // in [64, 65536] bytes (see MemoryUtils.h). numPages is derived from
    // mem-per-frame at creation time (ceil(memorySize / mem-per-frame)).
    uint64_t memorySize = 0;
    uint32_t numPages = 0;

    std::vector<Instruction> instructions;
    std::unordered_map<std::string, uint16_t> variables; // auto-declared, uint16_t, clamped
    std::shared_ptr<std::mutex> variablesMutex;

    std::vector<std::string> logs;
    std::shared_ptr<std::mutex> logMutex;

    // Cached flattened instruction stream (for FOR expansion)
    mutable std::shared_ptr<std::vector<Instruction>> flatInstructions;
    mutable std::shared_ptr<std::mutex> flatMutex;

    // Variable accessors (auto-declare on first use)
    uint16_t readVar(const std::string& name);
    void writeVar(const std::string& name, long long val); // accepts signed for negative handling

    // Logging
    void appendLog(int coreId, const std::string& msg);

    // Flattening cache control
    void invalidateFlatten();
    std::vector<Instruction> getFlattenedInstructions() const;
};