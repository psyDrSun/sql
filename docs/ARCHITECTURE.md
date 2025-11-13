# Mini-DBMS 架构设计文档

## 系统概览

Mini-DBMS 是一个轻量级关系数据库管理系统实现，采用经典的编译器三阶段架构（词法分析、语法分析、语义分析）并扩展为完整的 SQL 执行引擎。

### 核心设计理念

1. **简洁性优先**：每个模块职责单一，避免过度设计
2. **可读性至上**：代码结构清晰，即使无注释也易理解
3. **实用性导向**：支持核心 SQL 功能（DDL/DML/DQL），包括多表 JOIN

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    CLI Handler                          │
│  (用户交互层：命令行界面、文件读取、Watch模式)              │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                   SQL Parser                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Lexer       │→ │  Parser      │→ │  AST Nodes   │  │
│  │  (词法分析)   │  │  (语法分析)   │  │  (语法树)     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                Execution Engine                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  DDL         │  │  DML         │  │  DQL         │  │
│  │  (表结构)     │  │  (数据修改)   │  │  (查询)       │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└──────┬─────────────────────────────────────┬───────────┘
       │                                     │
       ▼                                     ▼
┌──────────────────┐              ┌──────────────────────┐
│ Catalog Manager  │              │  Storage Manager     │
│  (元数据管理)      │              │  (CSV 文件存储)       │
└──────────────────┘              └──────────────────────┘
```

## 模块详解

### 1. CLI Handler (`src/CLIHandler.cpp`, `include/db/CLIHandler.hpp`)

**职责**：
- 提供交互式命令行界面（REPL）
- 从文件批量执行 SQL 脚本
- 支持 Watch 模式（监听文件变化并自动重新执行）

**核心成员**：
- `p_`: SQLParser 实例（解析 SQL 语句）
- `e_`: ExecutionEngine 实例（执行已解析的语句）

**关键方法**：
- `run()`: 交互式 REPL 循环
- `run_script(istream&)`: 从输入流执行 SQL 脚本
- `run_watch_mode(const string&)`: 文件监视模式

**实现要点**：
- 支持多行 SQL 语句（按 `;` 分隔）
- 自动过滤空行和 SQL 注释（`--` 开头）
- 异常捕获与用户友好的错误提示

### 2. SQL Parser (`src/SQLParser.cpp`, `include/db/SQLParser.hpp`)

#### 2.1 Lexer（词法分析器）

**职责**：将 SQL 字符串切分为 Token 流

**Token 类型**：
```cpp
enum class TokenType {
    SELECT, INSERT, CREATE, UPDATE, DELETE, ALTER, DROP,
    TABLE, FROM, WHERE, VALUES, SET, INTO, JOIN, ON, AND, OR,
    INT, VARCHAR,
    IDENTIFIER, NUMBER, STRING,
    COMMA, SEMICOLON, LPAREN, RPAREN, STAR, EQUAL,
    LESS_THAN, GREATER_THAN, LESS_EQUAL, GREATER_EQUAL, NOT_EQUAL,
    END_OF_FILE
};
```

**核心方法**：
- `next()`: 读取下一个 Token
- `skip_whitespace()`: 跳过空白字符
- `scan_string()`: 扫描字符串字面量（支持单引号内的转义 `''`）
- `scan_number()`: 扫描数字字面量
- `scan_identifier()`: 扫描标识符和关键字（大小写不敏感）

**实现细节**：
- 使用位置指针 `pos_` 顺序扫描输入字符串
- 关键字通过大写比较识别
- 字符串支持 SQL 标准的单引号转义规则

#### 2.2 Parser（语法分析器）

**职责**：将 Token 流转换为抽象语法树（AST）

**支持的 SQL 语句**：

##### DDL（Data Definition Language）
```sql
CREATE TABLE table_name (col1 INT, col2 VARCHAR);
DROP TABLE table_name;
ALTER TABLE table_name RENAME TO new_name;
ALTER TABLE table_name ADD COLUMN col_name type;
ALTER TABLE table_name DROP COLUMN col_name;
ALTER TABLE table_name MODIFY COLUMN col_name new_type;
```

##### DML（Data Manipulation Language）
```sql
INSERT INTO table_name VALUES (val1, val2, ...);
UPDATE table_name SET col=val WHERE condition;
DELETE FROM table_name WHERE condition;
```

##### DQL（Data Query Language）
```sql
SELECT col1, col2, * FROM table_name WHERE condition;
SELECT * FROM t1 JOIN t2 ON t1.id = t2.id;
SELECT * FROM t1 JOIN t2 ON cond1 JOIN t3 ON cond2;
```

**核心方法**：
- `parse()`: 解析入口，返回 AST 根节点
- `parse_create_table()`: 解析 CREATE TABLE
- `parse_select()`: 解析 SELECT（含 JOIN 支持）
- `parse_insert()`: 解析 INSERT
- `parse_update()`: 解析 UPDATE
- `parse_delete()`: 解析 DELETE
- `parse_alter_table()`: 解析 ALTER TABLE

**递归下降解析器**：
- 每个语法结构对应一个 `parse_xxx()` 方法
- 使用 `peek()` 前瞻和 `expect()` 消耗 Token
- 构建并返回 AST 节点（`unique_ptr<ASTNode>`）

### 3. AST Nodes (`include/db/AST.hpp`)

**AST 节点类型**：

```cpp
struct ASTNode;                  
struct CreateTableStatement;     
struct DropTableStatement;       
struct AlterTableStatement;      
struct InsertStatement;          
struct SelectStatement;          
struct UpdateStatement;          
struct DeleteStatement;          
struct ColumnDefinition;         
struct JoinClause;               
struct WhereClause;              
struct Condition;                
```

**设计原则**：
- 所有节点继承自 `ASTNode` 基类
- 使用 `unique_ptr` 管理子节点（自动内存管理）
- 节点包含足够信息以便执行引擎使用

**关键节点**：

#### SelectStatement
```cpp
struct SelectStatement {
    vector<string> columns;           
    vector<string> tables;            
    vector<unique_ptr<JoinClause>> joins;  
    unique_ptr<WhereClause> where;    
};
```

#### JoinClause
```cpp
struct JoinClause {
    string table_name;                
    string left_table;                
    string left_column;               
    string right_column;              
};
```

### 4. Execution Engine (`src/ExecutionEngine.cpp`, `include/db/ExecutionEngine.hpp`)

**职责**：执行 AST 表示的 SQL 语句

**核心成员**：
- `c_`: CatalogManager（元数据管理器）
- `s_`: StorageManager（存储管理器）

**执行流程**：
```
AST → 分发到对应 handler → 元数据检查 → 存储层操作 → 返回结果
```

**关键方法**：

#### DDL 处理
- `handle_create_table()`: 创建表（注册元数据 + 创建存储）
- `handle_drop_table()`: 删除表（清除元数据 + 删除存储）
- `handle_alter_table()`: 修改表结构（支持 RENAME/ADD/DROP/MODIFY）

#### DML 处理
- `handle_insert()`: 插入数据（类型验证 + 追加写入）
- `handle_update()`: 更新数据（读取→修改→写回）
- `handle_delete()`: 删除数据（读取→过滤→写回）

#### DQL 处理（核心复杂度）
- `handle_select()`: 查询处理入口
- `evaluate_joins()`: 多表连接求值（嵌套循环算法）
- `evaluate_condition()`: WHERE 条件求值
- `project_columns()`: 列投影

**JOIN 实现算法**（嵌套循环连接）：

```
伪代码：
result = rows_from_first_table
for each join in joins:
    temp = []
    for row1 in result:
        for row2 in join_table:
            if join_condition(row1, row2):
                temp.append(merge(row1, row2))
    result = temp
return result
```

**时间复杂度**：O(n₁ × n₂ × ... × nₖ)，其中 k 是 JOIN 的表数量

**WHERE 条件支持的操作符**：
- `=`（等于）
- `<`（小于）
- `>`（大于）
- `<=`（小于等于）
- `>=`（大于等于）
- `!=` 或 `<>`（不等于）
- `AND`（逻辑与，支持多条件组合）

### 5. Catalog Manager (`src/CatalogManager.cpp`, `include/db/CatalogManager.hpp`)

**职责**：管理数据库元数据（表结构定义）

**核心成员**：
- `t_`: `map<string, TableSchema>`（表名 → 表结构映射）
- `c_`: 元数据持久化文件路径（`catalog.meta`）

**TableSchema 结构**：
```cpp
struct TableSchema {
    string name;                       
    vector<ColumnSchema> columns;      
};

struct ColumnSchema {
    string name;                       
    DataType type;                     
    size_t length;                     
};
```

**关键方法**：
- `create_table()`: 注册新表
- `drop_table()`: 删除表定义
- `rename_table()`: 重命名表
- `add_column()`: 添加列到已有表
- `drop_column()`: 删除列
- `modify_column()`: 修改列类型
- `get_table_schema()`: 查询表结构
- `table_exists()`: 检查表是否存在

**持久化机制**：
- 启动时从 `catalog.meta` 加载元数据（`load()`）
- 每次修改后自动保存（`save()`）
- 使用简单的文本格式存储（易于调试）

**序列化格式示例**：
```
students
3
id INT 4
name VARCHAR 255
age INT 4
```

### 6. Storage Manager (`src/StorageManager.cpp`, `include/db/StorageManager.hpp`)

**职责**：管理表数据的 CSV 文件存储

**核心成员**：
- `b_`: 数据文件存储基础路径（默认 `./data`）

**存储格式**：每个表对应一个 CSV 文件（`<table_name>.csv`）

**CSV 格式示例**：
```csv
id,name,age
101,Alice,20
102,Bob,22
```

**关键方法**：
- `create_table_storage()`: 创建表文件（写入列头）
- `drop_table_storage()`: 删除表文件
- `rename_table_storage()`: 重命名表文件
- `read_all_rows()`: 读取所有数据行
- `append_row()`: 追加一行数据
- `write_all_rows()`: 覆盖写入所有数据
- `add_column()`: 为已有数据添加新列（默认空值）
- `drop_column()`: 删除某列数据
- `modify_column()`: 修改列元数据（重写文件）

**CSV 处理细节**：
- 支持字段内包含逗号（使用双引号包围）
- 支持字段内包含引号（使用 `""` 转义）
- 处理 Windows (`\r\n`) 和 Unix (`\n`) 换行符

**辅助函数（namespace 内部）**：
- `split_csv_line()`: 解析 CSV 行为字段列表
- `escape_csv_field()`: 转义字段值
- `join_csv_fields()`: 拼接字段为 CSV 行
- `join_path()`: 拼接文件路径

### 7. Type System (`src/Types.cpp`, `include/db/Types.hpp`)

**支持的数据类型**：
```cpp
enum class DataType {
    Int,      
    Varchar   
};
```

**工具函数**：
- `parse_type(const string&)`: 字符串 → 数据类型
- `type_to_string(DataType)`: 数据类型 → 字符串
- `default_length(DataType)`: 获取默认长度

**类型规则**：
- `INT`: 固定 4 字节
- `VARCHAR`: 默认 255 字符，可指定长度

## 数据流示例

### CREATE TABLE 流程

```
用户输入: CREATE TABLE students (id INT, name VARCHAR);
    ↓
CLI Handler 接收并传递给 SQLParser
    ↓
Lexer 生成 Token 流:
  [CREATE] [TABLE] [IDENTIFIER:students] [(] [IDENTIFIER:id] [INT] ...
    ↓
Parser 构建 AST:
  CreateTableStatement {
    table_name: "students"
    columns: [
      ColumnDefinition{name:"id", type:INT},
      ColumnDefinition{name:"name", type:VARCHAR}
    ]
  }
    ↓
Execution Engine 处理:
  1. 调用 CatalogManager.create_table() 注册元数据
  2. 调用 StorageManager.create_table_storage() 创建 CSV 文件
    ↓
文件系统变化:
  - catalog.meta 新增表定义
  - data/students.csv 创建（包含列头行）
    ↓
返回成功消息给用户
```

### SELECT with JOIN 流程

```
用户输入: SELECT * FROM students JOIN courses ON students.id = courses.student_id;
    ↓
Parser 生成 AST:
  SelectStatement {
    columns: ["*"]
    tables: ["students"]
    joins: [
      JoinClause {
        table_name: "courses"
        left_table: "students"
        left_column: "id"
        right_column: "student_id"
      }
    ]
  }
    ↓
Execution Engine:
  1. 从 StorageManager 读取 students 所有行
  2. 从 StorageManager 读取 courses 所有行
  3. 执行嵌套循环连接:
     for each row1 in students:
       for each row2 in courses:
         if row1["id"] == row2["student_id"]:
           merged_row = row1 + row2
           result.append(merged_row)
  4. 返回结果集
    ↓
CLI Handler 格式化输出表格给用户
```

## 性能特征

### 优势
- **启动快**：无需预加载大量库或运行时
- **内存占用小**：仅加载元数据和当前操作的表数据
- **代码简洁**：总代码量 < 2000 行，易于维护

### 局限性
- **无索引**：所有查询均为全表扫描，O(n) 复杂度
- **JOIN 性能**：使用朴素嵌套循环，大表连接性能差
- **无并发控制**：不支持多用户/多进程并发访问
- **无事务支持**：操作非原子性
- **有限的数据类型**：仅支持 INT 和 VARCHAR
- **无查询优化**：按语法顺序直接执行

### 适用场景
- 学习数据库原理
- 快速原型验证
- 小型数据集（< 10,000 行）交互式查询
- 单用户环境

## 扩展点

如需增强系统，可从以下方向考虑：

1. **索引**：在 StorageManager 添加 B+ 树索引
2. **查询优化**：添加查询计划生成器和代价模型
3. **更多数据类型**：DATE, FLOAT, BLOB 等
4. **并发控制**：添加锁管理器和事务管理器
5. **更好的存储格式**：替换 CSV 为二进制格式（如 Parquet）
6. **SQL 功能**：GROUP BY, ORDER BY, LIMIT, 子查询
7. **网络协议**：支持客户端-服务器模式

## 编译与运行

### 构建系统
- CMake 3.10+
- C++17 编译器（GCC 7+, Clang 5+, MSVC 2017+）

### 编译命令
```bash
cmake -S . -B build
cmake --build build
```

### 运行模式

**交互式模式**：
```bash
./build/mini_dbms
```

**脚本执行**：
```bash
./build/mini_dbms --file script.sql
```

**行范围执行**：
```bash
./build/mini_dbms --file script.sql --lines 10-20
```

**Watch 模式**：
```bash
./build/mini_dbms --watch script.sql
```

## 代码风格特点

由于本项目经过"激进风格转换"，具有以下特征：

1. **使用 `using namespace std;`**：所有标准库类型无 `std::` 前缀
2. **极简变量名**：成员变量如 `c_`（catalog）、`s_`（storage）、`p_`（parser）
3. **无代码注释**：依赖文档和清晰的代码结构传达意图
4. **短函数名**：优先使用描述性但简洁的名称

这种风格牺牲了一定的可读性以换取代码简洁性，建议配合本文档阅读源码。
