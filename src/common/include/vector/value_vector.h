#pragma once

#include "src/common/include/data_chunk/data_chunk_state.h"
#include "src/common/include/expression_type.h"
#include "src/common/include/types.h"

namespace graphflow {
namespace common {

//! A Vector represents values of the same data type.
class ValueVector {

public:
    static function<void(ValueVector&, ValueVector&)> getUnaryOperation(ExpressionType type);

    static function<void(ValueVector&, ValueVector&, ValueVector&)> getBinaryOperation(
        ExpressionType type);

    ValueVector(DataType dataType, uint64_t vectorCapacity)
        : ValueVector(vectorCapacity, getDataTypeSize(dataType), dataType) {}

    ValueVector(uint64_t numBytesPerValue, DataType dataType)
        : ValueVector(MAX_VECTOR_SIZE, numBytesPerValue, dataType) {}

    ValueVector(DataType dataType) : ValueVector(dataType, MAX_VECTOR_SIZE) {}

    virtual void readNodeOffset(uint64_t pos, nodeID_t& nodeID) {
        throw invalid_argument("readNodeOffset unsupported.");
    }
    virtual void readNodeOffsetAndLabel(uint64_t pos, nodeID_t& nodeID) {
        throw invalid_argument("readNodeOffsetAndLabel unsupported.");
    }

    inline void reset() { values = buffer.get(); }

    void fillNullMask();

    virtual inline int64_t getNumBytesPerValue() { return getDataTypeSize(dataType); }

protected:
    ValueVector(uint64_t vectorCapacity, uint64_t numBytesPerValue, DataType dataType)
        : capacity{numBytesPerValue * vectorCapacity}, buffer{make_unique<uint8_t[]>(capacity)},
          nullMaskPtr{make_unique<bool[]>(capacity)}, dataType{dataType}, values{buffer.get()},
          nullMask{nullMaskPtr.get()} {
        fill_n(nullMask, capacity, false /* not null */);
    }

protected:
    size_t capacity;
    unique_ptr<uint8_t[]> buffer;
    unique_ptr<bool[]> nullMaskPtr;

public:
    DataType dataType;
    uint8_t* values;
    bool* nullMask;
    shared_ptr<DataChunkState> state;
};

} // namespace common
} // namespace graphflow
