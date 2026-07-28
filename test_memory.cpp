// ============================================================================
// test_memory.cpp  -   standalone tests for MO2 modules
// ============================================================================
// Exercises MemoryUtils + BackingStore in isolation, since Rai's Memory
// Manager (frame table / page fault handling) doesn't exist yet and can't be
// integration-tested. Not part of the production build; compile/run with:
//
//   g++ -std=c++17 test_memory.cpp MemoryUtils.cpp BackingStore.cpp \
//       util.cpp -o test_memory
//   ./test_memory
//
// Exits non-zero (and prints which check failed) if anything regresses.
// ============================================================================
#include "MemoryUtils.h"
#include "BackingStore.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL (%s:%d): %s\n", __FILE__, __LINE__, #cond);  \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_memory_utils() {
    std::printf("[test_memory_utils]\n");

    // Power-of-two boundary checks
    CHECK(MemoryUtils::isPowerOfTwo(64));
    CHECK(MemoryUtils::isPowerOfTwo(65536));
    CHECK(MemoryUtils::isPowerOfTwo(256));
    CHECK(!MemoryUtils::isPowerOfTwo(0));
    CHECK(!MemoryUtils::isPowerOfTwo(300));
    CHECK(!MemoryUtils::isPowerOfTwo(65535));

    // Valid range per spec: power of 2 in [64, 65536]
    CHECK(MemoryUtils::isValidMemorySize(64));      // min boundary
    CHECK(MemoryUtils::isValidMemorySize(65536));   // max boundary
    CHECK(MemoryUtils::isValidMemorySize(256));     // sample usage from spec
    CHECK(MemoryUtils::isValidMemorySize(4096));

    // Invalid: below min, above max, not power of 2
    CHECK(!MemoryUtils::isValidMemorySize(32));
    CHECK(!MemoryUtils::isValidMemorySize(131072));
    CHECK(!MemoryUtils::isValidMemorySize(300));
    CHECK(!MemoryUtils::isValidMemorySize(0));

    // Error message should mention the offending value
    std::string msg = MemoryUtils::invalidMemoryMessage(300);
    CHECK(msg.find("300") != std::string::npos);
    CHECK(msg.find("invalid memory allocation") != std::string::npos);

    // Page count: ceil(memSize / memPerFrame)
    CHECK(MemoryUtils::computeNumPages(4096, 16) == 256);
    CHECK(MemoryUtils::computeNumPages(64, 16) == 4);
    CHECK(MemoryUtils::computeNumPages(65, 16) == 5);   // rounds up
    CHECK(MemoryUtils::computeNumPages(0, 16) == 0);
    CHECK(MemoryUtils::computeNumPages(64, 0) == 0);    // guard div-by-zero
}

static void test_backing_store() {
    std::printf("[test_backing_store]\n");

    const std::string testFile = "test_backing_store_output.txt";
    BackingStore bs(testFile);

    auto initialStats = bs.getStats();
    CHECK(initialStats.pagedIn == 0);
    CHECK(initialStats.pagedOut == 0);
    CHECK(initialStats.residentPagesOnDisk == 0);
    CHECK(!bs.hasPage("p01", 0));

    // Store a page (simulates eviction)
    std::vector<uint8_t> page0 = {0x2A, 0x00, 0x0F, 0x00};
    bs.storePage("p01", 0, page0);
    CHECK(bs.hasPage("p01", 0));
    CHECK(bs.getStats().pagedOut == 1);
    CHECK(bs.getStats().residentPagesOnDisk == 1);

    // Store a second page, different process
    std::vector<uint8_t> page1 = {0x01, 0x02};
    bs.storePage("p02", 3, page1);
    CHECK(bs.getStats().residentPagesOnDisk == 2);

    // Load it back (simulates page fault handling bringing it into a frame)
    std::vector<uint8_t> loaded;
    bool ok = bs.loadPage("p01", 0, loaded);
    CHECK(ok);
    CHECK(loaded == page0);
    CHECK(!bs.hasPage("p01", 0)); // removed from backing store once resident again
    CHECK(bs.getStats().pagedIn == 1);
    CHECK(bs.getStats().residentPagesOnDisk == 1);

    // Loading a page that was never stored should fail cleanly
    std::vector<uint8_t> missing;
    CHECK(!bs.loadPage("p99", 5, missing));
    CHECK(bs.getStats().pagedIn == 1); // unchanged

    // releaseProcess should drop all of that process's pages
    bs.storePage("p02", 4, page0);
    CHECK(bs.getStats().residentPagesOnDisk == 2); // p02 pages 3 and 4
    bs.releaseProcess("p02");
    CHECK(!bs.hasPage("p02", 3));
    CHECK(!bs.hasPage("p02", 4));
    CHECK(bs.getStats().residentPagesOnDisk == 0);

    // The text file must exist and be non-empty (write-through worked)
    std::FILE* f = std::fopen(testFile.c_str(), "r");
    CHECK(f != nullptr);
    if (f) {
        std::fseek(f, 0, SEEK_END);
        long size = std::ftell(f);
        CHECK(size > 0);
        std::fclose(f);
    }
    std::remove(testFile.c_str());
}

int main() {
    test_memory_utils();
    test_backing_store();

    if (failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::printf("\n%d check(s) FAILED.\n", failures);
    return 1;
}
