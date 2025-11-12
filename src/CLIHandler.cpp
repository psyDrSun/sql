#include "db/CLIHandler.hpp"

#include "db/ExecutionEngine.hpp"
#include "db/SQLParser.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

namespace db {
namespace {

std::string trim(const std::string& input) {
    auto begin = input.begin();
    while (begin != input.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = input.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::string remove_sql_comments(const std::string& line) {
    auto pos = line.find("--");
    if (pos != std::string::npos) {
        return line.substr(0, pos);
    }
    return line;
}

void process_stream(std::istream& input,
                    SQLParser& parser,
                    ExecutionEngine& engine,
                    bool interactive) {
    std::string buffer;
    std::string line;

    if (interactive) {
        std::cout << "my-db> " << std::flush;
    }

    while (std::getline(input, line)) {
        // 移除 SQL 注释
        line = remove_sql_comments(line);
        
        auto trimmed_line = trim(line);
        if (interactive && (trimmed_line == ".exit" || trimmed_line == "exit;")) {
            break;
        }

        // 跳过空行
        if (trimmed_line.empty()) {
            if (interactive && buffer.empty()) {
                std::cout << "my-db> " << std::flush;
            }
            continue;
        }

        buffer.append(line);
        buffer.push_back(' ');  // 用空格连接多行，而不是换行

        auto pos = buffer.find(';');
        while (pos != std::string::npos) {
            std::string statement = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            statement = trim(statement);

            if (!statement.empty()) {
                try {
                    auto ast = parser.parse(statement);
                    auto result = engine.execute(*ast);
                    if (!result.empty()) {
                        std::cout << result << '\n';
                    }
                } catch (const std::exception& ex) {
                    std::cout << "Error: " << ex.what() << '\n';
                }
            }

            pos = buffer.find(';');
        }

        if (interactive) {
            std::cout << (buffer.empty() ? "my-db> " : "    -> ") << std::flush;
        }
    }

    if (interactive) {
        std::cout << "Bye!" << std::endl;
    } else {
        auto remaining = trim(buffer);
        if (!remaining.empty()) {
            std::cout << "Error: script ended without terminating ';'" << std::endl;
        }
    }
}

} // namespace

CLIHandler::CLIHandler(std::shared_ptr<SQLParser> parser,
                       std::shared_ptr<ExecutionEngine> engine)
    : parser_(std::move(parser)), engine_(std::move(engine)) {}

void CLIHandler::run() {
    process_stream(std::cin, *parser_, *engine_, true);
}

void CLIHandler::run_script(std::istream& input) {
    process_stream(input, *parser_, *engine_, false);
}

void CLIHandler::run_watch_mode(const std::string& sql_file_path) {
    std::cout << "=== Watch Mode ===" << std::endl;
    std::cout << "Monitoring: " << sql_file_path << std::endl;
    std::cout << "Press ENTER to execute the file, or type 'exit' and press ENTER to quit." << std::endl;
    std::cout << std::endl;

    std::string user_input;
    int execution_count = 0;

    while (true) {
        std::cout << "\n[Press ENTER to run] " << std::flush;
        std::getline(std::cin, user_input);

        auto trimmed = trim(user_input);
        if (trimmed == "exit" || trimmed == ".exit" || trimmed == "quit") {
            std::cout << "Exiting watch mode. Bye!" << std::endl;
            break;
        }

        ++execution_count;
        std::cout << "\n--- Execution #" << execution_count << " ---" << std::endl;

        std::ifstream sql_file(sql_file_path);
        if (!sql_file.is_open()) {
            std::cerr << "Error: Cannot open file: " << sql_file_path << std::endl;
            std::cerr << "Please check the file path and try again." << std::endl;
            continue;
        }

        process_stream(sql_file, *parser_, *engine_, false);
        sql_file.close();

        std::cout << "--- End of execution ---" << std::endl;
    }
}

} // namespace db
