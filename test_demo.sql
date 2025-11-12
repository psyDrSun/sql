-- ============================================================================
-- 简化版 SQL 解析器测试文件
-- 使用方法: ./test_parser test_demo.sql
-- ============================================================================

-- 测试1: 创建表 (成功)
CREATE TABLE students (id INT, name VARCHAR, age INT);

-- 测试2: 插入数据 (成功)
INSERT INTO students VALUES (101, 'Alice', 20);

-- 测试3: 插入数据 (成功)
INSERT INTO students VALUES (102, 'Bob', 22);

-- 测试4: 查询数据 (成功)
SELECT * FROM students WHERE age = 20;

-- 测试5: 类型错误 (失败 - id 应该是数字)
INSERT INTO students VALUES ('invalid', 'Charlie', 25);

-- 测试6: 表不存在 (失败)
SELECT * FROM courses;

-- 测试7: 创建第二张表 (成功)
CREATE TABLE courses (id INT, title VARCHAR);

-- 测试8: 重复创建表 (失败)
CREATE TABLE students (id INT);
