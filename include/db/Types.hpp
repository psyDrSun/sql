#pragma once

#include <cstddef>
#include <string>

using std::string;
using std::size_t;

namespace db {

enum class DataType {
    Int,
    Varchar
};

size_t default_length(DataType type);
string type_to_string(DataType type);
DataType parse_type(const string& type_name);

} // namespace db
