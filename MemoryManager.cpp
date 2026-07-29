// ============================================================================
// MemoryManager.cpp  -  Demand-Paging Allocator
// ============================================================================
// Implements frame allocation, FIFO page eviction, page fault handling, and
// context switching for the CSOPESY MO2 memory emulator.
//
// Integration:
//   - The scheduler calls allocateProcess() on process creation.
//   - The scheduler calls deallocateProcess() on process termination.
//   - On context switch: contextSwitch(oldProc, newProc) evicts old pages
//     to backing store, then loads new pages from backing store (or zero-fill).
//   - getUsedMemoryBytes() reports memory consumed for vmstat.
// ============================================================================

#include "MemoryManager.h"
#include <algorithm>
#include <iostream>

// ============================================================================
// Constructor
// ============================================================================
MemoryManager::MemoryManager(const Config& cfg, BackingStore& backingStore)
    : maxOverallMem_(cfg.maxOverallMem)
    , memPerFrame_(cfg.memPerFrame)
    , totalFrames_(static_cast<uint32_t>(cfg.maxOverallMem / cfg.memPerFrame))
    , backingStore_(backingStore)
    , pageFaultCount_(0)
{
    // Initialize the frame table with empty (unoccupied) entries.
    frameTable_.resize(totalFrames_);
}

// ============================================================================
// Process lifecycle
// ============================================================================

bool MemoryManager::allocateProcess(const std::string& processName, uint32_t numPages) {
    std::lock_guard<std::mutex> lk(mu_);

    // If the process already has a page table, ignore (idempotent).
    if (pageTables_.count(processName)) {
        return true;
    }

    // Create the page table for this process.
    ProcessPageTable pt;
    pt.totalPages = numPages;
    pageTables_[processName] = pt;

    // Eagerly allocate frames for all pages of the process.
    for (uint32_t page = 0; page < numPages; ++page) {
        int frame = findFreeFrame();
        if (frame < 0) {
            // No free frame — evict one via FIFO.
            frame = evictOne();
            if (frame < 0) {
                // Should never happen (eviction failed with no frames at all?).
                std::cerr << "[MemoryManager] FATAL: eviction failed for "
                          << processName << " page " << page << "\n";
                return false;
            }
        }

        // Place the page into the frame.
        loadPageIntoFrame(processName, page, frame);
    }

    return true;
}

void MemoryManager::deallocateProcess(const std::string& processName) {
    std::lock_guard<std::mutex> lk(mu_);

    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) return;

    // Free all frames occupied by this process.
    for (auto& [pageNum, frameIdx] : ptIt->second.pageToFrame) {
        if (frameIdx >= 0 && frameIdx < static_cast<int>(totalFrames_)) {
            frameTable_[frameIdx].occupied = false;
            frameTable_[frameIdx].processName.clear();
            frameTable_[frameIdx].pageNum = 0;

            // Remove from FIFO queue.
            fifoQueue_.erase(
                std::remove(fifoQueue_.begin(), fifoQueue_.end(), frameIdx),
                fifoQueue_.end());
        }
    }

    // Remove the page table entry entirely.
    pageTables_.erase(ptIt);

    // Also release any pages that were swapped out to backing store.
    backingStore_.releaseProcess(processName);
}

// ============================================================================
// Context switching
// ============================================================================

void MemoryManager::contextSwitch(const std::string& oldProcess, const std::string& newProcess) {
    std::lock_guard<std::mutex> lk(mu_);

    // --- Swap out: evict all resident pages of the old process to backing store ---
    if (!oldProcess.empty()) {
        auto ptIt = pageTables_.find(oldProcess);
        if (ptIt != pageTables_.end()) {
            // Collect pages to evict (can't modify map while iterating).
            std::vector<uint32_t> pagesToEvict;
            for (auto& [pageNum, frameIdx] : ptIt->second.pageToFrame) {
                if (frameIdx >= 0) {
                    pagesToEvict.push_back(pageNum);
                }
            }
            for (uint32_t pageNum : pagesToEvict) {
                evictPage(oldProcess, pageNum);
            }
        }
    }

    // --- Swap in: bring all pages of the new process into frames ---
    if (!newProcess.empty()) {
        auto ptIt = pageTables_.find(newProcess);
        if (ptIt != pageTables_.end()) {
            uint32_t totalPages = ptIt->second.totalPages;
            for (uint32_t page = 0; page < totalPages; ++page) {
                // Skip pages already resident.
                auto frameIt = ptIt->second.pageToFrame.find(page);
                if (frameIt != ptIt->second.pageToFrame.end() && frameIt->second >= 0) {
                    continue;
                }

                // Need a frame for this page.
                int frame = findFreeFrame();
                if (frame < 0) {
                    frame = evictOne();
                    if (frame < 0) {
                        std::cerr << "[MemoryManager] WARN: eviction failed during context switch for "
                                  << newProcess << " page " << page << "\n";
                        continue;
                    }
                }

                loadPageIntoFrame(newProcess, page, frame);
            }
        }
    }
}

// ============================================================================
// Page fault handling
// ============================================================================

int MemoryManager::handlePageFault(const std::string& processName, uint32_t pageNum) {
    std::lock_guard<std::mutex> lk(mu_);

    // Verify process exists in our page tables.
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) {
        return -1; // Unknown process.
    }

    // If already resident, just return the frame.
    auto frameIt = ptIt->second.pageToFrame.find(pageNum);
    if (frameIt != ptIt->second.pageToFrame.end() && frameIt->second >= 0) {
        return frameIt->second;
    }

    // Increment page fault counter.
    ++pageFaultCount_;

    // Find or evict a frame.
    int frame = findFreeFrame();
    if (frame < 0) {
        frame = evictOne();
        if (frame < 0) {
            return -1; // No memory available.
        }
    }

    // Load the page into the frame.
    loadPageIntoFrame(processName, pageNum, frame);

    return frame;
}

// ============================================================================
// Queries
// ============================================================================

uint64_t MemoryManager::getUsedMemoryBytes() const {
    std::lock_guard<std::mutex> lk(mu_);
    uint64_t usedFrames = 0;
    for (const auto& entry : frameTable_) {
        if (entry.occupied) ++usedFrames;
    }
    return usedFrames * memPerFrame_;
}

uint32_t MemoryManager::getFreeFrameCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    uint32_t freeCount = 0;
    for (const auto& entry : frameTable_) {
        if (!entry.occupied) ++freeCount;
    }
    return freeCount;
}

uint32_t MemoryManager::getTotalFrameCount() const {
    return totalFrames_;
}

bool MemoryManager::isPageResident(const std::string& processName, uint32_t pageNum) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) return false;

    auto frameIt = ptIt->second.pageToFrame.find(pageNum);
    return (frameIt != ptIt->second.pageToFrame.end() && frameIt->second >= 0);
}

int MemoryManager::getFrameOfPage(const std::string& processName, uint32_t pageNum) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) return -1;

    auto frameIt = ptIt->second.pageToFrame.find(pageNum);
    if (frameIt == ptIt->second.pageToFrame.end()) return -1;
    return frameIt->second;
}

uint64_t MemoryManager::getPageFaultCount() const {
    std::lock_guard<std::mutex> lk(mu_);
    return pageFaultCount_;
}

uint32_t MemoryManager::getResidentPageCount(const std::string& processName) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) return 0;

    uint32_t count = 0;
    for (const auto& [pageNum, frameIdx] : ptIt->second.pageToFrame) {
        if (frameIdx >= 0) ++count;
    }
    return count;
}

uint32_t MemoryManager::getTotalPageCount(const std::string& processName) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) return 0;
    return ptIt->second.totalPages;
}

// ============================================================================
// Internal operations
// ============================================================================

int MemoryManager::findFreeFrame() const {
    for (uint32_t i = 0; i < totalFrames_; ++i) {
        if (!frameTable_[i].occupied) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int MemoryManager::evictOne() {
    if (fifoQueue_.empty()) return -1;

    // FIFO: evict the oldest loaded frame (front of queue).
    int victimFrame = fifoQueue_.front();
    fifoQueue_.pop_front();

    FrameEntry& entry = frameTable_[victimFrame];
    if (!entry.occupied) {
        // Shouldn't happen, but be defensive.
        return victimFrame;
    }

    // Write the evicted page to backing store.
    // We store a dummy page of memPerFrame_ bytes (zeroed). In a real system
    // this would be the actual memory content; in this emulator, process
    // variables live in the Process struct, so we just store placeholder bytes.
    std::vector<uint8_t> pageData(memPerFrame_, 0);
    backingStore_.storePage(entry.processName, entry.pageNum, pageData);

    // Update the owner process's page table to mark this page as non-resident.
    auto ptIt = pageTables_.find(entry.processName);
    if (ptIt != pageTables_.end()) {
        ptIt->second.pageToFrame.erase(entry.pageNum);
    }

    // Clear the frame entry.
    entry.occupied = false;
    entry.processName.clear();
    entry.pageNum = 0;

    return victimFrame;
}

void MemoryManager::loadPageIntoFrame(const std::string& processName, uint32_t pageNum, int frameIdx) {
    // Try loading from backing store first (page was previously evicted).
    std::vector<uint8_t> pageData;
    backingStore_.loadPage(processName, pageNum, pageData);
    // If loadPage returns false (page not in backing store), it's a zero-fill
    // (fresh page). Either way, the page is now "in memory".

    // Update frame table.
    FrameEntry& entry = frameTable_[frameIdx];
    entry.occupied = true;
    entry.processName = processName;
    entry.pageNum = pageNum;

    // Update the process's page table.
    pageTables_[processName].pageToFrame[pageNum] = frameIdx;

    // Add to FIFO queue (newest at back).
    fifoQueue_.push_back(frameIdx);
}

void MemoryManager::evictPage(const std::string& processName, uint32_t pageNum) {
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end()) return;

    auto frameIt = ptIt->second.pageToFrame.find(pageNum);
    if (frameIt == ptIt->second.pageToFrame.end() || frameIt->second < 0) return;

    int frameIdx = frameIt->second;

    // Write page data to backing store.
    std::vector<uint8_t> pageData(memPerFrame_, 0);
    backingStore_.storePage(processName, pageNum, pageData);

    // Clear frame.
    FrameEntry& entry = frameTable_[frameIdx];
    entry.occupied = false;
    entry.processName.clear();
    entry.pageNum = 0;

    // Remove from FIFO queue.
    fifoQueue_.erase(
        std::remove(fifoQueue_.begin(), fifoQueue_.end(), frameIdx),
        fifoQueue_.end());

    // Remove from page table (mark non-resident).
    ptIt->second.pageToFrame.erase(frameIt);
}
