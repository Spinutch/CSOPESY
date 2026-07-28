#pragma once
// ============================================================================
// BackingStore.h  -  Heather: Memory Visualization & Backing Store Access
// ============================================================================
// Owns the on-disk representation of the demand-paging backing store.
//
// Rai's Memory Manager decides WHEN a page must be evicted (no free frames)
// or brought back in (page fault); this module owns HOW that page is
// persisted -- a thread-safe in-memory map, kept in sync with a
// human-readable "csopesy-backing-store.txt" file that can be opened and
// inspected at any time (write-through: the file is rewritten after every
// mutating call, so it never goes stale).
//
// Integration contract for Rai's Memory Manager:
//   - On eviction:      backingStore.storePage(procName, pageNum, bytes);
//   - On page fault-in: backingStore.loadPage(procName, pageNum, outBytes);
//   - On process exit:  backingStore.releaseProcess(procName);
//
// getStats() exposes pagedIn/pagedOut counters, which feed directly into the
// "Num paged in" / "Num paged out" rows of the vmstat command (built by
// Justine's CLI).
// ============================================================================
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class BackingStore {
public:
    explicit BackingStore(std::string filePath = "csopesy-backing-store.txt");

    // Writes (evicts) a page's raw bytes to the backing store, overwriting
    // any prior copy of the same (process, page) entry. Increments pagedOut.
    void storePage(const std::string& processName, uint32_t pageNum, const std::vector<uint8_t>& data);

    // Loads a page's bytes back out of the backing store into outData.
    // Returns true and removes the entry on success (the page now lives back
    // in a physical frame); returns false (outData untouched) if that page
    // was never swapped out. Increments pagedIn on success.
    bool loadPage(const std::string& processName, uint32_t pageNum, std::vector<uint8_t>& outData);

    bool hasPage(const std::string& processName, uint32_t pageNum) const;

    // Drops every page belonging to a process. Per spec, a process's
    // variables/pages are only released once the process finishes.
    void releaseProcess(const std::string& processName);

    struct Stats {
        uint64_t pagedIn = 0;
        uint64_t pagedOut = 0;
        size_t residentPagesOnDisk = 0;
    };
    Stats getStats() const;

    // Force a re-dump of the text file (rarely needed; every mutator already
    // does this, but exposed for e.g. an explicit "show backing store" command).
    void flush() const;

private:
    using PageKey = std::pair<std::string, uint32_t>; // (processName, pageNum)

    void dumpToFileLocked() const; // caller must hold mu_

    mutable std::mutex mu_;
    std::string filePath_;
    std::map<PageKey, std::vector<uint8_t>> pages_;
    uint64_t pagedIn_ = 0;
    uint64_t pagedOut_ = 0;
};
