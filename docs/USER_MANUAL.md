# Mini-DBMS 用户手册

## 目录

1. [快速开始](#快速开始)
2. [安装与编译](#安装与编译)
3. [运行模式](#运行模式)
4. [SQL 语法参考](#sql-语法参考)
5. [使用示例](#使用示例)
6. [常见问题](#常见问题)
7. [限制与注意事项](#限制与注意事项)

---

## 快速开始

### 5 分钟体验

```bash
cmake -S . -B build
cmake --build build

./build/mini_dbms
```

进入交互式界面后：

```sql
CREATE TABLE students (id INT, name VARCHAR, age INT);
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 22);
SELECT * FROM students;
exit;
```

---

## 安装与编译

### 系统要求

- **操作系统**: Linux, macOS, Windows（WSL/MinGW）
- **编译器**: 
  - GCC 7+ 或 Clang 5+（Linux/macOS）
  - MSVC 2017+（Windows）
- **构建工具**: CMake 3.10+
- **C++ 标准**: C++17

### 编译步骤

#### Linux / macOS

```bash
git clone <repository-url>
cd sql

cmake -S . -B build
cmake --build build

./build/mini_dbms
```

#### Windows（Visual Studio）

```cmd
git clone <repository-url>
cd sql

cmake -S . -B build -G "Visual Studio 16 2019"
cmake --build build --config Release

.\build\Release\mini_dbms.exe
```

### 验证安装

```bash
./build/mini_dbms --help
```

应显示帮助信息：

```
Usage: ./build/mini_dbms [options]
  -f, --file <path>         Execute statements from SQL file
  -l, --lines <start-end>   Limit execution to an inclusive line range
  -w, --watch <path>        Watch mode: press ENTER to re-execute SQL file
  -h, --help                Show this help message
```

---

## 运行模式

### 1. 交互式模式（REPL）

直接运行可执行文件进入交互式命令行：

```bash
./build/mini_dbms
```

**提示符**: `mini-dbms> `

**退出**: 输入 `exit;` 或 `quit;`

**特点**:
- 支持多行 SQL（按 `;` 结束）
- 实时显示执行结果
- 适合快速测试和调试

**示例会话**:
```
mini-dbms> CREATE TABLE users (
...> id INT,
...> name VARCHAR
...> );
Table created successfully.

mini-dbms> INSERT INTO users VALUES (1, 'Alice');
1 row inserted.

mini-dbms> SELECT * FROM users;
+----+-------+
| id | name  |
+----+-------+
| 1  | Alice |
+----+-------+
1 row(s) selected.

mini-dbms> exit;
Goodbye!
```

### 2. 脚本执行模式

从文件批量执行 SQL 语句：

```bash
./build/mini_dbms --file script.sql
```

或简写：

```bash
./build/mini_dbms -f script.sql
```

**特点**:
- 按顺序执行文件中所有 SQL 语句
- 跳过空行和注释（`--` 开头）
- 遇到错误时打印但继续执行后续语句
- 适合数据导入和批处理

**示例脚本** (`demo.sql`):
```sql
CREATE TABLE products (id INT, name VARCHAR, price INT);

INSERT INTO products VALUES (101, 'Laptop', 5000);
INSERT INTO products VALUES (102, 'Mouse', 50);
INSERT INTO products VALUES (103, 'Keyboard', 150);

SELECT * FROM products;
```

执行：
```bash
./build/mini_dbms -f demo.sql
```

### 3. 行范围执行模式

仅执行文件中指定行范围的 SQL：

```bash
./build/mini_dbms --file script.sql --lines 10-20
```

或简写：

```bash
./build/mini_dbms -f script.sql -l 10-20
```

**行号规则**:
- 从 1 开始计数
- 包含起始和结束行
- 支持 `-` 或 `:` 作为分隔符

**示例**:
```bash
./build/mini_dbms -f demo.sql -l 5-8
```

**适用场景**:
- 调试脚本中的特定部分
- 分段执行大型脚本
- 跳过已执行的初始化语句

### 4. Watch 模式（开发调试）

监视 SQL 文件变化并支持重复执行：

```bash
./build/mini_dbms --watch script.sql
```

或简写：

```bash
./build/mini_dbms -w script.sql
```

**工作流程**:
1. 执行脚本文件
2. 显示 "Press ENTER to re-execute, or type 'q' to quit:"
3. 按 Enter 键重新执行（可在编辑器中修改文件后再执行）
4. 输入 `q` 或 `quit` 退出

**适用场景**:
- 开发时实时查看 SQL 执行效果
- 快速迭代查询语句
- 教学演示

**示例**:
```bash
./build/mini_dbms -w demo.sql
```

终端显示：
```
--- Execution #1 ---
Table created successfully.
1 row inserted.
...
--- End of execution ---

Press ENTER to re-execute, or type 'q' to quit:
```

此时可以修改 `demo.sql`，然后按 Enter 看到新结果。

---

## SQL 语法参考

### 数据定义语言（DDL）

#### CREATE TABLE

创建新表。

**语法**:
```sql
CREATE TABLE table_name (
    column1 type,
    column2 type,
    ...
);
```

**支持的数据类型**:
- `INT`: 整数类型
- `VARCHAR`: 可变长字符串（默认 255 字符）

**示例**:
```sql
CREATE TABLE employees (
    id INT,
    name VARCHAR,
    age INT,
    department VARCHAR
);
```

**注意**:
- 表名和列名大小写不敏感
- 不支持主键、外键等约束
- 列名不能重复

#### DROP TABLE

删除表及其所有数据。

**语法**:
```sql
DROP TABLE table_name;
```

**示例**:
```sql
DROP TABLE employees;
```

**警告**: 此操作不可逆，将永久删除表和数据。

#### ALTER TABLE - RENAME

重命名表。

**语法**:
```sql
ALTER TABLE old_name RENAME TO new_name;
```

**示例**:
```sql
ALTER TABLE employees RENAME TO staff;
```

#### ALTER TABLE - ADD COLUMN

向已有表添加新列。

**语法**:
```sql
ALTER TABLE table_name ADD COLUMN column_name type;
```

**示例**:
```sql
ALTER TABLE employees ADD COLUMN salary INT;
```

**行为**: 已有数据行的新列值为空字符串。

#### ALTER TABLE - DROP COLUMN

删除表中的列。

**语法**:
```sql
ALTER TABLE table_name DROP COLUMN column_name;
```

**示例**:
```sql
ALTER TABLE employees DROP COLUMN age;
```

**警告**: 此操作不可逆，将永久删除该列的所有数据。

#### ALTER TABLE - MODIFY COLUMN

修改列的数据类型。

**语法**:
```sql
ALTER TABLE table_name MODIFY COLUMN column_name new_type;
```

**示例**:
```sql
ALTER TABLE employees MODIFY COLUMN salary VARCHAR;
```

**注意**: 已有数据不会自动转换，可能导致类型不一致。

---

### 数据操作语言（DML）

#### INSERT

插入新数据行。

**语法**:
```sql
INSERT INTO table_name VALUES (value1, value2, ...);
```

**示例**:
```sql
INSERT INTO employees VALUES (1, 'Alice', 30, 'Engineering');
INSERT INTO employees VALUES (2, 'Bob', 25, 'Marketing');
```

**要求**:
- 值的数量必须与表的列数一致
- 值的顺序必须与列定义顺序一致
- 字符串值用单引号包围
- 整数值直接写数字

**字符串转义**:
- 单引号内的单引号使用双单引号: `'It''s fine'`

#### UPDATE

更新已有数据。

**语法**:
```sql
UPDATE table_name SET column_name = new_value WHERE condition;
```

**示例**:
```sql
UPDATE employees SET age = 31 WHERE name = 'Alice';
UPDATE employees SET department = 'Sales' WHERE id = 2;
```

**WHERE 子句**:
- 支持 `=`, `<`, `>`, `<=`, `>=`, `!=` 运算符
- 支持 `AND` 连接多个条件
- 省略 WHERE 将更新所有行（危险操作）

**多条件示例**:
```sql
UPDATE employees SET salary = 5000 WHERE age > 25 AND department = 'Engineering';
```

#### DELETE

删除数据行。

**语法**:
```sql
DELETE FROM table_name WHERE condition;
```

**示例**:
```sql
DELETE FROM employees WHERE age < 20;
DELETE FROM employees WHERE name = 'Bob';
```

**警告**: 省略 WHERE 将删除所有行。

```sql
DELETE FROM employees;
```

---

### 数据查询语言（DQL）

#### SELECT - 基本查询

查询表中的数据。

**语法**:
```sql
SELECT column1, column2, ... FROM table_name;
SELECT * FROM table_name;
```

**示例**:
```sql
SELECT id, name FROM employees;
SELECT * FROM employees;
```

**投影**:
- `*`: 选择所有列
- `column1, column2`: 选择指定列

#### SELECT - 带条件查询

**语法**:
```sql
SELECT * FROM table_name WHERE condition;
```

**示例**:
```sql
SELECT * FROM employees WHERE age > 25;
SELECT * FROM employees WHERE department = 'Engineering';
SELECT name, age FROM employees WHERE age >= 30 AND department = 'Sales';
```

**支持的操作符**:
- `=`: 等于
- `<`: 小于
- `>`: 大于
- `<=`: 小于等于
- `>=`: 大于等于
- `!=` 或 `<>`: 不等于

**逻辑运算符**:
- `AND`: 逻辑与（所有条件都满足）

**示例**:
```sql
SELECT * FROM employees WHERE age > 20 AND age < 30;
SELECT * FROM employees WHERE department = 'HR' AND salary > 3000;
```

#### SELECT - 多表连接（JOIN）

连接两个或多个表。

**语法**:
```sql
SELECT * FROM table1 JOIN table2 ON table1.column = table2.column;
```

**示例（两表连接）**:
```sql
CREATE TABLE students (id INT, name VARCHAR);
CREATE TABLE scores (student_id INT, score INT);

INSERT INTO students VALUES (1, 'Alice');
INSERT INTO students VALUES (2, 'Bob');
INSERT INTO scores VALUES (1, 95);
INSERT INTO scores VALUES (2, 87);

SELECT * FROM students JOIN scores ON students.id = scores.student_id;
```

**结果**:
```
+----+-------+------------+-------+
| id | name  | student_id | score |
+----+-------+------------+-------+
| 1  | Alice | 1          | 95    |
| 2  | Bob   | 2          | 87    |
+----+-------+------------+-------+
```

**三表连接示例**:
```sql
CREATE TABLE courses (id INT, name VARCHAR);
INSERT INTO courses VALUES (101, 'Math');

CREATE TABLE enrollments (student_id INT, course_id INT);
INSERT INTO enrollments VALUES (1, 101);

SELECT * 
FROM students 
JOIN enrollments ON students.id = enrollments.student_id
JOIN courses ON enrollments.course_id = courses.id;
```

**JOIN 类型**:
- 仅支持 `INNER JOIN`（或简写为 `JOIN`）
- 不支持 `LEFT JOIN`, `RIGHT JOIN`, `OUTER JOIN`

**列名引用**:
- 在 JOIN ON 子句中必须使用 `table.column` 格式
- 在 SELECT 投影中可省略表名（如果列名无歧义）

---

## 使用示例

### 示例 1: 学生成绩管理系统

```sql
CREATE TABLE students (id INT, name VARCHAR, age INT);
CREATE TABLE courses (id INT, title VARCHAR);
CREATE TABLE enrollments (student_id INT, course_id INT, grade INT);

INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 22);
INSERT INTO students VALUES (3, 'Charlie', 21);

INSERT INTO courses VALUES (101, 'Math');
INSERT INTO courses VALUES (102, 'Physics');

INSERT INTO enrollments VALUES (1, 101, 95);
INSERT INTO enrollments VALUES (1, 102, 88);
INSERT INTO enrollments VALUES (2, 101, 92);
INSERT INTO enrollments VALUES (3, 102, 85);

SELECT * 
FROM students 
JOIN enrollments ON students.id = enrollments.student_id
JOIN courses ON enrollments.course_id = courses.id
WHERE enrollments.grade > 90;
```

**预期结果**: 显示成绩大于 90 分的学生及其课程信息。

### 示例 2: 员工部门查询

```sql
CREATE TABLE departments (id INT, name VARCHAR);
CREATE TABLE employees (id INT, name VARCHAR, dept_id INT, salary INT);

INSERT INTO departments VALUES (1, 'Engineering');
INSERT INTO departments VALUES (2, 'Sales');
INSERT INTO departments VALUES (3, 'HR');

INSERT INTO employees VALUES (101, 'Alice', 1, 5000);
INSERT INTO employees VALUES (102, 'Bob', 1, 4500);
INSERT INTO employees VALUES (103, 'Charlie', 2, 4000);
INSERT INTO employees VALUES (104, 'Diana', 3, 3500);

SELECT employees.name, departments.name, employees.salary
FROM employees
JOIN departments ON employees.dept_id = departments.id
WHERE employees.salary > 4000;
```

### 示例 3: 表结构动态修改

```sql
CREATE TABLE products (id INT, name VARCHAR);

INSERT INTO products VALUES (1, 'Laptop');
INSERT INTO products VALUES (2, 'Mouse');

ALTER TABLE products ADD COLUMN price INT;

UPDATE products SET price = 5000 WHERE id = 1;
UPDATE products SET price = 50 WHERE id = 2;

SELECT * FROM products;

ALTER TABLE products DROP COLUMN id;

SELECT * FROM products;
```

---

## 常见问题

### Q1: 如何查看当前有哪些表？

A: 目前没有 `SHOW TABLES` 命令。可以查看数据目录：

```bash
ls ./data/*.csv
```

或检查 `./data/catalog.meta` 文件。

### Q2: 如何导出数据？

A: 数据以 CSV 格式存储在 `./data` 目录。可以直接复制 CSV 文件或使用标准工具：

```bash
cp ./data/students.csv ~/backup/
```

### Q3: 如何导入外部数据？

A: 方法 1 - 编写 INSERT 语句脚本：

```sql
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 22);
...
```

方法 2 - 直接放置 CSV 文件（需确保格式正确）：

1. 创建表结构
2. 将 CSV 文件复制到 `./data/<table_name>.csv`
3. 确保第一行是列名

### Q4: 支持事务吗？

A: 不支持。每个 SQL 语句独立执行，没有 COMMIT/ROLLBACK 机制。

### Q5: 如何备份数据库？

A: 备份整个 `./data` 目录：

```bash
cp -r ./data ./backup_$(date +%Y%m%d)
```

恢复：

```bash
rm -rf ./data
cp -r ./backup_20251113 ./data
```

### Q6: 支持 NULL 值吗？

A: 不支持。删除列或添加列时，空值表示为空字符串 `""`。

### Q7: 字符串如何包含单引号？

A: 使用双单引号转义：

```sql
INSERT INTO quotes VALUES ('It''s a quote');
```

### Q8: 为什么 JOIN 查询很慢？

A: 使用嵌套循环算法，时间复杂度 O(n×m)。对于大表（>1000 行）建议：
- 减少 JOIN 的表数量
- 在 WHERE 中先过滤数据
- 考虑使用专业数据库系统

---

## 限制与注意事项

### 功能限制

**不支持的 SQL 功能**:
- ❌ 主键、外键、唯一约束
- ❌ 索引（所有查询为全表扫描）
- ❌ 事务（COMMIT/ROLLBACK）
- ❌ NULL 值处理
- ❌ 子查询
- ❌ GROUP BY / HAVING
- ❌ ORDER BY
- ❌ LIMIT / OFFSET
- ❌ 聚合函数（SUM, COUNT, AVG, MAX, MIN）
- ❌ DISTINCT
- ❌ LEFT/RIGHT/OUTER JOIN
- ❌ UNION / INTERSECT / EXCEPT
- ❌ 视图（VIEW）
- ❌ 存储过程
- ❌ 触发器

**数据类型限制**:
- 仅支持 `INT` 和 `VARCHAR`
- 不支持 DATE, FLOAT, DECIMAL, BLOB 等类型
- VARCHAR 默认最大 255 字符

### 性能限制

**适用规模**:
- ✅ 单表 < 1,000 行: 性能良好
- ⚠️  单表 1,000 - 10,000 行: 可用但较慢
- ❌ 单表 > 10,000 行: 不推荐

**JOIN 性能**:
- 两表 JOIN（各 100 行）: < 1 秒
- 两表 JOIN（各 1,000 行）: 数秒
- 三表 JOIN（各 100 行）: 数秒
- 不建议超过 3 表 JOIN

### 并发限制

- ❌ 不支持多用户并发访问
- ❌ 不支持多进程同时写入
- ⚠️  多个实例同时运行会导致数据损坏

### 数据持久性

- ✅ 元数据自动持久化到 `catalog.meta`
- ✅ 数据自动持久化到 CSV 文件
- ⚠️  异常终止可能导致部分数据丢失（无 WAL 日志）

### 安全性

- ❌ 无用户权限管理
- ❌ 无 SQL 注入防护（本地单用户环境问题不大）
- ❌ 无数据加密

### 建议使用场景

**✅ 适合**:
- 学习数据库原理和 SQL 语法
- 快速原型开发和演示
- 小规模数据分析（< 10,000 行）
- 单用户桌面应用
- 教学和实验

**❌ 不适合**:
- 生产环境
- 多用户 Web 应用
- 大数据分析
- 高并发场景
- 对数据安全性有严格要求的场景

---

## 获取帮助

### 命令行帮助

```bash
./build/mini_dbms --help
```

### 查看版本信息

```bash
./build/mini_dbms --version
```

### 报告问题

遇到 Bug 或有功能建议？请访问项目仓库提交 Issue。

### 文档索引

- [架构设计文档](ARCHITECTURE.md): 系统设计和内部实现
- [API 参考手册](API_REFERENCE.md): 编程接口文档
- [实现细节指南](IMPLEMENTATION.md): 代码实现思路
