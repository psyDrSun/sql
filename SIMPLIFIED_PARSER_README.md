# 简化版 SQL 解析器 - 三阶段编译流程

## 📚 设计理念

这是一个**教学导向**的简化 SQL 解析器，清晰展示标准编译器的三个核心阶段：

```
输入 SQL 字符串
      ↓
【阶段1】词法分析 (Lexical Analysis)
      ↓ Token 流
【阶段2】语法分析 (Syntax Analysis)  
      ↓ 抽象语法树 (AST)
【阶段3】语义分析 (Semantic Analysis)
      ↓ 验证通过/错误报告
```

---

## 🎯 支持的功能

### SQL 语句
```sql
-- 1. 创建表
CREATE TABLE students (id INT, name VARCHAR, age INT);

-- 2. 插入数据
INSERT INTO students VALUES (101, 'Alice', 20);

-- 3. 查询数据 (仅支持 SELECT *)
SELECT * FROM students WHERE age = 20;
```

### 检查项
- ✅ **词法错误**: 无效字符 (@, #, 等)
- ✅ **语法错误**: 缺少括号、逗号、关键字顺序错误
- ✅ **语义错误**: 表不存在、列不存在、类型不匹配、值数量不匹配

---

## 🔍 阶段详解

### 阶段1: 词法分析 (Lexer)

**目标**: 将字符串分解为 Token 流

**输入**:
```
"SELECT * FROM students WHERE age = 20;"
```

**输出**:
```
[SELECT] [STAR] [FROM] [IDENTIFIER:students] [WHERE] 
[IDENTIFIER:age] [EQUAL] [NUMBER:20] [SEMICOLON]
```

**实现关键**:
```cpp
class Lexer {
    Token scan_identifier();  // 识别关键字和标识符
    Token scan_number();      // 识别数字
    Token scan_string();      // 识别字符串 'xxx'
};
```

**识别规则**:
- 标识符: `[a-zA-Z_][a-zA-Z0-9_]*`
- 数字: `[0-9]+`
- 字符串: `'...'`
- 符号: `,`, `;`, `(`, `)`, `*`, `=`

---

### 阶段2: 语法分析 (Parser)

**目标**: 构建抽象语法树 (AST)

**输入**: Token 流

**输出**: AST 节点树

```
CreateTableStmt
├─ table_name: "students"
└─ columns:
    ├─ ColumnDef {name: "id",   type: "INT"}
    ├─ ColumnDef {name: "name", type: "VARCHAR"}
    └─ ColumnDef {name: "age",  type: "INT"}
```

**实现方法**: 递归下降解析

```cpp
class Parser {
    unique_ptr<CreateTableStmt> parse_create_table() {
        expect(CREATE);
        expect(TABLE);
        string table_name = consume_identifier();
        expect(LPAREN);
        while (not RPAREN) {
            parse_column_definition();
        }
        expect(RPAREN);
        expect(SEMICOLON);
    }
};
```

**语法规则**:
```
<create_table> ::= CREATE TABLE <identifier> '(' <column_list> ')' ';'
<column_list>  ::= <column_def> (',' <column_def>)*
<column_def>   ::= <identifier> <type>
<type>         ::= INT | VARCHAR

<insert>       ::= INSERT INTO <identifier> VALUES '(' <value_list> ')' ';'
<value_list>   ::= <value> (',' <value>)*
<value>        ::= <number> | <string>

<select>       ::= SELECT '*' FROM <identifier> [WHERE <condition>] ';'
<condition>    ::= <identifier> '=' <value>
```

---

### 阶段3: 语义分析 (Semantic Analyzer)

**目标**: 验证语义正确性

**检查项**:

#### 1. CREATE TABLE
- ✅ 表名是否已存在
- ✅ 列名是否重复
- ✅ 是否至少有一列

#### 2. INSERT
- ✅ 表是否存在
- ✅ 值的数量是否匹配列数
- ✅ 值的类型是否匹配列类型

#### 3. SELECT
- ✅ 表是否存在
- ✅ WHERE 子句中的列是否存在

**实现**:
```cpp
class SemanticAnalyzer {
    map<string, TableSchema> catalog;  // 表目录
    
    void analyze_create_table(const CreateTableStmt* stmt) {
        // 1. 检查表是否已存在
        if (catalog.find(stmt->table_name) != catalog.end()) {
            throw "表已存在";
        }
        
        // 2. 检查列名重复
        // 3. 注册到目录
        catalog[stmt->table_name] = schema;
    }
    
    void analyze_insert(const InsertStmt* stmt) {
        // 1. 表存在性检查
        auto schema = catalog[stmt->table_name];
        
        // 2. 值数量检查
        if (values.size() != schema.columns.size()) {
            throw "值数量不匹配";
        }
        
        // 3. 类型检查
        for (each value) {
            if (type_mismatch) throw "类型错误";
        }
    }
};
```

---

## 🎬 运行示例

### 成功案例
```bash
$ ./simple_parser

输入: CREATE TABLE students (id INT, name VARCHAR, age INT);

【阶段1】词法分析
  Token #0: [CREATE] "CREATE"
  Token #1: [TABLE] "TABLE"
  Token #2: [IDENTIFIER] "students"
  ...

【阶段2】语法分析
  CREATE TABLE: students
    Column: id (INT)
    Column: name (VARCHAR)
    Column: age (INT)

【阶段3】语义分析
  ✓ 表不存在冲突
  ✓ 列名无重复
  ✓ 已注册到目录

✅ 执行成功!
```

### 错误案例

#### 词法错误
```
输入: SELECT @ FROM students;

❌ 词法错误: 无效字符 '@'
```

#### 语法错误
```
输入: CREATE TABLE students id INT;

【阶段1】词法分析 ✓
【阶段2】语法分析
❌ 语法错误: 期望 LPAREN, 但得到 IDENTIFIER
```

#### 语义错误
```
输入: INSERT INTO students VALUES ('invalid', 'Bob', 22);

【阶段1】词法分析 ✓
【阶段2】语法分析 ✓
【阶段3】语义分析
❌ 语义错误: 列 'id' 是 INT 类型，但提供了非数字值 'invalid'
```

---

## 🔧 编译与使用

```bash
# 编译
g++ -std=c++17 simplified_parser.cpp -o simple_parser

# 运行
./simple_parser
```

---

## 📖 与完整版的对比

| 特性 | 简化版 | 完整版 (mini_dbms) |
|------|--------|-------------------|
| **词法分析** | 单文件实现 | Tokenizer 类 + TokenStream |
| **语法分析** | 简单递归下降 | 完整递归下降 + 错误恢复 |
| **语义分析** | 内存目录验证 | CatalogManager + 持久化 |
| **SELECT** | 仅支持 `*` | 支持列投影、多表 JOIN、AND 条件 |
| **WHERE** | 仅支持 `=` | 支持 `>`, `<`, `>=`, `<=`, `<>` |
| **类型系统** | INT, VARCHAR | INT, VARCHAR(n), DOUBLE |
| **存储** | 无 (仅验证) | CSV 文件持久化 |
| **执行** | 无 (仅分析) | ExecutionEngine 实际执行 |

---

## 🎓 学习要点

### 1. 状态机思想 (词法分析)
```cpp
while (pos < input.size()) {
    if (isalpha(ch))       → scan_identifier()
    else if (isdigit(ch))  → scan_number()
    else if (ch == '\'')   → scan_string()
    else                   → scan_symbol()
}
```

### 2. 递归下降 (语法分析)
```cpp
parse_create_table() {
    expect(CREATE);
    expect(TABLE);
    parse_identifier();
    expect(LPAREN);
    parse_column_list();  // 递归调用
    expect(RPAREN);
}
```

### 3. 符号表管理 (语义分析)
```cpp
map<string, TableSchema> catalog;  // 表目录

// 注册
catalog[table_name] = schema;

// 查询
if (catalog.find(table_name) == catalog.end()) {
    throw "表不存在";
}
```

---

## 🚀 扩展思路

### 简单扩展
1. 支持 `SELECT col1, col2` (列投影)
2. 支持 `>`, `<` 等比较运算符
3. 支持 `AND`, `OR` 逻辑运算
4. 支持 `DROP TABLE`

### 中等扩展
1. 多表 JOIN
2. ORDER BY / LIMIT
3. 聚合函数 (COUNT, SUM, AVG)
4. 子查询

### 高级扩展
1. 索引支持
2. 事务管理
3. 查询优化器
4. 执行引擎 (实际运行查询)

---

## 📝 核心代码结构

```
simplified_parser.cpp (700 行)
├── Token 定义 (20 行)
├── Lexer 类 (150 行)
│   ├── scan_identifier()
│   ├── scan_number()
│   ├── scan_string()
│   └── tokenize()
├── AST 节点定义 (100 行)
│   ├── CreateTableStmt
│   ├── InsertStmt
│   └── SelectStmt
├── Parser 类 (200 行)
│   ├── parse_create_table()
│   ├── parse_insert()
│   └── parse_select()
├── SemanticAnalyzer 类 (150 行)
│   ├── analyze_create_table()
│   ├── analyze_insert()
│   └── analyze_select()
└── main() 测试代码 (80 行)
```

---

## ✨ 总结

这个简化版本的核心价值在于：

1. **清晰的阶段划分**: 每个阶段独立完成特定任务
2. **完整的错误处理**: 覆盖词法、语法、语义三类错误
3. **教学友好**: 代码注释丰富，输出详细
4. **可扩展性**: 易于添加新的语句类型和检查规则

适合用于：
- 编译原理课程教学
- 理解 SQL 解析流程
- 作为完整数据库系统的原型
