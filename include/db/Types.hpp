using namespace std;



#pragma once

#include <cstddef>
#include <string>


namespace db {

enum class DataType {
    Int,
    Varchar
};

size_t default_length(DataType type);
string type_to_string(DataType type);
DataType parse_type(const string& type_name);

} 
