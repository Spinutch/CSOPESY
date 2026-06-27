#include "Process.h"
#include "mo1.h"
#include <sstream>
#include <memory>
#include <functional>

uint16_t Process::readVar(const std::string& name)
{
    if (!variablesMutex) variablesMutex = std::make_shared<std::mutex>();
    std::lock_guard<std::mutex> lk(*variablesMutex);
    auto it = variables.find(name);
    if (it == variables.end()) {
        variables[name] = 0;
        return 0;
    }
    return it->second;
}

void Process::writeVar(const std::string& name, long long val)
{
    if (!variablesMutex) variablesMutex = std::make_shared<std::mutex>();
    std::lock_guard<std::mutex> lk(*variablesMutex);
    if (val < 0) val = 0;
    if (val > 65535) val = 65535;
    variables[name] = static_cast<uint16_t>(val);
}

void Process::appendLog(int coreId, const std::string& msg)
{
    if (!logMutex) logMutex = std::make_shared<std::mutex>();
    std::lock_guard<std::mutex> lk(*logMutex);
    std::ostringstream ss;
    ss << "(" << formatTimestamp() << ") Core:" << coreId << " \"" << msg << "\"";
    logs.push_back(ss.str());
    // Keep logs bounded to avoid unbounded memory growth in tests
    if (logs.size() > 1000)
        logs.erase(logs.begin());
}

void Process::invalidateFlatten()
{
    if (!flatMutex) flatMutex = std::make_shared<std::mutex>();
    std::lock_guard<std::mutex> lk(*flatMutex);
    flatInstructions.reset();
}

std::vector<Instruction> Process::getFlattenedInstructions() const
{
    if (!flatMutex) const_cast<Process*>(this)->flatMutex = std::make_shared<std::mutex>();
    std::lock_guard<std::mutex> lk(*const_cast<Process*>(this)->flatMutex);

    if (flatInstructions && !flatInstructions->empty())
        return *flatInstructions;

    // Build flattened instructions with max nesting depth 3
    std::vector<Instruction> out;
    std::function<void(const std::vector<Instruction>&, int)> fl;
    fl = [&](const std::vector<Instruction>& src, int depth) {
        for (const auto &ins : src) {
            if (ins.type == InstructionType::FOR) {
                if (depth >= 3) {
                    // Exceeded nesting: skip expanding further
                    continue;
                }
                for (uint32_t i = 0; i < ins.repeatCount; ++i) {
                    fl(ins.forBody, depth + 1);
                }
            } else {
                out.push_back(ins);
            }
        }
    };

    fl(instructions, 0);

    // Cache
    const_cast<Process*>(this)->flatInstructions = std::make_shared<std::vector<Instruction>>(out);
    return out;
}

// Build a ProcView from a Process (M3 provides a real implementation)
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

    // Copy recent logs (thread-safe)
    const size_t maxLines = 50;
    if (p.logMutex) {
        std::lock_guard<std::mutex> lk(*p.logMutex);
        size_t start = (p.logs.size() > maxLines) ? (p.logs.size() - maxLines) : 0;
        for (size_t i = start; i < p.logs.size(); ++i)
            v.logs.push_back(p.logs[i]);
    } else {
        size_t start = (p.logs.size() > maxLines) ? (p.logs.size() - maxLines) : 0;
        for (size_t i = start; i < p.logs.size(); ++i)
            v.logs.push_back(p.logs[i]);
    }

    return v;
}
