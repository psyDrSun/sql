

#include "db/StorageManager.hpp"

#include "db/CatalogManager.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;




using filesystem::create_directories;
using filesystem::exists;
using filesystem::remove;
using filesystem::rename;
using filesystem::filesystem_error;

namespace db {

namespace {



string join_path(const string& base, const string& name) {
    return base + '/' + name + ".csv";
}


vector<string> split_csv_line(const string& line) {
    vector<string> tokens;
    string current;
    bool in_quotes = false;
    for (char ch : line) {
        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (ch == ',' && !in_quotes) {
            tokens.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    tokens.push_back(current);
    return tokens;
}


string escape_csv_field(const string& value) {
    if (value.find(',') != string::npos || value.find('"') != string::npos) {
        string escaped;
        escaped.reserve(value.size());
        for (char ch : value) {
            if (ch == '"') {
                escaped.push_back('"');
            }
            escaped.push_back(ch);
        }
        return '"' + escaped + '"';
    }
    return value;
}


string join_csv_fields(const vector<string>& fields) {
    ostringstream oss;
    for (size_t i = 0; i < fields.size(); ++i) {
        oss << escape_csv_field(fields[i]);
        if (i + 1 < fields.size()) {
            oss << ',';
        }
    }
    return oss.str();
}

} 

StorageManager::StorageManager(const string& base_path) : b_(base_path) {
    create_directories(b_);
}

void StorageManager::create_table_storage(const TableSchema& schema) {
    const auto path = join_path(b_, schema.name);
    ofstream file(path, ios::trunc);
    if (!file.is_open()) {
        throw runtime_error("Failed to create storage file: " + path);
    }

    vector<string> headers;
    headers.reserve(schema.columns.size());
    for (const auto& column : schema.columns) {
        headers.push_back(column.name);
    }
    file << join_csv_fields(headers) << '\n';
}

void StorageManager::drop_table_storage(const string& table_name) {
    const auto path = join_path(b_, table_name);
    if (exists(path)) {
        remove(path);
    }
}

void StorageManager::rename_table_storage(const string& old_name, const string& new_name) {
    const auto old_path = join_path(b_, old_name);
    const auto new_path = join_path(b_, new_name);

    if (!exists(old_path)) {
        return;
    }

    try {
        rename(old_path, new_path);
    } catch (const filesystem_error& ex) {
        throw runtime_error("Failed to rename storage file: " + string(ex.what()));
    }
}

void StorageManager::add_column(const string& table_name, const ColumnSchema& column) {
    const auto path = join_path(b_, table_name);
    ifstream input(path);
    if (!input.is_open()) {
        throw runtime_error("Failed to open table file for column addition: " + path);
    }

    vector<string> lines;
    string line;
    bool is_header = true;
    while (getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto tokens = split_csv_line(line);

        if (is_header) {
            tokens.push_back(column.name);
            is_header = false;
        } else {
            tokens.push_back("");
        }

        lines.push_back(join_csv_fields(tokens));
    }

    input.close();

    if (lines.empty()) {
        throw runtime_error("Table storage is empty when attempting to add a column: " + path);
    }

    ofstream output(path, ios::trunc);
    if (!output.is_open()) {
        throw runtime_error("Failed to write table file during column addition: " + path);
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

void StorageManager::drop_column(const string& table_name, const string& column_name) {
    const auto path = join_path(b_, table_name);
    ifstream input(path);
    if (!input.is_open()) {
        throw runtime_error("Failed to open table file for column drop: " + path);
    }

    vector<string> lines;
    string line;
    optional<size_t> column_index;
    bool header = true;
    while (getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto tokens = split_csv_line(line);

        if (header) {
            header = false;
            auto it = find(tokens.begin(), tokens.end(), column_name);
            if (it == tokens.end()) {
                throw runtime_error("Column not found in storage: " + column_name);
            }
            column_index = static_cast<size_t>(distance(tokens.begin(), it));
        }

        if (!column_index.has_value() || column_index.value() >= tokens.size()) {
            throw runtime_error("Column index out of range during drop: " + column_name);
        }

        tokens.erase(tokens.begin() + static_cast<ptrdiff_t>(column_index.value()));
        lines.push_back(join_csv_fields(tokens));
    }

    input.close();

    if (!column_index.has_value()) {
        throw runtime_error("Column not found in storage: " + column_name);
    }

    ofstream output(path, ios::trunc);
    if (!output.is_open()) {
        throw runtime_error("Failed to write table file during column drop: " + path);
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

void StorageManager::modify_column(const string& table_name, const ColumnSchema& column) {
    const auto path = join_path(b_, table_name);
    ifstream input(path);
    if (!input.is_open()) {
        throw runtime_error("Failed to open table file for column modify: " + path);
    }

    vector<string> lines;
    string line;
    optional<size_t> column_index;
    bool header = true;
    while (getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto tokens = split_csv_line(line);

        if (header) {
            header = false;
            auto it = find(tokens.begin(), tokens.end(), column.name);
            if (it == tokens.end()) {
                throw runtime_error("Column not found in storage: " + column.name);
            }
            column_index = static_cast<size_t>(distance(tokens.begin(), it));
            tokens[column_index.value()] = column.name;
        }

        lines.push_back(join_csv_fields(tokens));
    }

    input.close();

    if (!column_index.has_value()) {
        throw runtime_error("Column not found in storage: " + column.name);
    }

    ofstream output(path, ios::trunc);
    if (!output.is_open()) {
        throw runtime_error("Failed to write table file during column modify: " + path);
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

vector<vector<string>> StorageManager::read_all_rows(const string& table_name) const {
    const auto path = join_path(b_, table_name);
    ifstream input(path);
    if (!input.is_open()) {
        throw runtime_error("Failed to open table file for reading: " + path);
    }

    string header;
    if (!getline(input, header)) {
        return {};
    }

    vector<vector<string>> rows;
    string line;
    while (getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        rows.push_back(split_csv_line(line));
    }
    return rows;
}

void StorageManager::append_row(const string& table_name, const vector<string>& values) {
    const auto path = join_path(b_, table_name);
    ofstream output(path, ios::app);
    if (!output.is_open()) {
        throw runtime_error("Failed to open table file for append: " + path);
    }
    output << join_csv_fields(values) << '\n';
}

void StorageManager::write_all_rows(const string& table_name, const TableSchema& schema,
                                    const vector<vector<string>>& rows) {
    const auto path = join_path(b_, table_name);
    ofstream output(path, ios::trunc);
    if (!output.is_open()) {
        throw runtime_error("Failed to open table file for write: " + path);
    }

    vector<string> headers;
    headers.reserve(schema.columns.size());
    for (const auto& column : schema.columns) {
        headers.push_back(column.name);
    }
    output << join_csv_fields(headers) << '\n';

    for (size_t i = 0; i < rows.size(); ++i) {
        output << join_csv_fields(rows[i]);
        if (i + 1 < rows.size()) {
            output << '\n';
        }
    }
}

} 
