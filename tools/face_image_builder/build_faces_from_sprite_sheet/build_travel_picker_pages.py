#!/usr/bin/env python3
"""Build device-specific travel face picker backgrounds from face assets."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


PAGE_FACE_NAMES = (
    (
        "pet_anim_8.jpg",
        "pet_anim_10.jpg",
        "travel_wink.jpg",
        "travel_sparkle.jpg",
        "travel_surprised.jpg",
        "travel_shy.jpg",
        "travel_delicious.jpg",
        "travel_peace.jpg",
    ),
    (
        "dizzy_01.jpg",
        "dizzy_09.jpg",
        "pet_anim_13.jpg",
        "pet_anim_14.jpg",
        "travel_mischief.jpg",
        "travel_teary.jpg",
        "travel_yawn.jpg",
    ),
)

TARGET_LAYOUTS = {
    "cores3": {
        "size": (320, 240),
        "thumb_size": 66,
        "centers": (
            (44, 62),
            (121, 62),
            (198, 62),
            (275, 62),
            (44, 140),
            (121, 140),
            (198, 140),
            (275, 140),
        ),
    },
    "stopwatch": {
        "size": (386, 386),
        "thumb_size": 74,
        "centers": (
            (193, 52),
            (292, 92),
            (334, 193),
            (292, 294),
            (193, 334),
            (94, 294),
            (52, 193),
            (94, 92),
        ),
    },
}


def square_thumbnail(path: Path, size: int) -> Image.Image:
    with Image.open(path) as source:
        image = source.convert("RGB")
    side = min(image.width, image.height)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    image = image.crop((left, top, left + side, top + side))
    return image.resize((size, size), Image.Resampling.LANCZOS)


def build_pages(asset_dir: Path, output_dir: Path, target: str, quality: int) -> list[Path]:
    layout = TARGET_LAYOUTS[target]
    canvas_size = layout["size"]
    thumb_size = layout["thumb_size"]
    centers = layout["centers"]
    assert isinstance(canvas_size, tuple)
    assert isinstance(thumb_size, int)
    assert isinstance(centers, tuple)

    missing = [
        asset_dir / name
        for page_names in PAGE_FACE_NAMES
        for name in page_names
        if not (asset_dir / name).is_file()
    ]
    if missing:
        listing = "\n".join(f"- {path}" for path in missing)
        raise RuntimeError(f"travel picker source images are missing:\n{listing}")

    output_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[Path] = []
    for page_index, page_names in enumerate(PAGE_FACE_NAMES):
        page = Image.new("RGB", canvas_size, "black")
        for slot, name in enumerate(page_names):
            thumbnail = square_thumbnail(asset_dir / name, thumb_size)
            center_x, center_y = centers[slot]
            page.paste(
                thumbnail,
                (center_x - thumb_size // 2, center_y - thumb_size // 2),
            )

        output_path = output_dir / f"travel_picker_page_{page_index}.jpg"
        page.save(output_path, format="JPEG", quality=quality, optimize=True)
        outputs.append(output_path)
        print(f"wrote {output_path} ({canvas_size[0]}x{canvas_size[1]})")
    return outputs


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build CoreS3 or StopWatch travel face picker page JPGs.",
    )
    parser.add_argument("asset_dir", type=Path, help="Directory containing the face JPG assets.")
    parser.add_argument("--target", choices=sorted(TARGET_LAYOUTS), required=True)
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Output directory. Defaults to the input asset directory.",
    )
    parser.add_argument("--quality", type=int, default=82)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.quality < 1 or args.quality > 100:
        raise SystemExit("--quality must be between 1 and 100")
    asset_dir = args.asset_dir.expanduser().resolve()
    output_dir = (args.output_dir or asset_dir).expanduser().resolve()
    try:
        build_pages(asset_dir, output_dir, args.target, args.quality)
    except RuntimeError as exc:
        raise SystemExit(str(exc)) from exc


if __name__ == "__main__":
    main()
