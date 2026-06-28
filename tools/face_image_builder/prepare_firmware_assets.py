#!/usr/bin/env python3
"""Prepare firmware image directories with device size and file format."""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

try:
    from PIL import Image, ImageOps, UnidentifiedImageError
except ImportError as exc:  # pragma: no cover - only used when Pillow is missing.
    raise SystemExit(
        "Pillow is required. Install it with: "
        "python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt"
    ) from exc


TARGET_SIZES = {
    "cores3": (240, 240),
    "stopwatch": (386, 386),
    "atoms3r": (128, 128),
}

IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp", ".bmp"}
OBSOLETE_SUFFIXES = {".qoi"}
OUTPUT_SUFFIXES = {
    "jpg": ".jpg",
    "png": ".png",
}
SOURCE_PRIORITY = {
    ".png": 0,
    ".webp": 1,
    ".bmp": 2,
    ".jpg": 3,
    ".jpeg": 3,
}

FACE_STEMS = {
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
}

FIRMWARE_STEM_PATTERNS = (
    re.compile(r"^pet_anim_\d+$"),
    re.compile(r"^dir\d+$"),
    re.compile(r"^blink\d+$"),
    re.compile(r"^dizzy_\d+$"),
    re.compile(r"^dizzy_cw_\d+$"),
    re.compile(r"^dizzy_ccw_\d+$"),
)


def parse_size(text: str) -> tuple[int, int]:
    normalized = text.lower().replace("*", "x")
    if "x" in normalized:
        width_text, height_text = normalized.split("x", 1)
        width = int(width_text)
        height = int(height_text)
    else:
        width = int(normalized)
        height = width
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("size must be positive")
    return width, height


def parse_matte(text: str) -> tuple[int, int, int]:
    normalized = text.strip().lower()
    if normalized == "black":
        return 0, 0, 0
    if normalized == "white":
        return 255, 255, 255
    if normalized.startswith("#") and len(normalized) == 7:
        try:
            return (
                int(normalized[1:3], 16),
                int(normalized[3:5], 16),
                int(normalized[5:7], 16),
            )
        except ValueError as exc:
            raise argparse.ArgumentTypeError("matte must be black, white, or #RRGGBB") from exc
    raise argparse.ArgumentTypeError("matte must be black, white, or #RRGGBB")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Resize and format top-level Stack-chan firmware image assets.",
    )
    parser.add_argument("source", type=Path, help="Source firmware image directory.")
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        help="Output directory. Omit when using --in-place.",
    )
    parser.add_argument(
        "--target",
        choices=sorted(TARGET_SIZES),
        default="cores3",
        help="Device preset size. Defaults to cores3.",
    )
    parser.add_argument(
        "--size",
        type=parse_size,
        help="Override output size, for example 240 or 386x386.",
    )
    parser.add_argument(
        "--format",
        choices=sorted(OUTPUT_SUFFIXES),
        default="jpg",
        help="Output image format. Defaults to jpg.",
    )
    parser.add_argument(
        "--quality",
        type=int,
        default=82,
        help="JPEG quality. Defaults to 82.",
    )
    parser.add_argument(
        "--matte",
        type=parse_matte,
        default=(0, 0, 0),
        help="Background for transparent pixels: black, white, or #RRGGBB. Defaults to black.",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Replace recognized firmware images inside the source directory.",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove the output directory before writing. Ignored with --in-place.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be written without changing files.",
    )
    return parser.parse_args()


def is_firmware_stem(stem: str) -> bool:
    return stem in FACE_STEMS or any(pattern.match(stem) for pattern in FIRMWARE_STEM_PATTERNS)


def is_metadata_file(path: Path) -> bool:
    return path.suffix.lower() == ".txt" and path.name.endswith("_meta.txt")


def collect_assets(
    source_dir: Path,
) -> tuple[dict[str, Path], dict[str, list[Path]], list[Path], list[Path], list[Path]]:
    selected: dict[str, Path] = {}
    variants: dict[str, list[Path]] = {}
    metadata: list[Path] = []
    ignored_images: list[Path] = []
    obsolete_files: list[Path] = []

    for path in sorted(source_dir.iterdir()):
        if not path.is_file():
            continue
        suffix = path.suffix.lower()
        if suffix in IMAGE_SUFFIXES:
            if is_firmware_stem(path.stem):
                variants.setdefault(path.stem, []).append(path)
                current = selected.get(path.stem)
                if current is None or SOURCE_PRIORITY[suffix] < SOURCE_PRIORITY[current.suffix.lower()]:
                    selected[path.stem] = path
            elif path.name != ".DS_Store":
                ignored_images.append(path)
        elif suffix in OBSOLETE_SUFFIXES:
            obsolete_files.append(path)
        elif is_metadata_file(path):
            metadata.append(path)

    return selected, variants, metadata, ignored_images, obsolete_files


def prepare_image(source_path: Path, size: tuple[int, int], matte: tuple[int, int, int]) -> Image.Image:
    try:
        with Image.open(source_path) as image:
            image = ImageOps.exif_transpose(image)
            image = image.convert("RGBA")
            if image.size != size:
                image = image.resize(size, Image.Resampling.LANCZOS)
            background = Image.new("RGBA", image.size, (*matte, 255))
            return Image.alpha_composite(background, image).convert("RGB")
    except UnidentifiedImageError as exc:
        raise SystemExit(f"unsupported image file: {source_path}") from exc


def save_image(image: Image.Image, output_path: Path, output_format: str, quality: int) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_format == "jpg":
        image.save(output_path, "JPEG", quality=quality, optimize=True, subsampling=0)
    elif output_format == "png":
        image.save(output_path, "PNG", optimize=True)
    else:  # pragma: no cover - argparse prevents this.
        raise ValueError(output_format)


def write_assets(
    selected: dict[str, Path],
    metadata: list[Path],
    output_dir: Path,
    size: tuple[int, int],
    output_format: str,
    quality: int,
    matte: tuple[int, int, int],
    dry_run: bool,
) -> tuple[int, int]:
    output_suffix = OUTPUT_SUFFIXES[output_format]
    converted = 0
    copied_metadata = 0

    for stem, source_path in sorted(selected.items()):
        output_path = output_dir / f"{stem}{output_suffix}"
        if dry_run:
            print(f"convert {source_path} -> {output_path}")
        else:
            image = prepare_image(source_path, size, matte)
            save_image(image, output_path, output_format, quality)
        converted += 1

    for source_path in sorted(metadata):
        output_path = output_dir / source_path.name
        if dry_run:
            print(f"copy {source_path} -> {output_path}")
        else:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_path, output_path)
        copied_metadata += 1

    return converted, copied_metadata


def replace_in_place(
    source_dir: Path,
    selected: dict[str, Path],
    variants: dict[str, list[Path]],
    metadata: list[Path],
    obsolete_files: list[Path],
    size: tuple[int, int],
    output_format: str,
    quality: int,
    matte: tuple[int, int, int],
    dry_run: bool,
) -> tuple[int, int]:
    temp_dir = source_dir.parent / f".{source_dir.name}.prepare_tmp"

    if dry_run:
        return write_assets(selected, metadata, source_dir, size, output_format, quality, matte, True)

    if temp_dir.exists():
        shutil.rmtree(temp_dir)
    temp_dir.mkdir(parents=True)
    try:
        converted, copied_metadata = write_assets(
            selected,
            metadata,
            temp_dir,
            size,
            output_format,
            quality,
            matte,
            False,
        )

        for source_paths in variants.values():
            for path in source_paths:
                if path.exists():
                    path.unlink()
        for path in obsolete_files:
            if path.exists():
                path.unlink()

        ds_store = source_dir / ".DS_Store"
        if ds_store.exists():
            ds_store.unlink()

        for prepared in sorted(temp_dir.iterdir()):
            shutil.move(str(prepared), source_dir / prepared.name)
    finally:
        if temp_dir.exists():
            shutil.rmtree(temp_dir)

    return converted, copied_metadata


def main() -> None:
    args = parse_args()
    if args.quality < 1 or args.quality > 100:
        raise SystemExit("--quality must be between 1 and 100")
    if args.in_place and args.output is not None:
        raise SystemExit("do not pass output when using --in-place")
    if not args.in_place and args.output is None:
        raise SystemExit("output directory is required unless --in-place is used")

    source_dir = args.source.expanduser().resolve()
    if not source_dir.is_dir():
        raise SystemExit(f"missing source directory: {source_dir}")

    size = args.size or TARGET_SIZES[args.target]
    selected, variants, metadata, ignored_images, obsolete_files = collect_assets(source_dir)
    if not selected:
        raise SystemExit(f"no recognized firmware image assets found in {source_dir}")

    if args.in_place:
        output_dir = source_dir
        converted, copied_metadata = replace_in_place(
            source_dir,
            selected,
            variants,
            metadata,
            obsolete_files,
            size,
            args.format,
            args.quality,
            args.matte,
            args.dry_run,
        )
    else:
        output_dir = args.output.expanduser().resolve()
        if args.clean and output_dir.exists() and not args.dry_run:
            shutil.rmtree(output_dir)
        converted, copied_metadata = write_assets(
            selected,
            metadata,
            output_dir,
            size,
            args.format,
            args.quality,
            args.matte,
            args.dry_run,
        )

    print(
        f"prepared {converted} images as {args.format.upper()} "
        f"{size[0]}x{size[1]} in {output_dir}"
    )
    if copied_metadata:
        print(f"copied {copied_metadata} metadata files")
    if ignored_images:
        print("ignored non-firmware image sources:")
        for path in ignored_images:
            print(f"  {path.name}")
    if obsolete_files:
        if args.in_place and not args.dry_run:
            print(f"removed {len(obsolete_files)} obsolete QOI files")
        elif args.in_place:
            print(f"would remove {len(obsolete_files)} obsolete QOI files")
        else:
            print("ignored obsolete QOI files")


if __name__ == "__main__":
    main()
