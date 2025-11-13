-- Verification Scenario for the Mini-DBMS
-- Covers DDL, DML, DQL (incl. JOIN), and robustness checks

-- 1. DDL: create tables
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

-- 1.2 Robustness: duplicate create
CREATE TABLE students (
    s_id INT
);

-- 1.3 Reserved-word-ish names (may fail depending on parser)
CREATE TABLE select_keyword_table (
    from_keyword INT,
    where_keyword VARCHAR
);

-- 1.4 Syntax errors
CREATE TABL students_invalid (id INT);
CREATE TABLE students_invalid (id INT;);

-- 2. DML: inserts
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

-- Robustness: non-existent table and wrong arity/type
INSERT INTO non_exist_table VALUES (1, 1);
INSERT INTO students VALUES (106, 'Frank');
INSERT INTO students VALUES (107, 'Grace', 'twenty');

-- 2.5 sanity
SELECT * FROM students;
SELECT * FROM courses;
SELECT * FROM enrollments;

-- 3. DQL
SELECT * FROM students;
SELECT s_name, s_age FROM students;
SELECT c_name, c_teacher FROM courses;

SELECT * FROM students WHERE s_age > 20;
SELECT * FROM students WHERE s_name = 'Alice';
SELECT * FROM students WHERE s_id <> 102;
SELECT * FROM courses WHERE c_teacher = '张老师';

SELECT * FROM students WHERE s_age = 20 AND s_name = 'Charlie';
SELECT * FROM enrollments WHERE s_id = 101 AND grade > 90;

SELECT
    s.s_name,
    e.c_id
FROM
    students s
INNER JOIN
    enrollments e ON s.s_id = e.s_id;

SELECT
    s.s_name,
    c.c_name,
    c.c_teacher
FROM
    students s
INNER JOIN
    enrollments e ON s.s_id = e.s_id
INNER JOIN
    courses c ON e.c_id = c.c_id;

SELECT
    s.s_name,
    c.c_name,
    e.grade
FROM
    students s
INNER JOIN
    enrollments e ON s.s_id = e.s_id
INNER JOIN
    courses c ON e.c_id = c.c_id
WHERE
    c.c_teacher = '张老师' AND e.grade > 90;

SELECT
    c.c_name,
    e.grade
FROM
    courses c
INNER JOIN
    enrollments e ON c.c_id = e.c_id;

SELECT invalid_col FROM students;
SELECT * FROM invalid_table;

SELECT s_id FROM students INNER JOIN enrollments ON students.s_id = enrollments.s_id;

-- 4. UPDATE / DELETE
UPDATE students SET s_age = 21 WHERE s_name = 'Alice';
SELECT * FROM students WHERE s_name = 'Alice';

UPDATE students SET s_age = 19 WHERE s_age = 20;
SELECT * FROM students;

UPDATE students SET s_age = 100 WHERE s_name = 'Nobody';
SELECT * FROM students;

DELETE FROM enrollments WHERE s_id = 102;
SELECT * FROM enrollments;

DELETE FROM enrollments;
SELECT * FROM enrollments;

DELETE FROM students WHERE s_id = 999;
SELECT * FROM students;

-- 5. DROP
DROP TABLE students;
DROP TABLE courses;
DROP TABLE enrollments;
DROP TABLE select_keyword_table;

DROP TABLE students;
SELECT * FROM students;
