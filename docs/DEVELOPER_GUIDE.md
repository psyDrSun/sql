# 开发者入门指南

## 目录

1. [代码库导览](#代码库导览)
2. [从零开始读懂代码](#从零开始读懂代码)
3. [调试技巧](#调试技巧)
4. [如何添加新功能](#如何添加新功能)
5. [测试策略](#测试策略)
6. [常见开发任务](#常见开发任务)

---

## 代码库导览

### 目录结构

```
sql/
├── include/db/         
│   ├── AST.hpp         
│   ├── CLIHandler.hpp  
│   ├── SQLParser.hpp   
│   ├── ExecutionEngine.hpp
│   ├── CatalogManager.hpp
│   ├── StorageManager.hpp
│   └── Types.hpp       
├── src/                
│   ├── main.cpp        
│   ├── CLIHandler.cpp
│   ├── SQLParser.cpp
│   ├── ExecutionEngine.cpp
│   ├── CatalogManager.cpp
│   ├── StorageManager.cpp
│   └── Types.cpp
├── data/               
├── docs/               
└── build/              
```

### 文件职责一览表

| 文件 | 行数 | 职责 | 核心类/函数 |
|------|------|------|-------------|
| `main.cpp` | ~150 | 程序入口，命令行参数解析 | `main()`, `parse_line_range()` |
| `CLIHandler.cpp/hpp` | ~180 | 用户交互，REPL 循环 | `CLIHandler::run()`, `run_script()` |
| `SQLParser.cpp/hpp` | ~730 | SQL 解析，词法+语法分析 | `Lexer`, `Parser` |
| `ExecutionEngine.cpp/hpp` | ~700 | SQL 执行，核心业务逻辑 | `ExecutionEngine::execute()` |
| `CatalogManager.cpp/hpp` | ~200 | 元数据管理，表结构维护 | `CatalogManager` |
| `StorageManager.cpp/hpp` | ~350 | 数据存储，CSV 文件操作 | `StorageManager` |
| `Types.cpp/hpp` | ~50 | 数据类型定义和工具 | `DataType`, `parse_type()` |
| `AST.hpp` | ~180 | AST 节点定义（仅头文件） | 各类 Statement 结构 |

---

## 从零开始读懂代码

### 推荐阅读顺序

#### 第 1 步：理解数据流（15 分钟）

**从 `main.cpp` 开始**：
```cpp
auto c = make_shared<CatalogManager>();
auto s = make_shared<StorageManager>("./data");
auto p = make_shared<SQLParser>();
auto e = make_shared<ExecutionEngine>(c, s);
CLIHandler h(p, e);
h.run();
```

**理解**：
- 系统初始化顺序：元数据管理器 → 存储管理器 → 解析器 → 执行引擎 → CLI
- 依赖关系：ExecutionEngine 依赖 CatalogManager 和 StorageManager

#### 第 2 步：跟踪一条 SQL 的执行路径（30 分钟）

**示例 SQL**: `SELECT * FROM users;`

**路径**:

1. **CLIHandler.cpp** `run()` 方法
   ```cpp
   string sql = read_statement_from_user();
   auto ast = p_->parse(sql);  
   e_->execute(ast.get());     
   ```

2. **SQLParser.cpp** `parse()` 方法
   ```cpp
   Lexer lexer(sql);
   vector<Token> tokens = lexer.tokenize();
   Parser parser(tokens);
   return parser.parse();
   ```

3. **SQLParser.cpp** `Parser::parse_select()`
   ```cpp
   auto stmt = make_unique<SelectStatement>();
   
   stmt->columns = parse_columns();
   stmt->tables = parse_tables();
   stmt->where = parse_where_if_present();
   return stmt;
   ```

4. **ExecutionEngine.cpp** `execute()`
   ```cpp
   if (auto* sel = dynamic_cast<const SelectStatement*>(node)) {
       handle_select(sel);
   }
   ```

5. **ExecutionEngine.cpp** `handle_select()`
   ```cpp
   auto schema = c_->get_table_schema(stmt->tables[0]);
   auto rows = s_->read_all_rows(stmt->tables[0]);
   
   for (const auto& row : rows) {
       if (meets_where_condition(row)) {
           result.push_back(row);
       }
   }
   print_result(result);
   ```

**动手实践**：在每个步骤加断点或 `cout`，观察数据变化。

#### 第 3 步：理解核心数据结构（20 分钟）

**AST 节点继承树**：
```
ASTNode (基类，虚析构函数)
  ├─ CreateTableStatement
  │    ├─ string table_name
  │    └─ vector<unique_ptr<ColumnDefinition>> columns
  │
  ├─ SelectStatement
  │    ├─ vector<string> columns
  │    ├─ vector<string> tables
  │    ├─ vector<unique_ptr<JoinClause>> joins
  │    └─ unique_ptr<WhereClause> where
  │
  └─ ... (其他语句类型)
```

**查看 `include/db/AST.hpp`**，理解每种语句包含哪些信息。

**元数据结构**：
```cpp
struct ColumnSchema {
    string name;       
    DataType type;     
    size_t length;     
};

struct TableSchema {
    string name;
    vector<ColumnSchema> columns;
};
```

**查看 `include/db/CatalogManager.hpp`**，理解表结构如何表示。

#### 第 4 步：深入一个模块（45 分钟）

选择一个你感兴趣的模块深入阅读：

**选项 A：词法分析器**（适合对编译原理感兴趣者）
- 文件：`src/SQLParser.cpp` 中的 `Lexer` 类
- 关键方法：`next()`, `scan_identifier()`, `scan_string()`
- 理解：如何将字符流转换为 Token 流

**选项 B：JOIN 实现**（适合对算法感兴趣者）
- 文件：`src/ExecutionEngine.cpp`
- 关键方法：`evaluate_joins()`, `evaluate_join_condition()`
- 理解：嵌套循环连接算法

**选项 C：CSV 存储**（适合对 I/O 感兴趣者）
- 文件：`src/StorageManager.cpp`
- 关键函数：`split_csv_line()`, `escape_csv_field()`
- 理解：CSV 格式解析和生成

---

## 调试技巧

### 1. 使用 GDB/LLDB

**编译时开启调试符号**：
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**GDB 调试示例**：
```bash
gdb ./build/mini_dbms

(gdb) break SQLParser::parse
(gdb) run
mini-dbms> SELECT * FROM users;

(gdb) print sql
(gdb) next
(gdb) print tokens
```

**常用 GDB 命令**：
- `break <function>`: 设置断点
- `run`: 运行程序
- `next`: 单步执行（不进入函数）
- `step`: 单步执行（进入函数）
- `print <variable>`: 打印变量值
- `backtrace`: 查看调用栈

### 2. 添加日志输出

**在关键位置添加 `cout`**：
```cpp
void ExecutionEngine::handle_select(const SelectStatement* stmt) {
    cout << "[DEBUG] Executing SELECT on table: " << stmt->tables[0] << endl;
    
    auto rows = s_->read_all_rows(stmt->tables[0]);
    cout << "[DEBUG] Read " << rows.size() << " rows" << endl;
    
    
}
```

**编译后运行**：
```
[DEBUG] Executing SELECT on table: users
[DEBUG] Read 3 rows
```

**提示**：使用条件编译控制日志：
```cpp
#ifdef DEBUG_MODE
    cout << "[DEBUG] ..." << endl;
#endif
```

编译时：
```bash
cmake -S . -B build -DDEBUG_MODE=ON
```

### 3. 使用 Valgrind 检测内存问题

```bash
valgrind --leak-check=full ./build/mini_dbms
```

**常见问题**：
- 内存泄漏：忘记释放资源
- 野指针：访问已释放的内存
- 数组越界：访问超出范围的下标

### 4. 单元测试

**创建测试文件** `tests/test_lexer.cpp`：
```cpp
#include "db/SQLParser.hpp"
#include <cassert>
#include <iostream>

void test_scan_identifier() {
    db::SQLParser::Lexer lexer("SELECT FROM");
    auto token1 = lexer.next();
    assert(token1.type == db::TokenType::SELECT);
    
    auto token2 = lexer.next();
    assert(token2.type == db::TokenType::FROM);
    
    cout << "test_scan_identifier PASSED" << endl;
}

int main() {
    test_scan_identifier();
    return 0;
}
```

---

## 如何添加新功能

### 示例 1：添加 `COUNT(*)` 聚合函数

**步骤 1：扩展 AST**

编辑 `include/db/AST.hpp`：
```cpp
struct SelectStatement : ASTNode {
    vector<string> columns;
    vector<string> tables;
    vector<unique_ptr<JoinClause>> joins;
    unique_ptr<WhereClause> where;
    
    bool is_count = false;
};
```

**步骤 2：扩展 Lexer**

编辑 `src/SQLParser.cpp` 的 `Lexer::scan_identifier()`：
```cpp
if (upper == "COUNT") return Token(TokenType::COUNT, word);
```

添加到 `enum class TokenType`：
```cpp
enum class TokenType {
    
    COUNT,
    
};
```

**步骤 3：扩展 Parser**

编辑 `src/SQLParser.cpp` 的 `Parser::parse_select()`：
```cpp
if (current().type == TokenType::COUNT) {
    advance();
    expect(TokenType::LPAREN);
    expect(TokenType::STAR);
    expect(TokenType::RPAREN);
    stmt->is_count = true;
    stmt->columns.push_back("*");
} else {
    
}
```

**步骤 4：扩展 Executor**

编辑 `src/ExecutionEngine.cpp` 的 `handle_select()`：
```cpp
if (stmt->is_count) {
    size_t count = result_rows.size();
    cout << "COUNT(*)" << endl;
    cout << "--------" << endl;
    cout << count << endl;
    return;
}

```

**步骤 5：测试**

```sql
SELECT COUNT(*) FROM users;
```

预期输出：
```
COUNT(*)
--------
5
```

### 示例 2：添加 `ORDER BY` 支持

**步骤 1：扩展 AST**
```cpp
struct SelectStatement : ASTNode {
    
    string order_by_column;  
    bool order_asc = true;   
};
```

**步骤 2：扩展 Lexer**
```cpp
if (upper == "ORDER") return Token(TokenType::ORDER, word);
if (upper == "BY") return Token(TokenType::BY, word);
if (upper == "ASC") return Token(TokenType::ASC, word);
if (upper == "DESC") return Token(TokenType::DESC, word);
```

**步骤 3：扩展 Parser**
```cpp
if (current().type == TokenType::ORDER) {
    advance();
    expect(TokenType::BY);
    expect(TokenType::IDENTIFIER);
    stmt->order_by_column = tokens_[pos_ - 1].value;
    
    if (current().type == TokenType::ASC) {
        stmt->order_asc = true;
        advance();
    } else if (current().type == TokenType::DESC) {
        stmt->order_asc = false;
        advance();
    }
}
```

**步骤 4：扩展 Executor**
```cpp
if (!stmt->order_by_column.empty()) {
    sort(result_rows.begin(), result_rows.end(), 
         [&](const vector<string>& a, const vector<string>& b) {
             size_t col_idx = find_column_index(schema, stmt->order_by_column);
             if (stmt->order_asc) {
                 return a[col_idx] < b[col_idx];
             } else {
                 return a[col_idx] > b[col_idx];
             }
         });
}
```

---

## 测试策略

### 手动测试

**创建测试脚本** `tests/test_order_by.sql`：
```sql
CREATE TABLE products (id INT, name VARCHAR, price INT);
INSERT INTO products VALUES (1, 'Laptop', 5000);
INSERT INTO products VALUES (2, 'Mouse', 50);
INSERT INTO products VALUES (3, 'Keyboard', 150);

SELECT * FROM products ORDER BY price ASC;

SELECT * FROM products ORDER BY price DESC;
```

**运行**：
```bash
./build/mini_dbms -f tests/test_order_by.sql
```

### 自动化测试

**创建测试框架** `tests/run_tests.sh`：
```bash
#!/bin/bash

DBMS="./build/mini_dbms"
TESTS_DIR="./tests"

for test_file in $TESTS_DIR/*.sql; do
    echo "Running $test_file..."
    $DBMS -f $test_file > /tmp/output.txt 2>&1
    
    expected_file="${test_file%.sql}.expected"
    if [ -f "$expected_file" ]; then
        if diff /tmp/output.txt "$expected_file" > /dev/null; then
            echo "✅ PASSED"
        else
            echo "❌ FAILED"
            diff /tmp/output.txt "$expected_file"
        fi
    fi
done
```

**预期输出文件** `tests/test_order_by.expected`：
```
Table created successfully.
1 row inserted.
1 row inserted.
1 row inserted.
+----+----------+-------+
| id | name     | price |
+----+----------+-------+
| 2  | Mouse    | 50    |
| 3  | Keyboard | 150   |
| 1  | Laptop   | 5000  |
+----+----------+-------+
```

### 性能测试

**创建性能测试脚本** `tests/perf_test.sql`：
```sql
CREATE TABLE large_table (id INT, value VARCHAR);


SELECT COUNT(*) FROM large_table;
```

**使用 `time` 命令测量**：
```bash
time ./build/mini_dbms -f tests/perf_test.sql
```

---

## 常见开发任务

### 任务 1：添加新的 SQL 关键字

1. 在 `include/db/AST.hpp` 中定义新的 `TokenType`
2. 在 `src/SQLParser.cpp` 的 `Lexer::scan_identifier()` 中添加识别逻辑
3. 如果涉及新语句，创建对应的 AST 节点类
4. 在 `Parser` 中添加解析方法
5. 在 `ExecutionEngine` 中添加执行逻辑

### 任务 2：修复 Bug

**步骤**：
1. 重现 Bug（创建最小可复现案例）
2. 定位问题代码（使用调试器或日志）
3. 理解根本原因
4. 修复代码
5. 添加测试用例防止回归

**示例 Bug**：
```
问题：WHERE 子句不支持字符串比较
重现：SELECT * FROM users WHERE name = 'Alice';
错误：stoi() 抛出异常（尝试将 'Alice' 转换为整数）
定位：ExecutionEngine::evaluate_condition() 中的类型判断逻辑
修复：检查列类型，VARCHAR 使用字符串比较，INT 使用数值比较
```

### 任务 3：性能优化

**步骤**：
1. 使用性能分析工具（`perf`, `gprof`）找到热点
2. 分析算法复杂度
3. 选择优化策略（缓存、索引、并行等）
4. 实现并测试
5. 对比优化前后的性能

**示例优化**：
```
热点：JOIN 操作占用 95% CPU 时间
分析：嵌套循环 O(n²) 复杂度
策略：为等值 JOIN 列建立哈希表
实现：在 evaluate_joins() 中添加 Hash Join 路径
测试：1000×1000 JOIN 从 30 秒降到 2 秒
```

### 任务 4：重构代码

**常见重构**：
- 提取重复代码为函数
- 分解过长函数（> 50 行）
- 改善命名（去除缩写，使用描述性名称）
- 添加错误处理
- 统一代码风格

**示例重构**：

**重构前**：
```cpp
void handle_select(const SelectStatement* stmt) {
    
}
```

**重构后**：
```cpp
void handle_select(const SelectStatement* stmt) {
    auto result = fetch_and_filter_rows(stmt);
    result = apply_joins(stmt, result);
    result = apply_projection(stmt, result);
    print_result_table(result);
}

vector<vector<string>> fetch_and_filter_rows(const SelectStatement* stmt);
vector<vector<string>> apply_joins(const SelectStatement* stmt, 
                                    const vector<vector<string>>& input);
vector<vector<string>> apply_projection(const SelectStatement* stmt,
                                         const vector<vector<string>>& input);
void print_result_table(const vector<vector<string>>& rows);
```

---

## 开发环境设置

### VS Code 配置

**`.vscode/settings.json`**：
```json
{
    "files.associations": {
        "*.hpp": "cpp",
        "*.cpp": "cpp"
    },
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.default.compilerPath": "/usr/bin/g++",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/include"
    ]
}
```

**`.vscode/tasks.json`**（构建任务）：
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build",
            "type": "shell",
            "command": "cmake --build build",
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

**`.vscode/launch.json`**（调试配置）：
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug mini_dbms",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/mini_dbms",
            "args": [],
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb"
        }
    ]
}
```

### Git 工作流

**分支策略**：
```bash
main         
feature/order-by  
bugfix/where-clause  
```

**提交规范**：
```
feat: Add ORDER BY support
fix: Fix WHERE clause string comparison
docs: Update API reference
refactor: Extract JOIN logic into separate function
test: Add test cases for COUNT(*)
```

---

## 学习资源

### 推荐书籍
1. **《数据库系统概念》** (Silberschatz) - 数据库理论基础
2. **《编译原理》** (龙书) - 词法/语法分析
3. **《C++ Primer》** - C++ 语言深入

### 在线资源
- [CMU 15-445 数据库课程](https://15445.courses.cs.cmu.edu/)
- [Stanford CS143 编译器课程](http://web.stanford.edu/class/cs143/)

### 类似项目
- [SQLite](https://sqlite.org/) - 轻量级数据库
- [TinySQL](https://github.com/talent-plan/tinysql) - TiDB 教学版
- [SimpleDB](https://github.com/iamxpy/SimpleDB) - Java 实现的教学数据库

---

## 获取帮助

- **阅读文档**：先查看 `docs/` 目录下的文档
- **查看代码**：阅读相关模块的源代码
- **搜索问题**：在项目 Issue 中搜索类似问题
- **提问**：在项目仓库提交 Issue，详细描述问题和已尝试的方法

Happy Coding! 🚀
