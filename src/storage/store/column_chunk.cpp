#include "storage/store/column_chunk.h"

#include "common/serializer/deserializer.h"
#include "common/vector/value_vector.h"
#include "storage/storage_utils.h"
#include "storage/store/column.h"
#include "transaction/transaction.h"

using namespace kuzu::common;
using namespace kuzu::transaction;

namespace kuzu {
namespace storage {

ColumnChunk::ColumnChunk(const LogicalType& dataType, uint64_t capacity, bool enableCompression,
    ResidencyState residencyState)
    : enableCompression{enableCompression} {
    data = ColumnChunkFactory::createColumnChunkData(dataType.copy(), enableCompression, capacity,
        residencyState);
    KU_ASSERT(residencyState != ResidencyState::ON_DISK);
}

ColumnChunk::ColumnChunk(const LogicalType& dataType, bool enableCompression,
    ColumnChunkMetadata metadata)
    : enableCompression{enableCompression} {
    data = ColumnChunkFactory::createColumnChunkData(dataType.copy(), enableCompression, metadata,
        true);
}

ColumnChunk::ColumnChunk(bool enableCompression, std::unique_ptr<ColumnChunkData> data)
    : enableCompression{enableCompression}, data{std::move(data)} {}

void ColumnChunk::initializeScanState(ChunkState& state) const {
    data->initializeScanState(state);
}

// TODO(Guodong): Should remove `nodeID` here. We should only need to pass in a selVector.
void ColumnChunk::scan(const Transaction* transaction, const ChunkState& state, ValueVector& nodeID,
    ValueVector& output, offset_t offsetInChunk, length_t length) const {
    // Check if there is deletions or insertions. If so, update selVector based on transaction.
    switch (getResidencyState()) {
    case ResidencyState::IN_MEMORY: {
        data->scan(output, offsetInChunk, length);
    } break;
    case ResidencyState::ON_DISK: {
        state.column->scan(&DUMMY_TRANSACTION, state, offsetInChunk, length, &nodeID, &output);
    }
    }
    if (updateInfo) {
        auto [startVectorIdx, startOffsetInVector] =
            StorageUtils::getQuotientRemainder(offsetInChunk, DEFAULT_VECTOR_CAPACITY);
        auto [endVectorIdx, endOffsetInVector] =
            StorageUtils::getQuotientRemainder(offsetInChunk + length, DEFAULT_VECTOR_CAPACITY);
        idx_t idx = startVectorIdx;
        sel_t posInVector = 0u;
        while (idx <= endVectorIdx) {
            const auto startOffset = idx == startVectorIdx ? startOffsetInVector : 0;
            const auto endOffset =
                idx == endVectorIdx ? endOffsetInVector : DEFAULT_VECTOR_CAPACITY;
            const auto numRowsInVector = endOffset - startOffset;
            if (const auto vectorInfo = updateInfo->getVectorInfo(transaction, idx);
                vectorInfo && vectorInfo->numRowsUpdated > 0) {
                for (auto i = 0u; i < numRowsInVector; i++) {
                    if (auto itr = std::find_if(vectorInfo->rowsInVector.begin(),
                            vectorInfo->rowsInVector.begin() + vectorInfo->numRowsUpdated,
                            [i, startOffset](auto row) { return row == i + startOffset; });
                        itr != vectorInfo->rowsInVector.begin() + vectorInfo->numRowsUpdated) {
                        vectorInfo->data->lookup(itr - vectorInfo->rowsInVector.begin(), output,
                            posInVector + i);
                    }
                }
            }
            posInVector += numRowsInVector;
            idx++;
        }
    }
}

template<ResidencyState SCAN_RESIDENCY_STATE>
void ColumnChunk::scanCommitted(Transaction* transaction, ChunkState& chunkState,
    ColumnChunk& output, row_idx_t startRow, row_idx_t numRows) const {
    if (numRows == INVALID_ROW_IDX) {
        numRows = getNumValues();
    }
    const auto numValuesBeforeScan = output.getNumValues();
    switch (const auto residencyState = getResidencyState()) {
    case ResidencyState::ON_DISK: {
        if (SCAN_RESIDENCY_STATE == residencyState) {
            chunkState.column->scan(transaction, chunkState, &output.getData(), startRow, numRows);
            scanCommittedUpdates(transaction, output.getData(), numValuesBeforeScan);
        }
    } break;
    case ResidencyState::IN_MEMORY: {
        if (SCAN_RESIDENCY_STATE == residencyState) {
            output.getData().append(data.get(), startRow, numRows);
            scanCommittedUpdates(transaction, output.getData(), numValuesBeforeScan);
        }
    } break;
    default: {
        KU_UNREACHABLE;
    }
    }
}

template void ColumnChunk::scanCommitted<ResidencyState::ON_DISK>(Transaction* transaction,
    ChunkState& chunkState, ColumnChunk& output, row_idx_t startRow, row_idx_t numRows) const;
template void ColumnChunk::scanCommitted<ResidencyState::IN_MEMORY>(Transaction* transaction,
    ChunkState& chunkState, ColumnChunk& output, row_idx_t startRow, row_idx_t numRows) const;

bool ColumnChunk::hasUpdates(const Transaction* transaction, row_idx_t startRow,
    length_t numRows) const {
    return updateInfo && updateInfo->hasUpdates(transaction, startRow, numRows);
}

void ColumnChunk::scanCommittedUpdates(Transaction* transaction, ColumnChunkData& output,
    offset_t startOffsetInOutput) const {
    if (!updateInfo) {
        return;
    }
    const auto numVectors =
        (getNumValues() + DEFAULT_VECTOR_CAPACITY - 1) / DEFAULT_VECTOR_CAPACITY;
    for (auto vectorIdx = 0u; vectorIdx < numVectors; vectorIdx++) {
        if (const auto vectorInfo = updateInfo->getVectorInfo(transaction, vectorIdx)) {
            for (auto i = 0u; i < vectorInfo->numRowsUpdated; i++) {
                output.write(vectorInfo->data.get(), i,
                    startOffsetInOutput + vectorIdx * DEFAULT_VECTOR_CAPACITY +
                        vectorInfo->rowsInVector[i],
                    1);
            }
        }
    }
}

void ColumnChunk::lookup(Transaction* transaction, ChunkState& state, offset_t rowInChunk,
    ValueVector& output, sel_t posInOutputVector) const {
    switch (getResidencyState()) {
    case ResidencyState::IN_MEMORY: {
        data->lookup(rowInChunk, output, posInOutputVector);
    } break;
    case ResidencyState::ON_DISK: {
        state.column->lookupValue(transaction, state, rowInChunk, &output, posInOutputVector);
    }
    }
    if (updateInfo) {
        auto [vectorIdx, rowInVector] =
            StorageUtils::getQuotientRemainder(rowInChunk, DEFAULT_VECTOR_CAPACITY);
        if (auto vectorInfo = updateInfo->getVectorInfo(transaction, vectorIdx)) {
            for (auto i = 0u; i < vectorInfo->numRowsUpdated; i++) {
                if (vectorInfo->rowsInVector[i] == rowInVector) {
                    vectorInfo->data->lookup(i, output, posInOutputVector);
                    return;
                }
            }
        }
    }
}

void ColumnChunk::update(Transaction* transaction, offset_t offsetInChunk,
    const ValueVector& values) {
    if (transaction->getID() == Transaction::DUMMY_TRANSACTION_ID) {
        data->write(&values, values.state->getSelVector().getSelectedPositions()[0], offsetInChunk);
        return;
    }
    if (!updateInfo) {
        updateInfo = std::make_unique<UpdateInfo>();
    }
    const auto vectorIdx = offsetInChunk / DEFAULT_VECTOR_CAPACITY;
    const auto rowIdxInVector = offsetInChunk % DEFAULT_VECTOR_CAPACITY;
    const auto vectorUpdateInfo =
        updateInfo->update(transaction, vectorIdx, rowIdxInVector, values);
    transaction->pushVectorUpdateInfo(*updateInfo, vectorIdx, *vectorUpdateInfo);
}

void ColumnChunk::serialize(Serializer& serializer) const {
    serializer.writeDebuggingInfo("enable_compression");
    serializer.write<bool>(enableCompression);
    data->serialize(serializer);
}

std::unique_ptr<ColumnChunk> ColumnChunk::deserialize(Deserializer& deSer) {
    std::string key;
    bool enableCompression;
    deSer.validateDebuggingInfo(key, "enable_compression");
    deSer.deserializeValue<bool>(enableCompression);
    auto data = ColumnChunkData::deserialize(deSer);
    return std::make_unique<ColumnChunk>(enableCompression, std::move(data));
}

row_idx_t ColumnChunk::getNumUpdatedRows(const Transaction* transaction) const {
    return updateInfo ? updateInfo->getNumUpdatedRows(transaction) : 0;
}

std::pair<std::unique_ptr<ColumnChunk>, std::unique_ptr<ColumnChunk>> ColumnChunk::scanUpdates(
    Transaction* transaction) const {
    auto numUpdatedRows = getNumUpdatedRows(transaction);
    // TODO(Guodong): Actually for row idx in a column chunk, UINT32 should be enough.
    auto updatedRows = std::make_unique<ColumnChunk>(LogicalType::UINT64(), numUpdatedRows, false,
        ResidencyState::IN_MEMORY);
    auto updatedData = std::make_unique<ColumnChunk>(getDataType(), numUpdatedRows, false,
        ResidencyState::IN_MEMORY);
    const auto numUpdateVectors = updateInfo->getNumVectors();
    row_idx_t numAppendedRows = 0;
    for (auto vectorIdx = 0u; vectorIdx < numUpdateVectors; vectorIdx++) {
        const auto vectorInfo = updateInfo->getVectorInfo(transaction, vectorIdx);
        if (!vectorInfo) {
            continue;
        }
        const row_idx_t startRowIdx = vectorIdx * DEFAULT_VECTOR_CAPACITY;
        for (auto rowIdx = 0u; rowIdx < vectorInfo->numRowsUpdated; rowIdx++) {
            updatedRows->getData().setValue<row_idx_t>(
                vectorInfo->rowsInVector[rowIdx] + startRowIdx, numAppendedRows++);
        }
        updatedData->getData().append(vectorInfo->data.get(), 0, vectorInfo->numRowsUpdated);
        KU_ASSERT(updatedData->getData().getNumValues() == updatedRows->getData().getNumValues());
    }
    return {std::move(updatedRows), std::move(updatedData)};
}

} // namespace storage
} // namespace kuzu
