#pragma once
// ============================================================================
// MemoryUtils.h  -  Heather: Required Memory per Process
// ============================================================================
// Validation + sizing helpers for the "screen -s <name> <mem_size>" command.
// Per the MO2 spec: all per-process memory sizes must be a power of 2 within
// [2^6, 2^16] bytes (i.e. [64, 65536]). Anything outside that range, or not a
// power of 2, is an "invalid memory allocation".
// ============================================================================
#include <cstdint>
#include <string>

namespace MemoryUtils {

constexpr uint64_t kMinProcMem = 64;      // 2^6
constexpr uint64_t kMaxProcMem = 65536;   // 2^16

// Returns true iff v is a power of two (v > 0 and only one bit set).
bool isPowerOfTwo(uint64_t v);

// Returns true iff size is a valid per-process memory allocation:
// a power of 2 within [kMinProcMem, kMaxProcMem].
bool isValidMemorySize(uint64_t size);

// Human-readable error the console should print for an invalid allocation,
// e.g. "invalid memory allocation: 300 (must be a power of 2 in [64, 65536])".
std::string invalidMemoryMessage(uint64_t size);

// Number of pages a process of the given memory size needs, given the
// configured frame/page size (mem-per-frame). Rounds up (ceiling division)
// so a partially-filled final page still counts as one page.
uint32_t computeNumPages(uint64_t memSize, uint64_t memPerFrame);

} // namespace MemoryUtils
