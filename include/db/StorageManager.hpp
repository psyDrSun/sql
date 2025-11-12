#pragma once

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace db {

struct TableSchema;
struct ColumnSchema;

class StorageManager {
public:
    explicit StorageManager(const string& base_path);

    void create_table_storage(const TableSchema& schema);
    void drop_table_storage(const string& table_name);
    void rename_table_storage(const string& old_name, const string& new_name);
    void add_column(const string& table_name, const ColumnSchema& column);
    void drop_column(const string& table_name, const string& column_name);
    void modify_column(const string& table_name, const ColumnSchema& column);

    vector<vector<string>> read_all_rows(const string& table_name) const;
    void append_row(const string& table_name, const vector<string>& values);
    void write_all_rows(const string& table_name, const TableSchema& schema,
                        const vector<vector<string>>& rows);

private:
    string base_path_;
};

} // namespace db
