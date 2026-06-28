#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from PIL import Image, ImageOps


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
        description="Generate device-specific face JPG directories from CoreS3 data/ images.",
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
        help="Source directory containing CoreS3-sized firmware image files.",
    )
    parser.add_argument(
        "--quality",
        type=int,
        default=82,
        help="JPEG quality. Default: 82.",
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


def collect_images(source_dir: Path) -> list[Path]:
    selected: dict[str, Path] = {}
    for suffix in (".jpg", ".png", ".jpeg"):
        for path in sorted(source_dir.glob(f"*{suffix}")):
            selected.setdefault(path.stem, path)
    return [selected[stem] for stem in sorted(selected)]


def prepare_image(source: Path, size: tuple[int, int]) -> Image.Image:
    image = Image.open(source)
    image = ImageOps.exif_transpose(image).convert("RGBA")
    if image.size != size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    background = Image.new("RGBA", image.size, (0, 0, 0, 255))
    return Image.alpha_composite(background, image).convert("RGB")


def generate_target(source_dir: Path, target: str, clean: bool, quality: int) -> None:
    config = TARGETS[target]
    output_dir = config["output_dir"]
    size = config["size"]

    if not source_dir.exists():
        raise SystemExit(f"missing source directory: {source_dir}")

    image_files = collect_images(source_dir)
    if not image_files:
        raise SystemExit(f"no firmware image files found in source directory: {source_dir}")

    if clean and output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    total_bytes = 0
    count = 0
    for src in image_files:
        image = prepare_image(src, size)
        dst = output_dir / f"{src.stem}.jpg"
        image.save(
            dst,
            "JPEG",
            quality=quality,
            optimize=True,
            subsampling=0,
        )
        total_bytes += dst.stat().st_size
        count += 1

    print(f"{target}: generated {count} files in {output_dir}")
    print(f"{target}: total JPEG bytes {total_bytes} ({total_bytes / 1024 / 1024:.2f} MiB)")


def main() -> None:
    args = parse_args()
    source_dir = Path(args.source).expanduser().resolve()
    clean = not args.no_clean
    if not 1 <= args.quality <= 100:
        raise SystemExit("--quality must be 1..100")

    for target in expand_targets(args.targets):
        generate_target(source_dir, target, clean, args.quality)


if __name__ == "__main__":
    main()
