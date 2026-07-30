// ============================================================================
// InstructionParser.cpp  -  Danika: User-defined Instructions ("screen -c")
// ============================================================================
#include "InstructionParser.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
 
namespace InstructionParser {
namespace {
 
// The MO2 spec's own screen -c sample writes PRINT literals as
// PRINT(\"Result: \" + varC) -- i.e. backslash-escaped quotes, since the
// whole instruction string is itself wrapped in an outer pair of quotes on
// the command line. Un-escape \" -> " (and \\ -> \) up front so the rest of
// the parser can treat '"' as a normal, unambiguous string delimiter.
std::string unescapeQuotes(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && (s[i + 1] == '"' || s[i + 1] == '\\')) {
            out.push_back(s[i + 1]);
            ++i;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}
 
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
 
std::string toUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return r;
}
 
bool isNumericToken(const std::string& tok) {
    if (tok.empty()) return false;
    if (tok.size() > 2 && (tok[0] == '0') && (tok[1] == 'x' || tok[1] == 'X')) {
        for (size_t i = 2; i < tok.size(); ++i)
            if (!std::isxdigit(static_cast<unsigned char>(tok[i]))) return false;
        return true;
    }
    for (char c : tok)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}
 
uint32_t parseAddress(const std::string& tok) {
    // std::stoul with base 0 auto-detects a "0x" prefix vs plain decimal.
    return static_cast<uint32_t>(std::stoul(tok, nullptr, 0));
}
 
uint16_t clampToU16(long long v) {
    if (v < 0) return 0;
    if (v > 65535) return 65535;
    return static_cast<uint16_t>(v);
}
 
// Splits `s` on `delim` at "top level" only -- i.e. not inside a "..."
// quoted string, and not inside (), [], or {} nesting. Empty tokens (e.g.
// from a trailing delimiter) are dropped.
std::vector<std::string> splitTopLevel(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    bool inQuotes = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            cur.push_back(c);
            continue;
        }
        if (!inQuotes) {
            if (c == '(' || c == '[' || c == '{') { ++depth; cur.push_back(c); continue; }
            if (c == ')' || c == ']' || c == '}') { --depth; cur.push_back(c); continue; }
            if (c == delim && depth == 0) {
                std::string t = trim(cur);
                if (!t.empty()) out.push_back(t);
                cur.clear();
                continue;
            }
        }
        cur.push_back(c);
    }
    std::string t = trim(cur);
    if (!t.empty()) out.push_back(t);
    return out;
}
 
// Whitespace-splits a token into words (used for DECLARE/ADD/etc. which have
// no internal quoting/nesting to worry about).
std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string w;
    while (iss >> w) out.push_back(w);
    return out;
}
 
// Sets an operand (variable-or-literal) on the given dest fields, matching
// the convention already used by ADD/SUBTRACT/WRITE in Instructions.h.
void setOperand(const std::string& tok, bool& isVarOut, std::string& varOut, uint16_t& litOut) {
    if (isNumericToken(tok)) {
        isVarOut = false;
        litOut = clampToU16(static_cast<long long>(parseAddress(tok)));
    } else {
        isVarOut = true;
        varOut = tok;
    }
}
 
Instruction parseSingleInstruction(const std::string& rawToken); // fwd decl
 
// Parses the body of PRINT("literal" [+ var]*) into a msg string with
// embedded "{var}" placeholders -- the exact syntax interpreter.cpp's
// PRINT case already substitutes at execution time.
std::string parsePrintBody(const std::string& inner) {
    std::vector<std::string> parts = splitTopLevel(inner, '+');
    if (parts.empty())
        throw std::invalid_argument("empty PRINT body");
 
    std::string msg;
    for (auto& part : parts) {
        std::string p = trim(part);
        if (p.size() >= 2 && p.front() == '"' && p.back() == '"') {
            msg += p.substr(1, p.size() - 2); // literal text, quotes stripped
        } else if (!p.empty()) {
            msg += "{" + p + "}"; // variable reference
        }
    }
    return msg;
}
 
Instruction parseFor(const std::string& body) {
    // body is everything between "FOR(" and the matching ")".
    // Expected shape: "[ <instr>; <instr>; ... ], <repeatCount>"
    size_t openBracket = body.find('[');
    size_t closeBracket = std::string::npos;
    if (openBracket != std::string::npos) {
        int depth = 0;
        for (size_t i = openBracket; i < body.size(); ++i) {
            if (body[i] == '[') ++depth;
            else if (body[i] == ']') { --depth; if (depth == 0) { closeBracket = i; break; } }
        }
    }
    if (openBracket == std::string::npos || closeBracket == std::string::npos)
        throw std::invalid_argument("malformed FOR: missing [instructions]");
 
    std::string instrList = body.substr(openBracket + 1, closeBracket - openBracket - 1);
    std::string afterBracket = trim(body.substr(closeBracket + 1));
    if (afterBracket.empty() || afterBracket[0] != ',')
        throw std::invalid_argument("malformed FOR: missing repeat count");
    std::string repeatTok = trim(afterBracket.substr(1));
    if (!isNumericToken(repeatTok))
        throw std::invalid_argument("malformed FOR: repeat count must be numeric");
 
    Instruction ins;
    ins.type = InstructionType::FOR;
    ins.repeatCount = static_cast<uint32_t>(std::stoul(repeatTok, nullptr, 0));
    if (ins.repeatCount == 0)
        throw std::invalid_argument("malformed FOR: repeat count must be >= 1");
 
    for (const auto& sub : splitTopLevel(instrList, ';'))
        ins.forBody.push_back(parseSingleInstruction(sub));
 
    return ins;
}
 
Instruction parseSingleInstruction(const std::string& rawToken) {
    std::string tok = trim(rawToken);
    if (tok.empty())
        throw std::invalid_argument("empty instruction");
 
    // Keyword = leading run of non-space/non-'(' characters.
    size_t kwEnd = tok.find_first_of(" (");
    std::string keyword = toUpper(kwEnd == std::string::npos ? tok : tok.substr(0, kwEnd));
 
    if (keyword == "DECLARE") {
        auto w = splitWords(tok);
        if (w.size() != 3) throw std::invalid_argument("DECLARE needs <var> <value>");
        if (!isNumericToken(w[2])) throw std::invalid_argument("DECLARE value must be numeric");
        Instruction ins;
        ins.type = InstructionType::DECLARE;
        ins.varName = w[1];
        ins.varValue = clampToU16(static_cast<long long>(parseAddress(w[2])));
        return ins;
    }
    if (keyword == "ADD" || keyword == "SUBTRACT") {
        auto w = splitWords(tok);
        if (w.size() != 4) throw std::invalid_argument(keyword + " needs <dest> <src1> <src2>");
        Instruction ins;
        ins.type = (keyword == "ADD") ? InstructionType::ADD : InstructionType::SUBTRACT;
        ins.dest = w[1];
        setOperand(w[2], ins.src1IsVar, ins.src1, ins.src1Val);
        setOperand(w[3], ins.src2IsVar, ins.src2, ins.src2Val);
        return ins;
    }
    if (keyword == "READ") {
        auto w = splitWords(tok);
        if (w.size() != 3) throw std::invalid_argument("READ needs <var> <address>");
        Instruction ins;
        ins.type = InstructionType::READ;
        ins.varName = w[1];
        ins.memAddress = parseAddress(w[2]);
        return ins;
    }
    if (keyword == "WRITE") {
        auto w = splitWords(tok);
        if (w.size() != 3) throw std::invalid_argument("WRITE needs <address> <value>");
        Instruction ins;
        ins.type = InstructionType::WRITE;
        ins.memAddress = parseAddress(w[1]);
        setOperand(w[2], ins.src1IsVar, ins.src1, ins.src1Val);
        return ins;
    }
    if (keyword == "SLEEP") {
        auto w = splitWords(tok);
        if (w.size() != 2 || !isNumericToken(w[1]))
            throw std::invalid_argument("SLEEP needs <ticks>");
        Instruction ins;
        ins.type = InstructionType::SLEEP;
        unsigned long ticks = std::stoul(w[1], nullptr, 0);
        ins.sleepTicks = static_cast<uint8_t>(std::min<unsigned long>(ticks, 255));
        return ins;
    }
    if (keyword == "PRINT") {
        size_t open = tok.find('(');
        if (open == std::string::npos || tok.back() != ')')
            throw std::invalid_argument("PRINT needs (...)");
        std::string inner = tok.substr(open + 1, tok.size() - open - 2);
        Instruction ins;
        ins.type = InstructionType::PRINT;
        ins.msg = parsePrintBody(inner);
        return ins;
    }
    if (keyword == "FOR") {
        size_t open = tok.find('(');
        if (open == std::string::npos || tok.back() != ')')
            throw std::invalid_argument("FOR needs (...)");
        std::string body = tok.substr(open + 1, tok.size() - open - 2);
        return parseFor(body);
    }
 
    throw std::invalid_argument("unrecognized instruction: " + tok);
}
 
} // namespace
 
ParseResult parseUserInstructions(const std::string& raw) {
    ParseResult result;
 
    std::vector<std::string> tokens = splitTopLevel(unescapeQuotes(raw), ';');
 
    // MO2 spec: "screen -c" must contain 1-50 semicolon-separated
    // instructions; otherwise throw "invalid command".
    if (tokens.empty() || tokens.size() > 50) {
        result.ok = false;
        result.error = "invalid command";
        return result;
    }
 
    try {
        for (const auto& tok : tokens)
            result.instructions.push_back(parseSingleInstruction(tok));
    } catch (const std::exception&) {
        result.ok = false;
        result.error = "invalid command";
        result.instructions.clear();
        return result;
    }
 
    result.ok = true;
    return result;
}
 
} // namespace InstructionParser