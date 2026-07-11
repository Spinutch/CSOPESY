#include "mo1.h"

#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

std::string formatTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", tm);

    return std::string(buf);
}

ProcView viewOf(const Process& p)
{
    ProcView v;

    v.id = p.id;
    v.name = p.name;
    v.state = p.state;
    v.totalCommands = p.totalCommands;
    v.executedCommands = p.executedCommands;
    v.creationTimestamp = p.creationTimestamp;
    v.coreId = p.coreId;

    if (p.logMutex)
    {
        std::lock_guard<std::mutex> lk(*p.logMutex);
        v.logs = p.logs;
    }
    else
    {
        v.logs = p.logs;
    }

    return v;
}