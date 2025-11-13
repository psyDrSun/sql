

#include "db/CLIHandler.hpp"

#include "db/ExecutionEngine.hpp"
#include "db/SQLParser.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
using namespace std;




namespace db {
namespace {


string trim(const string& input) {
    auto begin = input.begin();
    while (begin != input.end() && isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = input.end();
    while (end != begin && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return string(begin, end);
}


string remove_sql_comments(const string& line) {
    auto pos = line.find("--");
    if (pos != string::npos) {
        return line.substr(0, pos);
    }
    return line;
}


void process_stream(istream& input,
                    SQLParser& parser,
                    ExecutionEngine& engine,
                    bool interactive) {
    string buffer;
    string line;

    if (interactive) {
        cout << "my-db> " << flush;
    }

    while (getline(input, line)) {
        
        line = remove_sql_comments(line);
        
        auto trimmed_line = trim(line);
        if (interactive && (trimmed_line == ".exit" || trimmed_line == "exit;")) {
            break;
        }

        
        if (trimmed_line.empty()) {
            if (interactive && buffer.empty()) {
                cout << "my-db> " << flush;
            }
            continue;
        }

        buffer.append(line);
        buffer.push_back(' ');  

        auto pos = buffer.find(';');
        while (pos != string::npos) {
            string statement = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            statement = trim(statement);

            if (!statement.empty()) {
                try {
                    auto ast = parser.parse(statement);
                    auto result = engine.execute(*ast);
                    if (!result.empty()) {
                        cout << result << '\n';
                    }
                } catch (const exception& ex) {
                    cout << "Error: " << ex.what() << '\n';
                }
            }

            pos = buffer.find(';');
        }

        if (interactive) {
            cout << (buffer.empty() ? "my-db> " : "    -> ") << flush;
        }
    }

    if (interactive) {
        cout << "Bye!" << endl;
    } else {
        auto remaining = trim(buffer);
        if (!remaining.empty()) {
            cout << "Error: script ended without terminating ';'" << endl;
        }
    }
}

} 

CLIHandler::CLIHandler(shared_ptr<SQLParser> p,
                       shared_ptr<ExecutionEngine> e)
    : p_(move(p)), e_(move(e)) {}

void CLIHandler::run() {
    process_stream(cin, *p_, *e_, true);
}

void CLIHandler::run_script(istream& in) {
    process_stream(in, *p_, *e_, false);
}

void CLIHandler::run_watch_mode(const string& f) {
    cout << "=== Watch Mode ===" << endl;
    cout << "Monitoring: " << f << endl;
    cout << "Press ENTER to execute the file, or type 'exit' and press ENTER to quit." << endl;
    cout << endl;

    string user_input;
    int execution_count = 0;

    while (true) {
        cout << "\n[Press ENTER to run] " << flush;
        getline(cin, user_input);

        auto trimmed = trim(user_input);
        if (trimmed == "exit" || trimmed == ".exit" || trimmed == "quit") {
            cout << "Exiting watch mode. Bye!" << endl;
            break;
        }

        ++execution_count;
        cout << "\n--- Execution #" << execution_count << " ---" << endl;

        ifstream sql_file(f);
        if (!sql_file.is_open()) {
            cerr << "Error: Cannot open file: " << f << endl;
            cerr << "Please check the file path and try again." << endl;
            continue;
        }

        process_stream(sql_file, *p_, *e_, false);
        sql_file.close();

        cout << "--- End of execution ---" << endl;
    }
}

} 
