#!/usr/bin/env python3
"""Reject developer-local absolute paths in tracked repository text files."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    patterns = [
        "/" + "Users" + r"/[^/[:space:]]+/",
        "/" + "home" + r"/[^/[:space:]]+/",
        r"[A-Za-z]:\\" + "Users" + r"\\[^\\[:space:]]+\\",
        "codex-" + "remote-attachments",
    ]
    expression = "(" + "|".join(patterns) + ")"
    result = subprocess.run(
        ["git", "grep", "-n", "-I", "-E", expression, "--", "."],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )

    if result.returncode == 1:
        print("OK: no developer-local absolute paths in tracked text files")
        return 0
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr, end="")
        return result.returncode

    print("Developer-local absolute paths found:", file=sys.stderr)
    print(result.stdout, file=sys.stderr, end="")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
