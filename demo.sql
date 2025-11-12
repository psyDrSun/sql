-- ============================================================================
-- Mini-DBMS 综合测试套件
-- 使用方法: ./build/mini_dbms --watch demo.sql
-- 然后在编辑器中修改此文件，保存后在终端按回车执行
-- ============================================================================

-- ============================================================================
-- 第一阶段：DDL - 表结构定义与初步验证
-- ============================================================================

-- 1.1 [基本功能] 创建三张核心表
CREATE TABLE students (
    s_id INT,
    s_name VARCHAR,
    s_age INT
);

CREATE TABLE courses (
    c_id INT,
    c_name VARCHAR,
    c_teacher VARCHAR
);

CREATE TABLE enrollments (
    s_id INT,
    c_id INT,
    grade INT
);

-- 1.2 [鲁棒性] 尝试创建已经存在的表 (应报错)
CREATE TABLE students (
    s_id INT
);

-- 1.3 [鲁棒性] 测试包含SQL关键字作为表名或列名
CREATE TABLE select_keyword_table (
    from_keyword INT,
    where_keyword VARCHAR
);

-- 1.4 [鲁棒性] 测试无效的语法 (应报错)
CREATE TABL students_invalid (id INT);

-- ============================================================================
-- 第二阶段：DML - 数据插入与初步验证
-- ============================================================================

-- 2.1 [基本功能] 向三张表中插入有效数据
INSERT INTO students VALUES (101, 'Alice', 20);
INSERT INTO students VALUES (102, 'Bob', 22);
INSERT INTO students VALUES (103, 'Charlie', 20);
INSERT INTO students VALUES (104, 'David', 23);
INSERT INTO students VALUES (105, 'Eve', 21);

INSERT INTO courses VALUES (1, '数据库原理', '张老师');
INSERT INTO courses VALUES (2, '操作系统', '李老师');
INSERT INTO courses VALUES (3, '计算机网络', '张老师');
INSERT INTO courses VALUES (4, '数据结构', '王老师');

INSERT INTO enrollments VALUES (101, 1, 95);
INSERT INTO enrollments VALUES (101, 2, 88);
INSERT INTO enrollments VALUES (102, 1, 92);
INSERT INTO enrollments VALUES (103, 2, 85);
INSERT INTO enrollments VALUES (103, 3, 91);
INSERT INTO enrollments VALUES (104, 1, 78);

-- 2.2 [鲁棒性] 插入到不存在的表中 (应报错)
INSERT INTO non_exist_table VALUES (1, 1);

-- 2.3 [鲁棒性] 插入错误数量的列值 (应报错)
INSERT INTO students VALUES (106, 'Frank');

-- 2.4 [鲁棒性] 插入类型不匹配的数据 (应报错)
INSERT INTO students VALUES (107, 'Grace', 'twenty');

-- 2.5 [验证] 查看插入后的数据
SELECT * FROM students;
SELECT * FROM courses;
SELECT * FROM enrollments;

-- ============================================================================
-- 第三阶段：DQL - 数据查询 (重点)
-- ============================================================================

-- 3.1 [基本查询] 全表查询和投影查询
SELECT * FROM students;
SELECT s_name, s_age FROM students;
SELECT c_name, c_teacher FROM courses;

-- 3.2 [带WHERE的简单查询] 测试不同操作符
SELECT * FROM students WHERE s_age > 20;
SELECT * FROM students WHERE s_name = 'Alice';
SELECT * FROM students WHERE s_id <> 102;
SELECT * FROM courses WHERE c_teacher = '张老师';

-- 3.3 [复杂WHERE查询] 测试多个AND条件
SELECT * FROM students WHERE s_age = 20 AND s_name = 'Charlie';
SELECT * FROM enrollments WHERE s_id = 101 AND grade > 90;

-- 3.4 [边界查询] 测试没有结果和全部是结果的查询
SELECT * FROM students WHERE s_age > 30;
SELECT * FROM students WHERE s_age > 0;

-- 3.5 [核心复杂度] 两表连接查询 (INNER JOIN)
SELECT s.s_name, e.c_id FROM students s INNER JOIN enrollments e ON s.s_id = e.s_id;

-- 3.6 [核心复杂度] 三表连接查询 (链式 INNER JOIN)
SELECT s.s_name, c.c_name, c.c_teacher FROM students s INNER JOIN enrollments e ON s.s_id = e.s_id INNER JOIN courses c ON e.c_id = c.c_id;

-- 3.7 [核心复杂度] 带筛选条件的复杂连接查询
SELECT s.s_name, c.c_name, e.grade FROM students s INNER JOIN enrollments e ON s.s_id = e.s_id INNER JOIN courses c ON e.c_id = c.c_id WHERE c.c_teacher = '张老师' AND e.grade > 90;

-- 3.8 [进阶复杂度] 验证JOIN部分是否正确
SELECT c.c_name, e.grade FROM courses c INNER JOIN enrollments e ON c.c_id = e.c_id;

-- 3.9 [鲁棒性] 查询不存在的列或表 (应报错)
SELECT invalid_col FROM students;
SELECT * FROM invalid_table;

-- 3.10 [鲁棒性] 在JOIN中查询有歧义的列 (应报错)
SELECT s_id FROM students INNER JOIN enrollments ON students.s_id = enrollments.s_id;

-- 3.11 [进阶鲁棒性] LEFT JOIN (不支持，应报错)
SELECT s.s_name, c.c_name FROM students s LEFT JOIN enrollments e ON s.s_id = e.s_id LEFT JOIN courses c ON e.c_id = c.c_id;

-- ============================================================================
-- 第四阶段：DDL - ALTER TABLE 表结构修改
-- ============================================================================

-- 4.1 [基本功能] 重命名表
ALTER TABLE select_keyword_table RENAME TO keyword_test;
SELECT * FROM keyword_test;

-- 4.2 [基本功能] 添加新列
ALTER TABLE keyword_test ADD COLUMN new_column VARCHAR;
SELECT * FROM keyword_test;

-- 4.3 [基本功能] 修改列类型
ALTER TABLE keyword_test MODIFY COLUMN from_keyword VARCHAR;
SELECT * FROM keyword_test;

-- 4.4 [基本功能] 删除列
ALTER TABLE keyword_test DROP COLUMN new_column;
SELECT * FROM keyword_test;

-- 4.5 [鲁棒性] 对不存在的表执行 ALTER (应报错)
ALTER TABLE non_exist_table ADD COLUMN test INT;

-- 4.6 [鲁棒性] 删除不存在的列 (应报错)
ALTER TABLE keyword_test DROP COLUMN non_exist_column;

-- ============================================================================
-- 第五阶段：DML - 数据修改与删除
-- ============================================================================

-- 5.1 [基本功能] 更新特定行
UPDATE students SET s_age = 21 WHERE s_name = 'Alice';
SELECT * FROM students WHERE s_name = 'Alice';

-- 5.2 [复杂性] 更新多行
UPDATE students SET s_age = 19 WHERE s_age = 20;
SELECT * FROM students;

-- 5.3 [边界情况] 更新不存在的行 (应不产生任何效果)
UPDATE students SET s_age = 100 WHERE s_name = 'Nobody';
SELECT * FROM students;

-- 5.4 [基本功能] 删除特定行
DELETE FROM enrollments WHERE s_id = 102;
SELECT * FROM enrollments;

-- 5.5 [危险操作] 删除整张表的数据 (无WHERE条件)
DELETE FROM enrollments;
SELECT * FROM enrollments;

-- 5.6 [鲁棒性] 删除不存在的行 (应不产生任何效果)
DELETE FROM students WHERE s_id = 999;
SELECT * FROM students;

-- ============================================================================
-- 第六阶段：DDL - 清理与收尾
-- ============================================================================

-- 6.1 [基本功能] 删除表
DROP TABLE students;
DROP TABLE courses;
DROP TABLE enrollments;
DROP TABLE keyword_test;

-- 6.2 [鲁棒性] 删除不存在的表 (应报错)
DROP TABLE students;

-- 6.3 [鲁棒性] 删除后再次查询，应报错 (表不存在)
SELECT * FROM students;
