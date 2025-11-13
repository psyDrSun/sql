#!/usr/bin/env python3
"""
Introduce per-symbol `using std::X;` declarations and drop `std::` qualifiers for single-segment symbols.

What it does (per file):
- Scans for tokens like `std::vector`, `std::string`, `std::getline`, etc. (i.e., not immediately followed by `::`).
- Inserts `using std::<symbol>;` after the last #include block (skips ones already present).
- Rewrites those occurrences to unqualified names (e.g., `std::vector` -> `vector`).

It intentionally DOES NOT touch nested uses such as `std::string::npos` or `std::filesystem::rename` to avoid invalid using of namespaces or class members. Handle those manually if desired.

Run from repo root:
  python3 scripts/introduce_std_using.py --apply

Dry-run (default): shows planned changes without writing files.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_DIRS = [ROOT / "include", ROOT / "src", ROOT / "test_parser", ROOT / "simple_parser"]
FILE_GLOBS = ["**/*.hpp", "**/*.h", "**/*.cpp", "**/*.cc", "**/*.cxx"]

# Matches std::Identifier where Identifier is NOT followed by '::'
STD_SINGLE_SEGMENT = re.compile(r"\bstd::([A-Za-z_]\w+)(?!::)")

# Basic heuristic to find insertion point: after the last contiguous #include near the top.
INCLUDE_LINE = re.compile(r"^\s*#\s*include\b")
USING_LINE = re.compile(r"^\s*using\s+std::[A-Za-z_][\w:]*\s*;\s*$")


def find_files():
    files: list[Path] = []
    for base in INCLUDE_DIRS:
        if not base.exists():
            continue
        for pat in FILE_GLOBS:
            files.extend(p for p in base.glob(pat) if p.is_file())
    # De-duplicate
    seen = set()
    unique = []
    for f in files:
        if f in seen:
            continue
        seen.add(f)
        unique.append(f)
    return unique


def compute_insert_index(lines: list[str]) -> int:
    idx = 0
    n = len(lines)
    # Skip any shebang/comment at the top
    while idx < n and lines[idx].strip().startswith("//"):
        idx += 1
    # Find last include in the initial block
    last_include = -1
    i = 0
    while i < n:
        line = lines[i]
        if INCLUDE_LINE.match(line):
            last_include = i
            i += 1
            continue
        # allow blank lines between includes at the top
        if last_include >= 0 and not line.strip():
            i += 1
            continue
        break
    return (last_include + 1) if last_include >= 0 else 0


def process_file(path: Path, apply: bool) -> tuple[bool, str]:
    text = path.read_text(encoding="utf-8")
    # Find symbols
    symbols = sorted(set(m.group(1) for m in STD_SINGLE_SEGMENT.finditer(text)))
    if not symbols:
        return False, f"{path}: no std::X occurrences"

    lines = text.splitlines()
    insert_at = compute_insert_index(lines)

    # Determine which using lines already exist
    existing_usings = set()
    for line in lines:
        m = USING_LINE.match(line)
        if m:
            # normalize whitespace
            existing_usings.add(line.strip().replace(" ", ""))

    new_using_lines = []
    for sym in symbols:
        candidate = f"using std::{sym};"
        normalized = candidate.replace(" ", "")
        if normalized in existing_usings:
            continue
        new_using_lines.append(candidate)

    if not new_using_lines and not symbols:
        return False, f"{path}: nothing to change"

    # Build new content
    header = lines[:insert_at]
    tail = lines[insert_at:]

    insertion_block = []
    if new_using_lines:
        insertion_block.append("")
        insertion_block.extend(new_using_lines)
        insertion_block.append("")

    # Replace occurrences in full text (safe single-segment only)
    def repl(m: re.Match[str]) -> str:
        return m.group(1)

    replaced_text = STD_SINGLE_SEGMENT.sub(repl, text)

    # Re-split to insert at the right spot
    new_lines = replaced_text.splitlines()
    new_text = "\n".join(new_lines[:insert_at] + insertion_block + new_lines[insert_at:]) + ("\n" if replaced_text.endswith("\n") else "")

    if apply:
        path.write_text(new_text, encoding="utf-8")
        return True, f"{path}: applied {len(new_using_lines)} using(s), replaced {len(symbols)} symbol(s)"
    else:
        # Diff-like summary
        summary = [f"{path}:"]
        for u in new_using_lines:
            summary.append(f"  + {u}")
        for s in symbols:
            summary.append(f"  ~ std::{s} -> {s}")
        return True, "\n".join(summary)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="Write changes to files")
    args = parser.parse_args()

    files = find_files()
    any_change = False
    for f in files:
        changed, msg = process_file(f, apply=args.apply)
        if changed:
            any_change = True
            print(msg)
    if not any_change:
        print("No changes needed.")


if __name__ == "__main__":
    main()
