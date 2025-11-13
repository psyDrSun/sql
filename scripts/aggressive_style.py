#!/usr/bin/env python3
"""
Aggressively make code look like "intern style":
- Insert `using namespace std;` after includes in all .hpp/.h/.cpp/.cc/.cxx files (idempotent)
- Strip all occurrences of `std::` (blindly), leaving nested namespace names like `filesystem::` in place.

Warning: This is intentionally unsafe and may cause name collisions. Run with version control.

Usage:
  python3 scripts/aggressive_style.py --apply
  python3 scripts/aggressive_style.py          # dry-run
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILE_GLOBS = ["**/*.hpp", "**/*.h", "**/*.cpp", "**/*.cc", "**/*.cxx"]
TARGET_DIRS = [ROOT / "include", ROOT / "src", ROOT / "test_parser", ROOT / "simple_parser"]

INCLUDE_LINE = re.compile(r"^\s*#\s*include\b")
USING_NS_STD = "using namespace std;"


def find_files():
    files = []
    for d in TARGET_DIRS:
        if not d.exists():
            continue
        for pat in FILE_GLOBS:
            files.extend(p for p in d.glob(pat) if p.is_file())
    # Dedup
    seen = set()
    uniq = []
    for f in files:
        if f in seen:
            continue
        seen.add(f)
        uniq.append(f)
    return uniq


def insert_using_namespace_std(lines: list[str]) -> tuple[list[str], bool]:
    # If already present anywhere, skip
    if any(USING_NS_STD in ln for ln in lines):
        return lines, False
    # Find insertion point after includes at top
    n = len(lines)
    i = 0
    last_inc = -1
    while i < n:
        if INCLUDE_LINE.match(lines[i]):
            last_inc = i
            i += 1
            continue
        if last_inc >= 0 and not lines[i].strip():
            i += 1
            continue
        break
    insert_at = (last_inc + 1) if last_inc >= 0 else 0
    new_lines = lines[:insert_at] + (["", USING_NS_STD, ""] if insert_at < n else [USING_NS_STD, ""]) + lines[insert_at:]
    return new_lines, True


STD_QUAL = re.compile(r"\bstd::")


def strip_std(text: str) -> tuple[str, int]:
    return STD_QUAL.subn("", text)


def process(path: Path, apply: bool) -> str | None:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    new_lines, inserted = insert_using_namespace_std(lines)
    new_text = "\n".join(new_lines) + ("\n" if text.endswith("\n") else "")

    stripped_text, nsubs = strip_std(new_text)

    if stripped_text == text:
        return None
    if apply:
        path.write_text(stripped_text, encoding="utf-8")
    changes = []
    if inserted:
        changes.append("+ using namespace std;")
    if nsubs:
        changes.append(f"~ removed {nsubs}x 'std::'")
    return f"{path}: " + ", ".join(changes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    files = find_files()
    any_change = False
    for f in files:
        summary = process(f, apply=args.apply)
        if summary:
            any_change = True
            print(summary)
    if not any_change:
        print("No changes.")


if __name__ == "__main__":
    main()
