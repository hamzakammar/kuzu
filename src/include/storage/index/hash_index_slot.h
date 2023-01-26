#pragma once

#include <cstdint>

#include "common/configs.h"
#include "common/types/internal_id_t.h"
#include "common/types/ku_string.h"

namespace kuzu {
namespace storage {

using entry_pos_t = uint8_t;
using slot_id_t = uint64_t;

class SlotHeader {
public:
    static const entry_pos_t INVALID_ENTRY_POS = UINT8_MAX;

    SlotHeader() : numEntries{0}, validityMask{0}, nextOvfSlotId{0} {}

    void reset() {
        numEntries = 0;
        validityMask = 0;
        nextOvfSlotId = 0;
    }

    inline bool isEntryValid(uint32_t entryPos) const {
        return validityMask & ((uint32_t)1 << entryPos);
    }
    inline void setEntryValid(entry_pos_t entryPos) { validityMask |= ((uint32_t)1 << entryPos); }
    inline void setEntryInvalid(entry_pos_t entryPos) {
        validityMask &= ~((uint32_t)1 << entryPos);
    }

public:
    entry_pos_t numEntries;
    uint32_t validityMask;
    slot_id_t nextOvfSlotId;
};

template<typename T>
struct SlotEntry {
    uint8_t data[sizeof(T) + sizeof(common::offset_t)];
};

template<typename T>
struct Slot {
    SlotHeader header;
    SlotEntry<T> entries[common::HashIndexConfig::SLOT_CAPACITY];
};

} // namespace storage
} // namespace kuzu
