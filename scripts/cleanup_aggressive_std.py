#!/usr/bin/env python3
"""
Cleanup after aggressive std removal:
- Ensure a single `using namespace std;` sits after the initial include block.
- Remove invalid `using X;` lines that have no qualifier `::` and are not type-aliases (no '=').

Run from repo root:
  python3 scripts/cleanup_aggressive_std.py --apply
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILE_GLOBS = ["**/*.hpp", "**/*.h", "**/*.cpp", "**/*.cc", "**/*.cxx"]
TARGET_DIRS = [ROOT / "include", ROOT / "src", ROOT / "test_parser", ROOT / "simple_parser"]

INCLUDE_LINE = re.compile(r"^\s*#\s*include\b")
USING_NS_STD = re.compile(r"^\s*using\s+namespace\s+std\s*;\s*$")
USING_BARE = re.compile(r"^\s*using\s+([A-Za-z_]\w*)\s*;\s*$")  # no :: and no =


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


def relocate_using_namespace_std(lines: list[str]) -> list[str]:
    # Remove all occurrences, remember to re-insert later
    lines_wo = [ln for ln in lines if not USING_NS_STD.match(ln)]
    # Find insertion index after initial includes
    n = len(lines_wo)
    i = 0
    last_inc = -1
    while i < n:
        if INCLUDE_LINE.match(lines_wo[i]):
            last_inc = i
            i += 1
            continue
        if last_inc >= 0 and not lines_wo[i].strip():
            i += 1
            continue
        break
    insert_at = (last_inc + 1) if last_inc >= 0 else 0
    new_lines = lines_wo[:insert_at] + (["using namespace std;", ""] if insert_at < n else ["using namespace std;"]) + lines_wo[insert_at:]
    return new_lines


def remove_invalid_using(lines: list[str]) -> list[str]:
    out = []
    for ln in lines:
        # Keep aliases like 'using T = ...;'
        if USING_BARE.match(ln):
            continue
        out.append(ln)
    return out


def process(path: Path, apply: bool) -> str | None:
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    new_lines = relocate_using_namespace_std(lines)
    new_lines = remove_invalid_using(new_lines)
    new_text = "\n".join(new_lines) + ("\n" if text.endswith("\n") else "")
    if new_text == text:
        return None
    if apply:
        path.write_text(new_text, encoding="utf-8")
    return f"{path}: cleaned"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    files = find_files()
    any_change = False
    for f in files:
        s = process(f, apply=args.apply)
        if s:
            any_change = True
            print(s)
    if not any_change:
        print("No changes.")


if __name__ == "__main__":
    main()
