#include "src/processor/include/physical_plan/operator/update/set.h"

namespace graphflow {
namespace processor {

shared_ptr<ResultSet> SetNodeStructuredProperty::init(ExecutionContext* context) {
    resultSet = PhysicalOperator::init(context);
    for (auto& vectorPos : nodeIDVectorPositions) {
        auto dataChunk = resultSet->dataChunks[vectorPos.dataChunkPos];
        auto nodeIDVector = dataChunk->valueVectors[vectorPos.valueVectorPos];
        nodeIDVectors.push_back(nodeIDVector);
    }
    for (auto& expressionEvaluator : expressionEvaluators) {
        expressionEvaluator->init(*resultSet, context->memoryManager);
    }
    return resultSet;
}

bool SetNodeStructuredProperty::getNextTuples() {
    metrics->executionTime.start();
    if (!children[0]->getNextTuples()) {
        metrics->executionTime.stop();
        return false;
    }
    for (auto i = 0u; i < nodeIDVectors.size(); ++i) {
        auto nodeIDVector = nodeIDVectors[i];
        auto column = columns[i];
        auto expressionVector = expressionEvaluators[i]->resultVector;
        if (nodeIDVector->state->isFlat() && expressionVector->state->isFlat()) {
            // update flat flat
        } else if (!nodeIDVector->state->isFlat() && expressionVector->state->isFlat()) {
            // update unflat flat
        } else if (nodeIDVector->state->isFlat() && !expressionVector->state->isFlat()) {
            // update flat unflat
        } else { // both unflat
            // update unflat unflat
        }
    }
    metrics->executionTime.stop();
    return true;
}

unique_ptr<PhysicalOperator> SetNodeStructuredProperty::clone() {
    vector<unique_ptr<BaseExpressionEvaluator>> clonedExpressionEvaluators;
    for (auto& expressionEvaluator : expressionEvaluators) {
        clonedExpressionEvaluators.push_back(expressionEvaluator->clone());
    }
    return make_unique<SetNodeStructuredProperty>(
        nodeIDVectorPositions, columns, move(clonedExpressionEvaluators), children[0]->clone(), id);
}

} // namespace processor
} // namespace graphflow
