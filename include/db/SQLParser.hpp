using namespace std;



#pragma once

#include "AST.hpp"

#include <memory>
#include <string>


namespace db {

class SQLParser {
public:
    SQLParser();
    StatementPtr parse(const string& sql);
};

using SQLParserPtr = shared_ptr<SQLParser>;

} 
