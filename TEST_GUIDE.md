# 📋 测试验证指南

## 🎯 目标

验证简化版 SQL 解析器的三阶段功能：
1. **词法分析** - Token 识别
2. **语法分析** - AST 构建
3. **语义分析** - 语义验证

---

## 🚀 快速开始

### 方法1: 使用测试脚本
```bash
# 执行测试
./run_test.sh

# 或者直接运行
./test_parser test_demo.sql
```

### 方法2: 自定义测试文件
```bash
# 编辑你的 SQL 文件
nano my_test.sql

# 运行测试
./test_parser my_test.sql
```

---

## 📝 测试工作流

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 编辑 test_demo.sql                                       │
│    添加/修改 SQL 语句                                       │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. 运行 ./run_test.sh                                       │
│    或 ./test_parser test_demo.sql                          │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. 查看输出                                                  │
│    • 词法分析结果                                           │
│    • 语法树 (AST)                                           │
│    • 语义检查结果                                           │
│    • 成功/失败统计                                          │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. 根据结果调整 SQL                                          │
│    修复错误或添加新测试                                     │
└─────────────────────────────────────────────────────────────┘
```

---

## ✅ 成功案例示例

### 测试 1: CREATE TABLE
```sql
CREATE TABLE students (id INT, name VARCHAR, age INT);
```

**预期输出:**
```
📊 CREATE TABLE: students
  ├─ id (INT)
  ├─ name (VARCHAR)
  ├─ age (INT)

  ✓ 表创建成功
✅ 成功
```

### 测试 2: INSERT
```sql
INSERT INTO students VALUES (101, 'Alice', 20);
```

**预期输出:**
```
➕ INSERT INTO: students
  └─ VALUES: (101, Alice, 20)

  ✓ 数据插入验证通过
✅ 成功
```

### 测试 3: SELECT
```sql
SELECT * FROM students WHERE age = 20;
```

**预期输出:**
```
🔍 SELECT FROM: students
  ├─ COLUMNS: *
  └─ WHERE: age = 20

  ✓ 查询验证通过
✅ 成功
```

---

## ❌ 错误案例示例

### 错误 1: 词法错误
```sql
SELECT @ FROM students;
```

**预期输出:**
```
❌ 词法错误: 无效字符 '@'
```

### 错误 2: 语法错误
```sql
CREATE TABLE students id INT;
```

**预期输出:**
```
❌ 语法错误: 期望 LPAREN (在 列定义 中), 但得到 IDENTIFIER
```

### 错误 3: 语义错误 - 表不存在
```sql
SELECT * FROM courses;
```

**预期输出:**
```
❌ 语义错误: 表 'courses' 不存在
```

### 错误 4: 语义错误 - 类型不匹配
```sql
INSERT INTO students VALUES ('invalid', 'Bob', 22);
```

**预期输出:**
```
❌ 语义错误: 列 'id' 是 INT 类型，但提供了非数字值 'invalid'
```

### 错误 5: 语义错误 - 表已存在
```sql
CREATE TABLE students (id INT);  -- students 已经存在
```

**预期输出:**
```
❌ 语义错误: 表 'students' 已存在
```

---

## 🧪 推荐测试序列

在 `test_demo.sql` 中按以下顺序测试：

```sql
-- 1. 基础 DDL
CREATE TABLE users (id INT, username VARCHAR);

-- 2. 基础 DML
INSERT INTO users VALUES (1, 'admin');

-- 3. 基础查询
SELECT * FROM users;

-- 4. 带 WHERE 的查询
SELECT * FROM users WHERE id = 1;

-- 5. 类型错误测试
INSERT INTO users VALUES ('abc', 'test');  -- 应失败

-- 6. 表不存在测试
SELECT * FROM products;  -- 应失败

-- 7. 表重复测试
CREATE TABLE users (id INT);  -- 应失败

-- 8. 多表测试
CREATE TABLE products (id INT, name VARCHAR);
INSERT INTO products VALUES (100, 'Laptop');
SELECT * FROM products;
```

---

## 📊 输出解读

### 成功执行
```
[语句 1]
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📝 CREATE TABLE students (id INT, name VARCHAR, age INT);

📊 CREATE TABLE: students        ← AST 结构
  ├─ id (INT)
  ├─ name (VARCHAR)
  ├─ age (INT)

  ✓ 表创建成功                   ← 语义检查通过
✅ 成功
```

### 失败执行
```
[语句 5]
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📝 INSERT INTO students VALUES ('invalid', 'Charlie', 25);

➕ INSERT INTO: students         ← AST 结构
  └─ VALUES: (invalid, Charlie, 25)

❌ 语义错误: 列 'id' 是 INT 类型，但提供了非数字值 'invalid'
```

### 最终统计
```
📚 表目录:
  • courses (id:INT, title:VARCHAR)
  • students (id:INT, name:VARCHAR, age:INT)

======================================================================
📊 执行统计: 成功 5 条, 失败 3 条
======================================================================
```

---

## 🔍 调试技巧

### 1. 逐条测试
如果某条语句失败，注释掉其他语句单独测试：
```sql
-- CREATE TABLE users (id INT);
-- INSERT INTO users VALUES (1, 'admin');
SELECT * FROM users WHERE id = 1;  -- 只测试这条
```

### 2. 检查注释语法
确保使用 SQL 标准注释：
```sql
-- 这是注释 ✓
# 这不是有效注释 ✗
/* 不支持块注释 */ ✗
```

### 3. 检查引号
字符串必须使用单引号：
```sql
INSERT INTO users VALUES (1, 'Alice');  -- ✓
INSERT INTO users VALUES (1, "Alice");  -- ✗
```

### 4. 检查分号
每条语句必须以分号结束：
```sql
CREATE TABLE users (id INT, name VARCHAR);  -- ✓
CREATE TABLE users (id INT, name VARCHAR)   -- ✗
```

---

## 📂 文件说明

```
/Users/drdotsun/Documents/sql/
├── test_parser              # 编译后的解析器可执行文件
├── test_parser.cpp          # 解析器源代码
├── test_demo.sql            # 测试 SQL 文件 (可编辑)
├── run_test.sh              # 快捷测试脚本
└── TEST_GUIDE.md            # 本文档
```

---

## 💡 扩展测试想法

### 测试边界情况
```sql
-- 空表名? (应失败)
CREATE TABLE (id INT);

-- 无列? (应失败)
CREATE TABLE users ();

-- 值数量不匹配? (应失败)
INSERT INTO users VALUES (1);
INSERT INTO users VALUES (1, 'a', 'extra');
```

### 测试特殊字符
```sql
-- 下划线
CREATE TABLE my_table (user_id INT);

-- 数字开头 (应失败)
CREATE TABLE 123table (id INT);
```

### 测试大小写
```sql
-- 关键字大小写混合
CrEaTe TaBlE mixed (id INT);
sElEcT * FrOm mixed;
```

---

## 🎓 学习要点

通过这个测试流程，您可以观察到：

1. **词法分析阶段**
   - 如何识别关键字、标识符、数字、字符串
   - 如何处理无效字符

2. **语法分析阶段**
   - 如何构建 AST
   - 如何检测语法错误

3. **语义分析阶段**
   - 如何维护符号表 (表目录)
   - 如何进行类型检查
   - 如何检测语义错误

---

## ✨ 快速参考

```bash
# 运行测试
./run_test.sh

# 查看测试文件
cat test_demo.sql

# 编辑测试文件
nano test_demo.sql

# 直接执行
./test_parser test_demo.sql

# 测试自定义文件
./test_parser my_custom.sql
```

---

祝测试顺利！🎉
