

#include "db/SQLParser.hpp"

#include "db/AST.hpp"
#include "db/Types.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace std;




namespace db {
namespace {

enum class TokenKind {
    Identifier,
    Number,
    String,
    Symbol,
    End
};

struct Token {
    TokenKind kind{TokenKind::End};
    string text;
};

class Tokenizer {
public:
    explicit Tokenizer(const string& input) : input_(input), pos_(0) {}

    vector<Token> tokenize() {
        vector<Token> tokens;
        Token token;
        do {
            token = next_token();
            tokens.push_back(token);
        } while (token.kind != TokenKind::End);
        return tokens;
    }

private:
    Token next_token() {
        
        skip_whitespace();
        if (pos_ >= input_.size()) {
            return {TokenKind::End, ""};
        }

        char ch = input_[pos_];

        if (isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            return consume_identifier();
        }
        if (isdigit(static_cast<unsigned char>(ch))) {
            return consume_number();
        }
        if (ch == '\'') {
            return consume_string();
        }

        return consume_symbol();
    }

    void skip_whitespace() {
        
        while (pos_ < input_.size() && isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    Token consume_identifier() {
        size_t start = pos_;
        while (pos_ < input_.size()) {
            char ch = input_[pos_];
            if (!isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                break;
            }
            ++pos_;
        }
        return {TokenKind::Identifier, input_.substr(start, pos_ - start)};
    }

    Token consume_number() {
        size_t start = pos_;
        while (pos_ < input_.size() && isdigit(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
        return {TokenKind::Number, input_.substr(start, pos_ - start)};
    }

    Token consume_string() {
        
        ++pos_; 
        string value;
        while (pos_ < input_.size()) {
            char ch = input_[pos_++];
            if (ch == '\'') {
                if (pos_ < input_.size() && input_[pos_] == '\'') {
                    value.push_back('\'');
                    ++pos_;
                } else {
                    return {TokenKind::String, value};
                }
            } else {
                value.push_back(ch);
            }
        }
        throw runtime_error("Unterminated string literal");
    }

    Token consume_symbol() {
        static const vector<string> multi = {"<>", "<=", ">="};
        for (const auto& op : multi) {
            if (input_.compare(pos_, op.size(), op) == 0) {
                pos_ += op.size();
                return {TokenKind::Symbol, op};
            }
        }

        char ch = input_[pos_++];
        return {TokenKind::Symbol, string(1, ch)};
    }

    const string& input_;
    size_t pos_;
};

class TokenStream {
public:
    explicit TokenStream(vector<Token> tokens) : tokens_(move(tokens)) {}

    const Token& peek(size_t offset = 0) const {
        
        size_t index = position_ + offset;
        if (index >= tokens_.size()) {
            static const Token end{TokenKind::End, ""};
            return end;
        }
        return tokens_[index];
    }

    Token consume() {
        const auto& token = peek();
        if (position_ < tokens_.size()) {
            ++position_;
        }
        return token;
    }

    bool match_symbol(const string& symbol) {
        if (peek().kind == TokenKind::Symbol && peek().text == symbol) {
            consume();
            return true;
        }
        return false;
    }

    void expect_symbol(const string& symbol) {
        if (!match_symbol(symbol)) {
            throw runtime_error("Expected symbol '" + symbol + "'");
        }
    }

    bool match_keyword(const string& keyword) {
        
        if (peek().kind == TokenKind::Identifier && equals_ignore_case(peek().text, keyword)) {
            consume();
            return true;
        }
        return false;
    }

    void expect_keyword(const string& keyword) {
        if (!match_keyword(keyword)) {
            throw runtime_error("Expected keyword '" + keyword + "'");
        }
    }

    string consume_identifier(const string& context) {
        const auto& token = peek();
        if (token.kind != TokenKind::Identifier) {
            throw runtime_error("Expected identifier for " + context);
        }
        consume();
        return token.text;
    }

    string consume_number(const string& context) {
        const auto& token = peek();
        if (token.kind != TokenKind::Number) {
            throw runtime_error("Expected numeric literal for " + context);
        }
        consume();
        return token.text;
    }

    string consume_string_literal() {
        const auto& token = peek();
        if (token.kind != TokenKind::String) {
            throw runtime_error("Expected string literal");
        }
        consume();
        return token.text;
    }

    void ensure_end() {
        if (peek().kind != TokenKind::End) {
            throw runtime_error("Unexpected token: " + peek().text);
        }
    }

private:
    static bool equals_ignore_case(const string& lhs, const string& rhs) {
        
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (toupper(static_cast<unsigned char>(lhs[i])) !=
                toupper(static_cast<unsigned char>(rhs[i]))) {
                return false;
            }
        }
        return true;
    }

    vector<Token> tokens_;
    size_t position_{0};
};

string to_upper(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(toupper(ch));
    });
    return value;
}

string trim(const string& input) {
    auto begin = input.begin();
    while (begin != input.end() && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = input.end();
    while (end != begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return string(begin, end);
}

bool is_reserved_keyword(const string& value) {
    static const vector<string> reserved = {
        "SELECT",  "FROM",   "WHERE",  "INNER",  "JOIN",   "LEFT",   "ON",     "AS",    "AND",
        "OR",      "INSERT", "INTO",   "VALUES", "UPDATE", "SET",    "DELETE", "CREATE", "TABLE",
        "DROP",    "ALTER",  "DISTINCT"};
    auto upper = to_upper(value);
    return find(reserved.begin(), reserved.end(), upper) != reserved.end();
}

ColumnDefinition parse_column_definition_tail(TokenStream& tokens, ColumnDefinition column) {
    const auto type_token = tokens.consume_identifier("column type");
    const auto upper_type = to_upper(type_token);
    if (upper_type == "INT") {
        column.type = DataType::Int;
        column.length = default_length(column.type);
    } else if (upper_type == "VARCHAR") {
        column.type = DataType::Varchar;
        if (tokens.match_symbol("(")) {
            auto length_str = tokens.consume_number("VARCHAR length");
            tokens.expect_symbol(")");
            column.length = static_cast<size_t>(stoul(length_str));
        } else {
            column.length = default_length(column.type);
        }
    } else {
        throw runtime_error("Unsupported column type: " + type_token);
    }

    return column;
}

ColumnDefinition parse_column_definition(TokenStream& tokens) {
    ColumnDefinition column;
    column.name = tokens.consume_identifier("column name");
    return parse_column_definition_tail(tokens, move(column));
}

LiteralValue parse_literal(TokenStream& tokens) {
    const auto& token = tokens.peek();
    if (token.kind == TokenKind::String) {
        LiteralValue value;
        value.type = LiteralType::String;
        value.string_value = tokens.consume_string_literal();
        return value;
    }
    if (token.kind == TokenKind::Symbol && token.text == "-" && tokens.peek(1).kind == TokenKind::Number) {
        tokens.consume();
        auto number_text = "-" + tokens.consume_number("numeric literal");
        LiteralValue value;
        value.type = LiteralType::Int;
        try {
            value.int_value = stoll(number_text);
        } catch (const exception&) {
            throw runtime_error("Invalid INTEGER literal: " + number_text);
        }
        return value;
    }
    if (token.kind == TokenKind::Number) {
        LiteralValue value;
        value.type = LiteralType::Int;
        auto text = tokens.consume_number("numeric literal");
        try {
            value.int_value = stoll(text);
        } catch (const exception&) {
            throw runtime_error("Invalid INTEGER literal: " + text);
        }
        return value;
    }
    throw runtime_error("Unsupported literal value: " + token.text);
}

unique_ptr<Expression> parse_operand_expression(TokenStream& tokens) {
    const auto& token = tokens.peek();
    if (token.kind == TokenKind::String || token.kind == TokenKind::Number ||
        (token.kind == TokenKind::Symbol && token.text == "-" && tokens.peek(1).kind == TokenKind::Number)) {
        auto literal_expr = make_unique<LiteralExpression>();
        literal_expr->value = parse_literal(tokens);
        return literal_expr;
    }

    if (token.kind != TokenKind::Identifier) {
        throw runtime_error("Expected column reference or literal, found: " + token.text);
    }

    auto column_expr = make_unique<ColumnExpression>();
    column_expr->table_alias = tokens.consume_identifier("column reference");
    if (tokens.match_symbol(".")) {
        column_expr->column_name = tokens.consume_identifier("column name");
    } else {
        column_expr->column_name = column_expr->table_alias;
        column_expr->table_alias.clear();
    }
    return column_expr;
}

ComparisonOperator parse_comparison_operator(TokenStream& tokens) {
    const auto& token = tokens.peek();
    if (token.kind != TokenKind::Symbol) {
        throw runtime_error("Expected comparison operator, found: " + token.text);
    }

    if (token.text == "=") {
        tokens.consume();
        return ComparisonOperator::Equal;
    }
    if (token.text == "<>") {
        tokens.consume();
        return ComparisonOperator::NotEqual;
    }
    if (token.text == "<") {
        tokens.consume();
        return ComparisonOperator::Less;
    }
    if (token.text == ">") {
        tokens.consume();
        return ComparisonOperator::Greater;
    }
    if (token.text == "<=") {
        tokens.consume();
        return ComparisonOperator::LessOrEqual;
    }
    if (token.text == ">=") {
        tokens.consume();
        return ComparisonOperator::GreaterOrEqual;
    }

    throw runtime_error("Unsupported comparison operator: " + token.text);
}

unique_ptr<Expression> parse_comparison(TokenStream& tokens) {
    auto left = parse_operand_expression(tokens);
    auto op = parse_comparison_operator(tokens);
    auto right = parse_operand_expression(tokens);

    auto comparison = make_unique<ComparisonExpression>();
    comparison->op = op;
    comparison->left = move(left);
    comparison->right = move(right);
    return comparison;
}

unique_ptr<Expression> parse_condition(TokenStream& tokens) {
    auto comparison = parse_comparison(tokens);
    if (!tokens.match_keyword("AND")) {
        return comparison;
    }

    auto and_expr = make_unique<AndExpression>();
    and_expr->terms.push_back(move(comparison));
    do {
        and_expr->terms.push_back(parse_comparison(tokens));
    } while (tokens.match_keyword("AND"));
    return and_expr;
}

StatementPtr parse_create_table(TokenStream& tokens) {
    tokens.expect_keyword("CREATE");
    tokens.expect_keyword("TABLE");

    auto stmt = make_unique<CreateTableStatement>();
    stmt->table_name = tokens.consume_identifier("table name");
    tokens.expect_symbol("(");

    while (true) {
        stmt->columns.push_back(parse_column_definition(tokens));
        if (tokens.match_symbol(")")) {
            break;
        }
        tokens.expect_symbol(",");
    }

    tokens.ensure_end();
    return stmt;
}

StatementPtr parse_drop_table(TokenStream& tokens) {
    tokens.expect_keyword("DROP");
    tokens.expect_keyword("TABLE");
    auto stmt = make_unique<DropTableStatement>();
    stmt->table_name = tokens.consume_identifier("table name");
    tokens.ensure_end();
    return stmt;
}

StatementPtr parse_alter_table(TokenStream& tokens) {
    tokens.expect_keyword("ALTER");
    tokens.expect_keyword("TABLE");

    auto stmt = make_unique<AlterTableStatement>();
    stmt->table_name = tokens.consume_identifier("table name");

    if (tokens.match_keyword("RENAME")) {
        tokens.expect_keyword("TO");
        stmt->action = AlterTableAction::RenameTable;
        stmt->new_table_name = tokens.consume_identifier("new table name");
        tokens.ensure_end();
        return stmt;
    }

    if (tokens.match_keyword("ADD")) {
        tokens.expect_keyword("COLUMN");
        stmt->action = AlterTableAction::AddColumn;
        stmt->column = parse_column_definition(tokens);
        tokens.ensure_end();
        return stmt;
    }

    if (tokens.match_keyword("DROP")) {
        tokens.expect_keyword("COLUMN");
        stmt->action = AlterTableAction::DropColumn;
        stmt->target_column_name = tokens.consume_identifier("column name");
        tokens.ensure_end();
        return stmt;
    }

    if (tokens.match_keyword("MODIFY")) {
        tokens.expect_keyword("COLUMN");
        auto column_name = tokens.consume_identifier("column name");
        ColumnDefinition column;
        column.name = column_name;
        auto column_def = parse_column_definition_tail(tokens, move(column));
        stmt->action = AlterTableAction::ModifyColumn;
        stmt->target_column_name = column_name;
        stmt->column = column_def;
        tokens.ensure_end();
        return stmt;
    }

    throw runtime_error("Unsupported ALTER TABLE action");
}

StatementPtr parse_insert(TokenStream& tokens) {
    tokens.expect_keyword("INSERT");
    tokens.expect_keyword("INTO");

    auto stmt = make_unique<InsertStatement>();
    stmt->table_name = tokens.consume_identifier("table name");
    tokens.expect_keyword("VALUES");
    tokens.expect_symbol("(");

    do {
        stmt->values.push_back(parse_literal(tokens));
    } while (tokens.match_symbol(","));
    tokens.expect_symbol(")");
    tokens.ensure_end();
    return stmt;
}

StatementPtr parse_update(TokenStream& tokens) {
    tokens.expect_keyword("UPDATE");
    auto stmt = make_unique<UpdateStatement>();
    stmt->table_name = tokens.consume_identifier("table name");

    tokens.expect_keyword("SET");
    do {
        Assignment assignment;
        assignment.column_name = tokens.consume_identifier("column name");
        tokens.expect_symbol("=");
        assignment.value = parse_literal(tokens);
        stmt->assignments.push_back(assignment);
    } while (tokens.match_symbol(","));

    if (tokens.match_keyword("WHERE")) {
        stmt->where = parse_condition(tokens);
    }

    tokens.ensure_end();
    return stmt;
}

StatementPtr parse_delete(TokenStream& tokens) {
    tokens.expect_keyword("DELETE");
    tokens.expect_keyword("FROM");

    auto stmt = make_unique<DeleteStatement>();
    stmt->table_name = tokens.consume_identifier("table name");

    if (tokens.match_keyword("WHERE")) {
        stmt->where = parse_condition(tokens);
    }

    tokens.ensure_end();
    return stmt;
}

TableReference parse_table_reference(TokenStream& tokens) {
    TableReference table;
    table.table_name = tokens.consume_identifier("table name");
    if (tokens.match_keyword("AS")) {
        table.alias = tokens.consume_identifier("table alias");
    } else if (tokens.peek().kind == TokenKind::Identifier && !is_reserved_keyword(tokens.peek().text)) {
        table.alias = tokens.consume_identifier("table alias");
    }
    return table;
}

unique_ptr<Expression> parse_join_condition(TokenStream& tokens) {
    tokens.expect_keyword("ON");
    return parse_condition(tokens);
}

StatementPtr parse_select(TokenStream& tokens) {
    tokens.expect_keyword("SELECT");

    if (tokens.match_keyword("DISTINCT")) {
        throw runtime_error("DISTINCT is not supported");
    }

    auto stmt = make_unique<SelectStatement>();

    while (true) {
        SelectItem item;
        const auto& token = tokens.peek();
        if (token.kind == TokenKind::Symbol && token.text == "*") {
            item.is_wildcard = true;
            tokens.consume();
        } else if (token.kind == TokenKind::Identifier && tokens.peek(1).kind == TokenKind::Symbol &&
                   tokens.peek(1).text == "." && tokens.peek(2).kind == TokenKind::Symbol &&
                   tokens.peek(2).text == "*") {
            item.is_wildcard = true;
            item.table_alias = tokens.consume_identifier("table alias");
            tokens.expect_symbol(".");
            tokens.expect_symbol("*");
        } else {
            auto operand = parse_operand_expression(tokens);
            if (operand->type == ExpressionType::Column) {
                auto* column_expr = static_cast<ColumnExpression*>(operand.get());
                item.table_alias = column_expr->table_alias;
                item.column_name = column_expr->column_name;
            } else {
                throw runtime_error("SELECT list only supports column references");
            }
        }

        if (tokens.match_keyword("AS")) {
            item.alias = tokens.consume_identifier("alias");
        } else if (!item.is_wildcard && tokens.peek().kind == TokenKind::Identifier &&
                   !is_reserved_keyword(tokens.peek().text)) {
            item.alias = tokens.consume_identifier("alias");
        }

        stmt->select_list.push_back(item);

        if (!tokens.match_symbol(",")) {
            break;
        }
    }

    tokens.expect_keyword("FROM");
    stmt->primary_table = parse_table_reference(tokens);

    while (true) {
        if (tokens.match_keyword("INNER")) {
            tokens.expect_keyword("JOIN");
        } else if (tokens.match_keyword("JOIN")) {
            
        } else if (tokens.match_keyword("LEFT")) {
            throw runtime_error("LEFT JOIN is not supported");
        } else {
            break;
        }

        JoinClause clause;
        clause.table = parse_table_reference(tokens);
        clause.condition = parse_join_condition(tokens);
        stmt->joins.push_back(move(clause));
    }

    if (tokens.match_keyword("WHERE")) {
        stmt->where = parse_condition(tokens);
    }

    tokens.ensure_end();
    return stmt;
}

} 

SQLParser::SQLParser() = default;

StatementPtr SQLParser::parse(const string& sql) {
    auto trimmed = trim(sql);
    if (trimmed.empty()) {
        throw runtime_error("Empty statement");
    }

    if (!trimmed.empty() && trimmed.back() == ';') {
        trimmed.pop_back();
        trimmed = trim(trimmed);
    }

    Tokenizer tokenizer(trimmed);
    TokenStream tokens(tokenizer.tokenize());
    
    const auto& first = tokens.peek();
    if (first.kind == TokenKind::Identifier) {
        auto keyword = to_upper(first.text);
        if (keyword == "CREATE") {
            return parse_create_table(tokens);
        }
        if (keyword == "DROP") {
            return parse_drop_table(tokens);
        }
        if (keyword == "ALTER") {
            return parse_alter_table(tokens);
        }
        if (keyword == "INSERT") {
            return parse_insert(tokens);
        }
        if (keyword == "UPDATE") {
            return parse_update(tokens);
        }
        if (keyword == "DELETE") {
            return parse_delete(tokens);
        }
        if (keyword == "SELECT") {
            return parse_select(tokens);
        }
    }

    throw runtime_error("Unsupported SQL statement");
}

} 
