#include "storage/store/struct_column.h"

#include "common/cast.h"
#include "storage/store/struct_column_chunk.h"

using namespace kuzu::catalog;
using namespace kuzu::common;
using namespace kuzu::transaction;

namespace kuzu {
namespace storage {

StructColumn::StructColumn(std::unique_ptr<LogicalType> dataType,
    const MetadataDAHInfo& metaDAHeaderInfo, BMFileHandle* dataFH, BMFileHandle* metadataFH,
    BufferManager* bufferManager, WAL* wal, Transaction* transaction,
    RWPropertyStats propertyStatistics, bool enableCompression)
    : Column{std::move(dataType), metaDAHeaderInfo, dataFH, metadataFH, bufferManager, wal,
          transaction, propertyStatistics, enableCompression, true /* requireNullColumn */} {
    auto fieldTypes = StructType::getFieldTypes(this->dataType.get());
    KU_ASSERT(metaDAHeaderInfo.childrenInfos.size() == fieldTypes.size());
    childColumns.resize(fieldTypes.size());
    for (auto i = 0u; i < fieldTypes.size(); i++) {
        childColumns[i] = ColumnFactory::createColumn(fieldTypes[i]->copy(),
            *metaDAHeaderInfo.childrenInfos[i], dataFH, metadataFH, bufferManager, wal, transaction,
            propertyStatistics, enableCompression);
    }
}

void StructColumn::scan(
    Transaction* transaction, node_group_idx_t nodeGroupIdx, ColumnChunk* columnChunk) {
    KU_ASSERT(columnChunk->getDataType()->getPhysicalType() == PhysicalTypeID::STRUCT);
    nullColumn->scan(transaction, nodeGroupIdx, columnChunk->getNullChunk());
    if (nodeGroupIdx >= metadataDA->getNumElements(transaction->getType())) {
        columnChunk->setNumValues(0);
    } else {
        auto chunkMetadata = metadataDA->get(nodeGroupIdx, transaction->getType());
        columnChunk->setNumValues(chunkMetadata.numValues);
    }
    auto structColumnChunk = ku_dynamic_cast<ColumnChunk*, StructColumnChunk*>(columnChunk);
    for (auto i = 0u; i < childColumns.size(); i++) {
        childColumns[i]->scan(transaction, nodeGroupIdx, structColumnChunk->getChild(i));
    }
}

void StructColumn::scan(Transaction* transaction, node_group_idx_t nodeGroupIdx,
    offset_t startOffsetInGroup, offset_t endOffsetInGroup, ValueVector* resultVector,
    uint64_t offsetInVector) {
    nullColumn->scan(transaction, nodeGroupIdx, startOffsetInGroup, endOffsetInGroup, resultVector,
        offsetInVector);
    for (auto i = 0u; i < childColumns.size(); i++) {
        auto fieldVector = StructVector::getFieldVector(resultVector, i).get();
        childColumns[i]->scan(transaction, nodeGroupIdx, startOffsetInGroup, endOffsetInGroup,
            fieldVector, offsetInVector);
    }
}

void StructColumn::scanInternal(
    Transaction* transaction, ValueVector* nodeIDVector, ValueVector* resultVector) {
    for (auto i = 0u; i < childColumns.size(); i++) {
        auto fieldVector = StructVector::getFieldVector(resultVector, i).get();
        childColumns[i]->scan(transaction, nodeIDVector, fieldVector);
    }
}

void StructColumn::lookupInternal(
    Transaction* transaction, ValueVector* nodeIDVector, ValueVector* resultVector) {
    for (auto i = 0u; i < childColumns.size(); i++) {
        auto fieldVector = StructVector::getFieldVector(resultVector, i).get();
        childColumns[i]->lookup(transaction, nodeIDVector, fieldVector);
    }
}

void StructColumn::write(
    offset_t nodeOffset, ValueVector* vectorToWriteFrom, uint32_t posInVectorToWriteFrom) {
    KU_ASSERT(vectorToWriteFrom->dataType.getPhysicalType() == PhysicalTypeID::STRUCT);
    nullColumn->write(nodeOffset, vectorToWriteFrom, posInVectorToWriteFrom);
    KU_ASSERT(childColumns.size() == StructVector::getFieldVectors(vectorToWriteFrom).size());
    for (auto i = 0u; i < childColumns.size(); i++) {
        auto fieldVector = StructVector::getFieldVector(vectorToWriteFrom, i).get();
        childColumns[i]->write(nodeOffset, fieldVector, posInVectorToWriteFrom);
    }
}

void StructColumn::append(ColumnChunk* columnChunk, uint64_t nodeGroupIdx) {
    Column::append(columnChunk, nodeGroupIdx);
    KU_ASSERT(columnChunk->getDataType()->getPhysicalType() == PhysicalTypeID::STRUCT);
    auto structColumnChunk = static_cast<StructColumnChunk*>(columnChunk);
    for (auto i = 0u; i < childColumns.size(); i++) {
        childColumns[i]->append(structColumnChunk->getChild(i), nodeGroupIdx);
    }
}

void StructColumn::checkpointInMemory() {
    Column::checkpointInMemory();
    for (const auto& childColumn : childColumns) {
        childColumn->checkpointInMemory();
    }
}

void StructColumn::rollbackInMemory() {
    Column::rollbackInMemory();
    for (const auto& childColumn : childColumns) {
        childColumn->rollbackInMemory();
    }
}

} // namespace storage
} // namespace kuzu
