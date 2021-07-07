#include "src/binder/include/expression/literal_expression.h"

namespace graphflow {
namespace binder {
LiteralExpression::LiteralExpression(
    ExpressionType expressionType, DataType dataType, const Literal& literal)
    : Expression(expressionType, dataType), literal{literal} {
    assert(dataType == BOOL || dataType == INT64 || dataType == DOUBLE || dataType == STRING);
}

void LiteralExpression::castToString() {
    string valAsString;
    switch (dataType) {
    case BOOL:
        valAsString = TypeUtils::toString(literal.val.booleanVal);
        break;
    case INT64:
        valAsString = TypeUtils::toString(literal.val.int64Val);
        break;
    case DOUBLE:
        valAsString = TypeUtils::toString(literal.val.doubleVal);
        break;
    default:
        assert(false);
    }
    literal.strVal = valAsString;
    dataType = STRING;
}

} // namespace binder
} // namespace graphflow
