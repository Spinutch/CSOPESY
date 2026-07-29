#pragma once
// ============================================================================
// MemoryManager.h  -  Rai: Demand-Paging Allocator
// ============================================================================
// Owns the frame table (physical memory) and per-process page tables.
// Implements:
//   - Frame allocation / deallocation
//   - Page fault handling (load from backing store or zero-fill)
//   - FIFO page eviction when no free frames available
//   - Context switching support (swap out old process, swap in new process)
//   - Used/free memory reporting for vmstat (wired into MemoryStats.h)
//
// Integration contract:
//   - Scheduler calls allocateProcess() when a process is created.
//   - Scheduler calls deallocateProcess() when a process finishes.
//   - On context switch: contextSwitch(oldProc, newProc) evicts old pages
//     and brings in new pages as needed.
//   - getUsedMemoryBytes() feeds into MemoryStats::buildMemoryStats().
//
// Eviction policy: FIFO (oldest-loaded frame evicted first).
// ============================================================================
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "BackingStore.h"
#include "Config.h"

class MemoryManager {
public:
    // Construct with system config and a reference to the shared backing store.
    MemoryManager(const Config& cfg, BackingStore& backingStore);

    // -----------------------------------------------------------------------
    // Process lifecycle
    // -----------------------------------------------------------------------

    // Allocates frames for a new process. Attempts to bring in all pages
    // immediately (eager allocation). If there aren't enough free frames,
    // evicts via FIFO. Returns true on success.
    bool allocateProcess(const std::string& processName, uint32_t numPages);

    // Releases all frames held by the process. Does NOT write to backing
    // store (per spec: process is done, its pages are discarded).
    // Also cleans up the page table and any backing-store remnants.
    void deallocateProcess(const std::string& processName);

    // -----------------------------------------------------------------------
    // Context switching
    // -----------------------------------------------------------------------

    // Evicts all resident pages of oldProcess to backing store, then brings
    // in all pages of newProcess (from backing store or zero-fill).
    // Either parameter can be empty ("" means no process on that side).
    void contextSwitch(const std::string& oldProcess, const std::string& newProcess);

    // -----------------------------------------------------------------------
    // Page fault handling (called when a process accesses a non-resident page)
    // -----------------------------------------------------------------------

    // Brings page `pageNum` of `processName` into a physical frame.
    // If no free frame exists, evicts the oldest frame (FIFO).
    // Returns the frame index the page was loaded into, or -1 on failure.
    int handlePageFault(const std::string& processName, uint32_t pageNum);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    // Returns the total bytes currently occupied by resident pages.
    uint64_t getUsedMemoryBytes() const;

    // Returns the number of free frames.
    uint32_t getFreeFrameCount() const;

    // Returns the total number of frames in the system.
    uint32_t getTotalFrameCount() const;

    // Returns true if the given page of a process is currently in memory.
    bool isPageResident(const std::string& processName, uint32_t pageNum) const;

    // Returns the frame index holding a process's page, or -1 if not resident.
    int getFrameOfPage(const std::string& processName, uint32_t pageNum) const;

    // Returns cumulative page fault count since construction.
    uint64_t getPageFaultCount() const;

    // Number of pages currently in memory for a given process.
    uint32_t getResidentPageCount(const std::string& processName) const;

    // Total number of pages a process was allocated (may not all be resident).
    uint32_t getTotalPageCount(const std::string& processName) const;

private:
    // -----------------------------------------------------------------------
    // Internal types
    // -----------------------------------------------------------------------

    // One entry in the frame table (one physical frame).
    struct FrameEntry {
        bool occupied = false;
        std::string processName;   // which process owns this frame
        uint32_t pageNum = 0;      // which page of that process
    };

    // Per-process page table: pageNum → frameIndex (-1 if not resident).
    struct ProcessPageTable {
        uint32_t totalPages = 0;
        std::unordered_map<uint32_t, int> pageToFrame; // only resident pages have entries
    };

    // -----------------------------------------------------------------------
    // Internal operations
    // -----------------------------------------------------------------------

    // Finds a free frame. Returns frame index, or -1 if none available.
    int findFreeFrame() const;

    // Evicts the oldest frame (FIFO order) to backing store.
    // Returns the freed frame index, or -1 if eviction failed.
    int evictOne();

    // Loads a page into the specified frame (zero-fill or from backing store).
    void loadPageIntoFrame(const std::string& processName, uint32_t pageNum, int frameIdx);

    // Evicts a specific page owned by a process from its frame to backing store.
    void evictPage(const std::string& processName, uint32_t pageNum);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex mu_;

    uint64_t maxOverallMem_;
    uint64_t memPerFrame_;
    uint32_t totalFrames_;

    // Frame table: index is the frame number.
    std::vector<FrameEntry> frameTable_;

    // FIFO eviction queue: front = oldest loaded frame index.
    std::deque<int> fifoQueue_;

    // Per-process page tables.
    std::unordered_map<std::string, ProcessPageTable> pageTables_;

    // Reference to the shared backing store.
    BackingStore& backingStore_;

    // Stats
    uint64_t pageFaultCount_ = 0;
};
