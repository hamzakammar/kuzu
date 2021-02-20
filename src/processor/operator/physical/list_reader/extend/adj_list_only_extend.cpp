#include "src/processor/include/operator/physical/list_reader/extend/adj_list_only_extend.h"

namespace graphflow {
namespace processor {

void AdjListOnlyExtend::getNextTuples() {
    if (handle->hasMoreToRead()) {
        readValuesFromList();
        return;
    }
    prevOperator->getNextTuples();
    if (inDataChunk->size > 0) {
        readValuesFromList();
    } else {
        outDataChunk->size = 0;
    }
}

} // namespace processor
} // namespace graphflow
