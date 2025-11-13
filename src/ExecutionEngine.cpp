#include "db/ExecutionEngine.hpp"

#include "db/AST.hpp"
#include "db/CatalogManager.hpp"
#include "db/StorageManager.hpp"
#include "db/Types.hpp"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
using namespace std;




namespace {

using db::ColumnSchema;
using db::ComparisonExpression;
using db::ComparisonOperator;
using db::DataType;
using db::Expression;
using db::ExpressionType;
using db::LiteralExpression;
using db::LiteralType;
using db::LiteralValue;
using db::TableReference;
using db::TableSchema;

struct TableBinding {
    const TableSchema* schema{nullptr};
    const vector<string>* row{nullptr};
    string table_name;
    string alias;
};

struct EvaluationContext {
    vector<TableBinding> tables;
    unordered_map<string, size_t> lookup;
};

EvaluationContext make_context(const vector<TableBinding>& tables) {
    EvaluationContext ctx;
    ctx.tables = tables;
    for (size_t i = 0; i < tables.size(); ++i) {
        ctx.lookup[tables[i].table_name] = i;
        if (!tables[i].alias.empty()) {
            ctx.lookup[tables[i].alias] = i;
        }
    }
    return ctx;
}

const TableBinding& resolve_binding(const EvaluationContext& ctx, const string& name) {
    auto it = ctx.lookup.find(name);
    if (it == ctx.lookup.end()) {
        throw runtime_error("Unknown table or alias: " + name);
    }
    return ctx.tables.at(it->second);
}

const ColumnSchema* try_find_column(const TableSchema& schema, const string& column_name, size_t& index) {
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        if (schema.columns[i].name == column_name) {
            index = i;
            return &schema.columns[i];
        }
    }
    return nullptr;
}

struct ColumnLookupResult {
    const TableBinding* binding{nullptr};
    const ColumnSchema* column{nullptr};
    size_t index{};
};

ColumnLookupResult lookup_column(const EvaluationContext& ctx, const db::ColumnExpression& expr) {
    ColumnLookupResult result;

    if (!expr.table_alias.empty()) {
        const auto& binding = resolve_binding(ctx, expr.table_alias);
        size_t index{};
        const ColumnSchema* column = try_find_column(*binding.schema, expr.column_name, index);
        if (column == nullptr) {
            throw runtime_error("Column not found: " + expr.table_alias + '.' + expr.column_name);
        }
        result.binding = &binding;
        result.column = column;
        result.index = index;
        return result;
    }

    bool found = false;
    for (const auto& binding : ctx.tables) {
        size_t index{};
        const ColumnSchema* column = try_find_column(*binding.schema, expr.column_name, index);
        if (column != nullptr) {
            if (found) {
                throw runtime_error("Ambiguous column: " + expr.column_name);
            }
            result.binding = &binding;
            result.column = column;
            result.index = index;
            found = true;
        }
    }

    if (!found) {
        throw runtime_error("Column not found: " + expr.column_name);
    }

    return result;
}

LiteralValue make_int_literal(int64_t value) {
    LiteralValue literal;
    literal.type = LiteralType::Int;
    literal.int_value = value;
    return literal;
}

LiteralValue make_string_literal(const string& value) {
    LiteralValue literal;
    literal.type = LiteralType::String;
    literal.string_value = value;
    return literal;
}

LiteralValue storage_to_literal(const ColumnSchema& column, const string& value) {
    if (column.type == DataType::Int) {
        if (value.empty()) {
            throw runtime_error("Empty value encountered for INT column: " + column.name);
        }
        try {
            return make_int_literal(stoll(value));
        } catch (const exception&) {
            throw runtime_error("Failed to parse INT value for column " + column.name + ": " + value);
        }
    }
    return make_string_literal(value);
}

string literal_to_storage(const LiteralValue& literal, const ColumnSchema& column) {
    if (column.type == DataType::Int) {
        if (literal.type != LiteralType::Int) {
            throw runtime_error("Type mismatch: column " + column.name + " expects INT");
        }
        return to_string(literal.int_value);
    }

    if (literal.type != LiteralType::String) {
        throw runtime_error("Type mismatch: column " + column.name + " expects VARCHAR");
    }
    if (column.length > 0 && literal.string_value.size() > column.length) {
        throw runtime_error("Value for column " + column.name + " exceeds maximum length");
    }
    return literal.string_value;
}

int compare_literals(const LiteralValue& left, const LiteralValue& right) {
    if (left.type != right.type) {
        throw runtime_error("Cannot compare values of different types");
    }
    if (left.type == LiteralType::Int) {
        if (left.int_value < right.int_value) {
            return -1;
        }
        if (left.int_value > right.int_value) {
            return 1;
        }
        return 0;
    }
    if (left.string_value < right.string_value) {
        return -1;
    }
    if (left.string_value > right.string_value) {
        return 1;
    }
    return 0;
}

LiteralValue evaluate_operand(const Expression& expr, const EvaluationContext& ctx);

bool evaluate_comparison(const ComparisonExpression& expr, const EvaluationContext& ctx) {
    const auto left = evaluate_operand(*expr.left, ctx);
    const auto right = evaluate_operand(*expr.right, ctx);
    switch (expr.op) {
    case ComparisonOperator::Equal:
        return compare_literals(left, right) == 0;
    case ComparisonOperator::NotEqual:
        return compare_literals(left, right) != 0;
    case ComparisonOperator::Greater:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw runtime_error("> comparisons require INT operands");
        }
        return left.int_value > right.int_value;
    case ComparisonOperator::Less:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw runtime_error("< comparisons require INT operands");
        }
        return left.int_value < right.int_value;
    case ComparisonOperator::GreaterOrEqual:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw runtime_error(">= comparisons require INT operands");
        }
        return left.int_value >= right.int_value;
    case ComparisonOperator::LessOrEqual:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw runtime_error("<= comparisons require INT operands");
        }
        return left.int_value <= right.int_value;
    }
    throw runtime_error("Unsupported comparison operator");
}

LiteralValue evaluate_operand(const Expression& expr, const EvaluationContext& ctx) {
    switch (expr.type) {
    case ExpressionType::Column: {
        const auto& column_expr = static_cast<const db::ColumnExpression&>(expr);
        auto lookup = lookup_column(ctx, column_expr);
        const auto& raw = lookup.binding->row->at(lookup.index);
        return storage_to_literal(*lookup.column, raw);
    }
    case ExpressionType::Literal: {
        return static_cast<const LiteralExpression&>(expr).value;
    }
    default:
        throw runtime_error("Unsupported operand in expression evaluation");
    }
}

bool evaluate_condition(const Expression* expr, const EvaluationContext& ctx) {
    if (expr == nullptr) {
        return true;
    }

    switch (expr->type) {
    case ExpressionType::Comparison:
        return evaluate_comparison(static_cast<const ComparisonExpression&>(*expr), ctx);
    case ExpressionType::And: {
        const auto& and_expr = static_cast<const db::AndExpression&>(*expr);
        for (const auto& term : and_expr.terms) {
            if (!evaluate_condition(term.get(), ctx)) {
                return false;
            }
        }
        return true;
    }
    default:
        throw runtime_error("Unsupported condition expression");
    }
}

string format_success(const string& message) {
    ostringstream oss;
    oss << "OK: " << message;
    return oss.str();
}

string format_result_table(const vector<string>& headers,
                                const vector<vector<string>>& rows) {
    if (headers.empty()) {
        return "(no columns)\n";
    }

    vector<size_t> widths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) {
        widths[i] = headers[i].size();
    }
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            widths[i] = max(widths[i], row[i].size());
        }
    }

    auto make_divider = [&]() {
        ostringstream divider;
        for (size_t i = 0; i < widths.size(); ++i) {
            divider << string(widths[i], '-') ;
            if (i + 1 < widths.size()) {
                divider << "-+-";
            }
        }
        return divider.str();
    };

    ostringstream oss;
    auto write_row = [&](const vector<string>& row) {
        for (size_t i = 0; i < row.size(); ++i) {
            oss << left << setw(static_cast<int>(widths[i])) << row[i];
            if (i + 1 < row.size()) {
                oss << " | ";
            }
        }
        oss << '\n';
    };

    write_row(headers);
    oss << make_divider() << '\n';
    for (const auto& row : rows) {
        write_row(row);
    }
    oss << '(' << rows.size() << (rows.size() == 1 ? " row)" : " rows)") << '\n';
    return oss.str();
}

} 

namespace db {

ExecutionEngine::ExecutionEngine(shared_ptr<CatalogManager> c,
                                 shared_ptr<StorageManager> s)
    : c_(move(c)), s_(move(s)) {}

string ExecutionEngine::execute(const Statement& statement) {
    switch (statement.type) {
    case StatementType::CreateTable:
        return handle_create_table(static_cast<const CreateTableStatement&>(statement));
    case StatementType::DropTable:
        return handle_drop_table(static_cast<const DropTableStatement&>(statement));
    case StatementType::AlterTable:
        return handle_alter_table(static_cast<const AlterTableStatement&>(statement));
    case StatementType::Insert:
        return handle_insert(static_cast<const InsertStatement&>(statement));
    case StatementType::Update:
        return handle_update(static_cast<const UpdateStatement&>(statement));
    case StatementType::Delete:
        return handle_delete(static_cast<const DeleteStatement&>(statement));
    case StatementType::Select:
        return handle_select(static_cast<const SelectStatement&>(statement));
    default:
        throw runtime_error("Unsupported statement type in execution engine");
    }
}

string ExecutionEngine::handle_create_table(const CreateTableStatement& statement) {
    TableSchema schema;
    schema.name = statement.table_name;

    for (const auto& column : statement.columns) {
        ColumnSchema column_schema;
        column_schema.name = column.name;
        column_schema.type = column.type;
        column_schema.length = column.length;
        schema.columns.push_back(column_schema);
    }

    c_->create_table(schema);
    s_->create_table_storage(schema);

    return format_success("Table created: " + schema.name);
}

string ExecutionEngine::handle_drop_table(const DropTableStatement& statement) {
    c_->drop_table(statement.table_name);
    s_->drop_table_storage(statement.table_name);
    return format_success("Table dropped: " + statement.table_name);
}

string ExecutionEngine::handle_alter_table(const AlterTableStatement& statement) {
    switch (statement.action) {
    case AlterTableAction::RenameTable: {
        const auto& old_name = statement.table_name;
        const auto& new_name = statement.new_table_name;

    if (!c_->table_exists(old_name)) {
            throw runtime_error("Table does not exist: " + old_name);
        }
    if (c_->table_exists(new_name)) {
            throw runtime_error("Target table already exists: " + new_name);
        }

    s_->rename_table_storage(old_name, new_name);
    c_->rename_table(old_name, new_name);
        return format_success("Table renamed: " + old_name + " -> " + new_name);
    }
    case AlterTableAction::AddColumn: {
    auto schema_opt = c_->get_table(statement.table_name);
        if (!schema_opt.has_value()) {
            throw runtime_error("Table does not exist: " + statement.table_name);
        }

        const auto& schema = schema_opt.value();
        const auto& new_column = statement.column;

        auto duplicate = any_of(schema.columns.begin(), schema.columns.end(), [&](const ColumnSchema& col) {
            return col.name == new_column.name;
        });
        if (duplicate) {
            throw runtime_error("Column already exists: " + new_column.name);
        }

        ColumnSchema column_schema{new_column.name, new_column.type, new_column.length};

    s_->add_column(statement.table_name, column_schema);
    c_->add_column(statement.table_name, column_schema);

        return format_success("Column added: " + statement.table_name + '.' + column_schema.name);
    }
    case AlterTableAction::DropColumn: {
    auto schema_opt = c_->get_table(statement.table_name);
        if (!schema_opt.has_value()) {
            throw runtime_error("Table does not exist: " + statement.table_name);
        }

        const auto& schema = schema_opt.value();
        const auto& column_name = statement.target_column_name;

        auto it = find_if(schema.columns.begin(), schema.columns.end(), [&](const ColumnSchema& col) {
            return col.name == column_name;
        });
        if (it == schema.columns.end()) {
            throw runtime_error("Column does not exist: " + column_name);
        }
        if (schema.columns.size() <= 1) {
            throw runtime_error("Cannot drop the last column from table: " + statement.table_name);
        }

    s_->drop_column(statement.table_name, column_name);
    c_->drop_column(statement.table_name, column_name);

        return format_success("Column dropped: " + statement.table_name + '.' + column_name);
    }
    case AlterTableAction::ModifyColumn: {
    auto schema_opt = c_->get_table(statement.table_name);
        if (!schema_opt.has_value()) {
            throw runtime_error("Table does not exist: " + statement.table_name);
        }

        const auto& schema = schema_opt.value();
        const auto& target_name = statement.target_column_name;

        auto it = find_if(schema.columns.begin(), schema.columns.end(), [&](const ColumnSchema& col) {
            return col.name == target_name;
        });
        if (it == schema.columns.end()) {
            throw runtime_error("Column does not exist: " + target_name);
        }

        ColumnSchema column_schema{statement.column.name, statement.column.type, statement.column.length};

    s_->modify_column(statement.table_name, column_schema);
    c_->modify_column(statement.table_name, column_schema);

        return format_success("Column modified: " + statement.table_name + '.' + column_schema.name);
    }
    default:
        throw runtime_error("ALTER TABLE action not implemented");
    }
}

string ExecutionEngine::handle_insert(const InsertStatement& statement) {
    auto schema_opt = c_->get_table(statement.table_name);
    if (!schema_opt.has_value()) {
        throw runtime_error("Table does not exist: " + statement.table_name);
    }

    const auto& schema = schema_opt.value();
    if (schema.columns.size() != statement.values.size()) {
        throw runtime_error("Values count does not match table schema for table " + statement.table_name);
    }

    vector<string> storage_values;
    storage_values.reserve(statement.values.size());
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        storage_values.push_back(literal_to_storage(statement.values[i], schema.columns[i]));
    }

    s_->append_row(statement.table_name, storage_values);
    return format_success("1 row inserted into " + statement.table_name);
}

string ExecutionEngine::handle_update(const UpdateStatement& statement) {
    auto schema_opt = c_->get_table(statement.table_name);
    if (!schema_opt.has_value()) {
        throw runtime_error("Table does not exist: " + statement.table_name);
    }
    const auto& schema = schema_opt.value();

    vector<size_t> assignment_indices;
    vector<string> assignment_values;
    assignment_indices.reserve(statement.assignments.size());
    assignment_values.reserve(statement.assignments.size());

    for (const auto& assignment : statement.assignments) {
        size_t index{};
        const ColumnSchema* column = try_find_column(schema, assignment.column_name, index);
        if (column == nullptr) {
            throw runtime_error("Column does not exist: " + assignment.column_name);
        }
        assignment_indices.push_back(index);
        assignment_values.push_back(literal_to_storage(assignment.value, *column));
    }

    auto rows = s_->read_all_rows(statement.table_name);
    size_t affected = 0;

    for (auto& row : rows) {
        TableBinding binding{&schema, &row, schema.name, schema.name};
        EvaluationContext ctx = make_context({binding});
        if (evaluate_condition(statement.where.get(), ctx)) {
            for (size_t i = 0; i < assignment_indices.size(); ++i) {
                row[assignment_indices[i]] = assignment_values[i];
            }
            ++affected;
        }
    }

    if (affected > 0) {
    s_->write_all_rows(statement.table_name, schema, rows);
    }

    return format_success(to_string(affected) + " row(s) updated in " + statement.table_name);
}

string ExecutionEngine::handle_delete(const DeleteStatement& statement) {
    auto schema_opt = c_->get_table(statement.table_name);
    if (!schema_opt.has_value()) {
        throw runtime_error("Table does not exist: " + statement.table_name);
    }
    const auto& schema = schema_opt.value();

    auto rows = s_->read_all_rows(statement.table_name);
    vector<vector<string>> kept_rows;
    kept_rows.reserve(rows.size());
    size_t removed = 0;

    for (auto& row : rows) {
        TableBinding binding{&schema, &row, schema.name, schema.name};
        EvaluationContext ctx = make_context({binding});
        if (evaluate_condition(statement.where.get(), ctx)) {
            ++removed;
        } else {
            kept_rows.push_back(row);
        }
    }

    if (removed > 0) {
    s_->write_all_rows(statement.table_name, schema, kept_rows);
    }

    return format_success(to_string(removed) + " row(s) deleted from " + statement.table_name);
}

struct TableData {
    TableSchema schema;
    string table_name;
    string alias;
    vector<vector<string>> rows;
};

TableData load_table_data(CatalogManager& catalog, StorageManager& storage, const TableReference& reference) {
    auto schema_opt = catalog.get_table(reference.table_name);
    if (!schema_opt.has_value()) {
        throw runtime_error("Table does not exist: " + reference.table_name);
    }

    TableData data;
    data.schema = schema_opt.value();
    data.table_name = reference.table_name;
    data.alias = reference.alias.empty() ? reference.table_name : reference.alias;
    data.rows = storage.read_all_rows(reference.table_name);
    return data;
}

TableBinding make_binding(const TableData& data, const vector<string>& row) {
    return TableBinding{&data.schema, &row, data.table_name, data.alias};
}

const TableData* find_table_data(const vector<const TableData*>& tables, const string& key) {
    for (const auto* table : tables) {
        if (table->alias == key || table->table_name == key) {
            return table;
        }
    }
    return nullptr;
}

vector<string> build_headers(const SelectStatement& statement,
                                       const vector<const TableData*>& tables) {
    vector<string> headers;
    for (const auto& item : statement.select_list) {
        if (item.is_wildcard) {
            if (item.table_alias.empty()) {
                for (const auto* table : tables) {
                    for (const auto& column : table->schema.columns) {
                        headers.push_back(table->alias + '.' + column.name);
                    }
                }
            } else {
                const auto* table = find_table_data(tables, item.table_alias);
                if (table == nullptr) {
                    throw runtime_error("Unknown table alias in wildcard: " + item.table_alias);
                }
                for (const auto& column : table->schema.columns) {
                    headers.push_back(table->alias + '.' + column.name);
                }
            }
        } else {
            string header;
            if (!item.alias.empty()) {
                header = item.alias;
            } else if (!item.table_alias.empty()) {
                header = item.table_alias + '.' + item.column_name;
            } else {
                header = item.column_name;
            }
            headers.push_back(header);
        }
    }
    return headers;
}

string ExecutionEngine::handle_select(const SelectStatement& statement) {
    TableData primary = load_table_data(*c_, *s_, statement.primary_table);
    vector<TableData> join_tables;
    join_tables.reserve(statement.joins.size());
    vector<const TableData*> table_sequence;
    table_sequence.push_back(&primary);

    vector<TableBinding> initial_bindings;
    initial_bindings.reserve(primary.rows.size());

    vector<vector<TableBinding>> current_rows;
    current_rows.reserve(primary.rows.size());
    for (const auto& row : primary.rows) {
        current_rows.push_back({make_binding(primary, row)});
    }

    for (const auto& join_clause : statement.joins) {
    join_tables.push_back(load_table_data(*c_, *s_, join_clause.table));
        const auto& join_table = join_tables.back();
        table_sequence.push_back(&join_table);

        vector<vector<TableBinding>> next_rows;
        for (const auto& existing : current_rows) {
            for (const auto& join_row : join_table.rows) {
                auto candidate = existing;
                candidate.push_back(make_binding(join_table, join_row));
                EvaluationContext ctx = make_context(candidate);
                if (evaluate_condition(join_clause.condition.get(), ctx)) {
                    next_rows.push_back(move(candidate));
                }
            }
        }
        current_rows = move(next_rows);
    }

    if (statement.where) {
        vector<vector<TableBinding>> filtered;
        for (auto& row : current_rows) {
            EvaluationContext ctx = make_context(row);
            if (evaluate_condition(statement.where.get(), ctx)) {
                filtered.push_back(row);
            }
        }
        current_rows = move(filtered);
    }

    auto headers = build_headers(statement, table_sequence);
    vector<vector<string>> result_rows;
    result_rows.reserve(current_rows.size());

    for (const auto& bindings : current_rows) {
        EvaluationContext ctx = make_context(bindings);
        vector<string> row;
        for (const auto& item : statement.select_list) {
            if (item.is_wildcard) {
                if (item.table_alias.empty()) {
                    for (const auto* table : table_sequence) {
                        const auto& binding = resolve_binding(ctx, table->alias);
                        for (size_t i = 0; i < binding.schema->columns.size(); ++i) {
                            row.push_back(binding.row->at(i));
                        }
                    }
                } else {
                    const auto& binding = resolve_binding(ctx, item.table_alias);
                    for (size_t i = 0; i < binding.schema->columns.size(); ++i) {
                        row.push_back(binding.row->at(i));
                    }
                }
            } else {
                db::ColumnExpression column_expr;
                column_expr.table_alias = item.table_alias;
                column_expr.column_name = item.column_name;
                auto lookup = lookup_column(ctx, column_expr);
                row.push_back(lookup.binding->row->at(lookup.index));
            }
        }
        result_rows.push_back(move(row));
    }

    return format_result_table(headers, result_rows);
}

} 
