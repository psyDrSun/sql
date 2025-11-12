#include "db/CatalogManager.hpp"

#include "db/Types.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using std::string;
using std::vector;
using std::size_t;
using std::optional;
using std::ifstream;
using std::ofstream;
using std::stringstream;
using std::getline;
using std::stoul;
using std::nullopt;
using std::remove_if;
using std::find_if;

namespace db {

namespace {
constexpr const char* kCatalogDir = "data";
constexpr const char* kCatalogFile = "data/catalog.meta";

vector<string> split(const string& input, char delim) {
    vector<string> tokens;
    stringstream ss(input);
    string item;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

} // namespace

CatalogManager::CatalogManager()
    : catalog_path_(kCatalogFile) {
    std::filesystem::create_directories(kCatalogDir);
    load_catalog();
}

bool CatalogManager::table_exists(const string& table_name) const {
    return tables_.find(table_name) != tables_.end();
}

optional<TableSchema> CatalogManager::get_table(const string& table_name) const {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        return nullopt;
    }
    return it->second;
}

void CatalogManager::create_table(const TableSchema& schema) {
    if (table_exists(schema.name)) {
        throw std::runtime_error("Table already exists: " + schema.name);
    }
    tables_.emplace(schema.name, schema);
    persist_catalog();
}

void CatalogManager::drop_table(const string& table_name) {
    if (!table_exists(table_name)) {
        throw std::runtime_error("Table does not exist: " + table_name);
    }
    tables_.erase(table_name);
    persist_catalog();
}

void CatalogManager::rename_table(const string& old_name, const string& new_name) {
    if (!table_exists(old_name)) {
        throw std::runtime_error("Table does not exist: " + old_name);
    }
    if (table_exists(new_name)) {
        throw std::runtime_error("Target table already exists: " + new_name);
    }
    auto schema = tables_.at(old_name);
    schema.name = new_name;
    tables_.erase(old_name);
    tables_.emplace(new_name, std::move(schema));
    persist_catalog();
}

void CatalogManager::add_column(const string& table_name, const ColumnSchema& column) {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        throw std::runtime_error("Table does not exist: " + table_name);
    }
    it->second.columns.push_back(column);
    persist_catalog();
}

void CatalogManager::drop_column(const string& table_name, const string& column_name) {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        throw std::runtime_error("Table does not exist: " + table_name);
    }

    auto& columns = it->second.columns;
    auto new_end = remove_if(columns.begin(), columns.end(), [&](const ColumnSchema& col) {
        return col.name == column_name;
    });

    if (new_end == columns.end()) {
        throw std::runtime_error("Column does not exist: " + column_name);
    }

    columns.erase(new_end, columns.end());
    persist_catalog();
}

void CatalogManager::modify_column(const string& table_name, const ColumnSchema& column) {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) {
        throw std::runtime_error("Table does not exist: " + table_name);
    }

    auto& columns = it->second.columns;
    auto found = find_if(columns.begin(), columns.end(), [&](ColumnSchema& col) {
        return col.name == column.name;
    });

    if (found == columns.end()) {
        throw std::runtime_error("Column does not exist: " + column.name);
    }

    found->type = column.type;
    found->length = column.length;
    persist_catalog();
}

void CatalogManager::refresh() {
    load_catalog();
}

void CatalogManager::load_catalog() {
    tables_.clear();

    ifstream input(catalog_path_);
    if (!input.is_open()) {
        return;
    }

    string line;
    while (getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        auto parts = split(line, '|');
        if (parts.size() < 2) {
            continue;
        }

        TableSchema schema;
        schema.name = parts[0];

        auto column_tokens = split(parts[1], ',');
        for (const auto& token : column_tokens) {
            if (token.empty()) {
                continue;
            }
            auto pieces = split(token, ':');
            if (pieces.size() < 2) {
                continue;
            }

            ColumnSchema column;
            column.name = pieces[0];
            column.type = parse_type(pieces[1]);
            if (pieces.size() > 2) {
                column.length = static_cast<size_t>(stoul(pieces[2]));
            } else {
                column.length = default_length(column.type);
            }
            schema.columns.push_back(column);
        }

        tables_.emplace(schema.name, std::move(schema));
    }
}

void CatalogManager::persist_catalog() const {
    ofstream output(catalog_path_, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open catalog file for writing");
    }

    for (const auto& [name, schema] : tables_) {
        output << name << '|';
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            const auto& column = schema.columns[i];
            output << column.name << ':' << type_to_string(column.type) << ':' << column.length;
            if (i + 1 < schema.columns.size()) {
                output << ',';
            }
        }
        output << '\n';
    }
}

} // namespace db
