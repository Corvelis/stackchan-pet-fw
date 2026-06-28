#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from PIL import Image, ImageChops, ImageOps


ROOT = Path(__file__).resolve().parents[1]

TARGETS = {
    "cores3": {
        "output_dir": ROOT / "data_local",
        "size": (240, 240),
        "matte": (0, 0, 0),
    },
    "stopwatch": {
        "output_dir": ROOT / "data_stopwatch_local",
        "size": (386, 386),
        "matte": (0, 0, 0),
    },
    "atoms3r": {
        "output_dir": ROOT / "data_atoms3r_local",
        "size": (128, 128),
        "matte": (0, 0, 0),
    },
}

COPY_RULES = {
    "talk_guarded_0.png": "idle_guarded_0.png",
    "shake_guarded_0.png": "furifuri_0.png",
    "shake_guarded_1.png": "furifuri_1.png",
    "shake_attached_0.png": "furifuri_0.png",
    "shake_attached_1.png": "furifuri_1.png",
}

SOURCE_SUFFIXES = (".png", ".jpg", ".jpeg")
JPEG_QUALITY = 82

BASE_STEMS = [
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
]

DIRECTION_STEMS = [f"dir{index}" for index in range(17)]
BLINK_DIRECTION_STEMS = [f"blink{index}" for index in range(17)]

PET_ANIMATION_SOURCES = [
    "nadenade_0",
    "nadenade_1",
    "pet_attached_0",
    "pet_attached_1",
    "pet_blink_attached_0",
    "pet_guarded_0",
    "pet_guarded_1",
    "pet_blink_guarded_0",
    "nadenade_0",
]

DIZZY_FRAME_PLAN = [
    ("furifuri_0", -4, -3, 1),
    ("furifuri_1", 4, 3, -1),
    ("shake_guarded_0", -6, -4, 2),
    ("shake_guarded_1", 6, 4, -2),
    ("shake_attached_0", -5, -3, 3),
    ("shake_attached_1", 5, 3, -3),
    ("tired_0", -3, -2, 1),
    ("tired_talk", 3, 2, -1),
    ("blink", -5, -3, 0),
    ("furifuri_0", 5, 3, 0),
    ("furifuri_1", -4, -2, 2),
    ("exhausted_0", 4, 2, -2),
    ("exhausted_talk", -2, -1, 1),
    ("exhausted_blink", 2, 1, -1),
    ("idle", 0, 0, 0),
]

ALL_SOURCE_STEMS = sorted(
    set(BASE_STEMS)
    | set(DIRECTION_STEMS)
    | set(BLINK_DIRECTION_STEMS)
    | set(PET_ANIMATION_SOURCES)
    | {stem for stem, _, _, _ in DIZZY_FRAME_PLAN}
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate ignored local LittleFS face directories from a backup data directory.",
    )
    parser.add_argument("source", type=Path, help="Backup directory containing source face image files.")
    parser.add_argument(
        "targets",
        nargs="*",
        choices=sorted([*TARGETS.keys(), "all"]),
        default=None,
        help="Target local asset set to generate. Defaults to all.",
    )
    parser.add_argument(
        "--quality",
        type=int,
        default=JPEG_QUALITY,
        help=f"JPEG quality. Defaults to {JPEG_QUALITY}.",
    )
    return parser.parse_args()


def expand_targets(targets: list[str] | None) -> list[str]:
    if not targets or "all" in targets:
        return list(TARGETS.keys())
    return targets


def source_for_stem(source_dir: Path, stem: str) -> Path:
    for suffix in SOURCE_SUFFIXES:
        source_path = source_dir / f"{stem}{suffix}"
        if source_path.exists():
            return source_path

    fallback = COPY_RULES.get(f"{stem}.png")
    if fallback is not None:
        return source_for_stem(source_dir, Path(fallback).stem)

    raise FileNotFoundError(stem)


def image_with_matte(source_path: Path, size: tuple[int, int], matte: tuple[int, int, int]) -> Image.Image:
    image = Image.open(source_path)
    image = ImageOps.exif_transpose(image).convert("RGBA")
    if image.size != size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    background = Image.new("RGBA", image.size, (*matte, 255))
    return Image.alpha_composite(background, image).convert("RGB")


def write_image(
    source_path: Path,
    dest_path: Path,
    size: tuple[int, int],
    matte: tuple[int, int, int],
    quality: int,
) -> None:
    image = image_with_matte(source_path, size, matte)
    save_jpeg(image, dest_path, quality)


def save_jpeg(image: Image.Image, dest_path: Path, quality: int) -> None:
    image.convert("RGB").save(
        dest_path,
        "JPEG",
        quality=quality,
        optimize=True,
        subsampling=0,
    )


def shifted_image(image: Image.Image, dx: int, dy: int, matte: tuple[int, int, int]) -> Image.Image:
    if dx == 0 and dy == 0:
        return image
    shifted = ImageChops.offset(image, dx, dy)
    width, height = image.size
    if dx > 0:
        shifted.paste(matte, (0, 0, dx, height))
    elif dx < 0:
        shifted.paste(matte, (width + dx, 0, width, height))
    if dy > 0:
        shifted.paste(matte, (0, 0, width, dy))
    elif dy < 0:
        shifted.paste(matte, (0, height + dy, width, height))
    return shifted


def write_dizzy_image(
    source_path: Path,
    dest_path: Path,
    size: tuple[int, int],
    matte: tuple[int, int, int],
    quality: int,
    angle: int,
    dx: int,
    dy: int,
) -> None:
    image = image_with_matte(source_path, size, matte)
    image = image.rotate(angle, resample=Image.Resampling.BICUBIC, fillcolor=matte)
    image = shifted_image(image, dx, dy, matte)
    save_jpeg(image, dest_path, quality)


def generate_target(source_dir: Path, target: str, quality: int) -> None:
    config = TARGETS[target]
    output_dir = config["output_dir"]
    size = config["size"]
    matte = config["matte"]

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    total_bytes = 0
    for stem in BASE_STEMS + DIRECTION_STEMS + BLINK_DIRECTION_STEMS:
        src = source_for_stem(source_dir, stem)
        dst = output_dir / f"{stem}.jpg"
        write_image(src, dst, size, matte, quality)
        total_bytes += dst.stat().st_size

    for index, stem in enumerate(PET_ANIMATION_SOURCES):
        src = source_for_stem(source_dir, stem)
        dst = output_dir / f"pet_anim_{index}.jpg"
        write_image(src, dst, size, matte, quality)
        total_bytes += dst.stat().st_size

    for index, (stem, angle, dx, dy) in enumerate(DIZZY_FRAME_PLAN, start=1):
        src = source_for_stem(source_dir, stem)
        dst = output_dir / f"dizzy_{index:02d}.jpg"
        write_dizzy_image(src, dst, size, matte, quality, angle, dx, dy)
        total_bytes += dst.stat().st_size

    written = len(list(output_dir.glob("*.jpg")))
    print(f"{target}: generated {written} JPEG files in {output_dir}")
    print(f"{target}: total JPEG bytes {total_bytes} ({total_bytes / 1024 / 1024:.2f} MiB)")


def main() -> None:
    args = parse_args()
    source_dir = args.source.expanduser().resolve()
    if not source_dir.exists():
        raise SystemExit(f"missing source directory: {source_dir}")
    if not source_dir.is_dir():
        raise SystemExit(f"source is not a directory: {source_dir}")
    if args.quality < 1 or args.quality > 100:
        raise SystemExit("--quality must be between 1 and 100")

    missing = []
    for stem in ALL_SOURCE_STEMS:
        try:
            source_for_stem(source_dir, stem)
        except FileNotFoundError:
            missing.append(stem)
    if missing:
        missing_list = "\n  - ".join(missing)
        raise SystemExit(f"source is missing required face files:\n  - {missing_list}")

    for target in expand_targets(args.targets):
        generate_target(source_dir, target, args.quality)


if __name__ == "__main__":
    main()
