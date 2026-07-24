#!/usr/bin/env python3
"""Validate Stack-chan runtime face assets without modifying them.

The repository is moving from the transitional 129-image layout to the v2
animated renderer.  During the migration, a directory without a manifest must
still match the complete transitional layout.  Once ``face_assets.json`` is
present, the directory is treated as v2 and partial v2 sets are rejected.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_NAME = "face_assets.json"


@dataclass(frozen=True)
class TargetSpec:
    name: str
    directory: str
    width: int
    height: int
    direction_frames: int
    direction_center_index: int


TARGETS = {
    "cores3": TargetSpec("cores3", "data", 240, 240, 17, 16),
    "stopwatch": TargetSpec("stopwatch", "data_stopwatch", 386, 386, 9, 8),
    "atoms3r": TargetSpec("atoms3r", "data_atoms3r", 128, 128, 17, 16),
}

LEGACY_MINIMAL_STEMS = (
    "idle",
    "listen",
    "talk_0",
    "talk_1",
    "blink",
)

LEGACY_FACE_STEMS = (
    "idle",
    "listen",
    "talk_0",
    "talk_1",
    "blink",
    "smile",
    "good_0",
    "good_1",
    "good_blink",
    "bad_0",
    "bad_1",
    "photo_0",
    "photo_1",
    "photo_blink",
    "photo_blink_talk",
    "photo_master_0",
    "photo_master_1",
    "nadenade_0",
    "nadenade_1",
    "furifuri_0",
    "furifuri_1",
    "idle_guarded_0",
    "blink_guarded_0",
    "talk_guarded_0",
    "talk_guarded_1",
    "idle_attached_0",
    "blink_attached_0",
    "talk_attached_0",
    "talk_attached_1",
    "pet_guarded_0",
    "pet_guarded_1",
    "pet_blink_guarded_0",
    "pet_attached_0",
    "pet_attached_1",
    "pet_blink_attached_0",
    "shake_guarded_0",
    "shake_guarded_1",
    "shake_attached_0",
    "shake_attached_1",
    "tired_0",
    "tired_talk",
    "tired_blink",
    "exhausted_0",
    "exhausted_talk",
    "exhausted_blink",
    "low_power_0",
    "low_power_talk",
    "low_power_blink",
)


@dataclass
class ValidationResult:
    target: TargetSpec
    directory: Path
    mode: str
    checked_files: int
    errors: list[str]
    warnings: list[str]

    @property
    def ok(self) -> bool:
        return not self.errors


def expected_transition_files() -> set[str]:
    names = {f"{stem}.jpg" for stem in LEGACY_FACE_STEMS}
    names.update(f"dir{index}.jpg" for index in range(17))
    names.update(f"blink{index}.jpg" for index in range(17))
    names.update(f"pet_anim_{index}.jpg" for index in range(16))
    names.update(f"dizzy_{index:02d}.jpg" for index in range(1, 16))
    names.update(
        f"voice_m{mouth}_e{eye}.jpg"
        for mouth in range(4)
        for eye in range(4)
    )
    return names


def expected_v2_files(target: TargetSpec) -> set[str]:
    names = {
        f"base_m{mouth}_e{eye}.jpg"
        for mouth in range(4)
        for eye in range(4)
    }
    names.update(f"pet_anim_{index}.jpg" for index in range(16))
    names.update(f"dir{index}.jpg" for index in range(target.direction_frames))
    names.add(f"blink{target.direction_center_index}.jpg")
    names.update(f"dizzy_{index:02d}.jpg" for index in range(1, 16))
    return names


def expected_legacy_minimal_files() -> set[str]:
    return {f"{stem}.jpg" for stem in LEGACY_MINIMAL_STEMS}


def build_v2_manifest(target: TargetSpec) -> dict[str, object]:
    return {
        "schemaVersion": 2,
        "renderer": "animated",
        "target": target.name,
        "canvas": {"width": target.width, "height": target.height},
        "assetCount": len(expected_v2_files(target)),
        "groups": {
            "base": {
                "pattern": "base_m{mouth}_e{eye}.jpg",
                "mouthFrames": 4,
                "eyeFrames": 4,
            },
            "pet": {
                "pattern": "pet_anim_{index}.jpg",
                "startIndex": 0,
                "frameCount": 16,
            },
            "direction": {
                "pattern": "dir{index}.jpg",
                "startIndex": 0,
                "frameCount": target.direction_frames,
                "centerIndex": target.direction_center_index,
            },
            "blink": {
                "files": [f"blink{target.direction_center_index}.jpg"],
            },
            "dizzy": {
                "pattern": "dizzy_{index:02d}.jpg",
                "startIndex": 1,
                "frameCount": 15,
            },
        },
    }


def _jpeg_dimensions(path: Path) -> tuple[int, int]:
    sof_markers = {
        0xC0,
        0xC1,
        0xC2,
        0xC3,
        0xC5,
        0xC6,
        0xC7,
        0xC9,
        0xCA,
        0xCB,
        0xCD,
        0xCE,
        0xCF,
    }
    with path.open("rb") as stream:
        if stream.read(2) != b"\xff\xd8":
            raise ValueError("content is not JPEG")

        while True:
            prefix = stream.read(1)
            if not prefix:
                break
            if prefix != b"\xff":
                continue

            marker_byte = stream.read(1)
            while marker_byte == b"\xff":
                marker_byte = stream.read(1)
            if not marker_byte:
                break
            marker = marker_byte[0]

            if marker in {0x01, *range(0xD0, 0xD9)}:
                continue
            if marker in {0xD9, 0xDA}:
                break

            length_bytes = stream.read(2)
            if len(length_bytes) != 2:
                break
            segment_length = struct.unpack(">H", length_bytes)[0]
            if segment_length < 2:
                raise ValueError("invalid JPEG segment length")

            if marker in sof_markers:
                payload = stream.read(5)
                if len(payload) != 5:
                    break
                height, width = struct.unpack(">HH", payload[1:5])
                if width <= 0 or height <= 0:
                    raise ValueError("invalid JPEG dimensions")
                return width, height

            stream.seek(segment_length - 2, 1)

    raise ValueError("JPEG dimensions not found")


def image_dimensions(path: Path) -> tuple[int, int]:
    return _jpeg_dimensions(path)


def _compare_manifest_subset(
    actual: object,
    expected: object,
    location: str,
    errors: list[str],
) -> None:
    if isinstance(expected, dict):
        if not isinstance(actual, dict):
            errors.append(f"{location} must be an object")
            return
        for key, expected_value in expected.items():
            if key not in actual:
                errors.append(f"{location}.{key} is missing")
                continue
            _compare_manifest_subset(actual[key], expected_value, f"{location}.{key}", errors)
        return
    if actual != expected:
        errors.append(f"{location} must be {expected!r}, got {actual!r}")


def validate_v2_manifest(path: Path, target: TargetSpec) -> list[str]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        return [f"cannot read {MANIFEST_NAME}: {exc}"]
    errors: list[str] = []
    _compare_manifest_subset(data, build_v2_manifest(target), "manifest", errors)
    return errors


def _visible_entries(directory: Path) -> Iterable[Path]:
    return (entry for entry in directory.iterdir() if not entry.name.startswith("."))


def validate_directory(directory: Path, target: TargetSpec, mode: str = "auto") -> ValidationResult:
    directory = directory.resolve()
    errors: list[str] = []
    warnings: list[str] = []
    manifest_path = directory / MANIFEST_NAME

    if not directory.is_dir():
        return ValidationResult(target, directory, mode, 0, ["directory does not exist"], warnings)

    selected_mode = mode
    if mode == "auto":
        if manifest_path.exists():
            selected_mode = "v2"
        else:
            selected_mode = "transition"
            warnings.append(
                f"{MANIFEST_NAME} is absent; validating the transitional 129-image layout"
            )

    if selected_mode == "v2":
        if not manifest_path.is_file():
            errors.append(f"{MANIFEST_NAME} is required for v2")
        else:
            errors.extend(validate_v2_manifest(manifest_path, target))
        expected = expected_v2_files(target)
        strict_file_set = True
    elif selected_mode == "transition":
        if manifest_path.exists():
            errors.append(f"{MANIFEST_NAME} is not allowed in transition mode")
        expected = expected_transition_files()
        strict_file_set = True
    elif selected_mode == "legacy-minimal":
        if manifest_path.exists():
            errors.append(f"{MANIFEST_NAME} is not allowed in legacy-minimal mode")
        expected = expected_legacy_minimal_files()
        strict_file_set = False
    else:
        raise ValueError(f"unsupported validation mode: {selected_mode}")

    entries = list(_visible_entries(directory))
    directories = sorted(entry.name for entry in entries if entry.is_dir())
    if directories:
        errors.append("unexpected subdirectories: " + ", ".join(directories))

    image_entries = [entry for entry in entries if entry.is_file() and entry.suffix.lower() == ".jpg"]
    actual = {entry.name for entry in image_entries}
    missing = sorted(expected - actual)
    if missing:
        errors.append("missing images: " + ", ".join(missing))

    if strict_file_set:
        unexpected = sorted(actual - expected)
        if unexpected:
            errors.append("unexpected JPG images: " + ", ".join(unexpected))

    other_image_suffixes = {".jpeg", ".png", ".webp", ".bmp", ".qoi"}
    other_images = sorted(
        entry.name
        for entry in entries
        if entry.is_file() and entry.suffix.lower() in other_image_suffixes
    )
    if other_images:
        errors.append("unsupported image files: " + ", ".join(other_images))

    checked_files = 0
    for name in sorted(expected & actual):
        path = directory / name
        try:
            dimensions = image_dimensions(path)
        except (OSError, ValueError) as exc:
            errors.append(f"{name}: {exc}")
            continue
        checked_files += 1
        expected_dimensions = (target.width, target.height)
        if dimensions != expected_dimensions:
            errors.append(
                f"{name}: expected {target.width}x{target.height}, "
                f"got {dimensions[0]}x{dimensions[1]}"
            )

    return ValidationResult(target, directory, selected_mode, checked_files, errors, warnings)


def infer_target(path: Path) -> TargetSpec | None:
    name = path.name
    aliases = {
        "data": "cores3",
        "data_local": "cores3",
        "data_stopwatch": "stopwatch",
        "data_stopwatch_local": "stopwatch",
        "data_atoms3r": "atoms3r",
        "data_atoms3r_local": "atoms3r",
    }
    target_name = aliases.get(name)
    return TARGETS.get(target_name) if target_name else None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate transitional, v2, or minimal legacy face asset directories.",
    )
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="Directories to validate. Defaults to data/, data_stopwatch/, and data_atoms3r/.",
    )
    parser.add_argument(
        "--mode",
        choices=("auto", "v2", "transition", "legacy-minimal"),
        default="auto",
        help="Validation profile. auto selects v2 only when face_assets.json exists.",
    )
    parser.add_argument(
        "--target",
        choices=("auto", *TARGETS.keys()),
        default="auto",
        help="Target for custom paths. Standard data directory names are inferred automatically.",
    )
    parser.add_argument("--quiet", action="store_true", help="Only print validation failures.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.paths:
        paths = [path.expanduser() for path in args.paths]
    else:
        paths = [ROOT / spec.directory for spec in TARGETS.values()]

    failed = False
    for path in paths:
        target = TARGETS.get(args.target) if args.target != "auto" else infer_target(path)
        if target is None:
            print(f"ERROR {path}: cannot infer target; pass --target", file=sys.stderr)
            failed = True
            continue

        result = validate_directory(path, target, args.mode)
        for warning in result.warnings:
            if not args.quiet:
                print(f"WARN  {result.directory}: {warning}")
        if result.ok:
            if not args.quiet:
                print(
                    f"OK    {result.directory}: target={target.name} mode={result.mode} "
                    f"images={result.checked_files}"
                )
            continue

        failed = True
        for error in result.errors:
            print(f"ERROR {result.directory}: {error}", file=sys.stderr)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
