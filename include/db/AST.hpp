#pragma once

#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::size_t;
using std::unique_ptr;
using std::int64_t;

namespace db {

enum class StatementType {
    CreateTable,
    DropTable,
    AlterTable,
    Insert,
    Update,
    Delete,
    Select,
    Unknown
};

struct Statement {
    explicit Statement(StatementType stmt_type) : type(stmt_type) {}
    virtual ~Statement() = default;
    StatementType type;
};

struct ColumnDefinition {
    string name;
    DataType type;
    size_t length;
};

enum class LiteralType {
    Int,
    String
};

struct LiteralValue {
    LiteralType type;
    int64_t int_value{};
    string string_value;
};

enum class ExpressionType {
    Column,
    Literal,
    Comparison,
    And
};

enum class ComparisonOperator {
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterOrEqual,
    LessOrEqual
};

struct Expression {
    explicit Expression(ExpressionType expression_type) : type(expression_type) {}
    virtual ~Expression() = default;
    ExpressionType type;
};

struct ColumnExpression : Expression {
    ColumnExpression() : Expression(ExpressionType::Column) {}
    string table_alias;
    string column_name;
};

struct LiteralExpression : Expression {
    LiteralExpression() : Expression(ExpressionType::Literal) {}
    LiteralValue value;
};

struct ComparisonExpression : Expression {
    ComparisonExpression() : Expression(ExpressionType::Comparison) {}
    ComparisonOperator op{ComparisonOperator::Equal};
    unique_ptr<Expression> left;
    unique_ptr<Expression> right;
};

struct AndExpression : Expression {
    AndExpression() : Expression(ExpressionType::And) {}
    vector<unique_ptr<Expression>> terms;
};

struct CreateTableStatement : Statement {
    CreateTableStatement() : Statement(StatementType::CreateTable) {}
    string table_name;
    vector<ColumnDefinition> columns;
};

struct DropTableStatement : Statement {
    DropTableStatement() : Statement(StatementType::DropTable) {}
    string table_name;
};

enum class AlterTableAction {
    RenameTable,
    AddColumn,
    DropColumn,
    ModifyColumn
};

struct AlterTableStatement : Statement {
    AlterTableStatement() : Statement(StatementType::AlterTable) {}
    AlterTableAction action{AlterTableAction::RenameTable};
    string table_name;
    string new_table_name;      // For rename
    ColumnDefinition column;         // For add/modify
    string target_column_name;  // For drop/modify
};

struct InsertStatement : Statement {
    InsertStatement() : Statement(StatementType::Insert) {}
    string table_name;
    vector<LiteralValue> values;
};

struct Assignment {
    string column_name;
    LiteralValue value;
};

struct UpdateStatement : Statement {
    UpdateStatement() : Statement(StatementType::Update) {}
    string table_name;
    vector<Assignment> assignments;
    unique_ptr<Expression> where;
};

struct DeleteStatement : Statement {
    DeleteStatement() : Statement(StatementType::Delete) {}
    string table_name;
    unique_ptr<Expression> where;
};

struct SelectItem {
    bool is_wildcard{false};
    string table_alias;
    string column_name;
    string alias;
};

struct TableReference {
    string table_name;
    string alias;
};

struct JoinClause {
    TableReference table;
    unique_ptr<Expression> condition;
};

struct SelectStatement : Statement {
    SelectStatement() : Statement(StatementType::Select) {}
    vector<SelectItem> select_list;
    TableReference primary_table;
    vector<JoinClause> joins;
    unique_ptr<Expression> where;
};

using StatementPtr = unique_ptr<Statement>;
using StatementList = vector<StatementPtr>;

} // namespace db
