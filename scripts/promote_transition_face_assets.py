#!/usr/bin/env python3
"""Promote validated transitional face directories to the minimal v2 layout.

The source JPEG files are copied byte-for-byte. This keeps hardware-tested
encoding and image alignment intact while removing legacy-only runtime files.
Generated directories are written under ``face_assets_v2_work/`` by default;
the repository runtime directories are never modified by this command.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.validate_face_assets import (
    MANIFEST_NAME,
    ROOT,
    TARGETS,
    TargetSpec,
    build_v2_manifest,
    expected_v2_files,
    validate_directory,
)


DEFAULT_OUTPUT_ROOT = ROOT / "face_assets_v2_work" / "promoted"
STOPWATCH_DIRECTION_SOURCES = (*range(0, 16, 2), 16)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Copy complete transitional runtime image directories into the "
            "minimal face asset v2 layout without recompressing JPEG files."
        ),
    )
    parser.add_argument(
        "targets",
        nargs="*",
        choices=(*TARGETS.keys(), "all"),
        default=["all"],
        help="Targets to promote. Defaults to all.",
    )
    parser.add_argument(
        "--source-root",
        type=Path,
        default=ROOT,
        help="Repository-style root containing data directories. Defaults to the project root.",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=DEFAULT_OUTPUT_ROOT,
        help="Parent directory for promoted target folders.",
    )
    parser.add_argument(
        "--replace",
        action="store_true",
        help="Replace an existing promoted target after the new set validates.",
    )
    return parser.parse_args()


def expand_targets(targets: list[str]) -> list[TargetSpec]:
    if not targets or "all" in targets:
        return list(TARGETS.values())
    return [TARGETS[name] for name in targets]


def source_name_for_output(target: TargetSpec, output_name: str) -> str:
    stem = Path(output_name).stem
    if stem.startswith("base_m"):
        return output_name.replace("base_", "voice_", 1)
    if target.name == "stopwatch" and stem.startswith("dir"):
        output_index = int(stem.removeprefix("dir"))
        return f"dir{STOPWATCH_DIRECTION_SOURCES[output_index]}.jpg"
    if stem == f"blink{target.direction_center_index}":
        return "blink16.jpg"
    return output_name


def promote_target(
    source_root: Path,
    output_root: Path,
    target: TargetSpec,
    replace: bool,
) -> Path:
    source = source_root / target.directory
    source_validation = validate_directory(source, target, "transition")
    if not source_validation.ok:
        raise RuntimeError(
            f"source transition set is invalid for {target.name}: "
            + "; ".join(source_validation.errors)
        )

    destination = output_root / target.name
    if destination.exists() and not replace:
        raise RuntimeError(f"output already exists (pass --replace): {destination}")

    output_root.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{target.name}-", dir=output_root))
    try:
        for output_name in sorted(expected_v2_files(target)):
            source_name = source_name_for_output(target, output_name)
            shutil.copy2(source / source_name, staging / output_name)

        manifest = build_v2_manifest(target)
        manifest["generatedBy"] = "scripts/promote_transition_face_assets.py"
        (staging / MANIFEST_NAME).write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        result = validate_directory(staging, target, "v2")
        if not result.ok:
            raise RuntimeError(
                f"promoted set failed validation for {target.name}: "
                + "; ".join(result.errors)
            )

        if destination.exists():
            shutil.rmtree(destination)
        staging.replace(destination)
        print(
            f"{target.name}: copied {result.checked_files} images and {MANIFEST_NAME} "
            f"to {destination}"
        )
        return destination
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise


def main() -> int:
    args = parse_args()
    source_root = args.source_root.expanduser().resolve()
    output_root = args.output_root.expanduser().resolve()
    protected = {(ROOT / spec.directory).resolve() for spec in TARGETS.values()}
    if output_root in protected or any(parent in protected for parent in output_root.parents):
        raise SystemExit("refusing to generate inside a runtime data directory")

    try:
        for target in expand_targets(args.targets):
            promote_target(source_root, output_root, target, args.replace)
    except RuntimeError as exc:
        raise SystemExit(str(exc)) from exc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
