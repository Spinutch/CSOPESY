#include "BackingStore.h"
#include "mo1.h" // formatTimestamp()
#include <fstream>
#include <iomanip>
#include <sstream>

BackingStore::BackingStore(std::string filePath) : filePath_(std::move(filePath)) {
    // Start with a fresh, valid (empty) file so it's inspectable immediately,
    // even before any page has ever been swapped out.
    std::lock_guard<std::mutex> lk(mu_);
    dumpToFileLocked();
}

void BackingStore::storePage(const std::string& processName, uint32_t pageNum, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lk(mu_);
    pages_[{processName, pageNum}] = data;
    ++pagedOut_;
    dumpToFileLocked();
}

bool BackingStore::loadPage(const std::string& processName, uint32_t pageNum, std::vector<uint8_t>& outData) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = pages_.find({processName, pageNum});
    if (it == pages_.end()) return false;

    outData = it->second;
    pages_.erase(it);
    ++pagedIn_;
    dumpToFileLocked();
    return true;
}

bool BackingStore::hasPage(const std::string& processName, uint32_t pageNum) const {
    std::lock_guard<std::mutex> lk(mu_);
    return pages_.count({processName, pageNum}) > 0;
}

void BackingStore::releaseProcess(const std::string& processName) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = pages_.begin(); it != pages_.end(); ) {
        if (it->first.first == processName) it = pages_.erase(it);
        else ++it;
    }
    dumpToFileLocked();
}

BackingStore::Stats BackingStore::getStats() const {
    std::lock_guard<std::mutex> lk(mu_);
    Stats s;
    s.pagedIn = pagedIn_;
    s.pagedOut = pagedOut_;
    s.residentPagesOnDisk = pages_.size();
    return s;
}

void BackingStore::flush() const {
    std::lock_guard<std::mutex> lk(mu_);
    dumpToFileLocked();
}

void BackingStore::dumpToFileLocked() const {
    std::ofstream f(filePath_, std::ios::trunc);
    if (!f.is_open()) return; // best-effort; don't crash the emulator over disk I/O

    static const std::string DIV(75, '=');
    static const std::string DIV2(75, '-');

    f << DIV << "\n";
    f << "CSOPESY Backing Store\n";
    f << "Last updated   : " << formatTimestamp() << "\n";
    f << "Pages on disk  : " << pages_.size() << "\n";
    f << "Paged out (all): " << pagedOut_ << "\n";
    f << "Paged in  (all): " << pagedIn_ << "\n";
    f << DIV << "\n";

    if (pages_.empty()) {
        f << "(backing store is empty)\n";
    }

    for (const auto& [key, bytes] : pages_) {
        const auto& [procName, pageNum] = key;
        f << "Process: " << procName << " | Page: " << pageNum
          << " | Size: " << bytes.size() << " bytes\n";

        // Hex dump, 16 bytes per line.
        for (size_t off = 0; off < bytes.size(); off += 16) {
            f << "  Offset 0x" << std::hex << std::setw(4) << std::setfill('0') << off << std::dec << ": ";
            size_t lineEnd = std::min(bytes.size(), off + 16);
            for (size_t i = off; i < lineEnd; ++i) {
                f << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(bytes[i]) << std::dec << " ";
            }
            f << "\n";
        }
        f << DIV2 << "\n";
    }
}
