using namespace std;



#pragma once

#include "Types.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace db {

struct ColumnSchema {
    string name;
    DataType type;
    size_t length;
};

struct TableSchema {
    string name;
    vector<ColumnSchema> columns;
};

class CatalogManager {
public:
    CatalogManager();

    bool table_exists(const string& table_name) const;
    optional<TableSchema> get_table(const string& table_name) const;

    void create_table(const TableSchema& schema);
    void drop_table(const string& table_name);
    void rename_table(const string& old_name, const string& new_name);
    void add_column(const string& table_name, const ColumnSchema& column);
    void drop_column(const string& table_name, const string& column_name);
    void modify_column(const string& table_name, const ColumnSchema& column);

    void refresh();

private:
    void load_catalog();
    void persist_catalog() const;

    unordered_map<string, TableSchema> t_;
    string c_;
};

} 
