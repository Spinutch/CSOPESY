#include "MemoryUtils.h"
#include <sstream>

namespace MemoryUtils {

bool isPowerOfTwo(uint64_t v) {
    return v > 0 && (v & (v - 1)) == 0;
}

bool isValidMemorySize(uint64_t size) {
    if (size < kMinProcMem || size > kMaxProcMem) return false;
    return isPowerOfTwo(size);
}

std::string invalidMemoryMessage(uint64_t size) {
    std::ostringstream ss;
    ss << "invalid memory allocation: " << size
       << " (must be a power of 2 in [" << kMinProcMem << ", " << kMaxProcMem << "])";
    return ss.str();
}

uint32_t computeNumPages(uint64_t memSize, uint64_t memPerFrame) {
    if (memPerFrame == 0) return 0;
    return static_cast<uint32_t>((memSize + memPerFrame - 1) / memPerFrame);
}

} // namespace MemoryUtils
