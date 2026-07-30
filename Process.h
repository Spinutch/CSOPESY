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

    // READ/WRITE memory space + access-violation tracking 
    // Simulated per-process memory space addressed [0, memorySize), separate
    // from the symbol table ('variables' above). Values are emulated (not a
    // 1:1 mapping of physical RAM) per the MO2 spec.
    std::unordered_map<uint32_t, uint16_t> memSpace;
    std::shared_ptr<std::mutex> memSpaceMutex;

    // Set when a READ/WRITE targets an address outside [0, memorySize).
    // "screen -r" checks this to print the shutdown-due-to-violation message
    // instead of re-attaching.
    bool crashed = false;
    uint32_t violationAddress = 0;   // the invalid address that caused the crash
    std::string violationTimestamp; // "HH:MM:SS" at the moment of the crash

    // Variable accessors (auto-declare on first use)
    uint16_t readVar(const std::string& name);
    void writeVar(const std::string& name, long long val); // accepts signed for negative handling

    // Simulated memory space accessors (READ/WRITE instructions)
    bool isValidAddress(uint32_t addr) const;
    uint16_t readMem(uint32_t addr);
    void writeMem(uint32_t addr, uint16_t val);

    // Logging
    void appendLog(int coreId, const std::string& msg);

    // Flattening cache control
    void invalidateFlatten();
    std::vector<Instruction> getFlattenedInstructions() const;
};