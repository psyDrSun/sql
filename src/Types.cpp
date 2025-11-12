#include "db/Types.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

using std::size_t;
using std::string;

namespace db {

size_t default_length(DataType type) {
    switch (type) {
    case DataType::Int:
        return sizeof(int);
    case DataType::Varchar:
        return 255U;
    default:
        return 0U;
    }
}

string type_to_string(DataType type) {
    switch (type) {
    case DataType::Int:
        return "INT";
    case DataType::Varchar:
        return "VARCHAR";
    default:
        return "UNKNOWN";
    }
}

DataType parse_type(const string& type_name) {
    string upper = type_name;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    if (upper == "INT") {
        return DataType::Int;
    }
    if (upper == "VARCHAR") {
        return DataType::Varchar;
    }
    throw std::runtime_error("Unknown data type: " + type_name);
}

} // namespace db
