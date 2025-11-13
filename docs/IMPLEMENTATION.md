# Mini-DBMS 实现细节指南

## 目录

1. [总体设计思路](#总体设计思路)
2. [词法分析实现](#词法分析实现)
3. [语法分析实现](#语法分析实现)
4. [AST 设计与遍历](#ast-设计与遍历)
5. [执行引擎实现](#执行引擎实现)
6. [JOIN 算法详解](#join-算法详解)
7. [存储层设计](#存储层设计)
8. [元数据管理](#元数据管理)
9. [错误处理策略](#错误处理策略)
10. [性能优化思路](#性能优化思路)

---

## 总体设计思路

### 架构选择：三阶段编译 + 解释执行

```
SQL 字符串 → Token 流 → AST → 执行结果
   ↓           ↓         ↓         ↓
 Lexer      Parser   Executor   Output
```

**为什么采用这种架构？**

1. **职责分离**：每个阶段只关注一个问题
   - Lexer: 字符 → Token（识别词素）
   - Parser: Token → AST（识别结构）
   - Executor: AST → 结果（执行操作）

2. **易于扩展**：添加新语法只需修改 Parser 和 Executor
   
3. **易于调试**：可以单独测试每个阶段

4. **符合编译原理经典范式**：便于学习和理解

### 数据流转

```cpp
string sql = "SELECT * FROM users WHERE age > 20;";

SQLParser parser;
auto ast = parser.parse(sql);

ExecutionEngine engine(catalog, storage);
engine.execute(ast.get());
```

**内部流程**:

1. **Lexer 阶段** (`SQLParser::Lexer`)
   ```
   输入: "SELECT * FROM users WHERE age > 20;"
   输出: [SELECT] [*] [FROM] [IDENTIFIER:users] [WHERE] 
         [IDENTIFIER:age] [>] [NUMBER:20] [;]
   ```

2. **Parser 阶段** (`SQLParser::Parser`)
   ```
   输入: Token 流
   输出: SelectStatement {
           columns: ["*"],
           tables: ["users"],
           where: WhereClause {
             conditions: [Condition{left:"age", op:">", right:"20"}]
           }
         }
   ```

3. **Executor 阶段** (`ExecutionEngine`)
   ```
   输入: AST
   步骤:
     1. 从 catalog 获取表结构
     2. 从 storage 读取所有行
     3. 逐行评估 WHERE 条件
     4. 过滤并输出结果
   ```

---

## 词法分析实现

### 核心数据结构

```cpp
class Lexer {
    string input_;     
    size_t pos_;       
    
    Token next();                   
    void skip_whitespace();         
    Token scan_identifier();        
    Token scan_number();            
    Token scan_string();            
};
```

### Token 识别策略

#### 1. 跳过空白字符

```cpp
void Lexer::skip_whitespace() {
    while (pos_ < input_.size() && isspace(input_[pos_])) {
        ++pos_;
    }
}
```

**思路**: 简单循环，消耗所有连续空白字符。

#### 2. 识别标识符和关键字

```cpp
Token Lexer::scan_identifier() {
    size_t start = pos_;
    while (pos_ < input_.size() && 
           (isalnum(input_[pos_]) || input_[pos_] == '_')) {
        ++pos_;
    }
    string word = input_.substr(start, pos_ - start);
    
    string upper = to_upper(word);
    
    if (upper == "SELECT") return Token(TokenType::SELECT, word);
    if (upper == "FROM") return Token(TokenType::FROM, word);
    
    return Token(TokenType::IDENTIFIER, word);
}
```

**关键点**:
- 先贪婪扫描整个单词
- 转换为大写后与关键字对比（实现大小写不敏感）
- 不匹配则归类为普通标识符

**为什么不用哈希表？**
- 关键字数量少（<30 个）
- 线性查找在小数据集上足够快
- 代码更简洁

#### 3. 识别字符串字面量

```cpp
Token Lexer::scan_string() {
    ++pos_;  
    
    string value;
    while (pos_ < input_.size() && input_[pos_] != '\'') {
        if (input_[pos_] == '\'' && pos_ + 1 < input_.size() 
            && input_[pos_ + 1] == '\'') {
            value.push_back('\'');
            pos_ += 2;
        } else {
            value.push_back(input_[pos_]);
            ++pos_;
        }
    }
    
    if (pos_ >= input_.size()) {
        throw runtime_error("Unterminated string literal");
    }
    
    ++pos_;  
    return Token(TokenType::STRING, value);
}
```

**处理的边界情况**:
- 字符串内的单引号转义: `'It''s'` → `It's`
- 未闭合字符串: 抛出异常
- 空字符串: `''` → 空 value

#### 4. 识别数字

```cpp
Token Lexer::scan_number() {
    size_t start = pos_;
    while (pos_ < input_.size() && isdigit(input_[pos_])) {
        ++pos_;
    }
    return Token(TokenType::NUMBER, input_.substr(start, pos_ - start));
}
```

**简化设计**:
- 仅支持整数（不支持浮点数、科学计数法）
- 不检查数字溢出（留给运行时处理）

### 主扫描循环

```cpp
Token Lexer::next() {
    skip_whitespace();
    
    if (pos_ >= input_.size()) {
        return Token(TokenType::END_OF_FILE);
    }
    
    char ch = input_[pos_];
    
    if (isalpha(ch) || ch == '_') {
        return scan_identifier();
    }
    if (isdigit(ch)) {
        return scan_number();
    }
    if (ch == '\'') {
        return scan_string();
    }
    if (ch == ',') {
        ++pos_;
        return Token(TokenType::COMMA, ",");
    }
    
    throw runtime_error(string("Unexpected character: ") + ch);
}
```

**决策树**:
```
当前字符
  ├─ 字母/下划线 → scan_identifier()
  ├─ 数字 → scan_number()
  ├─ 单引号 → scan_string()
  ├─ 单字符符号 → 直接返回对应 Token
  └─ 其他 → 抛出异常
```

---

## 语法分析实现

### 递归下降解析器

**核心思想**: 每个语法规则对应一个函数。

**BNF 示例**:
```
SelectStatement ::= SELECT Columns FROM TableName [WhereClause]
Columns ::= * | ColumnList
ColumnList ::= IDENTIFIER (, IDENTIFIER)*
WhereClause ::= WHERE Condition (AND Condition)*
Condition ::= Expression Operator Expression
```

**对应的 Parser 方法**:
```cpp
unique_ptr<SelectStatement> Parser::parse_select();
void Parser::parse_columns(SelectStatement* stmt);
unique_ptr<WhereClause> Parser::parse_where();
unique_ptr<Condition> Parser::parse_condition();
```

### Token 流管理

```cpp
class Parser {
    vector<Token> tokens_;
    size_t pos_;
    
    const Token& current() const {
        return tokens_[pos_];
    }
    
    const Token& peek(int offset = 1) const {
        if (pos_ + offset < tokens_.size()) {
            return tokens_[pos_ + offset];
        }
        return tokens_.back();  
    }
    
    void advance() {
        if (pos_ < tokens_.size() - 1) {
            ++pos_;
        }
    }
    
    void expect(TokenType type) {
        if (current().type != type) {
            throw runtime_error("Syntax error: expected " + 
                              token_name(type));
        }
        advance();
    }
};
```

**关键方法**:
- `current()`: 查看当前 Token（不消耗）
- `peek()`: 前瞻 N 个 Token（用于决策）
- `advance()`: 消耗当前 Token，移动到下一个
- `expect()`: 断言当前 Token 类型，消耗并前进

### 解析 SELECT 语句示例

```cpp
unique_ptr<SelectStatement> Parser::parse_select() {
    expect(TokenType::SELECT);
    
    auto stmt = make_unique<SelectStatement>();
    
    if (current().type == TokenType::STAR) {
        stmt->columns.push_back("*");
        advance();
    } else {
        while (true) {
            expect(TokenType::IDENTIFIER);
            stmt->columns.push_back(tokens_[pos_ - 1].value);
            
            if (current().type != TokenType::COMMA) {
                break;
            }
            advance();  
        }
    }
    
    expect(TokenType::FROM);
    
    expect(TokenType::IDENTIFIER);
    stmt->tables.push_back(tokens_[pos_ - 1].value);
    
    while (current().type == TokenType::JOIN) {
        stmt->joins.push_back(parse_join());
    }
    
    if (current().type == TokenType::WHERE) {
        stmt->where = parse_where();
    }
    
    expect(TokenType::SEMICOLON);
    
    return stmt;
}
```

**流程图**:
```
parse_select()
  ├─ 消耗 SELECT
  ├─ 解析列列表 (* 或 col1, col2, ...)
  ├─ 消耗 FROM
  ├─ 解析表名
  ├─ 循环解析 JOIN 子句（如果有）
  ├─ 解析 WHERE 子句（如果有）
  └─ 消耗 ;
```

### 错误恢复策略

**当前实现**: 快速失败（Fail Fast）

```cpp
void Parser::expect(TokenType type) {
    if (current().type != type) {
        throw runtime_error("Syntax error at token " + 
                          to_string(pos_) + 
                          ": expected " + token_name(type) +
                          ", got " + token_name(current().type));
    }
    advance();
}
```

**为什么不做错误恢复？**
- 简化实现
- 单用户交互式环境下，快速失败更直观
- 错误消息已足够定位问题

**未来可改进方向**:
- Panic Mode: 跳过 Token 直到遇到 `;` 或关键字
- 同步点恢复: 在每个语句边界尝试继续解析

---

## AST 设计与遍历

### AST 节点继承层次

```
ASTNode (抽象基类)
  ├─ CreateTableStatement
  ├─ DropTableStatement
  ├─ AlterTableStatement
  ├─ InsertStatement
  ├─ SelectStatement
  ├─ UpdateStatement
  ├─ DeleteStatement
  ├─ ColumnDefinition
  ├─ JoinClause
  ├─ WhereClause
  └─ Condition
```

### 为什么使用 `unique_ptr`？

```cpp
unique_ptr<WhereClause> where;
vector<unique_ptr<JoinClause>> joins;
```

**优势**:
1. **自动内存管理**: 无需手动 delete
2. **所有权语义清晰**: 父节点拥有子节点
3. **防止内存泄漏**: 异常安全
4. **零运行时开销**: 编译期优化

**替代方案及缺点**:
- `shared_ptr`: 引用计数开销，AST 是树形结构不需要共享
- 裸指针: 需要手动管理内存，易出错
- 值语义: 大对象拷贝开销高

### Visitor 模式（未实现，但可扩展）

**当前实现**: 直接 `dynamic_cast` 类型判断

```cpp
void ExecutionEngine::execute(const ASTNode* node) {
    if (auto* stmt = dynamic_cast<const SelectStatement*>(node)) {
        handle_select(stmt);
    } else if (auto* stmt = dynamic_cast<const InsertStatement*>(node)) {
        handle_insert(stmt);
    }
    
}
```

**Visitor 模式改进**（如果需要更复杂的遍历）:

```cpp
class ASTVisitor {
public:
    virtual void visit(CreateTableStatement* stmt) = 0;
    virtual void visit(SelectStatement* stmt) = 0;
    
};

class ASTNode {
public:
    virtual void accept(ASTVisitor* visitor) = 0;
};

class SelectStatement : public ASTNode {
    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};
```

**为什么当前不使用 Visitor？**
- 节点类型固定（< 10 种）
- 仅一种遍历方式（执行）
- 代码简洁性优先

---

## 执行引擎实现

### 分发逻辑

```cpp
void ExecutionEngine::execute(const ASTNode* node) {
    if (auto* stmt = dynamic_cast<const CreateTableStatement*>(node)) {
        handle_create_table(stmt);
    } else if (auto* stmt = dynamic_cast<const SelectStatement*>(node)) {
        handle_select(stmt);
    }
    
}
```

**类型判断性能**: O(1)（虚表查找）

### DDL 执行：CREATE TABLE

```cpp
void ExecutionEngine::handle_create_table(const CreateTableStatement* stmt) {
    TableSchema schema;
    schema.name = stmt->table_name;
    
    for (const auto& col_def : stmt->columns) {
        ColumnSchema column;
        column.name = col_def->name;
        column.type = parse_type(col_def->type_name);
        column.length = default_length(column.type);
        schema.columns.push_back(column);
    }
    
    c_->create_table(schema);
    
    s_->create_table_storage(schema);
    
    cout << "Table created successfully." << endl;
}
```

**步骤**:
1. 构建 `TableSchema` 对象
2. 调用 `CatalogManager` 注册元数据
3. 调用 `StorageManager` 创建存储文件
4. 输出成功消息

**原子性问题**: 
- 如果步骤 3 失败，元数据已注册但文件未创建
- **改进方向**: 使用事务或两阶段提交

### DML 执行：INSERT

```cpp
void ExecutionEngine::handle_insert(const InsertStatement* stmt) {
    auto schema = c_->get_table_schema(stmt->table_name);
    
    if (stmt->values.size() != schema.columns.size()) {
        throw runtime_error("Column count mismatch");
    }
    
    for (size_t i = 0; i < stmt->values.size(); ++i) {
        if (schema.columns[i].type == DataType::Int) {
            if (!is_number(stmt->values[i])) {
                throw runtime_error("Type mismatch for column " + 
                                  schema.columns[i].name);
            }
        }
    }
    
    s_->append_row(stmt->table_name, stmt->values);
    
    cout << "1 row inserted." << endl;
}
```

**类型检查策略**:
- 仅检查 INT 类型（值必须是纯数字）
- VARCHAR 类型不检查（任何字符串都接受）
- **改进方向**: 检查 VARCHAR 长度限制

### DQL 执行：SELECT（无 JOIN）

```cpp
void ExecutionEngine::handle_select(const SelectStatement* stmt) {
    auto schema = c_->get_table_schema(stmt->tables[0]);
    auto rows = s_->read_all_rows(stmt->tables[0]);
    
    vector<vector<string>> result_rows;
    for (const auto& row : rows) {
        if (!stmt->where || evaluate_condition(stmt->where.get(), schema, row)) {
            result_rows.push_back(row);
        }
    }
    
    print_table(result_rows, schema);
}
```

**伪代码**:
```
1. 从 storage 读取所有行
2. 对每行:
     if WHERE 条件满足 (或无 WHERE):
         加入结果集
3. 格式化输出结果
```

**时间复杂度**: O(n)，其中 n 是表的行数

### WHERE 条件评估

```cpp
bool ExecutionEngine::evaluate_condition(
    const WhereClause* where,
    const TableSchema& schema,
    const vector<string>& row
) {
    for (const auto& cond : where->conditions) {
        string left_val = get_column_value(cond->left_operand, schema, row);
        string right_val = cond->right_operand;
        
        bool satisfied = false;
        if (cond->op == "=") {
            satisfied = (left_val == right_val);
        } else if (cond->op == ">") {
            satisfied = (stoi(left_val) > stoi(right_val));
        }
        
        
        if (!satisfied) {
            return false;  
        }
    }
    return true;  
}
```

**AND 语义**: 所有条件都必须满足（短路求值）

**类型比较**:
- 字符串操作符（`=`, `!=`）: 字典序比较
- 数值操作符（`<`, `>`, `<=`, `>=`）: 转换为整数后比较

**缺失功能**: OR 逻辑运算符

---

## JOIN 算法详解

### 嵌套循环连接（Nested Loop Join）

**伪代码**:
```
result = rows_from_first_table

for each join_clause in joins:
    temp = []
    join_table_rows = read_all_rows(join_clause.table_name)
    
    for row1 in result:
        for row2 in join_table_rows:
            if join_condition(row1, row2, join_clause):
                merged = merge(row1, row2)
                temp.append(merged)
    
    result = temp

return result
```

**实际代码**:
```cpp
vector<vector<string>> ExecutionEngine::evaluate_joins(
    const SelectStatement* stmt,
    const vector<TableSchema>& all_schemas
) {
    auto base_schema = c_->get_table_schema(stmt->tables[0]);
    auto result = s_->read_all_rows(stmt->tables[0]);
    
    vector<TableSchema> merged_schemas = {base_schema};
    
    for (const auto& join : stmt->joins) {
        auto join_schema = c_->get_table_schema(join->table_name);
        auto join_rows = s_->read_all_rows(join->table_name);
        
        vector<vector<string>> temp;
        
        for (const auto& row1 : result) {
            for (const auto& row2 : join_rows) {
                if (evaluate_join_condition(join.get(), merged_schemas, 
                                           join_schema, row1, row2)) {
                    vector<string> merged_row = row1;
                    merged_row.insert(merged_row.end(), 
                                    row2.begin(), row2.end());
                    temp.push_back(merged_row);
                }
            }
        }
        
        result = move(temp);
        merged_schemas.push_back(join_schema);
    }
    
    return result;
}
```

**时间复杂度分析**:

假设：
- 表 A 有 n 行
- 表 B 有 m 行
- 表 C 有 p 行

**两表 JOIN**: O(n × m)

**三表 JOIN**: O(n × m × p)

**示例性能**（粗略估算）:
- 100 × 100: 10,000 次比较（毫秒级）
- 1,000 × 1,000: 1,000,000 次比较（秒级）
- 10,000 × 10,000: 100,000,000 次比较（分钟级）

### 为什么不用其他算法？

**Hash Join**: 
- 需要额外内存建立哈希表
- 代码复杂度高
- 当前数据规模下收益不明显

**Sort-Merge Join**:
- 需要先排序（O(n log n)）
- 对无序数据不一定更快
- 实现复杂

**Block Nested Loop**:
- 需要内存管理和块策略
- 当前全内存操作已足够

**索引嵌套循环**:
- 需要先实现索引系统
- 复杂度显著增加

### JOIN 条件评估

```cpp
bool ExecutionEngine::evaluate_join_condition(
    const JoinClause* join,
    const vector<TableSchema>& left_schemas,
    const TableSchema& right_schema,
    const vector<string>& left_row,
    const vector<string>& right_row
) {
    string left_val = get_qualified_column_value(
        join->left_table, join->left_column, left_schemas, left_row
    );
    
    string right_val = get_column_value_simple(
        join->right_column, right_schema, right_row
    );
    
    return left_val == right_val;
}
```

**关键**: 需要从多表合并行中定位列值

**定位策略**:
```cpp
string get_qualified_column_value(
    const string& table_name,
    const string& column_name,
    const vector<TableSchema>& schemas,
    const vector<string>& row
) {
    size_t offset = 0;
    for (const auto& schema : schemas) {
        if (schema.name == table_name) {
            for (size_t i = 0; i < schema.columns.size(); ++i) {
                if (schema.columns[i].name == column_name) {
                    return row[offset + i];
                }
            }
        }
        offset += schema.columns.size();
    }
    throw runtime_error("Column not found");
}
```

**思路**: 
1. 遍历已合并的表结构列表
2. 找到目标表，计算其在合并行中的起始偏移
3. 在该表的列中找到目标列，返回对应值

---

## 存储层设计

### CSV 格式选择

**优点**:
- 人类可读，便于调试
- 无需额外解析库
- 与外部工具（Excel, Python pandas）兼容
- 简单可靠

**缺点**:
- 解析开销（相比二进制格式）
- 空间效率低（文本存储数字）
- 无压缩
- 无索引支持

### CSV 解析器实现

#### 字段分割

```cpp
vector<string> split_csv_line(const string& line) {
    vector<string> tokens;
    string current;
    bool in_quotes = false;
    
    for (char ch : line) {
        if (ch == '"') {
            in_quotes = !in_quotes;
        } else if (ch == ',' && !in_quotes) {
            tokens.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    tokens.push_back(current);
    return tokens;
}
```

**处理的边界情况**:
- 引号内的逗号不分割: `"a,b",c` → `["a,b", "c"]`
- 连续逗号表示空字段: `a,,b` → `["a", "", "b"]`
- 引号本身不保留在字段值中

#### 字段转义

```cpp
string escape_csv_field(const string& value) {
    if (value.find(',') == string::npos && 
        value.find('"') == string::npos) {
        return value;
    }
    
    string escaped;
    for (char ch : value) {
        if (ch == '"') {
            escaped.push_back('"');  
        }
        escaped.push_back(ch);
    }
    return '"' + escaped + '"';
}
```

**转义规则**:
- 包含逗号或引号的字段用双引号包围
- 字段内的引号加倍: `"` → `""`
- 示例: `a"b,c` → `"a""b,c"`

### 文件操作策略

#### 读取所有行

```cpp
vector<vector<string>> StorageManager::read_all_rows(
    const string& table_name
) const {
    auto path = join_path(b_, table_name);
    ifstream input(path);
    
    string header;
    getline(input, header);  
    
    vector<vector<string>> rows;
    string line;
    while (getline(input, line)) {
        rows.push_back(split_csv_line(line));
    }
    return rows;
}
```

**全部读入内存**:
- 优点: 简单，随机访问方便
- 缺点: 大文件占用内存高
- 适用场景: 小表（< 10,000 行）

#### 追加写入

```cpp
void StorageManager::append_row(
    const string& table_name,
    const vector<string>& values
) {
    auto path = join_path(b_, table_name);
    ofstream output(path, ios::app);  
    output << join_csv_fields(values) << '\n';
}
```

**性能特征**:
- 时间复杂度: O(列数)
- 无需读取已有数据
- 适合 INSERT 操作

#### 覆盖写入

```cpp
void StorageManager::write_all_rows(
    const string& table_name,
    const TableSchema& schema,
    const vector<vector<string>>& rows
) {
    auto path = join_path(b_, table_name);
    ofstream output(path, ios::trunc);  
    
    output << join_csv_fields(header_from_schema(schema)) << '\n';
    
    for (const auto& row : rows) {
        output << join_csv_fields(row) << '\n';
    }
}
```

**适用场景**:
- UPDATE: 读取 → 修改 → 写回
- DELETE: 读取 → 过滤 → 写回
- ALTER: 调整列结构 → 写回

---

## 元数据管理

### 持久化格式

**文件**: `./data/catalog.meta`

**格式示例**:
```
students
3
id INT 4
name VARCHAR 255
age INT 4
```

**格式规则**:
```
表名
列数量
列名1 类型1 长度1
列名2 类型2 长度2
...
```

### 加载元数据

```cpp
void CatalogManager::load() {
    ifstream file(c_);
    if (!file.is_open()) {
        return;  
    }
    
    while (file.peek() != EOF) {
        TableSchema schema;
        getline(file, schema.name);
        
        size_t col_count;
        file >> col_count;
        file.ignore();  
        
        for (size_t i = 0; i < col_count; ++i) {
            ColumnSchema col;
            string type_str;
            file >> col.name >> type_str >> col.length;
            file.ignore();
            col.type = parse_type(type_str);
            schema.columns.push_back(col);
        }
        
        t_[schema.name] = schema;
    }
}
```

**错误处理**: 
- 文件不存在: 正常（首次运行）
- 文件格式错误: **未处理**（会导致崩溃）
- **改进方向**: 添加格式校验和版本号

### 保存元数据

```cpp
void CatalogManager::save() {
    ofstream file(c_, ios::trunc);
    
    for (const auto& [name, schema] : t_) {
        file << schema.name << '\n';
        file << schema.columns.size() << '\n';
        
        for (const auto& col : schema.columns) {
            file << col.name << ' '
                 << type_to_string(col.type) << ' '
                 << col.length << '\n';
        }
    }
}
```

**何时调用**:
- 每次修改元数据后立即调用
- 无延迟写入，无缓存

**改进方向**:
- 批量提交（减少 I/O 次数）
- 写前备份（防止写入失败损坏元数据）
- 使用结构化格式（JSON/YAML）

---

## 错误处理策略

### 异常类型

**所有错误统一使用 `std::runtime_error`**

**示例**:
```cpp
throw runtime_error("Table '" + table_name + "' does not exist");
throw runtime_error("Syntax error: unexpected token");
throw runtime_error("Failed to open file: " + path);
```

### 错误传播

```
CLI Handler
   ↓ try-catch
SQLParser::parse()
   ↓ throw
Lexer::next()
   ↓ throw (词法错误)

ExecutionEngine::execute()
   ↓ throw
CatalogManager::get_table_schema()
   ↓ throw (表不存在)
```

**顶层捕获**:
```cpp
void CLIHandler::run() {
    while (true) {
        try {
            string sql = read_statement();
            auto ast = p_->parse(sql);
            e_->execute(ast.get());
        } catch (const exception& ex) {
            cerr << "Error: " << ex.what() << endl;
        }
    }
}
```

### 错误恢复

**当前策略**: 打印错误，继续下一个语句

**未实现的恢复机制**:
- 部分执行回滚（无事务）
- 损坏数据修复
- 自动重试

---

## 性能优化思路

### 已实现的优化

1. **移动语义**: 使用 `std::move` 避免大对象拷贝
   ```cpp
   result = move(temp);  
   ```

2. **引用传递**: 避免参数拷贝
   ```cpp
   void execute(const ASTNode* node);  
   ```

3. **预留容量**: 减少动态数组扩容
   ```cpp
   headers.reserve(schema.columns.size());
   ```

4. **短路求值**: AND 条件第一个失败立即返回
   ```cpp
   if (!satisfied) return false;
   ```

### 未实现但可考虑的优化

#### 1. 索引

**B+ 树索引**:
```cpp
class BTreeIndex {
    void insert(const string& key, size_t row_id);
    vector<size_t> search(const string& key);
};
```

**收益**: WHERE 查询从 O(n) 降到 O(log n)

**代价**: 插入变慢，额外存储开销

#### 2. 查询优化器

**当前**: 按语法顺序直接执行

**改进**: 生成查询计划
```cpp
class QueryOptimizer {
    QueryPlan optimize(const SelectStatement* stmt);
};
```

**示例优化**:
- JOIN 顺序调整（小表在前）
- WHERE 条件下推（先过滤再 JOIN）
- 索引选择

#### 3. 列式存储

**当前**: 行式 CSV（每行一条记录）

**改进**: 列式格式（每列一个文件）

**收益**:
- 列投影时仅读取需要的列
- 更好的压缩率（同列数据类型相同）

#### 4. 内存池

**当前**: 每次 `new` 动态分配

**改进**: 预分配内存池
```cpp
class MemoryPool {
    void* allocate(size_t size);
    void reset();
};
```

**收益**: 减少 malloc 调用，提升小对象分配速度

#### 5. 并行执行

**当前**: 单线程顺序执行

**改进**: JOIN 使用线程池并行
```cpp
vector<thread> workers;
for (auto& partition : partitions) {
    workers.emplace_back([&] {
        process_partition(partition);
    });
}
for (auto& t : workers) t.join();
```

**收益**: 大表 JOIN 性能提升（多核 CPU）

**代价**: 并发控制复杂度

### 性能测试方法

**基准测试脚本**:
```sql
CREATE TABLE large_table (id INT, value VARCHAR);


SELECT COUNT(*) FROM ...;  

SELECT * FROM t1 JOIN t2 ON t1.id = t2.id;
```

**测量工具**:
```cpp
auto start = chrono::high_resolution_clock::now();
engine.execute(ast.get());
auto end = chrono::high_resolution_clock::now();
cout << "Time: " 
     << chrono::duration_cast<chrono::milliseconds>(end - start).count()
     << "ms" << endl;
```

---

## 总结

### 核心设计原则

1. **简洁性 > 性能**: 代码易懂比微优化更重要
2. **正确性 > 完备性**: 核心功能正确比功能丰富更重要
3. **可维护性 > 技巧性**: 清晰的结构比炫技更重要

### 代码度量

- **总代码量**: ~2,000 行（不含注释）
- **类数量**: 7 个核心类
- **支持的 SQL 类型**: 7 种语句
- **平均函数长度**: < 30 行

### 适合学习的知识点

1. **编译原理**: 词法分析、语法分析、AST
2. **数据结构**: 树、向量、哈希表
3. **算法**: 递归下降、嵌套循环连接
4. **C++ 特性**: 智能指针、移动语义、异常处理
5. **软件工程**: 模块化、接口设计、错误处理

### 扩展项目建议

1. 添加更多 SQL 功能（GROUP BY, ORDER BY）
2. 实现简单的 B+ 树索引
3. 支持更多数据类型（FLOAT, DATE）
4. 添加查询优化器
5. 实现事务和 MVCC
6. 添加网络协议支持（客户端-服务器）
