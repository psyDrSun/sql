#include "db/CLIHandler.hpp"
#include "db/CatalogManager.hpp"
#include "db/ExecutionEngine.hpp"
#include "db/SQLParser.hpp"
#include "db/StorageManager.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

using std::string;
using std::size_t;
using std::pair;
using std::optional;

namespace {

void print_usage(const char* executable) {
    std::cout << "Usage: " << executable << " [options]\n"
              << "  -f, --file <path>         Execute statements from SQL file\n"
              << "  -l, --lines <start-end>   Limit execution to an inclusive line range\n"
              << "  -w, --watch <path>        Watch mode: press ENTER to re-execute SQL file\n"
              << "  -h, --help                Show this help message\n";
}

pair<size_t, size_t> parse_line_range(const string& spec) {
    auto delimiter = spec.find('-');
    if (delimiter == string::npos) {
        delimiter = spec.find(':');
    }
    if (delimiter == string::npos) {
        throw std::runtime_error("Line range must use '-' or ':' delimiter");
    }

    auto start_part = spec.substr(0, delimiter);
    auto end_part = spec.substr(delimiter + 1);
    if (start_part.empty() || end_part.empty()) {
        throw std::runtime_error("Line range requires start and end values");
    }

    size_t start = std::stoul(start_part);
    size_t end = std::stoul(end_part);
    if (start == 0 || end == 0) {
        throw std::runtime_error("Line numbers start at 1");
    }
    if (end < start) {
        throw std::runtime_error("Line range end must be >= start");
    }
    return {start, end};
}

} // namespace

int main(int argc, char* argv[]) {
    optional<string> script_path;
    optional<pair<size_t, size_t>> line_range;
    optional<string> watch_path;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-f" || arg == "--file") {
            if (i + 1 >= argc) {
                std::cerr << arg << " requires a file path" << std::endl;
                return 1;
            }
            script_path = argv[++i];
        } else if (arg == "-w" || arg == "--watch") {
            if (i + 1 >= argc) {
                std::cerr << arg << " requires a file path" << std::endl;
                return 1;
            }
            watch_path = argv[++i];
        } else if (arg == "-l" || arg == "--lines") {
            if (i + 1 >= argc) {
                std::cerr << arg << " requires a line range" << std::endl;
                return 1;
            }
            try {
                line_range = parse_line_range(argv[++i]);
            } catch (const std::exception& ex) {
                std::cerr << "Invalid line range: " << ex.what() << '\n';
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            print_usage(argv[0]);
            return 1;
        }
    }

    if (line_range && !script_path) {
        std::cerr << "--lines requires --file" << std::endl;
        return 1;
    }

    if (watch_path && script_path) {
        std::cerr << "Cannot use --watch and --file together" << std::endl;
        return 1;
    }

    if (watch_path && line_range) {
        std::cerr << "Cannot use --watch and --lines together" << std::endl;
        return 1;
    }

    auto catalog = std::make_shared<db::CatalogManager>();
    auto storage = std::make_shared<db::StorageManager>("./data");
    auto parser = std::make_shared<db::SQLParser>();
    auto engine = std::make_shared<db::ExecutionEngine>(catalog, storage);
    db::CLIHandler cli(parser, engine);

    try {
        if (watch_path) {
            cli.run_watch_mode(*watch_path);
        } else if (script_path) {
            std::ifstream sql_file(*script_path);
            if (!sql_file.is_open()) {
                std::cerr << "Failed to open SQL file: " << *script_path << std::endl;
                return 1;
            }

            if (line_range) {
                size_t current_line = 0;
                std::ostringstream selected;
                string line;
                const auto [start, end] = *line_range;
                while (std::getline(sql_file, line)) {
                    ++current_line;
                    if (current_line >= start && current_line <= end) {
                        selected << line << '\n';
                    }
                    if (current_line > end) {
                        break;
                    }
                }

                if (current_line < start) {
                    std::cerr << "Line range starts beyond end of file" << std::endl;
                    return 1;
                }

                auto script = selected.str();
                if (script.empty()) {
                    std::cerr << "No statements found in requested line range" << std::endl;
                    return 1;
                }

                std::istringstream buffer(script);
                cli.run_script(buffer);
            } else {
                cli.run_script(sql_file);
            }
        } else {
            cli.run();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception" << '\n';
        return 1;
    }

    return 0;
}
