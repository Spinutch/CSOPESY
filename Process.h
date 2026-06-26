#pragma once
#include <queue>
#include <string>

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
};