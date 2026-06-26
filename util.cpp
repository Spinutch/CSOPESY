// ============================================================================
// util.cpp  –  Member 1: Shared Utilities
// ============================================================================
#include "mo1.h"
#include <ctime>
#include <sstream>
#include <iomanip>

// Returns wall-clock time as "MM/DD/YYYY HH:MM:SSam/pm"
// Example: "06/27/2026 02:45:30PM"
std::string formatTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm*    tm  = std::localtime(&now);

    char buf[32];
    // %I = 12-hr hour, %p = AM/PM (locale-dependent but standard on Linux/macOS)
    std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", tm);
    return std::string(buf);
}
