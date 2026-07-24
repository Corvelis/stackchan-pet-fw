#!/usr/bin/env python3
"""Require public documentation to exist and change in Japanese/English pairs."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def documentation_pairs() -> set[tuple[Path, Path]]:
    pairs = {
        (ROOT / "CHANGELOG.ja.md", ROOT / "CHANGELOG.md"),
        (ROOT / "README.ja.md", ROOT / "README.en.md"),
    }

    docs_dir = ROOT / "docs"
    for japanese in docs_dir.glob("*.ja.md"):
        english = japanese.with_name(japanese.name.replace(".ja.md", ".md"))
        pairs.add((japanese, english))
    for english in docs_dir.glob("*.md"):
        if english.name.endswith(".ja.md"):
            continue
        japanese = english.with_name(english.name.replace(".md", ".ja.md"))
        pairs.add((japanese, english))

    tools_dir = ROOT / "tools" / "face_image_builder"
    for japanese in tools_dir.rglob("README.md"):
        pairs.add((japanese, japanese.with_name("README.en.md")))
    for english in tools_dir.rglob("README.en.md"):
        pairs.add((english.with_name("README.md"), english))

    return pairs


def changed_files(base: str) -> set[str]:
    if not base:
        return set()
    committed = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMRT", f"{base}...HEAD"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    staged = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMRT"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return {
        line
        for output in (committed.stdout, staged.stdout, untracked.stdout)
        for line in output.splitlines()
        if line
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--base",
        default="",
        help="Git base revision; when set, both sides of every changed pair must change.",
    )
    args = parser.parse_args()

    failures: list[str] = []
    pairs = documentation_pairs()
    for japanese, english in sorted(pairs, key=lambda pair: relative(pair[0])):
        missing = [relative(path) for path in (japanese, english) if not path.is_file()]
        if missing:
            failures.append("missing paired document: " + ", ".join(missing))

    contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")
    for required_heading in ("## 日本語", "## English"):
        if required_heading not in contributing:
            failures.append(f"CONTRIBUTING.md is missing {required_heading!r}")

    changed = changed_files(args.base)
    if changed:
        for japanese, english in sorted(pairs, key=lambda pair: relative(pair[0])):
            japanese_name = relative(japanese)
            english_name = relative(english)
            japanese_changed = japanese_name in changed
            english_changed = english_name in changed
            if japanese_changed != english_changed:
                changed_name = japanese_name if japanese_changed else english_name
                missing_name = english_name if japanese_changed else japanese_name
                failures.append(
                    f"{changed_name} changed without paired update to {missing_name}"
                )

    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1

    suffix = f"; compared with {args.base}" if args.base else ""
    print(f"OK: {len(pairs)} Japanese/English documentation pairs{suffix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
