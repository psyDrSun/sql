/*
 * ============================================================================
 * 简化版 SQL 解析器 - 文件读取版本
 * ============================================================================
 * 
 * 使用方法:
 *   ./test_parser demo.sql
 * 
 * 或者 watch 模式 (监听文件变化):
 *   ./test_parser --watch demo.sql
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cctype>
#include <stdexcept>
#include <fstream>
#include <sstream>

using namespace std;

// ============================================================================
// Token 和 Lexer (与之前相同)
// ============================================================================

enum class TokenType {
    SELECT, INSERT, CREATE, TABLE, INTO, FROM, WHERE, VALUES,
    INT, VARCHAR,
    IDENTIFIER, NUMBER, STRING,
    COMMA, SEMICOLON, LPAREN, RPAREN, STAR, EQUAL,
    END_OF_FILE
};

struct Token {
    TokenType type;
    string value;
    Token(TokenType t, string v = "") : type(t), value(v) {}
};

class Lexer {
private:
    string input;
    size_t pos;
    
    void skip_whitespace_and_comments() {
        while (pos < input.size()) {
            // 跳过空白
            if (isspace(input[pos])) {
                pos++;
                continue;
            }
            // 跳过 SQL 注释 --
            if (pos + 1 < input.size() && input[pos] == '-' && input[pos + 1] == '-') {
                while (pos < input.size() && input[pos] != '\n') {
                    pos++;
                }
                continue;
            }
            break;
        }
    }
    
    Token scan_identifier() {
        size_t start = pos;
        while (pos < input.size() && (isalnum(input[pos]) || input[pos] == '_')) {
            pos++;
        }
        string word = input.substr(start, pos - start);
        
        string upper_word = word;
        for (char& c : upper_word) c = toupper(c);
        
        if (upper_word == "SELECT")   return Token(TokenType::SELECT, word);
        if (upper_word == "INSERT")   return Token(TokenType::INSERT, word);
        if (upper_word == "CREATE")   return Token(TokenType::CREATE, word);
        if (upper_word == "TABLE")    return Token(TokenType::TABLE, word);
        if (upper_word == "INTO")     return Token(TokenType::INTO, word);
        if (upper_word == "FROM")     return Token(TokenType::FROM, word);
        if (upper_word == "WHERE")    return Token(TokenType::WHERE, word);
        if (upper_word == "VALUES")   return Token(TokenType::VALUES, word);
        if (upper_word == "INT")      return Token(TokenType::INT, word);
        if (upper_word == "VARCHAR")  return Token(TokenType::VARCHAR, word);
        
        return Token(TokenType::IDENTIFIER, word);
    }
    
    Token scan_number() {
        size_t start = pos;
        while (pos < input.size() && isdigit(input[pos])) {
            pos++;
        }
        return Token(TokenType::NUMBER, input.substr(start, pos - start));
    }
    
    Token scan_string() {
        pos++;
        size_t start = pos;
        while (pos < input.size() && input[pos] != '\'') {
            pos++;
        }
        if (pos >= input.size()) {
            throw runtime_error("❌ 词法错误: 未结束的字符串字面量");
        }
        string value = input.substr(start, pos - start);
        pos++;
        return Token(TokenType::STRING, value);
    }
    
public:
    Lexer(const string& sql) : input(sql), pos(0) {}
    
    vector<Token> tokenize() {
        vector<Token> tokens;
        
        while (pos < input.size()) {
            skip_whitespace_and_comments();
            if (pos >= input.size()) break;
            
            char ch = input[pos];
            
            if (isalpha(ch) || ch == '_') {
                tokens.push_back(scan_identifier());
            }
            else if (isdigit(ch)) {
                tokens.push_back(scan_number());
            }
            else if (ch == '\'') {
                tokens.push_back(scan_string());
            }
            else if (ch == ',') {
                tokens.push_back(Token(TokenType::COMMA, ","));
                pos++;
            }
            else if (ch == ';') {
                tokens.push_back(Token(TokenType::SEMICOLON, ";"));
                pos++;
            }
            else if (ch == '(') {
                tokens.push_back(Token(TokenType::LPAREN, "("));
                pos++;
            }
            else if (ch == ')') {
                tokens.push_back(Token(TokenType::RPAREN, ")"));
                pos++;
            }
            else if (ch == '*') {
                tokens.push_back(Token(TokenType::STAR, "*"));
                pos++;
            }
            else if (ch == '=') {
                tokens.push_back(Token(TokenType::EQUAL, "="));
                pos++;
            }
            else {
                throw runtime_error(string("❌ 词法错误: 无效字符 '") + ch + "'");
            }
        }
        
        tokens.push_back(Token(TokenType::END_OF_FILE));
        return tokens;
    }
    
    static string token_type_name(TokenType type) {
        switch (type) {
            case TokenType::SELECT: return "SELECT";
            case TokenType::INSERT: return "INSERT";
            case TokenType::CREATE: return "CREATE";
            case TokenType::TABLE: return "TABLE";
            case TokenType::INTO: return "INTO";
            case TokenType::FROM: return "FROM";
            case TokenType::WHERE: return "WHERE";
            case TokenType::VALUES: return "VALUES";
            case TokenType::INT: return "INT";
            case TokenType::VARCHAR: return "VARCHAR";
            case TokenType::IDENTIFIER: return "ID";
            case TokenType::NUMBER: return "NUM";
            case TokenType::STRING: return "STR";
            case TokenType::COMMA: return ",";
            case TokenType::SEMICOLON: return ";";
            case TokenType::LPAREN: return "(";
            case TokenType::RPAREN: return ")";
            case TokenType::STAR: return "*";
            case TokenType::EQUAL: return "=";
            default: return "EOF";
        }
    }
};

// ============================================================================
// AST 节点
// ============================================================================

struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
};

struct ColumnDef : ASTNode {
    string name;
    string type;
    ColumnDef(string n, string t) : name(n), type(t) {}
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "  ├─ " << name << " (" << type << ")\n";
    }
};

struct CreateTableStmt : ASTNode {
    string table_name;
    vector<unique_ptr<ColumnDef>> columns;
    CreateTableStmt(string name) : table_name(name) {}
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "📊 CREATE TABLE: " << table_name << "\n";
        for (const auto& col : columns) {
            col->print(indent);
        }
    }
};

struct InsertStmt : ASTNode {
    string table_name;
    vector<string> values;
    InsertStmt(string name) : table_name(name) {}
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "➕ INSERT INTO: " << table_name << "\n";
        cout << string(indent, ' ') << "  └─ VALUES: (";
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) cout << ", ";
            cout << values[i];
        }
        cout << ")\n";
    }
};

struct SelectStmt : ASTNode {
    string table_name;
    vector<string> columns;
    string where_column;
    string where_value;
    SelectStmt(string name) : table_name(name) {}
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "🔍 SELECT FROM: " << table_name << "\n";
        cout << string(indent, ' ') << "  ├─ COLUMNS: ";
        for (size_t i = 0; i < columns.size(); i++) {
            if (i > 0) cout << ", ";
            cout << columns[i];
        }
        cout << "\n";
        if (!where_column.empty()) {
            cout << string(indent, ' ') << "  └─ WHERE: " << where_column << " = " << where_value << "\n";
        }
    }
};

// ============================================================================
// Parser
// ============================================================================

class Parser {
private:
    vector<Token> tokens;
    size_t pos;
    
    const Token& current() const { return tokens[pos]; }
    const Token& peek(int offset = 1) const {
        if (pos + offset < tokens.size()) return tokens[pos + offset];
        return tokens.back();
    }
    void advance() {
        if (pos < tokens.size() - 1) pos++;
    }
    
    void expect(TokenType type, const string& context = "") {
        if (current().type != type) {
            string msg = "❌ 语法错误: 期望 " + Lexer::token_type_name(type);
            if (!context.empty()) msg += " (在 " + context + " 中)";
            msg += ", 但得到 " + Lexer::token_type_name(current().type);
            throw runtime_error(msg);
        }
        advance();
    }
    
    unique_ptr<CreateTableStmt> parse_create_table() {
        expect(TokenType::CREATE);
        expect(TokenType::TABLE, "CREATE TABLE");
        
        if (current().type != TokenType::IDENTIFIER) {
            throw runtime_error("❌ 语法错误: 表名必须是标识符");
        }
        string table_name = current().value;
        advance();
        
        auto stmt = make_unique<CreateTableStmt>(table_name);
        expect(TokenType::LPAREN, "列定义");
        
        while (current().type != TokenType::RPAREN) {
            if (current().type != TokenType::IDENTIFIER) {
                throw runtime_error("❌ 语法错误: 列名必须是标识符");
            }
            string col_name = current().value;
            advance();
            
            if (current().type != TokenType::INT && current().type != TokenType::VARCHAR) {
                throw runtime_error("❌ 语法错误: 列类型必须是 INT 或 VARCHAR");
            }
            string col_type = current().value;
            advance();
            
            stmt->columns.push_back(make_unique<ColumnDef>(col_name, col_type));
            
            if (current().type == TokenType::COMMA) {
                advance();
            } else if (current().type != TokenType::RPAREN) {
                throw runtime_error("❌ 语法错误: 列定义之间需要逗号分隔");
            }
        }
        
        expect(TokenType::RPAREN, "列定义");
        expect(TokenType::SEMICOLON, "语句结束");
        return stmt;
    }
    
    unique_ptr<InsertStmt> parse_insert() {
        expect(TokenType::INSERT);
        expect(TokenType::INTO, "INSERT INTO");
        
        if (current().type != TokenType::IDENTIFIER) {
            throw runtime_error("❌ 语法错误: 表名必须是标识符");
        }
        string table_name = current().value;
        advance();
        
        auto stmt = make_unique<InsertStmt>(table_name);
        expect(TokenType::VALUES, "INSERT INTO");
        expect(TokenType::LPAREN, "VALUES");
        
        while (current().type != TokenType::RPAREN) {
            if (current().type == TokenType::NUMBER || current().type == TokenType::STRING) {
                stmt->values.push_back(current().value);
                advance();
            } else {
                throw runtime_error("❌ 语法错误: 值必须是数字或字符串");
            }
            
            if (current().type == TokenType::COMMA) {
                advance();
            } else if (current().type != TokenType::RPAREN) {
                throw runtime_error("❌ 语法错误: 值之间需要逗号分隔");
            }
        }
        
        expect(TokenType::RPAREN, "VALUES");
        expect(TokenType::SEMICOLON, "语句结束");
        return stmt;
    }
    
    unique_ptr<SelectStmt> parse_select() {
        expect(TokenType::SELECT);
        
        if (current().type == TokenType::STAR) {
            advance();
        } else {
            throw runtime_error("❌ 语法错误: 简化版仅支持 SELECT *");
        }
        
        expect(TokenType::FROM, "SELECT");
        
        if (current().type != TokenType::IDENTIFIER) {
            throw runtime_error("❌ 语法错误: 表名必须是标识符");
        }
        string table_name = current().value;
        advance();
        
        auto stmt = make_unique<SelectStmt>(table_name);
        stmt->columns.push_back("*");
        
        if (current().type == TokenType::WHERE) {
            advance();
            
            if (current().type != TokenType::IDENTIFIER) {
                throw runtime_error("❌ 语法错误: WHERE 列名必须是标识符");
            }
            stmt->where_column = current().value;
            advance();
            
            expect(TokenType::EQUAL, "WHERE 条件");
            
            if (current().type == TokenType::NUMBER || current().type == TokenType::STRING) {
                stmt->where_value = current().value;
                advance();
            } else {
                throw runtime_error("❌ 语法错误: WHERE 值必须是数字或字符串");
            }
        }
        
        expect(TokenType::SEMICOLON, "语句结束");
        return stmt;
    }
    
public:
    Parser(vector<Token> toks) : tokens(std::move(toks)), pos(0) {}
    
    unique_ptr<ASTNode> parse() {
        if (current().type == TokenType::CREATE) {
            return parse_create_table();
        } else if (current().type == TokenType::INSERT) {
            return parse_insert();
        } else if (current().type == TokenType::SELECT) {
            return parse_select();
        } else if (current().type == TokenType::END_OF_FILE) {
            return nullptr;
        } else {
            throw runtime_error("❌ 语法错误: 不支持的语句类型");
        }
    }
};

// ============================================================================
// Semantic Analyzer
// ============================================================================

struct TableSchema {
    string name;
    vector<pair<string, string>> columns;
};

class SemanticAnalyzer {
private:
    map<string, TableSchema> catalog;
    
public:
    void analyze_create_table(const CreateTableStmt* stmt) {
        if (catalog.find(stmt->table_name) != catalog.end()) {
            throw runtime_error("❌ 语义错误: 表 '" + stmt->table_name + "' 已存在");
        }
        
        map<string, bool> col_names;
        for (const auto& col : stmt->columns) {
            if (col_names[col->name]) {
                throw runtime_error("❌ 语义错误: 列名 '" + col->name + "' 重复");
            }
            col_names[col->name] = true;
        }
        
        if (stmt->columns.empty()) {
            throw runtime_error("❌ 语义错误: 表必须至少有一列");
        }
        
        TableSchema schema;
        schema.name = stmt->table_name;
        for (const auto& col : stmt->columns) {
            schema.columns.push_back({col->name, col->type});
        }
        catalog[stmt->table_name] = schema;
        
        cout << "  ✓ 表创建成功\n";
    }
    
    void analyze_insert(const InsertStmt* stmt) {
        auto it = catalog.find(stmt->table_name);
        if (it == catalog.end()) {
            throw runtime_error("❌ 语义错误: 表 '" + stmt->table_name + "' 不存在");
        }
        
        const auto& schema = it->second;
        
        if (stmt->values.size() != schema.columns.size()) {
            throw runtime_error("❌ 语义错误: 值的数量(" + to_string(stmt->values.size()) + 
                              ") 与列数量(" + to_string(schema.columns.size()) + ") 不匹配");
        }
        
        for (size_t i = 0; i < stmt->values.size(); i++) {
            const string& col_type = schema.columns[i].second;
            const string& value = stmt->values[i];
            
            bool is_number = !value.empty() && all_of(value.begin(), value.end(), ::isdigit);
            
            if (col_type == "INT" && !is_number) {
                throw runtime_error("❌ 语义错误: 列 '" + schema.columns[i].first + 
                                  "' 是 INT 类型，但提供了非数字值 '" + value + "'");
            }
        }
        
        cout << "  ✓ 数据插入验证通过\n";
    }
    
    void analyze_select(const SelectStmt* stmt) {
        auto it = catalog.find(stmt->table_name);
        if (it == catalog.end()) {
            throw runtime_error("❌ 语义错误: 表 '" + stmt->table_name + "' 不存在");
        }
        
        const auto& schema = it->second;
        
        if (!stmt->where_column.empty()) {
            bool found = false;
            for (const auto& col : schema.columns) {
                if (col.first == stmt->where_column) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw runtime_error("❌ 语义错误: WHERE 列 '" + stmt->where_column + "' 不存在");
            }
        }
        
        cout << "  ✓ 查询验证通过\n";
    }
    
    void analyze(const ASTNode* node) {
        if (auto* create_stmt = dynamic_cast<const CreateTableStmt*>(node)) {
            analyze_create_table(create_stmt);
        } else if (auto* insert_stmt = dynamic_cast<const InsertStmt*>(node)) {
            analyze_insert(insert_stmt);
        } else if (auto* select_stmt = dynamic_cast<const SelectStmt*>(node)) {
            analyze_select(select_stmt);
        }
    }
    
    void print_catalog() const {
        if (catalog.empty()) {
            cout << "\n📚 表目录: (空)\n";
            return;
        }
        cout << "\n📚 表目录:\n";
        for (const auto& [name, schema] : catalog) {
            cout << "  • " << name << " (";
            for (size_t i = 0; i < schema.columns.size(); i++) {
                if (i > 0) cout << ", ";
                cout << schema.columns[i].first << ":" << schema.columns[i].second;
            }
            cout << ")\n";
        }
    }
};

// ============================================================================
// 文件处理和主程序
// ============================================================================

string read_file(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("❌ 无法打开文件: " + filename);
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

vector<string> split_statements(const string& sql) {
    vector<string> statements;
    string current;
    
    for (char ch : sql) {
        current += ch;
        if (ch == ';') {
            // 去掉空白
            string trimmed;
            for (char c : current) {
                if (!isspace(c) || !trimmed.empty()) {
                    trimmed += c;
                }
            }
            if (!trimmed.empty() && trimmed != ";") {
                statements.push_back(current);
            }
            current.clear();
        }
    }
    
    return statements;
}

void execute_file(const string& filename) {
    cout << "\n" << string(70, '=') << "\n";
    cout << "📄 读取文件: " << filename << "\n";
    cout << string(70, '=') << "\n";
    
    try {
        string content = read_file(filename);
        vector<string> statements = split_statements(content);
        
        SemanticAnalyzer analyzer;
        int success_count = 0;
        int error_count = 0;
        
        for (size_t i = 0; i < statements.size(); i++) {
            string sql = statements[i];
            
            // 跳过空语句和纯注释
            bool is_empty = true;
            for (char ch : sql) {
                if (!isspace(ch) && ch != ';' && ch != '-') {
                    is_empty = false;
                    break;
                }
            }
            if (is_empty) continue;
            
            cout << "\n[语句 " << (i + 1) << "]\n";
            cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            
            // 显示 SQL (去掉前后空白)
            string trimmed_sql = sql;
            size_t first = trimmed_sql.find_first_not_of(" \t\n\r");
            size_t last = trimmed_sql.find_last_not_of(" \t\n\r");
            if (first != string::npos && last != string::npos) {
                trimmed_sql = trimmed_sql.substr(first, last - first + 1);
            }
            cout << "📝 " << trimmed_sql << "\n\n";
            
            try {
                // 词法分析
                Lexer lexer(sql);
                vector<Token> tokens = lexer.tokenize();
                
                // 语法分析
                Parser parser(tokens);
                unique_ptr<ASTNode> ast = parser.parse();
                
                if (!ast) continue; // 空语句
                
                // 显示 AST
                ast->print(0);
                
                // 语义分析
                cout << "\n";
                analyzer.analyze(ast.get());
                
                cout << "✅ 成功\n";
                success_count++;
                
            } catch (const exception& e) {
                cout << e.what() << "\n";
                error_count++;
            }
        }
        
        // 显示表目录
        analyzer.print_catalog();
        
        // 统计
        cout << "\n" << string(70, '=') << "\n";
        cout << "📊 执行统计: ";
        cout << "成功 " << success_count << " 条";
        if (error_count > 0) {
            cout << ", 失败 " << error_count << " 条";
        }
        cout << "\n" << string(70, '=') << "\n\n";
        
    } catch (const exception& e) {
        cout << "\n" << e.what() << "\n\n";
    }
}

int main(int argc, char* argv[]) {
    cout << R"(
╔══════════════════════════════════════════════════════════════════╗
║        简化版 SQL 解析器 - 三阶段编译流程验证工具                 ║
╚══════════════════════════════════════════════════════════════════╝
)";
    
    if (argc < 2) {
        cout << "用法: " << argv[0] << " <sql文件>\n\n";
        cout << "示例:\n";
        cout << "  " << argv[0] << " demo.sql\n\n";
        return 1;
    }
    
    string filename = argv[1];
    execute_file(filename);
    
    return 0;
}
