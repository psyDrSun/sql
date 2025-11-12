# Mini-DBMS

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