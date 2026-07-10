// ============================================================================
// MemoryManager.cpp  –  Member 3: First-Fit Flat Memory Allocator
// ============================================================================
// NOTE FOR M3: This is a working first draft wired up by M4 so the full
// project builds end-to-end while you refine the allocator. Please review:
//   - block splitting policy on partial allocation
//   - coalescing correctness after deallocate()
//   - exact snapshot text formatting vs. the assignment mockup
// ============================================================================

#include "MemoryManager.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

void MemoryManager::configure(uint64_t maxOverallMem, uint64_t memPerFrame, uint64_t memPerProc) {
    std::lock_guard<std::mutex> lk(mu_);
    maxOverallMem_ = maxOverallMem;
    memPerFrame_ = memPerFrame;
    memPerProc_ = memPerProc;

    blocks_.clear();
    // Start as one big free block covering all of main memory.
    blocks_.push_back(Block{0, maxOverallMem_, true, -1, ""});
}

bool MemoryManager::allocate(int pid, const std::string& procName) {
    std::lock_guard<std::mutex> lk(mu_);

    // First-fit: scan blocks in address order, take the first free block
    // large enough to hold memPerProc_ bytes.
    for (size_t i = 0; i < blocks_.size(); ++i) {
        Block& b = blocks_[i];
        if (!b.free || b.size < memPerProc_) continue;

        if (b.size == memPerProc_) {
            // Exact fit: just occupy it.
            b.free = false;
            b.pid = pid;
            b.procName = procName;
        } else {
            // Split: carve memPerProc_ bytes off the front of this free
            // block, leave the remainder free.
            Block occupied{b.start, memPerProc_, false, pid, procName};
            Block remainder{b.start + memPerProc_, b.size - memPerProc_, true, -1, ""};
            blocks_[i] = occupied;
            blocks_.insert(blocks_.begin() + i + 1, remainder);
        }
        return true;
    }
    return false; // No first-fit block available — memory is full.
}

bool MemoryManager::deallocate(int pid) {
    std::lock_guard<std::mutex> lk(mu_);

    auto it = std::find_if(blocks_.begin(), blocks_.end(),
                            [pid](const Block& b) { return !b.free && b.pid == pid; });
    if (it == blocks_.end()) return false;

    it->free = true;
    it->pid = -1;
    it->procName.clear();

    // Coalesce with adjacent free blocks (blocks_ stays sorted by start,
    // so we only need to check immediate neighbors).
    for (size_t i = 0; i + 1 < blocks_.size(); ) {
        if (blocks_[i].free && blocks_[i + 1].free) {
            blocks_[i].size += blocks_[i + 1].size;
            blocks_.erase(blocks_.begin() + i + 1);
            // Re-check the same index in case of a 3-way merge.
        } else {
            ++i;
        }
    }
    return true;
}

int MemoryManager::getProcessCountInMemory() const {
    std::lock_guard<std::mutex> lk(mu_);
    int count = 0;
    for (const auto& b : blocks_) if (!b.free) ++count;
    return count;
}

uint64_t MemoryManager::getExternalFragmentationBytes() const {
    std::lock_guard<std::mutex> lk(mu_);
    uint64_t total = 0;
    for (const auto& b : blocks_) if (b.free) total += b.size;
    return total;
}

void MemoryManager::writeSnapshot(uint64_t quantumCycle) const {
    std::vector<Block> snapshot;
    uint64_t maxMem;
    {
        std::lock_guard<std::mutex> lk(mu_);
        snapshot = blocks_;
        maxMem = maxOverallMem_;
    }

    // Timestamp in the same "MM/DD/YYYY HH:MM:SSAM/PM" format used elsewhere
    // in the project (see util.cpp's formatTimestamp — kept independent here
    // so MemoryManager has no dependency on M1's module).
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char tsBuf[32];
    std::strftime(tsBuf, sizeof(tsBuf), "%m/%d/%Y %I:%M:%S%p", tm);

    int procCount = 0;
    uint64_t fragBytes = 0;
    for (const auto& b : snapshot) {
        if (!b.free) ++procCount;
        else fragBytes += b.size;
    }

    std::ostringstream fname;
    fname << "memory_stamp_" << std::setfill('0') << std::setw(2) << quantumCycle << ".txt";

    std::ofstream out(fname.str(), std::ios::trunc);
    if (!out.is_open()) return;

    out << "Timestamp: (" << tsBuf << ")\n";
    out << "Number of processes in memory: " << procCount << "\n";
    // NOTE: the assignment's mockup labels this "in KB" but the sample value
    // (8192) is the raw fragmentation byte count with NO /1024 conversion —
    // confirmed against the provided screenshot. Print fragBytes as-is.
    out << "Total external fragmentation in KB: " << fragBytes << "\n\n";

    out << "----end---- = " << maxMem << "\n\n";

    // Sort occupied blocks top-to-bottom (descending start address) to
    // match the mockup's layout.
    std::vector<const Block*> occupied;
    for (const auto& b : snapshot) if (!b.free) occupied.push_back(&b);
    std::sort(occupied.begin(), occupied.end(),
              [](const Block* a, const Block* b) { return a->start > b->start; });

    for (const auto* b : occupied) {
        out << (b->start + b->size) << "\n";
        out << b->procName << "\n";
        out << b->start << "\n\n";
    }

    out << "----start----- = 0\n";
    out.close();
}