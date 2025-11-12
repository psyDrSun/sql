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

namespace db {

namespace {

std::string join_path(const std::string& base, const std::string& name) {
    return base + '/' + name + ".csv";
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
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

std::string escape_csv_field(const std::string& value) {
    if (value.find(',') != std::string::npos || value.find('"') != std::string::npos) {
        std::string escaped;
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

std::string join_csv_fields(const std::vector<std::string>& fields) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        oss << escape_csv_field(fields[i]);
        if (i + 1 < fields.size()) {
            oss << ',';
        }
    }
    return oss.str();
}

} // namespace

StorageManager::StorageManager(const std::string& base_path) : base_path_(base_path) {
    std::filesystem::create_directories(base_path_);
}

void StorageManager::create_table_storage(const TableSchema& schema) {
    const auto path = join_path(base_path_, schema.name);
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create storage file: " + path);
    }

    std::vector<std::string> headers;
    headers.reserve(schema.columns.size());
    for (const auto& column : schema.columns) {
        headers.push_back(column.name);
    }
    file << join_csv_fields(headers) << '\n';
}

void StorageManager::drop_table_storage(const std::string& table_name) {
    const auto path = join_path(base_path_, table_name);
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

void StorageManager::rename_table_storage(const std::string& old_name, const std::string& new_name) {
    const auto old_path = join_path(base_path_, old_name);
    const auto new_path = join_path(base_path_, new_name);

    if (!std::filesystem::exists(old_path)) {
        return;
    }

    try {
        std::filesystem::rename(old_path, new_path);
    } catch (const std::filesystem::filesystem_error& ex) {
        throw std::runtime_error("Failed to rename storage file: " + std::string(ex.what()));
    }
}

void StorageManager::add_column(const std::string& table_name, const ColumnSchema& column) {
    const auto path = join_path(base_path_, table_name);
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open table file for column addition: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    bool is_header = true;
    while (std::getline(input, line)) {
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
        throw std::runtime_error("Table storage is empty when attempting to add a column: " + path);
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to write table file during column addition: " + path);
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

void StorageManager::drop_column(const std::string& table_name, const std::string& column_name) {
    const auto path = join_path(base_path_, table_name);
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open table file for column drop: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    std::optional<std::size_t> column_index;
    bool header = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto tokens = split_csv_line(line);

        if (header) {
            header = false;
            auto it = std::find(tokens.begin(), tokens.end(), column_name);
            if (it == tokens.end()) {
                throw std::runtime_error("Column not found in storage: " + column_name);
            }
            column_index = static_cast<std::size_t>(std::distance(tokens.begin(), it));
        }

        if (!column_index.has_value() || column_index.value() >= tokens.size()) {
            throw std::runtime_error("Column index out of range during drop: " + column_name);
        }

        tokens.erase(tokens.begin() + static_cast<std::ptrdiff_t>(column_index.value()));
        lines.push_back(join_csv_fields(tokens));
    }

    input.close();

    if (!column_index.has_value()) {
        throw std::runtime_error("Column not found in storage: " + column_name);
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to write table file during column drop: " + path);
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

void StorageManager::modify_column(const std::string& table_name, const ColumnSchema& column) {
    const auto path = join_path(base_path_, table_name);
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open table file for column modify: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    std::optional<std::size_t> column_index;
    bool header = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto tokens = split_csv_line(line);

        if (header) {
            header = false;
            auto it = std::find(tokens.begin(), tokens.end(), column.name);
            if (it == tokens.end()) {
                throw std::runtime_error("Column not found in storage: " + column.name);
            }
            column_index = static_cast<std::size_t>(std::distance(tokens.begin(), it));
            tokens[column_index.value()] = column.name;
        }

        lines.push_back(join_csv_fields(tokens));
    }

    input.close();

    if (!column_index.has_value()) {
        throw std::runtime_error("Column not found in storage: " + column.name);
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to write table file during column modify: " + path);
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

std::vector<std::vector<std::string>> StorageManager::read_all_rows(const std::string& table_name) const {
    const auto path = join_path(base_path_, table_name);
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open table file for reading: " + path);
    }

    std::string header;
    if (!std::getline(input, header)) {
        return {};
    }

    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        rows.push_back(split_csv_line(line));
    }
    return rows;
}

void StorageManager::append_row(const std::string& table_name, const std::vector<std::string>& values) {
    const auto path = join_path(base_path_, table_name);
    std::ofstream output(path, std::ios::app);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open table file for append: " + path);
    }
    output << join_csv_fields(values) << '\n';
}

void StorageManager::write_all_rows(const std::string& table_name, const TableSchema& schema,
                                    const std::vector<std::vector<std::string>>& rows) {
    const auto path = join_path(base_path_, table_name);
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open table file for write: " + path);
    }

    std::vector<std::string> headers;
    headers.reserve(schema.columns.size());
    for (const auto& column : schema.columns) {
        headers.push_back(column.name);
    }
    output << join_csv_fields(headers) << '\n';

    for (std::size_t i = 0; i < rows.size(); ++i) {
        output << join_csv_fields(rows[i]);
        if (i + 1 < rows.size()) {
            output << '\n';
        }
    }
}

} // namespace db
