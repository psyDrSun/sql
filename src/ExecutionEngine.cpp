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
    const std::vector<std::string>* row{nullptr};
    std::string table_name;
    std::string alias;
};

struct EvaluationContext {
    std::vector<TableBinding> tables;
    std::unordered_map<std::string, std::size_t> lookup;
};

EvaluationContext make_context(const std::vector<TableBinding>& tables) {
    EvaluationContext ctx;
    ctx.tables = tables;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        ctx.lookup[tables[i].table_name] = i;
        if (!tables[i].alias.empty()) {
            ctx.lookup[tables[i].alias] = i;
        }
    }
    return ctx;
}

const TableBinding& resolve_binding(const EvaluationContext& ctx, const std::string& name) {
    auto it = ctx.lookup.find(name);
    if (it == ctx.lookup.end()) {
        throw std::runtime_error("Unknown table or alias: " + name);
    }
    return ctx.tables.at(it->second);
}

const ColumnSchema* try_find_column(const TableSchema& schema, const std::string& column_name, std::size_t& index) {
    for (std::size_t i = 0; i < schema.columns.size(); ++i) {
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
    std::size_t index{};
};

ColumnLookupResult lookup_column(const EvaluationContext& ctx, const db::ColumnExpression& expr) {
    ColumnLookupResult result;

    if (!expr.table_alias.empty()) {
        const auto& binding = resolve_binding(ctx, expr.table_alias);
        std::size_t index{};
        const ColumnSchema* column = try_find_column(*binding.schema, expr.column_name, index);
        if (column == nullptr) {
            throw std::runtime_error("Column not found: " + expr.table_alias + '.' + expr.column_name);
        }
        result.binding = &binding;
        result.column = column;
        result.index = index;
        return result;
    }

    bool found = false;
    for (const auto& binding : ctx.tables) {
        std::size_t index{};
        const ColumnSchema* column = try_find_column(*binding.schema, expr.column_name, index);
        if (column != nullptr) {
            if (found) {
                throw std::runtime_error("Ambiguous column: " + expr.column_name);
            }
            result.binding = &binding;
            result.column = column;
            result.index = index;
            found = true;
        }
    }

    if (!found) {
        throw std::runtime_error("Column not found: " + expr.column_name);
    }

    return result;
}

LiteralValue make_int_literal(std::int64_t value) {
    LiteralValue literal;
    literal.type = LiteralType::Int;
    literal.int_value = value;
    return literal;
}

LiteralValue make_string_literal(const std::string& value) {
    LiteralValue literal;
    literal.type = LiteralType::String;
    literal.string_value = value;
    return literal;
}

LiteralValue storage_to_literal(const ColumnSchema& column, const std::string& value) {
    if (column.type == DataType::Int) {
        if (value.empty()) {
            throw std::runtime_error("Empty value encountered for INT column: " + column.name);
        }
        try {
            return make_int_literal(std::stoll(value));
        } catch (const std::exception&) {
            throw std::runtime_error("Failed to parse INT value for column " + column.name + ": " + value);
        }
    }
    return make_string_literal(value);
}

std::string literal_to_storage(const LiteralValue& literal, const ColumnSchema& column) {
    if (column.type == DataType::Int) {
        if (literal.type != LiteralType::Int) {
            throw std::runtime_error("Type mismatch: column " + column.name + " expects INT");
        }
        return std::to_string(literal.int_value);
    }

    if (literal.type != LiteralType::String) {
        throw std::runtime_error("Type mismatch: column " + column.name + " expects VARCHAR");
    }
    if (column.length > 0 && literal.string_value.size() > column.length) {
        throw std::runtime_error("Value for column " + column.name + " exceeds maximum length");
    }
    return literal.string_value;
}

int compare_literals(const LiteralValue& left, const LiteralValue& right) {
    if (left.type != right.type) {
        throw std::runtime_error("Cannot compare values of different types");
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
            throw std::runtime_error("> comparisons require INT operands");
        }
        return left.int_value > right.int_value;
    case ComparisonOperator::Less:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw std::runtime_error("< comparisons require INT operands");
        }
        return left.int_value < right.int_value;
    case ComparisonOperator::GreaterOrEqual:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw std::runtime_error(">= comparisons require INT operands");
        }
        return left.int_value >= right.int_value;
    case ComparisonOperator::LessOrEqual:
        if (left.type != LiteralType::Int || right.type != LiteralType::Int) {
            throw std::runtime_error("<= comparisons require INT operands");
        }
        return left.int_value <= right.int_value;
    }
    throw std::runtime_error("Unsupported comparison operator");
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
        throw std::runtime_error("Unsupported operand in expression evaluation");
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
        throw std::runtime_error("Unsupported condition expression");
    }
}

std::string format_success(const std::string& message) {
    std::ostringstream oss;
    oss << "OK: " << message;
    return oss.str();
}

std::string format_result_table(const std::vector<std::string>& headers,
                                const std::vector<std::vector<std::string>>& rows) {
    if (headers.empty()) {
        return "(no columns)\n";
    }

    std::vector<std::size_t> widths(headers.size());
    for (std::size_t i = 0; i < headers.size(); ++i) {
        widths[i] = headers[i].size();
    }
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }

    auto make_divider = [&]() {
        std::ostringstream divider;
        for (std::size_t i = 0; i < widths.size(); ++i) {
            divider << std::string(widths[i], '-') ;
            if (i + 1 < widths.size()) {
                divider << "-+-";
            }
        }
        return divider.str();
    };

    std::ostringstream oss;
    auto write_row = [&](const std::vector<std::string>& row) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            oss << std::left << std::setw(static_cast<int>(widths[i])) << row[i];
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

} // namespace

namespace db {

ExecutionEngine::ExecutionEngine(std::shared_ptr<CatalogManager> catalog,
                                 std::shared_ptr<StorageManager> storage)
    : catalog_(std::move(catalog)), storage_(std::move(storage)) {}

std::string ExecutionEngine::execute(const Statement& statement) {
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
        throw std::runtime_error("Unsupported statement type in execution engine");
    }
}

std::string ExecutionEngine::handle_create_table(const CreateTableStatement& statement) {
    TableSchema schema;
    schema.name = statement.table_name;

    for (const auto& column : statement.columns) {
        ColumnSchema column_schema;
        column_schema.name = column.name;
        column_schema.type = column.type;
        column_schema.length = column.length;
        schema.columns.push_back(column_schema);
    }

    catalog_->create_table(schema);
    storage_->create_table_storage(schema);

    return format_success("Table created: " + schema.name);
}

std::string ExecutionEngine::handle_drop_table(const DropTableStatement& statement) {
    catalog_->drop_table(statement.table_name);
    storage_->drop_table_storage(statement.table_name);
    return format_success("Table dropped: " + statement.table_name);
}

std::string ExecutionEngine::handle_alter_table(const AlterTableStatement& statement) {
    switch (statement.action) {
    case AlterTableAction::RenameTable: {
        const auto& old_name = statement.table_name;
        const auto& new_name = statement.new_table_name;

        if (!catalog_->table_exists(old_name)) {
            throw std::runtime_error("Table does not exist: " + old_name);
        }
        if (catalog_->table_exists(new_name)) {
            throw std::runtime_error("Target table already exists: " + new_name);
        }

        storage_->rename_table_storage(old_name, new_name);
        catalog_->rename_table(old_name, new_name);
        return format_success("Table renamed: " + old_name + " -> " + new_name);
    }
    case AlterTableAction::AddColumn: {
        auto schema_opt = catalog_->get_table(statement.table_name);
        if (!schema_opt.has_value()) {
            throw std::runtime_error("Table does not exist: " + statement.table_name);
        }

        const auto& schema = schema_opt.value();
        const auto& new_column = statement.column;

        auto duplicate = std::any_of(schema.columns.begin(), schema.columns.end(), [&](const ColumnSchema& col) {
            return col.name == new_column.name;
        });
        if (duplicate) {
            throw std::runtime_error("Column already exists: " + new_column.name);
        }

        ColumnSchema column_schema{new_column.name, new_column.type, new_column.length};

        storage_->add_column(statement.table_name, column_schema);
        catalog_->add_column(statement.table_name, column_schema);

        return format_success("Column added: " + statement.table_name + '.' + column_schema.name);
    }
    case AlterTableAction::DropColumn: {
        auto schema_opt = catalog_->get_table(statement.table_name);
        if (!schema_opt.has_value()) {
            throw std::runtime_error("Table does not exist: " + statement.table_name);
        }

        const auto& schema = schema_opt.value();
        const auto& column_name = statement.target_column_name;

        auto it = std::find_if(schema.columns.begin(), schema.columns.end(), [&](const ColumnSchema& col) {
            return col.name == column_name;
        });
        if (it == schema.columns.end()) {
            throw std::runtime_error("Column does not exist: " + column_name);
        }
        if (schema.columns.size() <= 1) {
            throw std::runtime_error("Cannot drop the last column from table: " + statement.table_name);
        }

        storage_->drop_column(statement.table_name, column_name);
        catalog_->drop_column(statement.table_name, column_name);

        return format_success("Column dropped: " + statement.table_name + '.' + column_name);
    }
    case AlterTableAction::ModifyColumn: {
        auto schema_opt = catalog_->get_table(statement.table_name);
        if (!schema_opt.has_value()) {
            throw std::runtime_error("Table does not exist: " + statement.table_name);
        }

        const auto& schema = schema_opt.value();
        const auto& target_name = statement.target_column_name;

        auto it = std::find_if(schema.columns.begin(), schema.columns.end(), [&](const ColumnSchema& col) {
            return col.name == target_name;
        });
        if (it == schema.columns.end()) {
            throw std::runtime_error("Column does not exist: " + target_name);
        }

        ColumnSchema column_schema{statement.column.name, statement.column.type, statement.column.length};

        storage_->modify_column(statement.table_name, column_schema);
        catalog_->modify_column(statement.table_name, column_schema);

        return format_success("Column modified: " + statement.table_name + '.' + column_schema.name);
    }
    default:
        throw std::runtime_error("ALTER TABLE action not implemented");
    }
}

std::string ExecutionEngine::handle_insert(const InsertStatement& statement) {
    auto schema_opt = catalog_->get_table(statement.table_name);
    if (!schema_opt.has_value()) {
        throw std::runtime_error("Table does not exist: " + statement.table_name);
    }

    const auto& schema = schema_opt.value();
    if (schema.columns.size() != statement.values.size()) {
        throw std::runtime_error("Values count does not match table schema for table " + statement.table_name);
    }

    std::vector<std::string> storage_values;
    storage_values.reserve(statement.values.size());
    for (std::size_t i = 0; i < schema.columns.size(); ++i) {
        storage_values.push_back(literal_to_storage(statement.values[i], schema.columns[i]));
    }

    storage_->append_row(statement.table_name, storage_values);
    return format_success("1 row inserted into " + statement.table_name);
}

std::string ExecutionEngine::handle_update(const UpdateStatement& statement) {
    auto schema_opt = catalog_->get_table(statement.table_name);
    if (!schema_opt.has_value()) {
        throw std::runtime_error("Table does not exist: " + statement.table_name);
    }
    const auto& schema = schema_opt.value();

    std::vector<std::size_t> assignment_indices;
    std::vector<std::string> assignment_values;
    assignment_indices.reserve(statement.assignments.size());
    assignment_values.reserve(statement.assignments.size());

    for (const auto& assignment : statement.assignments) {
        std::size_t index{};
        const ColumnSchema* column = try_find_column(schema, assignment.column_name, index);
        if (column == nullptr) {
            throw std::runtime_error("Column does not exist: " + assignment.column_name);
        }
        assignment_indices.push_back(index);
        assignment_values.push_back(literal_to_storage(assignment.value, *column));
    }

    auto rows = storage_->read_all_rows(statement.table_name);
    std::size_t affected = 0;

    for (auto& row : rows) {
        TableBinding binding{&schema, &row, schema.name, schema.name};
        EvaluationContext ctx = make_context({binding});
        if (evaluate_condition(statement.where.get(), ctx)) {
            for (std::size_t i = 0; i < assignment_indices.size(); ++i) {
                row[assignment_indices[i]] = assignment_values[i];
            }
            ++affected;
        }
    }

    if (affected > 0) {
        storage_->write_all_rows(statement.table_name, schema, rows);
    }

    return format_success(std::to_string(affected) + " row(s) updated in " + statement.table_name);
}

std::string ExecutionEngine::handle_delete(const DeleteStatement& statement) {
    auto schema_opt = catalog_->get_table(statement.table_name);
    if (!schema_opt.has_value()) {
        throw std::runtime_error("Table does not exist: " + statement.table_name);
    }
    const auto& schema = schema_opt.value();

    auto rows = storage_->read_all_rows(statement.table_name);
    std::vector<std::vector<std::string>> kept_rows;
    kept_rows.reserve(rows.size());
    std::size_t removed = 0;

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
        storage_->write_all_rows(statement.table_name, schema, kept_rows);
    }

    return format_success(std::to_string(removed) + " row(s) deleted from " + statement.table_name);
}

struct TableData {
    TableSchema schema;
    std::string table_name;
    std::string alias;
    std::vector<std::vector<std::string>> rows;
};

TableData load_table_data(CatalogManager& catalog, StorageManager& storage, const TableReference& reference) {
    auto schema_opt = catalog.get_table(reference.table_name);
    if (!schema_opt.has_value()) {
        throw std::runtime_error("Table does not exist: " + reference.table_name);
    }

    TableData data;
    data.schema = schema_opt.value();
    data.table_name = reference.table_name;
    data.alias = reference.alias.empty() ? reference.table_name : reference.alias;
    data.rows = storage.read_all_rows(reference.table_name);
    return data;
}

TableBinding make_binding(const TableData& data, const std::vector<std::string>& row) {
    return TableBinding{&data.schema, &row, data.table_name, data.alias};
}

const TableData* find_table_data(const std::vector<const TableData*>& tables, const std::string& key) {
    for (const auto* table : tables) {
        if (table->alias == key || table->table_name == key) {
            return table;
        }
    }
    return nullptr;
}

std::vector<std::string> build_headers(const SelectStatement& statement,
                                       const std::vector<const TableData*>& tables) {
    std::vector<std::string> headers;
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
                    throw std::runtime_error("Unknown table alias in wildcard: " + item.table_alias);
                }
                for (const auto& column : table->schema.columns) {
                    headers.push_back(table->alias + '.' + column.name);
                }
            }
        } else {
            std::string header;
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

std::string ExecutionEngine::handle_select(const SelectStatement& statement) {
    TableData primary = load_table_data(*catalog_, *storage_, statement.primary_table);
    std::vector<TableData> join_tables;
    join_tables.reserve(statement.joins.size());
    std::vector<const TableData*> table_sequence;
    table_sequence.push_back(&primary);

    std::vector<TableBinding> initial_bindings;
    initial_bindings.reserve(primary.rows.size());

    std::vector<std::vector<TableBinding>> current_rows;
    current_rows.reserve(primary.rows.size());
    for (const auto& row : primary.rows) {
        current_rows.push_back({make_binding(primary, row)});
    }

    for (const auto& join_clause : statement.joins) {
        join_tables.push_back(load_table_data(*catalog_, *storage_, join_clause.table));
        const auto& join_table = join_tables.back();
        table_sequence.push_back(&join_table);

        std::vector<std::vector<TableBinding>> next_rows;
        for (const auto& existing : current_rows) {
            for (const auto& join_row : join_table.rows) {
                auto candidate = existing;
                candidate.push_back(make_binding(join_table, join_row));
                EvaluationContext ctx = make_context(candidate);
                if (evaluate_condition(join_clause.condition.get(), ctx)) {
                    next_rows.push_back(std::move(candidate));
                }
            }
        }
        current_rows = std::move(next_rows);
    }

    if (statement.where) {
        std::vector<std::vector<TableBinding>> filtered;
        for (auto& row : current_rows) {
            EvaluationContext ctx = make_context(row);
            if (evaluate_condition(statement.where.get(), ctx)) {
                filtered.push_back(row);
            }
        }
        current_rows = std::move(filtered);
    }

    auto headers = build_headers(statement, table_sequence);
    std::vector<std::vector<std::string>> result_rows;
    result_rows.reserve(current_rows.size());

    for (const auto& bindings : current_rows) {
        EvaluationContext ctx = make_context(bindings);
        std::vector<std::string> row;
        for (const auto& item : statement.select_list) {
            if (item.is_wildcard) {
                if (item.table_alias.empty()) {
                    for (const auto* table : table_sequence) {
                        const auto& binding = resolve_binding(ctx, table->alias);
                        for (std::size_t i = 0; i < binding.schema->columns.size(); ++i) {
                            row.push_back(binding.row->at(i));
                        }
                    }
                } else {
                    const auto& binding = resolve_binding(ctx, item.table_alias);
                    for (std::size_t i = 0; i < binding.schema->columns.size(); ++i) {
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
        result_rows.push_back(std::move(row));
    }

    return format_result_table(headers, result_rows);
}

} // namespace db
