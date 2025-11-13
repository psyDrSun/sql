# Mini-DBMS 快速参考卡

## 常用命令

### 编译与运行
```bash
cmake -S . -B build && cmake --build build

./build/mini_dbms

./build/mini_dbms -f script.sql

./build/mini_dbms -f script.sql -l 10-20

./build/mini_dbms -w demo.sql
```

---

## SQL 语法速查

### DDL（表结构）

```sql
CREATE TABLE users (id INT, name VARCHAR, age INT);

DROP TABLE users;

ALTER TABLE users RENAME TO members;
ALTER TABLE users ADD COLUMN email VARCHAR;
ALTER TABLE users DROP COLUMN age;
ALTER TABLE users MODIFY COLUMN name VARCHAR;
```

### DML（数据操作）

```sql
INSERT INTO users VALUES (1, 'Alice', 25);

UPDATE users SET age = 26 WHERE id = 1;

DELETE FROM users WHERE age < 18;
```

### DQL（查询）

```sql
SELECT * FROM users;
SELECT id, name FROM users WHERE age > 20;

SELECT * FROM users JOIN orders ON users.id = orders.user_id;

SELECT * FROM a JOIN b ON a.id = b.a_id JOIN c ON b.id = c.b_id;

SELECT * FROM users WHERE age > 18 AND age < 30;
```

---

## WHERE 操作符

| 操作符 | 说明 | 示例 |
|--------|------|------|
| `=` | 等于 | `WHERE id = 1` |
| `<` | 小于 | `WHERE age < 18` |
| `>` | 大于 | `WHERE price > 100` |
| `<=` | 小于等于 | `WHERE score <= 60` |
| `>=` | 大于等于 | `WHERE age >= 18` |
| `!=` 或 `<>` | 不等于 | `WHERE status != 'deleted'` |
| `AND` | 逻辑与 | `WHERE age > 18 AND age < 30` |

---

## 数据类型

| 类型 | 说明 | 默认长度 |
|------|------|---------|
| `INT` | 整数 | 4 字节 |
| `VARCHAR` | 可变长字符串 | 255 字符 |

---

## 常见错误

| 错误信息 | 原因 | 解决方法 |
|---------|------|---------|
| `Table 'xxx' does not exist` | 表不存在 | 先执行 CREATE TABLE |
| `Column count mismatch` | INSERT 值数量与列数不符 | 检查 VALUES 列表 |
| `Column 'xxx' does not exist` | 引用的列不存在 | 检查列名拼写 |
| `Syntax error: unexpected token` | SQL 语法错误 | 检查 SQL 语句语法 |
| `Type mismatch for column 'xxx'` | 插入值类型错误 | INT 列只接受数字 |

---

## 文件与路径

| 路径 | 说明 |
|------|------|
| `./build/mini_dbms` | 可执行文件 |
| `./data/` | 数据文件目录 |
| `./data/catalog.meta` | 元数据文件 |
| `./data/<table>.csv` | 表数据文件 |

---

## 特殊命令

| 命令 | 说明 |
|------|------|
| `exit;` 或 `quit;` | 退出交互式模式 |
| Ctrl+C | 强制退出 |
| Ctrl+D | EOF（退出） |

---

## 性能参考

| 场景 | 数据规模 | 性能 |
|------|---------|------|
| SELECT（无 WHERE） | 1,000 行 | < 1 秒 |
| SELECT（WHERE） | 1,000 行 | < 1 秒 |
| 两表 JOIN | 100×100 | < 1 秒 |
| 两表 JOIN | 1,000×1,000 | 数秒 |
| 三表 JOIN | 100×100×100 | 数秒 |
| INSERT | 单行 | 毫秒级 |

---

## 限制

### ❌ 不支持的功能
- 主键/外键/唯一约束
- 索引
- 事务（COMMIT/ROLLBACK）
- NULL 值
- 子查询
- GROUP BY / ORDER BY / LIMIT
- 聚合函数（COUNT, SUM, AVG）
- LEFT/RIGHT/OUTER JOIN
- DISTINCT
- UNION / INTERSECT

### ⚠️  注意事项
- 仅单用户使用
- 无并发控制
- 大表（>10,000 行）性能差
- JOIN 使用嵌套循环（慢）

---

## 文档导航

| 文档 | 用途 | 时长 |
|------|------|------|
| [README.md](../README.md) | 项目总览 | 10 分钟 |
| [用户手册](USER_MANUAL.md) | 完整使用指南 | 30 分钟 |
| [架构文档](ARCHITECTURE.md) | 系统设计 | 45 分钟 |
| [API 参考](API_REFERENCE.md) | 编程接口 | 查阅式 |
| [实现细节](IMPLEMENTATION.md) | 算法详解 | 1-2 小时 |
| [开发指南](DEVELOPER_GUIDE.md) | 贡献代码 | 1 小时 |

---

## 快速示例

### 创建学生成绩系统

```sql
CREATE TABLE students (id INT, name VARCHAR, age INT);
CREATE TABLE courses (id INT, title VARCHAR);
CREATE TABLE scores (student_id INT, course_id INT, grade INT);

INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 22);

INSERT INTO courses VALUES (101, 'Math');
INSERT INTO courses VALUES (102, 'Physics');

INSERT INTO scores VALUES (1, 101, 95);
INSERT INTO scores VALUES (1, 102, 88);
INSERT INTO scores VALUES (2, 101, 92);

SELECT students.name, courses.title, scores.grade
FROM students
JOIN scores ON students.id = scores.student_id
JOIN courses ON scores.course_id = courses.id
WHERE scores.grade > 90;
```

### 员工部门查询

```sql
CREATE TABLE departments (id INT, name VARCHAR);
CREATE TABLE employees (id INT, name VARCHAR, dept_id INT, salary INT);

INSERT INTO departments VALUES (1, 'Engineering');
INSERT INTO departments VALUES (2, 'Sales');

INSERT INTO employees VALUES (101, 'Alice', 1, 5000);
INSERT INTO employees VALUES (102, 'Bob', 2, 4000);

SELECT employees.name, departments.name, employees.salary
FROM employees
JOIN departments ON employees.dept_id = departments.id
WHERE employees.salary > 4500;
```

---

## 调试技巧

### 查看数据文件
```bash
cat ./data/users.csv

ls ./data/
```

### 查看元数据
```bash
cat ./data/catalog.meta
```

### 备份数据
```bash
cp -r ./data ./backup_$(date +%Y%m%d)
```

### 恢复数据
```bash
rm -rf ./data
cp -r ./backup_20251113 ./data
```

---

## 帮助资源

- **用户手册**: `docs/USER_MANUAL.md`
- **在线帮助**: `./build/mini_dbms --help`
- **示例脚本**: `demo.sql`, `verification_scenario.sql`
- **开发文档**: `docs/DEVELOPER_GUIDE.md`

---

**提示**: 将此文件打印或保存为书签，方便快速查阅！
