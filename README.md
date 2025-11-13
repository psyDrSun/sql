# Mini-DBMS

一个轻量级关系数据库管理系统，采用经典的编译器三阶段架构实现，支持完整的 DDL/DML/DQL 操作，包括多表 JOIN 查询。

## 特性

✅ **完整的 SQL 支持**
- DDL: CREATE TABLE, DROP TABLE, ALTER TABLE (RENAME/ADD/DROP/MODIFY COLUMN)
- DML: INSERT, UPDATE, DELETE
- DQL: SELECT with WHERE, multi-table JOIN (链式 INNER JOIN)

✅ **三阶段编译架构**
- 词法分析（Lexer）: 字符流 → Token 流
- 语法分析（Parser）: Token 流 → AST
- 执行引擎（Executor）: AST → 结果

✅ **多种运行模式**
- 交互式 REPL
- 脚本批量执行
- 行范围执行
- Watch 模式（开发调试）

✅ **数据持久化**
- CSV 格式存储（人类可读）
- 元数据自动持久化
- 支持外部工具导入导出

## 快速开始

### 编译

```bash
cmake -S . -B build
cmake --build build
```

### 运行示例

```bash
./build/mini_dbms
```

```sql
CREATE TABLE students (id INT, name VARCHAR, age INT);
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 22);
SELECT * FROM students WHERE age > 20;
exit;
```

### 脚本执行

```bash
./build/mini_dbms --file demo.sql
```

## 文档导航

### 📚 用户文档
- **[用户手册](docs/USER_MANUAL.md)** - SQL 语法、使用示例、常见问题
  - 安装与编译
  - 运行模式详解
  - 完整 SQL 语法参考
  - 使用示例与最佳实践

### 🏗️ 开发者文档
- **[架构设计文档](docs/ARCHITECTURE.md)** - 系统架构、模块设计、数据流
  - 系统总体架构
  - 各模块职责与接口
  - 数据流转与执行流程
  - 性能特征与扩展点

- **[API 参考手册](docs/API_REFERENCE.md)** - 编程接口文档
  - 所有类和方法的详细说明
  - 参数、返回值、异常
  - 使用示例代码

- **[实现细节指南](docs/IMPLEMENTATION.md)** - 核心算法与代码思路
  - 词法/语法分析实现
  - AST 设计与遍历
  - JOIN 算法详解
  - 存储层与元数据管理
  - 性能优化思路

## 项目结构

```
.
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
│   ├── catalog.meta            
│   └── *.csv                   
├── docs/                
│   ├── ARCHITECTURE.md         
│   ├── API_REFERENCE.md        
│   ├── USER_MANUAL.md          
│   └── IMPLEMENTATION.md       
└── CMakeLists.txt       
```

## 技术栈

- **语言**: C++17
- **构建系统**: CMake 3.10+
- **标准库**: 仅使用 STL（无第三方依赖）
- **存储格式**: CSV 文本文件

## SQL 语法示例

### DDL - 数据定义

```sql
CREATE TABLE employees (id INT, name VARCHAR, salary INT);

ALTER TABLE employees ADD COLUMN department VARCHAR;
ALTER TABLE employees DROP COLUMN salary;
ALTER TABLE employees MODIFY COLUMN name VARCHAR;
ALTER TABLE employees RENAME TO staff;

DROP TABLE staff;
```

### DML - 数据操作

```sql
INSERT INTO employees VALUES (1, 'Alice', 5000, 'Engineering');
UPDATE employees SET salary = 5500 WHERE id = 1;
DELETE FROM employees WHERE salary < 3000;
```

### DQL - 数据查询

```sql
SELECT * FROM employees;
SELECT name, salary FROM employees WHERE salary > 4000;

SELECT * 
FROM employees 
JOIN departments ON employees.dept_id = departments.id
WHERE departments.name = 'Engineering';

SELECT * 
FROM students 
JOIN enrollments ON students.id = enrollments.student_id
JOIN courses ON enrollments.course_id = courses.id;
```

## 运行模式

### 交互式模式（REPL）
```bash
./build/mini_dbms
```

### 脚本执行
```bash
./build/mini_dbms --file script.sql
```

### 行范围执行
```bash
./build/mini_dbms --file script.sql --lines 10-20
```

### Watch 模式（开发调试）
```bash
./build/mini_dbms --watch demo.sql
```

## 性能特征

### 适用规模
- ✅ 单表 < 1,000 行: 性能良好
- ⚠️  单表 1,000 - 10,000 行: 可用但较慢
- ❌ 单表 > 10,000 行: 不推荐

### JOIN 性能
- 两表 JOIN（各 100 行）: < 1 秒
- 两表 JOIN（各 1,000 行）: 数秒
- 不建议超过 3 表 JOIN

### 时间复杂度
- SELECT（无 JOIN）: O(n)
- WHERE 过滤: O(n)
- 两表 JOIN: O(n × m)
- 三表 JOIN: O(n × m × p)

## 限制

### 功能限制
- ❌ 无索引（全表扫描）
- ❌ 无事务支持
- ❌ 无并发控制
- ❌ 仅支持 INT 和 VARCHAR 类型
- ❌ 不支持子查询、GROUP BY、ORDER BY

### 适用场景
- ✅ 学习数据库原理
- ✅ 快速原型验证
- ✅ 小规模数据分析
- ✅ 教学演示

### 不适用场景
- ❌ 生产环境
- ❌ 多用户 Web 应用
- ❌ 大数据处理
- ❌ 高并发场景

## 示例用例

完整示例请参考 [`demo.sql`](demo.sql) 或 [`verification_scenario.sql`](verification_scenario.sql)。

## 贡献指南

欢迎提交 Issue 和 Pull Request！

主要扩展方向：
1. 添加索引支持（B+ 树）
2. 实现查询优化器
3. 支持更多数据类型（FLOAT, DATE, BLOB）
4. 添加 GROUP BY / ORDER BY
5. 实现事务和 MVCC

## 许可证

MIT License

## 作者

psyDrSun

## 致谢

本项目用于学习数据库系统原理，参考了经典的教科书和开源数据库实现。

A miniature SQL database engine written in modern C++17. The system exposes a CLI that accepts multi-line SQL and persists data on disk. The current build supports the core DDL, DML, and the subset of DQL needed for common CRUD workflows.

## Features
- Modular architecture: CLI handler, SQL parser, execution engine, catalog and storage managers.
- Support for `CREATE TABLE`, `DROP TABLE`, all four `ALTER TABLE` actions (rename/add/drop/modify column), plus `INSERT`, `UPDATE`, `DELETE`, and `SELECT` with simple joins and predicates.
- File-backed storage with per-table CSV files and catalog metadata tracker.
- Defensive error handling with clear, user-facing messages.
- SQL comment support (`--`) and multi-line statement parsing.

## Requirements
- CMake 3.15+
- A C++17-compatible compiler (clang++, g++)

## Build
```bash
cmake -S . -B build
cmake --build build
```
The resulting binary lives at `build/mini_dbms`.

## Usage

### Interactive Mode
```bash
./build/mini_dbms
```
Example:
```sql
my-db> CREATE TABLE students (id INT, name VARCHAR);
OK: Table created: students
my-db> INSERT INTO students VALUES (1, 'Alice');
OK: 1 row inserted into students
my-db> SELECT * FROM students;
id | name 
---+------
1  | Alice
(1 row)
my-db> exit;
```

### Watch Mode (Recommended)
Edit SQL in a file, save, and press ENTER to execute:
```bash
./build/mini_dbms --watch demo.sql
```
Workflow:
1. Edit `demo.sql` in your editor
2. Save (Cmd+S / Ctrl+S)
3. Switch to terminal and press ENTER
4. See results immediately
5. Type `exit` and press ENTER to quit

### Execute SQL File
```bash
./build/mini_dbms --file demo.sql
```

### Execute Specific Lines
```bash
./build/mini_dbms --file demo.sql --lines 1-10
```

## Project Layout
```
include/db/    # Public headers (AST, managers, parser, engine, types)
src/           # Implementation files
build/         # CMake build outputs (generated)
data/          # Runtime catalog + table storage (created on demand)
demo.sql       # Example SQL file for watch mode
```

## Supported SQL Syntax

### DDL (Data Definition Language)
- `CREATE TABLE table_name (col1 INT, col2 VARCHAR);`
- `DROP TABLE table_name;`
- `ALTER TABLE table_name RENAME TO new_name;`
- `ALTER TABLE table_name ADD COLUMN col_name VARCHAR;`
- `ALTER TABLE table_name DROP COLUMN col_name;`
- `ALTER TABLE table_name MODIFY COLUMN col_name INT;`

### DML (Data Manipulation Language)
- `INSERT INTO table_name VALUES (val1, val2);`
- `UPDATE table_name SET col1 = val WHERE col2 = val;`
- `DELETE FROM table_name WHERE col = val;`

### DQL (Data Query Language)
- `SELECT * FROM table_name;`
- `SELECT col1, col2 FROM table_name WHERE col3 > 10;`
- `SELECT t1.col1, t2.col2 FROM table1 t1 JOIN table2 t2 ON t1.id = t2.id;`
- Supports: `=`, `<>`, `>`, `<`, `>=`, `<=`, `AND`

## Contributing
- Run `clang-format` (or your style tool) before submitting patches.
- Add unit or integration coverage for new SQL syntax or execution paths.
- Report bugs with the SQL statement, observed output, and the expected behaviour.

## Developer Tips: simplify `std::` qualifiers

If you prefer unqualified standard symbols (e.g., write `vector<string>` instead of `std::vector<std::string>`), this repo includes a helper script that:
- Inserts `using std::X;` for single-segment standard names used in a file (vector, string, getline, ifstream, etc.).
- Rewrites occurrences like `std::vector` -> `vector` within that file.
- Leaves nested forms (e.g., `std::string::npos`, `std::filesystem::rename`) unchanged for safety.

Run a dry run from repo root:
```bash
python3 scripts/introduce_std_using.py
```

Apply changes:
```bash
python3 scripts/introduce_std_using.py --apply
```

Notes:
- For nested namespaces like `std::filesystem::rename`, you can manually add `using std::filesystem::rename;` and use `rename(...)` at call sites, or keep the original qualified form. Avoid `using namespace ...`.
- For class members such as `std::string::npos`, do not try to import the member (that’s invalid); instead, keep it as `string::npos` by importing `using std::string;` or leave the original.