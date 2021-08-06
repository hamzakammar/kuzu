#include "src/common/include/vector/value_vector.h"

#include "src/common/include/operations/comparison_operations.h"

using namespace graphflow::common::operation;

namespace graphflow {
namespace common {

shared_ptr<NullMask> NullMask::clone(uint64_t vectorCapacity) {
    auto newNullMask = make_shared<NullMask>(vectorCapacity);
    memcpy(newNullMask->mask.get(), mask.get(), vectorCapacity);
    return newNullMask;
}

template<class T>
static void fillOperandNullMask(ValueVector& operand) {
    auto values = (T*)operand.values;
    if (operand.state->isFlat()) {
        operand.setNull(operand.state->getPositionOfCurrIdx(),
            IsNull::operation(values[operand.state->getPositionOfCurrIdx()]));
    } else {
        auto size = operand.state->selectedSize;
        for (uint64_t i = 0; i < size; i++) {
            operand.setNull(i, IsNull::operation(values[operand.state->selectedPositions[i]]));
        }
    }
}

void ValueVector::fillNullMask() {
    switch (dataType) {
    case BOOL:
        fillOperandNullMask<uint8_t>(*this);
        break;
    case INT64:
        fillOperandNullMask<int64_t>(*this);
        break;
    case DOUBLE:
        fillOperandNullMask<double_t>(*this);
        break;
    case DATE:
        fillOperandNullMask<date_t>(*this);
        break;
    case STRING:
        // TODO: fillOperandNullMask<gf_string_t>(*this);
        //  Currently we do not distinguish empty and NULL gf_string_t.
        break;
    default:
        throw std::invalid_argument("Invalid or unsupported type for comparison: " +
                                    TypeUtils::dataTypeToString(dataType) + ".");
    }
}

// Notice that this clone function only copies values and mask without copying string buffers.
shared_ptr<ValueVector> ValueVector::clone() {
    auto newVector = make_shared<ValueVector>(memoryManager, dataType, vectorCapacity == 1);
    // Warning: This is a potential bug because sometimes nullMasks of ValueVectors point to
    // null masks of other ValueVectors, e.g., the result ValueVectors of unary expressions, point
    // to the nullMasks of operands. In which case these should not be copied over.
    newVector->nullMask = nullMask->clone(vectorCapacity);
    memcpy(newVector->values, values, vectorCapacity * getNumBytesPerValue());
    return newVector;
}

void ValueVector::allocateStringOverflowSpace(gf_string_t& result, uint64_t len) const {
    assert(dataType == STRING || dataType == UNSTRUCTURED);
    if (len > gf_string_t::SHORT_STR_LENGTH) {
        stringBuffer->allocateLargeString(result, len);
    }
}

void ValueVector::addString(uint64_t pos, char* value, uint64_t len) const {
    assert(dataType == STRING);
    auto vectorData = (gf_string_t*)values;
    auto& result = vectorData[pos];
    allocateStringOverflowSpace(result, len);
    result.set(value, len);
}

void ValueVector::addString(uint64_t pos, string value) const {
    addString(pos, value.data(), value.length());
}

} // namespace common
} // namespace graphflow
