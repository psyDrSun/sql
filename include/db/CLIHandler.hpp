using namespace std;




#pragma once

#include <iosfwd>
#include <memory>
#include <string>



namespace db {

class SQLParser;
class ExecutionEngine;

class CLIHandler {
public:
    CLIHandler(shared_ptr<SQLParser> p,
               shared_ptr<ExecutionEngine> e);

    void run();
    void run_script(istream& in);
    void run_watch_mode(const string& f);

private:
    shared_ptr<SQLParser> p_;
    shared_ptr<ExecutionEngine> e_;
};

} 
