/*
 * ============================================================================
 * 简化版 SQL 解析器 - 标准编译流程演示
 * ============================================================================
 * 
 * 实现: 词法分析 → 语法分析 → 语义分析
 * 
 * 支持语句:
 *   - CREATE TABLE tablename (col1 INT, col2 VARCHAR);
 *   - INSERT INTO tablename VALUES (val1, val2);
 *   - SELECT * FROM tablename WHERE col = val;
 * 
 * 编译: g++ -std=c++17 simplified_parser.cpp -o simple_parser
 * 运行: ./simple_parser
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cctype>
#include <stdexcept>

using namespace std;

// ============================================================================
// 第一阶段: 词法分析 (Lexical Analysis)
// ============================================================================

enum class TokenType {
    // 关键字
    SELECT, INSERT, CREATE, TABLE, INTO, FROM, WHERE, VALUES,
    // 数据类型
    INT, VARCHAR,
    // 标识符和字面量
    IDENTIFIER, NUMBER, STRING,
    // 符号
    COMMA, SEMICOLON, LPAREN, RPAREN, STAR, EQUAL,
    // 结束符
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
    
    void skip_whitespace() {
        while (pos < input.size() && isspace(input[pos])) {
            pos++;
        }
    }
    
    Token scan_identifier() {
        size_t start = pos;
        while (pos < input.size() && (isalnum(input[pos]) || input[pos] == '_')) {
            pos++;
        }
        string word = input.substr(start, pos - start);
        
        // 关键字识别 (大小写不敏感)
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
        
        // 普通标识符
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
        pos++; // 跳过开头的引号
        size_t start = pos;
        while (pos < input.size() && input[pos] != '\'') {
            pos++;
        }
        if (pos >= input.size()) {
            throw runtime_error("❌ 词法错误: 未结束的字符串字面量");
        }
        string value = input.substr(start, pos - start);
        pos++; // 跳过结尾的引号
        return Token(TokenType::STRING, value);
    }
    
public:
    Lexer(const string& sql) : input(sql), pos(0) {}
    
    // 核心函数: 扫描所有 Token
    vector<Token> tokenize() {
        vector<Token> tokens;
        
        while (pos < input.size()) {
            skip_whitespace();
            if (pos >= input.size()) break;
            
            char ch = input[pos];
            
            // 标识符或关键字
            if (isalpha(ch) || ch == '_') {
                tokens.push_back(scan_identifier());
            }
            // 数字
            else if (isdigit(ch)) {
                tokens.push_back(scan_number());
            }
            // 字符串
            else if (ch == '\'') {
                tokens.push_back(scan_string());
            }
            // 符号
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
    
    // 调试函数: 打印所有 Token
    static void print_tokens(const vector<Token>& tokens) {
        cout << "\n📋 词法分析结果 (Token 流):\n";
        cout << "──────────────────────────────────────\n";
        for (size_t i = 0; i < tokens.size() && tokens[i].type != TokenType::END_OF_FILE; i++) {
            cout << "  Token #" << i << ": ";
            cout << "[" << token_type_name(tokens[i].type) << "] ";
            if (!tokens[i].value.empty()) {
                cout << "\"" << tokens[i].value << "\"";
            }
            cout << "\n";
        }
        cout << "\n";
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
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::NUMBER: return "NUMBER";
            case TokenType::STRING: return "STRING";
            case TokenType::COMMA: return "COMMA";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::STAR: return "STAR";
            case TokenType::EQUAL: return "EQUAL";
            default: return "EOF";
        }
    }
};

// ============================================================================
// 第二阶段: 语法分析 (Syntax Analysis) - 构建抽象语法树 (AST)
// ============================================================================

// AST 节点基类
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
};

// 列定义节点
struct ColumnDef : ASTNode {
    string name;
    string type;
    
    ColumnDef(string n, string t) : name(n), type(t) {}
    
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Column: " << name << " (" << type << ")\n";
    }
};

// CREATE TABLE 语句
struct CreateTableStmt : ASTNode {
    string table_name;
    vector<unique_ptr<ColumnDef>> columns;
    
    CreateTableStmt(string name) : table_name(name) {}
    
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "CREATE TABLE: " << table_name << "\n";
        for (const auto& col : columns) {
            col->print(indent + 2);
        }
    }
};

// INSERT 语句
struct InsertStmt : ASTNode {
    string table_name;
    vector<string> values;
    
    InsertStmt(string name) : table_name(name) {}
    
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "INSERT INTO: " << table_name << "\n";
        cout << string(indent + 2, ' ') << "VALUES: [";
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) cout << ", ";
            cout << values[i];
        }
        cout << "]\n";
    }
};

// SELECT 语句
struct SelectStmt : ASTNode {
    string table_name;
    vector<string> columns;
    string where_column;
    string where_value;
    
    SelectStmt(string name) : table_name(name) {}
    
    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "SELECT FROM: " << table_name << "\n";
        cout << string(indent + 2, ' ') << "COLUMNS: [";
        for (size_t i = 0; i < columns.size(); i++) {
            if (i > 0) cout << ", ";
            cout << columns[i];
        }
        cout << "]\n";
        if (!where_column.empty()) {
            cout << string(indent + 2, ' ') << "WHERE: " << where_column << " = " << where_value << "\n";
        }
    }
};

// 语法分析器
class Parser {
private:
    vector<Token> tokens;
    size_t pos;
    
    const Token& current() const {
        return tokens[pos];
    }
    
    const Token& peek(int offset = 1) const {
        if (pos + offset < tokens.size()) {
            return tokens[pos + offset];
        }
        return tokens.back(); // EOF
    }
    
    void advance() {
        if (pos < tokens.size() - 1) {
            pos++;
        }
    }
    
    void expect(TokenType type, const string& context = "") {
        if (current().type != type) {
            string msg = "❌ 语法错误: 期望 " + Lexer::token_type_name(type);
            if (!context.empty()) {
                msg += " (在 " + context + " 中)";
            }
            msg += ", 但得到 " + Lexer::token_type_name(current().type);
            throw runtime_error(msg);
        }
        advance();
    }
    
    // 解析 CREATE TABLE
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
        
        // 解析列定义
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
    
    // 解析 INSERT
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
        
        // 解析值列表
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
    
    // 解析 SELECT
    unique_ptr<SelectStmt> parse_select() {
        expect(TokenType::SELECT);
        
        // 解析列列表 (简化版只支持 *)
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
        
        // 可选的 WHERE 子句
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
    
    // 核心函数: 解析入口
    unique_ptr<ASTNode> parse() {
        if (current().type == TokenType::CREATE) {
            return parse_create_table();
        } else if (current().type == TokenType::INSERT) {
            return parse_insert();
        } else if (current().type == TokenType::SELECT) {
            return parse_select();
        } else {
            throw runtime_error("❌ 语法错误: 不支持的语句类型");
        }
    }
};

// ============================================================================
// 第三阶段: 语义分析 (Semantic Analysis)
// ============================================================================

// 简化的表模式
struct TableSchema {
    string name;
    vector<pair<string, string>> columns; // (列名, 类型)
};

class SemanticAnalyzer {
private:
    map<string, TableSchema> catalog; // 表目录
    
public:
    // 验证 CREATE TABLE
    void analyze_create_table(const CreateTableStmt* stmt) {
        cout << "🔍 语义分析 [CREATE TABLE " << stmt->table_name << "]:\n";
        
        // 检查表是否已存在
        if (catalog.find(stmt->table_name) != catalog.end()) {
            throw runtime_error("❌ 语义错误: 表 '" + stmt->table_name + "' 已存在");
        }
        
        // 检查列名重复
        map<string, bool> col_names;
        for (const auto& col : stmt->columns) {
            if (col_names[col->name]) {
                throw runtime_error("❌ 语义错误: 列名 '" + col->name + "' 重复");
            }
            col_names[col->name] = true;
        }
        
        // 检查列数量
        if (stmt->columns.empty()) {
            throw runtime_error("❌ 语义错误: 表必须至少有一列");
        }
        
        // 注册到目录
        TableSchema schema;
        schema.name = stmt->table_name;
        for (const auto& col : stmt->columns) {
            schema.columns.push_back({col->name, col->type});
        }
        catalog[stmt->table_name] = schema;
        
        cout << "  ✓ 表不存在冲突\n";
        cout << "  ✓ 列名无重复\n";
        cout << "  ✓ 列数量有效 (" << stmt->columns.size() << " 列)\n";
        cout << "  ✓ 已注册到目录\n\n";
    }
    
    // 验证 INSERT
    void analyze_insert(const InsertStmt* stmt) {
        cout << "🔍 语义分析 [INSERT INTO " << stmt->table_name << "]:\n";
        
        // 检查表是否存在
        auto it = catalog.find(stmt->table_name);
        if (it == catalog.end()) {
            throw runtime_error("❌ 语义错误: 表 '" + stmt->table_name + "' 不存在");
        }
        
        const auto& schema = it->second;
        
        // 检查值的数量
        if (stmt->values.size() != schema.columns.size()) {
            throw runtime_error("❌ 语义错误: 值的数量(" + to_string(stmt->values.size()) + 
                              ") 与列数量(" + to_string(schema.columns.size()) + ") 不匹配");
        }
        
        // 简化的类型检查 (仅检查 INT vs 字符串)
        for (size_t i = 0; i < stmt->values.size(); i++) {
            const string& col_type = schema.columns[i].second;
            const string& value = stmt->values[i];
            
            bool is_number = !value.empty() && all_of(value.begin(), value.end(), ::isdigit);
            
            if (col_type == "INT" && !is_number) {
                throw runtime_error("❌ 语义错误: 列 '" + schema.columns[i].first + 
                                  "' 是 INT 类型，但提供了非数字值 '" + value + "'");
            }
        }
        
        cout << "  ✓ 表存在\n";
        cout << "  ✓ 值数量匹配 (" << stmt->values.size() << " 个)\n";
        cout << "  ✓ 类型检查通过\n\n";
    }
    
    // 验证 SELECT
    void analyze_select(const SelectStmt* stmt) {
        cout << "🔍 语义分析 [SELECT FROM " << stmt->table_name << "]:\n";
        
        // 检查表是否存在
        auto it = catalog.find(stmt->table_name);
        if (it == catalog.end()) {
            throw runtime_error("❌ 语义错误: 表 '" + stmt->table_name + "' 不存在");
        }
        
        const auto& schema = it->second;
        
        // 检查 WHERE 列是否存在
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
        
        cout << "  ✓ 表存在\n";
        if (!stmt->where_column.empty()) {
            cout << "  ✓ WHERE 列存在\n";
        }
        cout << "\n";
    }
    
    // 统一入口
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
        cout << "📚 当前表目录:\n";
        cout << "──────────────────────────────────────\n";
        if (catalog.empty()) {
            cout << "  (空)\n\n";
            return;
        }
        for (const auto& [name, schema] : catalog) {
            cout << "  表: " << name << "\n";
            cout << "    列: ";
            for (size_t i = 0; i < schema.columns.size(); i++) {
                if (i > 0) cout << ", ";
                cout << schema.columns[i].first << " (" << schema.columns[i].second << ")";
            }
            cout << "\n\n";
        }
    }
};

// ============================================================================
// 主程序 - 演示三阶段流程
// ============================================================================

void execute_sql(const string& sql, SemanticAnalyzer& analyzer) {
    cout << "\n" << string(70, '=') << "\n";
    cout << "📝 输入 SQL:\n  " << sql << "\n";
    cout << string(70, '=') << "\n";
    
    try {
        // 阶段1: 词法分析
        cout << "\n【阶段 1/3】词法分析 (Lexical Analysis)\n";
        Lexer lexer(sql);
        vector<Token> tokens = lexer.tokenize();
        Lexer::print_tokens(tokens);
        
        // 阶段2: 语法分析
        cout << "【阶段 2/3】语法分析 (Syntax Analysis)\n";
        cout << "──────────────────────────────────────\n";
        Parser parser(tokens);
        unique_ptr<ASTNode> ast = parser.parse();
        cout << "🌳 抽象语法树 (AST):\n";
        ast->print(2);
        cout << "\n";
        
        // 阶段3: 语义分析
        cout << "【阶段 3/3】语义分析 (Semantic Analysis)\n";
        cout << "──────────────────────────────────────\n";
        analyzer.analyze(ast.get());
        
        cout << "✅ 执行成功!\n\n";
        
    } catch (const exception& e) {
        cout << "\n" << e.what() << "\n\n";
    }
}

int main() {
    cout << R"(
╔══════════════════════════════════════════════════════════════════╗
║            简化版 SQL 解析器 - 三阶段编译流程演示                 ║
║                                                                  ║
║  词法分析 → 语法分析 → 语义分析                                   ║
╚══════════════════════════════════════════════════════════════════╝
)";
    
    SemanticAnalyzer analyzer;
    
    // 测试1: CREATE TABLE
    execute_sql("CREATE TABLE students (id INT, name VARCHAR, age INT);", analyzer);
    analyzer.print_catalog();
    
    // 测试2: INSERT (成功)
    execute_sql("INSERT INTO students VALUES (101, 'Alice', 20);", analyzer);
    
    // 测试3: INSERT (失败 - 类型错误)
    execute_sql("INSERT INTO students VALUES ('invalid', 'Bob', 22);", analyzer);
    
    // 测试4: SELECT
    execute_sql("SELECT * FROM students WHERE age = 20;", analyzer);
    
    // 测试5: 表不存在
    execute_sql("SELECT * FROM courses;", analyzer);
    
    // 测试6: 词法错误
    execute_sql("SELECT @ FROM students;", analyzer);
    
    // 测试7: 语法错误
    execute_sql("CREATE TABLE students id INT;", analyzer);
    
    return 0;
}
