#include "src/planner/include/binder.h"

namespace graphflow {
namespace planner {

static vector<shared_ptr<LogicalExpression>> splitExpressionOnAND(
    shared_ptr<LogicalExpression> expr);

static void validateQueryNodeWithSameName(QueryNode& queryNodeInGraph, QueryNode& queryNodeToMerge);

static void rebindSrcAndDstNode(QueryRel& queryRel, const QueryGraph& queryGraph);

static void mergeQueryGraphs(QueryGraph& mergedQueryGraph, QueryGraph& otherQueryGraph);

unique_ptr<BoundSingleQuery> Binder::bindSingleQuery(const SingleQuery& singleQuery) {
    auto mergedQueryGraph = make_unique<QueryGraph>();
    vector<shared_ptr<LogicalExpression>> whereExprsSplitOnAND;
    for (auto& statement : singleQuery.statements) {
        auto boundStatement = bindStatement(*statement);
        mergeQueryGraphs(*mergedQueryGraph, *boundStatement->queryGraph);
        if (boundStatement->whereExpression) {
            auto exprs = splitExpressionOnAND(boundStatement->whereExpression);
            for (auto& expr : exprs) {
                whereExprsSplitOnAND.push_back(expr);
            }
        }
    }
    return make_unique<BoundSingleQuery>(
        move(mergedQueryGraph), move(whereExprsSplitOnAND), make_unique<BoundReturnStatement>());
}

unique_ptr<BoundMatchStatement> Binder::bindStatement(const MatchStatement& matchStatement) {
    auto queryGraph = make_unique<QueryGraph>();
    for (auto& patternElement : matchStatement.graphPattern) {
        bindQueryRels(*patternElement, *queryGraph);
    }
    if (!queryGraph->isConnected()) {
        throw invalid_argument("Disconnected query graph is not yet supported.");
    }
    if (matchStatement.whereClause) {
        auto expressionBinder = make_unique<ExpressionBinder>(*queryGraph, catalog);
        return make_unique<BoundMatchStatement>(
            move(queryGraph), expressionBinder->bindExpression(*matchStatement.whereClause));
    }
    return make_unique<BoundMatchStatement>(move(queryGraph));
}

void Binder::bindQueryRels(const PatternElement& patternElement, QueryGraph& queryGraph) {
    QueryNode* leftNode = bindQueryNode(*patternElement.nodePattern, queryGraph);
    for (auto& patternElementChain : patternElement.patternElementChains) {
        auto rel = bindQueryRel(*patternElementChain, leftNode, queryGraph);
        leftNode = ArrowDirection::RIGHT == patternElementChain->relPattern->arrowDirection ?
                       rel->dstNode :
                       rel->srcNode;
    }
}

QueryRel* Binder::bindQueryRel(
    const PatternElementChain& patternElementChain, QueryNode* leftNode, QueryGraph& queryGraph) {
    auto parsedName = patternElementChain.relPattern->name;
    if (parsedName.empty()) {
        parsedName = "_gfR_" + to_string(queryGraph.queryRels.size());
    } else if (queryGraph.containsQueryRel(parsedName)) {
        throw invalid_argument("Reuse name: " + parsedName + " for different relationships.");
    } else if (queryGraph.containsQueryNode(parsedName)) {
        throw invalid_argument("Reuse name: " + parsedName + " for node and relationship.");
    }
    auto queryRel =
        make_unique<QueryRel>(parsedName, bindRelLabel(patternElementChain.relPattern->label));
    auto leftNodeIsSrc = ArrowDirection::RIGHT == patternElementChain.relPattern->arrowDirection;
    bindNodeToRel(queryRel.get(), leftNode, leftNodeIsSrc);
    bindNodeToRel(queryRel.get(), bindQueryNode(*patternElementChain.nodePattern, queryGraph),
        !leftNodeIsSrc);
    queryGraph.addQueryRel(move(queryRel));
    return queryGraph.getQueryRel(parsedName);
}

QueryNode* Binder::bindQueryNode(const NodePattern& nodePattern, QueryGraph& queryGraph) {
    auto parsedName = nodePattern.name;
    if (parsedName.empty()) { // create anonymous node
        parsedName = "_gfN_" + to_string(queryGraph.queryNodes.size());
    } else if (queryGraph.containsQueryRel(parsedName)) {
        throw invalid_argument("Reuse name: " + parsedName + " for node and relationship");
    } else if (queryGraph.containsQueryNode(parsedName)) { // bind to previous bound node
        auto boundNode = queryGraph.getQueryNode(parsedName);
        auto label = bindNodeLabel(nodePattern.label);
        if (ANY_LABEL == label || boundNode->label == label) {
            return boundNode;
        } else {
            throw invalid_argument("Multi-label query nodes are not supported. " + parsedName +
                                   " is given multiple labels");
        }
    }
    auto queryNode = make_unique<QueryNode>(parsedName, bindNodeLabel(nodePattern.label));
    queryGraph.addQueryNode(move(queryNode));
    return queryGraph.getQueryNode(parsedName);
}

label_t Binder::bindRelLabel(const string& parsed_label) {
    if (parsed_label.empty()) {
        return ANY_LABEL;
    }
    if (!catalog.containRelLabel(parsed_label.c_str())) {
        throw invalid_argument("Rel label: " + parsed_label + " does not exist.");
    }
    return catalog.getRelLabelFromString(parsed_label.c_str());
}

label_t Binder::bindNodeLabel(const string& parsed_label) {
    if (parsed_label.empty()) {
        return ANY_LABEL;
    }
    if (!catalog.containNodeLabel(parsed_label.c_str())) {
        throw invalid_argument("Node label: " + parsed_label + " does not exist.");
    }
    return catalog.getNodeLabelFromString(parsed_label.c_str());
}

void Binder::bindNodeToRel(QueryRel* queryRel, QueryNode* queryNode, bool isSrcNode) {
    if (ANY_LABEL != queryNode->label && ANY_LABEL != queryRel->label) {
        auto relLabels =
            catalog.getRelLabelsForNodeLabelDirection(queryNode->label, isSrcNode ? FWD : BWD);
        for (auto& relLabel : relLabels) {
            if (relLabel == queryRel->label) {
                isSrcNode ? queryRel->srcNode = queryNode : queryRel->dstNode = queryNode;
                return;
            }
        }
        throw invalid_argument("Node: " + queryNode->name +
                               " doesn't connect to edge with same type as: " + queryRel->name);
    }
    isSrcNode ? queryRel->srcNode = queryNode : queryRel->dstNode = queryNode;
}

vector<shared_ptr<LogicalExpression>> splitExpressionOnAND(shared_ptr<LogicalExpression> expr) {
    auto result = vector<shared_ptr<LogicalExpression>>();
    if (AND == expr->expressionType) {
        for (auto& child : expr->childrenExpr) {
            for (auto& exp : splitExpressionOnAND(child)) {
                result.push_back(exp);
            }
        }
    } else {
        result.push_back(expr);
    }
    return result;
}

void mergeQueryGraphs(QueryGraph& mergedQueryGraph, QueryGraph& otherQueryGraph) {
    for (auto& otherNode : otherQueryGraph.queryNodes) {
        if (mergedQueryGraph.containsQueryNode(otherNode->name)) {
            validateQueryNodeWithSameName(
                *mergedQueryGraph.getQueryNode(otherNode->name), *otherNode);
        } else {
            mergedQueryGraph.addQueryNode(move(otherNode));
        }
    }
    for (auto& otherRel : otherQueryGraph.queryRels) {
        if (mergedQueryGraph.containsQueryRel(otherRel->name)) {
            throw invalid_argument("Reuse name: " + otherRel->name + " for relationship.");
        } else {
            rebindSrcAndDstNode(*otherRel, mergedQueryGraph);
            mergedQueryGraph.addQueryRel(move(otherRel));
        }
    }
}

void validateQueryNodeWithSameName(QueryNode& queryNodeInGraph, QueryNode& queryNodeToMerge) {
    if (ANY_LABEL == queryNodeInGraph.label && ANY_LABEL != queryNodeToMerge.label) {
        queryNodeInGraph.label = queryNodeToMerge.label;
        return;
    }
    if (ANY_LABEL != queryNodeInGraph.label && queryNodeInGraph.label != queryNodeToMerge.label) {
        throw invalid_argument("Multi-label query nodes are not supported. " +
                               queryNodeInGraph.name + " is given multiple labels.");
    }
}

void rebindSrcAndDstNode(QueryRel& queryRel, const QueryGraph& queryGraph) {
    queryRel.srcNode = queryGraph.getQueryNode(queryRel.getSrcNodeName());
    queryRel.dstNode = queryGraph.getQueryNode(queryRel.getDstNodeName());
}

} // namespace planner
} // namespace graphflow
