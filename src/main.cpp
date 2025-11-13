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
using namespace std;





namespace {

void print_usage(const char* executable) {
    cout << "Usage: " << executable << " [options]\n"
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
        throw runtime_error("Line range must use '-' or ':' delimiter");
    }

    auto start_part = spec.substr(0, delimiter);
    auto end_part = spec.substr(delimiter + 1);
    if (start_part.empty() || end_part.empty()) {
        throw runtime_error("Line range requires start and end values");
    }

    size_t start = stoul(start_part);
    size_t end = stoul(end_part);
    if (start == 0 || end == 0) {
        throw runtime_error("Line numbers start at 1");
    }
    if (end < start) {
        throw runtime_error("Line range end must be >= start");
    }
    return {start, end};
}

} 

int main(int argc, char* argv[]) {
    optional<string> f;
    optional<pair<size_t, size_t>> lr;
    optional<string> w;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-f" || arg == "--file") {
            if (i + 1 >= argc) {
                cerr << arg << " requires a file path" << endl;
                return 1;
            }
            f = argv[++i];
        } else if (arg == "-w" || arg == "--watch") {
            if (i + 1 >= argc) {
                cerr << arg << " requires a file path" << endl;
                return 1;
            }
            w = argv[++i];
        } else if (arg == "-l" || arg == "--lines") {
            if (i + 1 >= argc) {
                cerr << arg << " requires a line range" << endl;
                return 1;
            }
            try {
                lr = parse_line_range(argv[++i]);
            } catch (const exception& ex) {
                cerr << "Invalid line range: " << ex.what() << '\n';
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            cerr << "Unknown argument: " << arg << '\n';
            print_usage(argv[0]);
            return 1;
        }
    }

    if (lr && !f) {
        cerr << "--lines requires --file" << endl;
        return 1;
    }

    if (w && f) {
        cerr << "Cannot use --watch and --file together" << endl;
        return 1;
    }

    if (w && lr) {
        cerr << "Cannot use --watch and --lines together" << endl;
        return 1;
    }

    auto c = make_shared<db::CatalogManager>();
    auto s = make_shared<db::StorageManager>("./data");
    auto p = make_shared<db::SQLParser>();
    auto e = make_shared<db::ExecutionEngine>(c, s);
    db::CLIHandler h(p, e);

    try {
        if (w) {
            h.run_watch_mode(*w);
        } else if (f) {
            ifstream sql_file(*f);
            if (!sql_file.is_open()) {
                cerr << "Failed to open SQL file: " << *f << endl;
                return 1;
            }

            if (lr) {
                size_t current_line = 0;
                ostringstream selected;
                string line;
                const auto [start, end] = *lr;
                while (getline(sql_file, line)) {
                    ++current_line;
                    if (current_line >= start && current_line <= end) {
                        selected << line << '\n';
                    }
                    if (current_line > end) {
                        break;
                    }
                }

                if (current_line < start) {
                    cerr << "Line range starts beyond end of file" << endl;
                    return 1;
                }

                auto script = selected.str();
                if (script.empty()) {
                    cerr << "No statements found in requested line range" << endl;
                    return 1;
                }

                istringstream buffer(script);
                h.run_script(buffer);
            } else {
                h.run_script(sql_file);
            }
        } else {
            h.run();
        }
    } catch (const exception& ex) {
        cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        cerr << "Fatal error: unknown exception" << '\n';
        return 1;
    }

    return 0;
}
