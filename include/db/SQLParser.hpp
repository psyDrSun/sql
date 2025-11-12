#pragma once

#include "AST.hpp"

#include <memory>
#include <string>

using std::shared_ptr;
using std::string;

namespace db {

class SQLParser {
public:
    SQLParser();
    StatementPtr parse(const string& sql);
};

using SQLParserPtr = shared_ptr<SQLParser>;

} // namespace db
