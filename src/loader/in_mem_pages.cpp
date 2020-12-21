#include "src/loader/include/in_mem_pages.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

namespace graphflow {
namespace loader {

void InMemPages::saveToFile() {
    if (0 == fname.length()) {
        throw invalid_argument("InMemPages: Empty filename");
    }
    uint32_t f = open(fname.c_str(), O_WRONLY | O_CREAT, 0666);
    if (-1u == f) {
        throw invalid_argument("InMemPages: Cannot create file: " + fname);
    }
    auto size = numPages * PAGE_SIZE;
    if (size != write(f, data.get(), size)) {
        throw invalid_argument("InMemPages: Cannot write in file.");
    }
    close(f);
}

void InMemAdjPages::set(const PageCursor& cursor, const nodeID_t& nbrNodeID) {
    auto writeOffset = data.get() + (PAGE_SIZE * cursor.idx) + cursor.offset;
    memcpy(writeOffset, &nbrNodeID.label, numBytesPerLabel);
    memcpy(writeOffset + numBytesPerLabel, &nbrNodeID.offset, numBytesPerOffset);
}

void InMemStringOverflowPages::set(
    const char* originalString, PageCursor& cursor, gf_string_t* encodedString) {
    encodedString->len = strlen(originalString);
    memcpy(&encodedString->prefix, originalString, min(encodedString->len, 4u));
    if (encodedString->len <= 4) {
        return;
    }
    if (encodedString->len > 4 && encodedString->len <= 12) {
        memcpy(&encodedString->overflowPtr, originalString + 4, encodedString->len - 4);
        return;
    }
    copyOverflowString(cursor, (uint8_t*)originalString, encodedString);
}

void InMemStringOverflowPages::copyOverflowString(
    PageCursor& cursor, uint8_t* ptrToCopy, gf_string_t* encodedString) {
    if (cursor.offset + encodedString->len - 4 >= PAGE_SIZE || 0 > cursor.idx) {
        cursor.offset = 0;
        cursor.idx = getNewOverflowPageIdx();
    }
    encodedString->setOverflowPtrFromPageCursor(cursor);
    auto writeOffset = data.get() + (cursor.idx * PAGE_SIZE) + cursor.offset;
    memcpy(writeOffset, ptrToCopy, encodedString->len);
    cursor.offset += encodedString->len;
}

uint32_t InMemStringOverflowPages::getNewOverflowPageIdx() {
    lock_guard lck(lock);
    auto page = numPages++;
    if (numPages != maxPages) {
        return page;
    }
    uint64_t newMaxPages = 1.5 * maxPages;
    auto newPages = new uint8_t[PAGE_SIZE * newMaxPages];
    memcpy(newPages, data.get(), PAGE_SIZE * maxPages);
    data.reset(newPages);
    maxPages = newMaxPages;
    return page;
}

} // namespace loader
} // namespace graphflow
