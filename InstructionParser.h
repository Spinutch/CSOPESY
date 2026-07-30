// ============================================================================
// InstructionParser.h  -  Danika: User-defined Instructions ("screen -c")
// ============================================================================
// Parses the semicolon-separated instruction string supplied to
//   screen -c <process_name> <process_memory_size> "<instructions>"
// into a std::vector<Instruction> that interpreter.cpp can execute directly
// -- no changes to the interpreter's data model are needed beyond what
// Instructions.h already defines.
//
// Grammar supported (case-insensitive keywords; operands are whitespace-
// separated tokens unless noted):
//   DECLARE <var> <value>
//   ADD <dest> <src1> <src2>          src1/src2: variable name or literal
//   SUBTRACT <dest> <src1> <src2>     uint16
//   READ <var> <address>              address: decimal or 0x-prefixed hex
//   WRITE <address> <value>           value: variable name or literal uint16
//   SLEEP <ticks>
//   PRINT("<literal>" [+ <var>]*)     any number of "+ var" concatenations
//   FOR([<instr>; <instr>; ...], <repeatCount>)   nestable
//
// Per the MO2 spec, "screen -c" must contain 1-50 (top-level) instructions;
// parseUserInstructions() enforces that bound and reports "invalid command"
// (via ParseResult::ok == false) for anything that fails to parse or falls
// outside that range.
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include "Instructions.h"
 
namespace InstructionParser {
 
struct ParseResult {
    bool ok = false;
    std::vector<Instruction> instructions;
    std::string error; // set when ok == false, e.g. "invalid command"
};
 
// Parses `raw` (the contents between the quotes in screen -c's third
// argument) into a list of Instructions ready to hand to Scheduler /
// interpreter.cpp. See the grammar note above for supported forms.
ParseResult parseUserInstructions(const std::string& raw);

} // namespace InstructionParser