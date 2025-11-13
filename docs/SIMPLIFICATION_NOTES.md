# Simplifying the code safely (without losing features)

This mini-DBMS aims to stay readable and predictable. Below are practical guardrails to simplify the codebase while keeping behavior correct (including multi-table joins). The lexing/parsing/semantic parts are excluded per request; focus on the rest of the system.

## Principles

- Prefer straight-line code
  - Fewer branches and helpers that do one thing well (e.g., `load_table_data`, `literal_to_storage`).
  - Return early on errors; avoid deep nesting.
- Use plain data structures
  - `vector`, `string`, `unordered_map` are enough here; avoid custom containers.
  - Represent rows as `vector<string>` (storage) and convert at the edge.
- Make conversions explicit and local
  - Only convert string <-> int at the boundary (when reading/writing storage or matching predicates).
  - Keep helper functions pure where possible for easier reasoning and testing.
- Avoid cleverness
  - No template metaprogramming, no operator overloading, no complex traits.
  - Prefer simple loops over multi-step algorithms for small data sizes.

## Concrete changes that help

- CSV storage utilities
  - Keep `split_csv_line`, `escape_csv_field`, and `join_csv_fields` as the only CSV helpers.
  - No partial parsing in-place; read line -> split -> transform -> write.
- Error handling
  - Throw `runtime_error` with a short, user-facing message; catch at the CLI layer and print.
  - Keep messages consistent: mention which table/column/operation failed.
- Execution path (joins and filters)
  - Implement joins as nested loops with an optional hash lookup:
    - Build a small `unordered_map` when joining by equality on obvious keys.
    - Otherwise, keep the nested loops; the data is small and clarity wins.
  - Evaluate predicates with a single dispatcher that checks operand types and operators.
- Names and structure
  - One translation unit per subsystem (`CLIHandler.cpp`, `SQLParser.cpp`, `ExecutionEngine.cpp`, `StorageManager.cpp`).
  - Short, consistent names for members and locals (c_/s_/p_ etc.) to reduce visual noise.

## What NOT to simplify (excluded by request)

- Lexing/Parsing/Semantics: leave the grammar recognition and AST as-is or better tested. It is acceptable to add comments and tests, but avoid refactors that change the grammar shape.

## Mini road-map (optional)

- Add unit-ish checks for helpers (pure functions):
  - `split_csv_line`, `escape_csv_field`, `join_csv_fields`, `literal_to_storage`, `storage_to_literal`.
- Consolidate error text to a single place if needed (optional `errors.hpp`).
- Keep the SELECT path as: load tables -> build binding context -> evaluate WHERE -> format result.

