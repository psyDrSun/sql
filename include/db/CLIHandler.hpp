#pragma once

#include <iosfwd>
#include <memory>
#include <string>

using std::shared_ptr;
using std::string;
using std::istream;

namespace db {

class SQLParser;
class ExecutionEngine;

class CLIHandler {
public:
    CLIHandler(shared_ptr<SQLParser> parser,
               shared_ptr<ExecutionEngine> engine);

    void run();
    void run_script(istream& input);
    void run_watch_mode(const string& sql_file_path);

private:
    shared_ptr<SQLParser> parser_;
    shared_ptr<ExecutionEngine> engine_;
};

} // namespace db
