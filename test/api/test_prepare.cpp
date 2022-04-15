#include "test/test_utility/include/test_helper.h"

#include "src/main/include/graphflowdb.h"

using namespace graphflow::testing;
using namespace graphflow::main;

TEST_F(ApiTest, basic_prepare) {
    auto preparedStatement =
        conn->prepare("MATCH (a:person) WHERE a.fName STARTS WITH $n RETURN COUNT(*)");
    auto result = conn->execute(preparedStatement.get(), make_pair(string("n"), "A"));
    ASSERT_TRUE(result->hasNext());
    auto tuple = result->getNext();
    ASSERT_EQ(tuple->getValue(0)->val.int64Val, 1);
    ASSERT_FALSE(result->hasNext());
}

TEST_F(ApiTest, multi_params_prepare) {
    auto preparedStatement = conn->prepare(
        "MATCH (a:person) WHERE a.fName STARTS WITH $n OR a.fName CONTAINS $xx RETURN COUNT(*)");
    auto result = conn->execute(
        preparedStatement.get(), make_pair(string("n"), "A"), make_pair(string("xx"), "ooq"));
    ASSERT_TRUE(result->hasNext());
    auto tuple = result->getNext();
    ASSERT_EQ(tuple->getValue(0)->val.int64Val, 2);
    ASSERT_FALSE(result->hasNext());
}

TEST_F(ApiTest, prepare_bool) {
    auto preparedStatement =
        conn->prepare("MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)");
    auto result = conn->execute(preparedStatement.get(), make_pair(string("1"), true));
    ASSERT_TRUE(result->hasNext());
    auto tuple = result->getNext();
    ASSERT_EQ(tuple->getValue(0)->val.int64Val, 3);
    ASSERT_FALSE(result->hasNext());
}

TEST_F(ApiTest, prepare_int) {
    auto preparedStatement = conn->prepare("MATCH (a:person) WHERE a.age = 35 RETURN a.age + $1");
    auto result = conn->execute(preparedStatement.get(), make_pair(string("1"), (int64_t)10));
    ASSERT_TRUE(result->hasNext());
    auto tuple = result->getNext();
    ASSERT_EQ(tuple->getValue(0)->val.int64Val, 45);
    ASSERT_FALSE(result->hasNext());
}

TEST_F(ApiTest, prepare_double) {
    auto preparedStatement =
        conn->prepare("MATCH (a:person) WHERE a.age = 35 RETURN a.eyeSight + $1");
    auto result = conn->execute(preparedStatement.get(), make_pair(string("1"), (double_t)10.5));
    ASSERT_TRUE(result->hasNext());
    auto tuple = result->getNext();
    ASSERT_EQ(tuple->getValue(0)->val.doubleVal, 15.5);
    ASSERT_FALSE(result->hasNext());
}

TEST_F(ApiTest, param_not_exist) {
    auto preparedStatement =
        conn->prepare("MATCH (a:person) WHERE a.fName STARTS WITH $n RETURN COUNT(*)");
    try {
        conn->execute(preparedStatement.get(), make_pair(string("a"), "A"));
    } catch (const invalid_argument& exception) {
        ASSERT_STREQ("Parameter a not found.", exception.what());
    }
}

TEST_F(ApiTest, param_type_error) {
    auto preparedStatement =
        conn->prepare("MATCH (a:person) WHERE a.fName STARTS WITH $n RETURN COUNT(*)");
    try {
        conn->execute(preparedStatement.get(), make_pair(string("n"), (int64_t)36));
    } catch (const invalid_argument& exception) {
        ASSERT_STREQ("Parameter n has data type INT64 but expect STRING.", exception.what());
    }
}
