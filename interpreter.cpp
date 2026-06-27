#include "interpreter.h"
#include <vector>
#include <sstream>

static uint16_t resolveOperand(Process &p, const Instruction &ins, bool isSrc1) {
    if (isSrc1) {
        if (ins.src1IsVar) return p.readVar(ins.src1);
        return ins.src1Val;
    } else {
        if (ins.src2IsVar) return p.readVar(ins.src2);
        return ins.src2Val;
    }
}

StepResult stepProcess(Process &p, int coreId, uint64_t tick) {
    std::vector<Instruction> flat = p.getFlattenedInstructions();

    if (p.executedCommands >= static_cast<int>(flat.size())) {
        p.state = ProcessState::FINISHED;
        return StepResult{StepResult::FINISHED, 0};
    }

    const Instruction ins = flat[p.executedCommands];

    switch (ins.type) {
    case InstructionType::PRINT: {
        std::string outMsg;
        const std::string &s = ins.msg;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '{') {
                size_t j = s.find('}', i+1);
                if (j != std::string::npos) {
                    std::string var = s.substr(i+1, j - (i+1));
                    uint16_t val = p.readVar(var);
                    outMsg += std::to_string(val);
                    i = j;
                    continue;
                }
            }
            outMsg.push_back(s[i]);
        }
        // If no placeholders and src1/src2 provided, append them
        if (s.find('{') == std::string::npos) {
            if (ins.src1IsVar) outMsg += std::to_string(p.readVar(ins.src1));
            else if (!ins.src1.empty()) outMsg += std::to_string(ins.src1Val);
            if (ins.src2IsVar) outMsg += std::to_string(p.readVar(ins.src2));
            else if (!ins.src2.empty()) outMsg += std::to_string(ins.src2Val);
        }

        p.appendLog(coreId, outMsg);
        p.executedCommands++;
        return StepResult{StepResult::RAN, 0};
    }
    case InstructionType::DECLARE: {
        p.writeVar(ins.varName, ins.varValue);
        p.executedCommands++;
        return StepResult{StepResult::RAN, 0};
    }
    case InstructionType::ADD: {
        uint32_t a = resolveOperand(p, ins, true);
        uint32_t b = resolveOperand(p, ins, false);
        uint32_t res = a + b;
        p.writeVar(ins.dest, res);
        p.executedCommands++;
        return StepResult{StepResult::RAN, 0};
    }
    case InstructionType::SUBTRACT: {
        uint32_t a = resolveOperand(p, ins, true);
        uint32_t b = resolveOperand(p, ins, false);
        uint32_t res = (a > b) ? (a - b) : 0;
        p.writeVar(ins.dest, res);
        p.executedCommands++;
        return StepResult{StepResult::RAN, 0};
    }
    case InstructionType::SLEEP: {
        // Advance the instruction pointer and request sleep
        p.executedCommands++;
        return StepResult{StepResult::SLEEPING, static_cast<uint64_t>(ins.sleepTicks)};
    }
    case InstructionType::FOR:
        // FORs are expanded; should not reach here. Treat as NOP if it does.
        p.executedCommands++;
        return StepResult{StepResult::RAN, 0};
    }

    // Fallback
    p.executedCommands++;
    return StepResult{StepResult::RAN, 0};
}
