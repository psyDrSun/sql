#include "db/CatalogManager.hpp"

#include "db/Types.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace std;





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

} 

CatalogManager::CatalogManager()
    : c_(kCatalogFile) {
    filesystem::create_directories(kCatalogDir);
    load_catalog();
}

bool CatalogManager::table_exists(const string& n) const {
    return t_.find(n) != t_.end();
}

optional<TableSchema> CatalogManager::get_table(const string& n) const {
    auto it = t_.find(n);
    if (it == t_.end()) {
        return nullopt;
    }
    return it->second;
}

void CatalogManager::create_table(const TableSchema& s) {
    if (table_exists(s.name)) {
        throw runtime_error("Table already exists: " + s.name);
    }
    t_.emplace(s.name, s);
    persist_catalog();
}

void CatalogManager::drop_table(const string& n) {
    if (!table_exists(n)) {
        throw runtime_error("Table does not exist: " + n);
    }
    t_.erase(n);
    persist_catalog();
}

void CatalogManager::rename_table(const string& o, const string& nn) {
    if (!table_exists(o)) {
        throw runtime_error("Table does not exist: " + o);
    }
    if (table_exists(nn)) {
        throw runtime_error("Target table already exists: " + nn);
    }
    auto sch = t_.at(o);
    sch.name = nn;
    t_.erase(o);
    t_.emplace(nn, move(sch));
    persist_catalog();
}

void CatalogManager::add_column(const string& tn, const ColumnSchema& c) {
    auto it = t_.find(tn);
    if (it == t_.end()) {
        throw runtime_error("Table does not exist: " + tn);
    }
    it->second.columns.push_back(c);
    persist_catalog();
}

void CatalogManager::drop_column(const string& tn, const string& cn) {
    auto it = t_.find(tn);
    if (it == t_.end()) {
        throw runtime_error("Table does not exist: " + tn);
    }

    auto& columns = it->second.columns;
    auto new_end = remove_if(columns.begin(), columns.end(), [&](const ColumnSchema& col) {
        return col.name == cn;
    });

    if (new_end == columns.end()) {
        throw runtime_error("Column does not exist: " + cn);
    }

    columns.erase(new_end, columns.end());
    persist_catalog();
}

void CatalogManager::modify_column(const string& tn, const ColumnSchema& c) {
    auto it = t_.find(tn);
    if (it == t_.end()) {
        throw runtime_error("Table does not exist: " + tn);
    }

    auto& columns = it->second.columns;
    auto found = find_if(columns.begin(), columns.end(), [&](ColumnSchema& col) {
        return col.name == c.name;
    });

    if (found == columns.end()) {
        throw runtime_error("Column does not exist: " + c.name);
    }

    found->type = c.type;
    found->length = c.length;
    persist_catalog();
}

void CatalogManager::refresh() {
    load_catalog();
}

void CatalogManager::load_catalog() {
    t_.clear();

    ifstream input(c_);
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

        t_.emplace(schema.name, move(schema));
    }
}

void CatalogManager::persist_catalog() const {
    ofstream output(c_, ios::trunc);
    if (!output.is_open()) {
        throw runtime_error("Failed to open catalog file for writing");
    }

    for (const auto& [name, schema] : t_) {
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

} 
