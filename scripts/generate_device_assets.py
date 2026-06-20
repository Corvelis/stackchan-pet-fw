#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_DIR = ROOT / "data"

TARGETS = {
    "stopwatch": {
        "output_dir": ROOT / "data_stopwatch",
        "size": (386, 386),
    },
    "atoms3r": {
        "output_dir": ROOT / "data_atoms3r",
        "size": (128, 128),
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate device-specific face PNG directories from CoreS3 data/ images.",
    )
    parser.add_argument(
        "targets",
        nargs="*",
        choices=sorted([*TARGETS.keys(), "all"]),
        default=["all"],
        help="Target asset set to generate. Defaults to all.",
    )
    parser.add_argument(
        "--source",
        default=str(DEFAULT_SOURCE_DIR),
        help="Source directory containing CoreS3-sized PNG files.",
    )
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="Do not remove the output directory before writing.",
    )
    return parser.parse_args()


def expand_targets(targets: list[str]) -> list[str]:
    if not targets or "all" in targets:
        return list(TARGETS.keys())
    return targets


def generate_target(source_dir: Path, target: str, clean: bool) -> None:
    config = TARGETS[target]
    output_dir = config["output_dir"]
    size = config["size"]

    if not source_dir.exists():
        raise SystemExit(f"missing source directory: {source_dir}")

    png_files = sorted(source_dir.glob("*.png"))
    if not png_files:
        raise SystemExit(f"no PNG files found in source directory: {source_dir}")

    if clean and output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    total_bytes = 0
    count = 0
    for src in png_files:
        image = Image.open(src).convert("RGB")
        image = image.resize(size, Image.Resampling.LANCZOS)
        image = image.quantize(
            colors=256,
            method=Image.Quantize.MEDIANCUT,
            dither=Image.Dither.FLOYDSTEINBERG,
        )
        dst = output_dir / src.name
        image.save(dst, optimize=True)
        total_bytes += dst.stat().st_size
        count += 1

    print(f"{target}: generated {count} files in {output_dir}")
    print(f"{target}: total PNG bytes {total_bytes} ({total_bytes / 1024 / 1024:.2f} MiB)")


def main() -> None:
    args = parse_args()
    source_dir = Path(args.source).expanduser().resolve()
    clean = not args.no_clean

    for target in expand_targets(args.targets):
        generate_target(source_dir, target, clean)


if __name__ == "__main__":
    main()
