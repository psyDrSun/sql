# Mini-DBMS API 参考手册

## 目录

1. [CLI Handler API](#cli-handler-api)
2. [SQL Parser API](#sql-parser-api)
3. [Execution Engine API](#execution-engine-api)
4. [Catalog Manager API](#catalog-manager-api)
5. [Storage Manager API](#storage-manager-api)
6. [Type System API](#type-system-api)

---

## CLI Handler API

**头文件**: `include/db/CLIHandler.hpp`  
**实现**: `src/CLIHandler.cpp`

### 类: `CLIHandler`

#### 构造函数

```cpp
CLIHandler(shared_ptr<SQLParser> p, shared_ptr<ExecutionEngine> e);
```

**参数**:
- `p`: SQL 解析器实例
- `e`: 执行引擎实例

**示例**:
```cpp
auto parser = make_shared<SQLParser>();
auto engine = make_shared<ExecutionEngine>(catalog, storage);
CLIHandler cli(parser, engine);
```

#### 公共方法

##### `void run()`

启动交互式命令行界面（REPL 循环）。

**行为**:
- 显示提示符 `mini-dbms> `
- 读取用户输入直到遇到 `;`
- 解析并执行 SQL 语句
- 输出结果或错误信息
- 输入 `exit;` 或 `quit;` 退出

**示例**:
```cpp
cli.run();
```

##### `void run_script(istream& in)`

从输入流批量执行 SQL 脚本。

**参数**:
- `in`: 输入流（可以是 `ifstream`、`istringstream` 等）

**行为**:
- 按 `;` 分隔语句
- 跳过空行和 SQL 注释（`--`）
- 顺序执行所有语句
- 遇到错误时打印但继续执行后续语句

**示例**:
```cpp
ifstream file("script.sql");
cli.run_script(file);
```

##### `void run_watch_mode(const string& f)`

监视文件变化并自动重新执行。

**参数**:
- `f`: SQL 脚本文件路径

**行为**:
- 执行脚本文件
- 等待用户按 Enter 键
- 重新执行脚本（支持文件修改后的新内容）
- 循环直到用户输入 `q` 或 `quit`

**示例**:
```cpp
cli.run_watch_mode("demo.sql");
```

**适用场景**: 开发调试时实时查看 SQL 执行结果

---

## SQL Parser API

**头文件**: `include/db/SQLParser.hpp`  
**实现**: `src/SQLParser.cpp`

### 类: `SQLParser`

#### 公共方法

##### `unique_ptr<ASTNode> parse(const string& sql)`

解析 SQL 字符串并返回 AST。

**参数**:
- `sql`: SQL 语句字符串

**返回值**:
- 成功: `unique_ptr<ASTNode>` 指向具体 AST 节点
- 失败: 抛出 `runtime_error` 异常

**支持的 SQL 类型**:
- `CreateTableStatement`
- `DropTableStatement`
- `AlterTableStatement`
- `InsertStatement`
- `SelectStatement`
- `UpdateStatement`
- `DeleteStatement`

**异常**:
- `runtime_error`: 词法错误、语法错误

**示例**:
```cpp
SQLParser parser;
try {
    auto ast = parser.parse("CREATE TABLE users (id INT, name VARCHAR);");
    auto* create_stmt = dynamic_cast<CreateTableStatement*>(ast.get());
    cout << "Table: " << create_stmt->table_name << endl;
} catch (const runtime_error& e) {
    cerr << "Parse error: " << e.what() << endl;
}
```

---

## Execution Engine API

**头文件**: `include/db/ExecutionEngine.hpp`  
**实现**: `src/ExecutionEngine.cpp`

### 类: `ExecutionEngine`

#### 构造函数

```cpp
ExecutionEngine(shared_ptr<CatalogManager> c, shared_ptr<StorageManager> s);
```

**参数**:
- `c`: 元数据管理器
- `s`: 存储管理器

#### 公共方法

##### `void execute(const ASTNode* node)`

执行 AST 节点表示的 SQL 语句。

**参数**:
- `node`: AST 根节点指针

**行为**:
- 根据节点类型分发到对应处理器
- 执行元数据和数据操作
- 输出结果或错误信息

**异常**:
- `runtime_error`: 表不存在、列不存在、类型错误等

**示例**:
```cpp
ExecutionEngine engine(catalog, storage);
auto ast = parser.parse("SELECT * FROM users;");
engine.execute(ast.get());
```

#### 内部处理方法（供参考）

这些方法通常不直接调用，由 `execute()` 内部分发：

- `handle_create_table(const CreateTableStatement*)`: 处理 CREATE TABLE
- `handle_drop_table(const DropTableStatement*)`: 处理 DROP TABLE
- `handle_alter_table(const AlterTableStatement*)`: 处理 ALTER TABLE
- `handle_insert(const InsertStatement*)`: 处理 INSERT
- `handle_select(const SelectStatement*)`: 处理 SELECT
- `handle_update(const UpdateStatement*)`: 处理 UPDATE
- `handle_delete(const DeleteStatement*)`: 处理 DELETE

---

## Catalog Manager API

**头文件**: `include/db/CatalogManager.hpp`  
**实现**: `src/CatalogManager.cpp`

### 结构: `ColumnSchema`

```cpp
struct ColumnSchema {
    string name;        
    DataType type;      
    size_t length;      
};
```

### 结构: `TableSchema`

```cpp
struct TableSchema {
    string name;                    
    vector<ColumnSchema> columns;   
};
```

### 类: `CatalogManager`

#### 构造函数

```cpp
CatalogManager();
```

**行为**:
- 默认元数据文件路径: `./data/catalog.meta`
- 自动加载已有元数据（如果文件存在）

#### 公共方法

##### `void create_table(const TableSchema& schema)`

注册新表到元数据。

**参数**:
- `schema`: 表结构定义

**异常**:
- `runtime_error`: 表已存在

**示例**:
```cpp
TableSchema schema;
schema.name = "users";
schema.columns.push_back({"id", DataType::Int, 4});
schema.columns.push_back({"name", DataType::Varchar, 255});
catalog.create_table(schema);
```

##### `void drop_table(const string& table_name)`

删除表定义。

**参数**:
- `table_name`: 表名

**异常**:
- `runtime_error`: 表不存在

##### `void rename_table(const string& old_name, const string& new_name)`

重命名表。

**参数**:
- `old_name`: 原表名
- `new_name`: 新表名

**异常**:
- `runtime_error`: 原表不存在或新表名已存在

##### `void add_column(const string& table_name, const ColumnSchema& column)`

添加列到已有表。

**参数**:
- `table_name`: 表名
- `column`: 列定义

**异常**:
- `runtime_error`: 表不存在或列名重复

##### `void drop_column(const string& table_name, const string& column_name)`

删除表中的列。

**参数**:
- `table_name`: 表名
- `column_name`: 列名

**异常**:
- `runtime_error`: 表不存在或列不存在

##### `void modify_column(const string& table_name, const string& column_name, DataType new_type)`

修改列的数据类型。

**参数**:
- `table_name`: 表名
- `column_name`: 列名
- `new_type`: 新数据类型

**异常**:
- `runtime_error`: 表不存在或列不存在

##### `TableSchema get_table_schema(const string& table_name) const`

获取表结构定义。

**参数**:
- `table_name`: 表名

**返回值**:
- `TableSchema`: 表结构的副本

**异常**:
- `runtime_error`: 表不存在

##### `bool table_exists(const string& table_name) const`

检查表是否存在。

**参数**:
- `table_name`: 表名

**返回值**:
- `true`: 表存在
- `false`: 表不存在

##### `vector<string> list_tables() const`

获取所有表名列表。

**返回值**:
- `vector<string>`: 所有表名

---

## Storage Manager API

**头文件**: `include/db/StorageManager.hpp`  
**实现**: `src/StorageManager.cpp`

### 类: `StorageManager`

#### 构造函数

```cpp
StorageManager(const string& base_path);
```

**参数**:
- `base_path`: 数据文件存储目录（默认 `./data`）

**行为**:
- 如果目录不存在，自动创建

#### 公共方法

##### `void create_table_storage(const TableSchema& schema)`

创建表的存储文件（CSV）。

**参数**:
- `schema`: 表结构定义

**行为**:
- 创建 `<table_name>.csv` 文件
- 写入列头行

**异常**:
- `runtime_error`: 文件创建失败

##### `void drop_table_storage(const string& table_name)`

删除表的存储文件。

**参数**:
- `table_name`: 表名

**行为**:
- 删除对应的 CSV 文件（如果存在）

##### `void rename_table_storage(const string& old_name, const string& new_name)`

重命名表的存储文件。

**参数**:
- `old_name`: 原表名
- `new_name`: 新表名

**异常**:
- `runtime_error`: 文件系统操作失败

##### `vector<vector<string>> read_all_rows(const string& table_name) const`

读取表的所有数据行。

**参数**:
- `table_name`: 表名

**返回值**:
- `vector<vector<string>>`: 二维字符串数组（每行是一个 `vector<string>`）

**行为**:
- 跳过列头行
- 解析每行 CSV 为字段列表

**异常**:
- `runtime_error`: 文件打开失败

**示例**:
```cpp
auto rows = storage.read_all_rows("users");
for (const auto& row : rows) {
    for (const auto& field : row) {
        cout << field << " ";
    }
    cout << endl;
}
```

##### `void append_row(const string& table_name, const vector<string>& values)`

追加一行数据到表。

**参数**:
- `table_name`: 表名
- `values`: 字段值列表（顺序与列定义一致）

**行为**:
- 以追加模式打开文件
- 写入 CSV 格式的一行

**异常**:
- `runtime_error`: 文件打开失败

##### `void write_all_rows(const string& table_name, const TableSchema& schema, const vector<vector<string>>& rows)`

覆盖写入表的所有数据。

**参数**:
- `table_name`: 表名
- `schema`: 表结构（用于生成列头）
- `rows`: 数据行列表

**行为**:
- 截断文件（删除旧内容）
- 写入列头行
- 写入所有数据行

**异常**:
- `runtime_error`: 文件打开失败

##### `void add_column(const string& table_name, const TableSchema& schema, const ColumnSchema& column)`

为已有数据添加新列。

**参数**:
- `table_name`: 表名
- `schema`: 更新后的表结构
- `column`: 新增列定义

**行为**:
- 读取所有行
- 在每行末尾添加空字段
- 重写文件

##### `void drop_column(const string& table_name, const TableSchema& schema, const string& column_name)`

删除表中的某列数据。

**参数**:
- `table_name`: 表名
- `schema`: 更新后的表结构（已移除该列）
- `column_name`: 要删除的列名

**行为**:
- 读取所有行
- 移除指定列的字段
- 重写文件

##### `void modify_column(const string& table_name, const TableSchema& schema, const ColumnSchema& column)`

修改列元数据（重新生成存储）。

**参数**:
- `table_name`: 表名
- `schema`: 更新后的表结构
- `column`: 修改后的列定义

**行为**:
- 读取所有行
- 更新列头行
- 重写文件（数据内容不变）

---

## Type System API

**头文件**: `include/db/Types.hpp`  
**实现**: `src/Types.cpp`

### 枚举: `DataType`

```cpp
enum class DataType {
    Int,        
    Varchar     
};
```

### 函数

##### `DataType parse_type(const string& type_name)`

将字符串解析为数据类型。

**参数**:
- `type_name`: 类型名（大小写不敏感）

**返回值**:
- `DataType::Int`: 如果输入是 "INT"
- `DataType::Varchar`: 如果输入是 "VARCHAR"

**异常**:
- `runtime_error`: 未知的数据类型

**示例**:
```cpp
DataType type = parse_type("INT");      
type = parse_type("varchar");           
```

##### `string type_to_string(DataType type)`

将数据类型转换为字符串。

**参数**:
- `type`: 数据类型

**返回值**:
- `"INT"` 或 `"VARCHAR"`

**示例**:
```cpp
string s = type_to_string(DataType::Int);  
```

##### `size_t default_length(DataType type)`

获取数据类型的默认长度。

**参数**:
- `type`: 数据类型

**返回值**:
- `DataType::Int`: 4 字节
- `DataType::Varchar`: 255 字节

**示例**:
```cpp
size_t len = default_length(DataType::Varchar);  
```

---

## AST 节点定义

**头文件**: `include/db/AST.hpp`

所有 AST 节点类型（仅列出关键字段）：

### `CreateTableStatement`
```cpp
struct CreateTableStatement : ASTNode {
    string table_name;
    vector<unique_ptr<ColumnDefinition>> columns;
};
```

### `DropTableStatement`
```cpp
struct DropTableStatement : ASTNode {
    string table_name;
};
```

### `AlterTableStatement`
```cpp
struct AlterTableStatement : ASTNode {
    string table_name;
    AlterAction action;         
    string new_table_name;      
    ColumnDefinition column;    
    string target_column_name;  
};
```

### `InsertStatement`
```cpp
struct InsertStatement : ASTNode {
    string table_name;
    vector<string> values;
};
```

### `SelectStatement`
```cpp
struct SelectStatement : ASTNode {
    vector<string> columns;
    vector<string> tables;
    vector<unique_ptr<JoinClause>> joins;
    unique_ptr<WhereClause> where;
};
```

### `UpdateStatement`
```cpp
struct UpdateStatement : ASTNode {
    string table_name;
    string column_name;
    string new_value;
    unique_ptr<WhereClause> where;
};
```

### `DeleteStatement`
```cpp
struct DeleteStatement : ASTNode {
    string table_name;
    unique_ptr<WhereClause> where;
};
```

### `JoinClause`
```cpp
struct JoinClause : ASTNode {
    string table_name;
    string left_table;
    string left_column;
    string right_column;
};
```

### `WhereClause`
```cpp
struct WhereClause : ASTNode {
    vector<unique_ptr<Condition>> conditions;
};
```

### `Condition`
```cpp
struct Condition : ASTNode {
    string left_operand;
    string op;              
    string right_operand;
};
```

---

## 使用示例：完整流程

```cpp
#include "db/CatalogManager.hpp"
#include "db/StorageManager.hpp"
#include "db/SQLParser.hpp"
#include "db/ExecutionEngine.hpp"

int main() {
    auto catalog = make_shared<CatalogManager>();
    auto storage = make_shared<StorageManager>("./data");
    auto parser = make_shared<SQLParser>();
    auto engine = make_shared<ExecutionEngine>(catalog, storage);

    try {
        auto ast1 = parser->parse("CREATE TABLE users (id INT, name VARCHAR);");
        engine->execute(ast1.get());

        auto ast2 = parser->parse("INSERT INTO users VALUES (1, 'Alice');");
        engine->execute(ast2.get());

        auto ast3 = parser->parse("SELECT * FROM users;");
        engine->execute(ast3.get());

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
```

---

## 错误处理

所有 API 在遇到错误时抛出 `std::runtime_error` 异常，包含描述性错误信息。

**常见错误类型**：
- 表不存在: `"Table 'xxx' does not exist"`
- 表已存在: `"Table 'xxx' already exists"`
- 列不存在: `"Column 'xxx' does not exist"`
- 类型错误: `"Type mismatch for column 'xxx'"`
- 语法错误: `"Syntax error: unexpected token ..."`
- 文件 I/O 错误: `"Failed to open/write file: ..."`

**最佳实践**：
```cpp
try {
    engine->execute(ast.get());
} catch (const runtime_error& e) {
    cerr << "Execution failed: " << e.what() << endl;
}
```
