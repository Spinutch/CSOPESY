#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR
};

struct Instruction {
    InstructionType type;

    // PRINT
    std::string msg;

    // DECLARE
    std::string varName;
    uint16_t varValue = 0;

    // ADD / SUBTRACT: dest = src1 op src2
    std::string dest, src1, src2;
    bool src1IsVar = false, src2IsVar = false;
    uint16_t src1Val = 0, src2Val = 0;

    // SLEEP
    uint8_t sleepTicks = 0;

    // FOR
    uint32_t repeatCount = 1;
    std::vector<Instruction> forBody;
};
