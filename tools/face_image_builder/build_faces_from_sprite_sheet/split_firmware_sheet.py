#!/usr/bin/env python3
"""Split sprite/contact sheets into firmware-ready image assets."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

TOOL_ROOT = Path(__file__).resolve().parents[1]
if str(TOOL_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOL_ROOT))

from prepare_firmware_assets import OUTPUT_SUFFIXES, TARGET_SIZES, parse_matte, parse_size, save_image


# Sprite detection and 5x5 direction mapping are adapted from the
# `split_guruguru_sheet.py` tool in the M5-guruguru project.
#
# MIT License
# Copyright (c) 2026 NANANA
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

DIRECTION_MAPS = {
    9: [
        ((0, 0), 0),
        ((0, 2), 1),
        ((0, 4), 2),
        ((2, 0), 3),
        ((2, 2), 4),
        ((2, 4), 5),
        ((4, 0), 6),
        ((4, 2), 7),
        ((4, 4), 8),
    ],
    17: [
        ((2, 4), 0),
        ((3, 4), 1),
        ((4, 4), 2),
        ((4, 3), 3),
        ((4, 2), 4),
        ((4, 1), 5),
        ((4, 0), 6),
        ((3, 0), 7),
        ((2, 0), 8),
        ((1, 0), 9),
        ((0, 0), 10),
        ((0, 1), 11),
        ((0, 2), 12),
        ((0, 3), 13),
        ((0, 4), 14),
        ((1, 4), 15),
        ((2, 2), 16),
    ],
    25: [
        ((row, col), row * 5 + col)
        for row in range(5)
        for col in range(5)
    ],
}

TRAVEL_EXPRESSION_NAMES = (
    "travel_wink",
    "travel_sparkle",
    "travel_surprised",
    "travel_shy",
    "travel_delicious",
    "travel_mischief",
    "travel_teary",
    "travel_yawn",
    "travel_peace",
)


@dataclass(frozen=True)
class SheetSpec:
    path: Path
    prefix: str


@dataclass(frozen=True)
class GridSpec:
    rows: int
    cols: int


def parse_sheet_spec(value: str) -> SheetSpec:
    if ":" in value:
        path_text, prefix = value.rsplit(":", 1)
    else:
        path = Path(value)
        path_text = value
        prefix = path.stem

    if not prefix:
        raise argparse.ArgumentTypeError("sheet prefix must not be empty")
    return SheetSpec(Path(path_text), prefix)


def parse_grid_spec(value: str) -> GridSpec:
    match = re.fullmatch(r"(\d+)x(\d+)", value.lower())
    if not match:
        raise argparse.ArgumentTypeError("grid must use ROWSxCOLS format, for example 3x3")

    rows = int(match.group(1))
    cols = int(match.group(2))
    if rows < 1 or cols < 1:
        raise argparse.ArgumentTypeError("grid rows and columns must be positive")
    return GridSpec(rows=rows, cols=cols)


def parse_crop_size(value: str) -> int | None:
    if value.lower() == "auto":
        return None
    try:
        size = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("crop size must be a positive integer or auto") from exc
    if size <= 0:
        raise argparse.ArgumentTypeError("crop size must be a positive integer or auto")
    return size


def parse_column_x_offsets(value: str) -> tuple[int, ...]:
    parts = value.split(",")
    if not parts or any(not part.strip() for part in parts):
        raise argparse.ArgumentTypeError(
            "column x offsets must be comma-separated integers, for example -11,-11,-9,0"
        )
    try:
        return tuple(int(part.strip()) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(
            "column x offsets must be comma-separated integers, for example -11,-11,-9,0"
        ) from exc


def find_runs(counts: list[int], threshold: int) -> list[tuple[int, int]]:
    runs: list[tuple[int, int]] = []
    start: int | None = None

    for index, count in enumerate(counts):
        if count > threshold and start is None:
            start = index
        elif count <= threshold and start is not None:
            runs.append((start, index - 1))
            start = None

    if start is not None:
        runs.append((start, len(counts) - 1))
    return runs


def filter_short_runs(runs: list[tuple[int, int]], min_length: int) -> list[tuple[int, int]]:
    if min_length <= 1:
        return runs
    return [
        (start, end)
        for start, end in runs
        if end - start + 1 >= min_length
    ]


def find_sprite_centers(
    image: Image.Image,
    grid: GridSpec,
    pixel_threshold: int,
    run_threshold: int,
    min_run_length: int,
) -> tuple[list[float], list[float], list[tuple[int, int]], list[tuple[int, int]]]:
    row_counts = [0] * image.height
    col_counts = [0] * image.width
    pixels = image.load()

    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a > 0 and max(r, g, b) > pixel_threshold:
                row_counts[y] += 1
                col_counts[x] += 1

    row_runs = filter_short_runs(find_runs(row_counts, run_threshold), min_run_length)
    col_runs = filter_short_runs(find_runs(col_counts, run_threshold), min_run_length)
    if len(row_runs) != grid.rows or len(col_runs) != grid.cols:
        raise RuntimeError(
            f"expected a {grid.rows}x{grid.cols} sprite sheet, "
            f"got {len(row_runs)} rows and {len(col_runs)} columns"
        )

    row_centers = [(start + end) / 2 for start, end in row_runs]
    col_centers = [(start + end) / 2 for start, end in col_runs]
    return row_centers, col_centers, row_runs, col_runs


def even_sprite_centers(
    image: Image.Image,
    grid: GridSpec,
) -> tuple[list[float], list[float], list[tuple[int, int]], list[tuple[int, int]]]:
    row_step = image.height / grid.rows
    col_step = image.width / grid.cols
    row_runs = [
        (round(row * row_step), round((row + 1) * row_step) - 1)
        for row in range(grid.rows)
    ]
    col_runs = [
        (round(col * col_step), round((col + 1) * col_step) - 1)
        for col in range(grid.cols)
    ]
    row_centers = [(start + end) / 2 for start, end in row_runs]
    col_centers = [(start + end) / 2 for start, end in col_runs]
    return row_centers, col_centers, row_runs, col_runs


def find_cell_bounds(centers: list[float], image_size: int) -> list[tuple[float, float]]:
    bounds: list[tuple[float, float]] = []
    for index, center in enumerate(centers):
        start = 0.0 if index == 0 else (centers[index - 1] + center) / 2
        end = float(image_size) if index == len(centers) - 1 else (center + centers[index + 1]) / 2
        bounds.append((start, end))
    return bounds


def crop_square_around_center(
    image: Image.Image,
    center_x: float,
    center_y: float,
    requested_size: int | None,
    bounds: tuple[float, float, float, float],
) -> Image.Image:
    min_x, min_y, max_x, max_y = bounds
    available_size = int(min(max_x - min_x, max_y - min_y))
    if available_size <= 0:
        raise RuntimeError("cell bounds are too small to crop")

    size = available_size if requested_size is None else min(requested_size, available_size)
    half_size = size / 2
    center_x = min(max(center_x, min_x + half_size), max_x - half_size)
    center_y = min(max(center_y, min_y + half_size), max_y - half_size)

    left = round(center_x - half_size)
    top = round(center_y - half_size)
    right = left + size
    bottom = top + size

    src_box = (
        max(0, left),
        max(0, top),
        min(image.width, right),
        min(image.height, bottom),
    )
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    tile.alpha_composite(image.crop(src_box), (src_box[0] - left, src_box[1] - top))
    return tile


def build_output_map(grid: GridSpec, output_count: int) -> list[tuple[tuple[int, int], int]]:
    if grid == GridSpec(5, 5) and output_count in DIRECTION_MAPS:
        return DIRECTION_MAPS[output_count]

    cell_count = grid.rows * grid.cols
    if output_count <= cell_count:
        return [
            ((row, col), row * grid.cols + col)
            for row in range(grid.rows)
            for col in range(grid.cols)
        ][:output_count]

    raise RuntimeError(
        f"unsupported output count {output_count} for {grid.rows}x{grid.cols}; "
        f"use at most {cell_count} for row-major export"
    )


def clean_prefix_outputs(out_dir: Path, prefix: str, output_naming: str) -> None:
    if output_naming == "base-mouth-eye":
        pattern = re.compile(
            rf"^{re.escape(prefix)}m\d+_e\d+\.(?:jpg|jpeg|png)$",
            re.IGNORECASE,
        )
    elif output_naming == "travel-expressions":
        names = "|".join(re.escape(name) for name in TRAVEL_EXPRESSION_NAMES)
        pattern = re.compile(rf"^(?:{names})\.(?:jpg|jpeg|png)$", re.IGNORECASE)
    else:
        pattern = re.compile(rf"^{re.escape(prefix)}\d+\.(?:jpg|jpeg|png)$", re.IGNORECASE)
    for path in out_dir.iterdir():
        if path.is_file() and pattern.match(path.name):
            path.unlink()


def output_name(
    prefix: str,
    index: int,
    pad: int,
    suffix: str,
    row: int,
    col: int,
    output_naming: str,
) -> str:
    if output_naming == "base-mouth-eye":
        return f"{prefix}m{row}_e{col}{suffix}"
    if output_naming == "travel-expressions":
        return f"{TRAVEL_EXPRESSION_NAMES[index]}{suffix}"
    number = str(index).zfill(pad) if pad > 0 else str(index)
    return f"{prefix}{number}{suffix}"


def image_with_matte(image: Image.Image, matte: tuple[int, int, int]) -> Image.Image:
    image = image.convert("RGBA")
    background = Image.new("RGBA", image.size, (*matte, 255))
    return Image.alpha_composite(background, image).convert("RGB")


def translate_x(
    image: Image.Image,
    offset: int,
    matte: tuple[int, int, int],
) -> Image.Image:
    if offset == 0:
        return image
    if abs(offset) >= image.width:
        raise RuntimeError(
            f"column x offset {offset} must be smaller than the cropped tile width {image.width}"
        )

    source_left = max(0, -offset)
    source_right = min(image.width, image.width - offset)
    destination_left = max(0, offset)
    shifted = Image.new("RGBA", image.size, (*matte, 255))
    shifted.alpha_composite(
        image.crop((source_left, 0, source_right, image.height)),
        (destination_left, 0),
    )
    return shifted


def split_sheet(
    spec: SheetSpec,
    out_dir: Path,
    grid: GridSpec,
    output_count: int,
    source_crop_size: int | None,
    target_size: tuple[int, int],
    output_format: str,
    quality: int,
    matte: tuple[int, int, int],
    pixel_threshold: int,
    run_threshold: int,
    min_run_length: int,
    layout: str,
    cell_inset: int,
    row_top_mask: int,
    column_side_mask: int,
    column_x_offsets: tuple[int, ...] | None,
    start_index: int,
    pad: int,
    output_naming: str,
    clean: bool,
    dry_run: bool,
) -> list[Path]:
    image = Image.open(spec.path).convert("RGBA")
    if layout == "even":
        row_centers, col_centers, row_runs, col_runs = even_sprite_centers(image, grid)
    else:
        try:
            row_centers, col_centers, row_runs, col_runs = find_sprite_centers(
                image,
                grid,
                pixel_threshold,
                run_threshold,
                min_run_length,
            )
        except RuntimeError:
            if layout != "auto":
                raise
            print(f"{spec.path}: detection failed; falling back to even {grid.rows}x{grid.cols} split")
            row_centers, col_centers, row_runs, col_runs = even_sprite_centers(image, grid)
    row_bounds = find_cell_bounds(row_centers, image.height)
    col_bounds = find_cell_bounds(col_centers, image.width)
    output_map = build_output_map(grid, output_count)
    suffix = OUTPUT_SUFFIXES[output_format]

    if clean and not dry_run:
        clean_prefix_outputs(out_dir, spec.prefix, output_naming)

    outputs: list[Path] = []
    for (row, col), mapped_index in output_map:
        min_x, max_x = col_bounds[col]
        min_y, max_y = row_bounds[row]
        bounds = (
            min_x + cell_inset,
            min_y + cell_inset,
            max_x - cell_inset,
            max_y - cell_inset,
        )
        output_index = start_index + mapped_index
        out_path = out_dir / output_name(
            spec.prefix,
            output_index,
            pad,
            suffix,
            row,
            col,
            output_naming,
        )
        if dry_run:
            print(f"split {spec.path} [{row},{col}] -> {out_path}")
        else:
            tile = crop_square_around_center(
                image,
                col_centers[col],
                row_centers[row],
                source_crop_size,
                bounds,
            )
            if row > 0 and row_top_mask > 0:
                mask_height = min(row_top_mask, tile.height)
                tile.paste((*matte, 255), (0, 0, tile.width, mask_height))
            if column_side_mask > 0:
                mask_width = min(column_side_mask, tile.width)
                if col > 0:
                    tile.paste((*matte, 255), (0, 0, mask_width, tile.height))
                if col < grid.cols - 1:
                    tile.paste(
                        (*matte, 255),
                        (tile.width - mask_width, 0, tile.width, tile.height),
                    )
            if column_x_offsets is not None:
                tile = translate_x(tile, column_x_offsets[col], matte)
            tile = tile.resize(target_size, Image.Resampling.LANCZOS)
            save_image(image_with_matte(tile, matte), out_path, output_format, quality)
        outputs.append(out_path)

    print(f"{spec.path}: rows={row_runs} cols={col_runs}")
    print(f"wrote {len(outputs)} {output_format.upper()} files with prefix '{spec.prefix}'")
    return outputs


def write_preview(paths: list[Path], preview_path: Path, thumb_size: int = 160) -> None:
    if not paths:
        return

    columns = 5 if len(paths) == 25 else 4 if len(paths) > 9 else 3
    rows = (len(paths) + columns - 1) // columns
    gap = 8
    width = columns * thumb_size + (columns + 1) * gap
    height = rows * thumb_size + (rows + 1) * gap
    preview = Image.new("RGB", (width, height), "white")

    for index, path in enumerate(paths):
        image = Image.open(path).convert("RGB")
        image.thumbnail((thumb_size, thumb_size), Image.Resampling.LANCZOS)
        col = index % columns
        row = index // columns
        x = gap + col * (thumb_size + gap) + (thumb_size - image.width) // 2
        y = gap + row * (thumb_size + gap) + (thumb_size - image.height) // 2
        preview.paste(image, (x, y))

    preview_path.parent.mkdir(parents=True, exist_ok=True)
    preview.save(preview_path)
    print(f"wrote preview: {preview_path}")


def preview_name(prefix: str) -> str:
    stem = prefix.rstrip("_") or "sheet"
    return f"{stem}_preview.jpg"


def default_sheet_specs() -> list[SheetSpec]:
    specs = [SheetSpec(Path("sheet.png"), "dir")]
    if Path("blink.png").exists():
        specs.append(SheetSpec(Path("blink.png"), "blink"))
    return specs


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Split face animation sheets into firmware-ready JPG or PNG assets.",
    )
    parser.add_argument(
        "--sheet",
        action="append",
        type=parse_sheet_spec,
        help="Input sheet and output prefix as PATH:PREFIX. Can be repeated.",
    )
    parser.add_argument("--out-dir", type=Path, default=Path("data_local"))
    parser.add_argument(
        "--grid",
        type=parse_grid_spec,
        default=GridSpec(5, 5),
        help="Detected sprite grid as ROWSxCOLS. Defaults to 5x5.",
    )
    parser.add_argument("--directions", type=int, default=17)
    parser.add_argument("--crop-size", type=parse_crop_size, default=250)
    parser.add_argument(
        "--target",
        choices=sorted(TARGET_SIZES),
        default="cores3",
        help="Device preset size. Defaults to cores3.",
    )
    parser.add_argument("--size", type=parse_size, help="Override output size, for example 240 or 386x386.")
    parser.add_argument("--format", choices=sorted(OUTPUT_SUFFIXES), default="jpg")
    parser.add_argument("--quality", type=int, default=82)
    parser.add_argument(
        "--matte",
        type=parse_matte,
        default=(0, 0, 0),
        help="Background for transparent pixels: black, white, or #RRGGBB. Defaults to black.",
    )
    parser.add_argument("--pixel-threshold", type=int, default=18)
    parser.add_argument("--run-threshold", type=int, default=20)
    parser.add_argument(
        "--min-run-length",
        type=int,
        default=4,
        help="Ignore detected row/column runs shorter than this many source pixels. Defaults to 4.",
    )
    parser.add_argument(
        "--layout",
        choices=("auto", "detect", "even"),
        default="auto",
        help="Use black-background detection, equal grid splitting, or auto fallback. Defaults to auto.",
    )
    parser.add_argument(
        "--cell-inset",
        type=int,
        default=0,
        help="Inset each detected cell boundary by this many source pixels before cropping.",
    )
    parser.add_argument(
        "--row-top-mask",
        type=int,
        default=0,
        help=(
            "Cover this many source pixels at the top of every row after the first "
            "with the matte color. Use this for fragments left by the previous row "
            "without zooming or shifting the frame."
        ),
    )
    parser.add_argument(
        "--column-side-mask",
        type=int,
        default=0,
        help=(
            "Cover this many source pixels along internal left/right cell boundaries "
            "with the matte color. This removes fragments crossing between columns "
            "without zooming or shifting the frame."
        ),
    )
    parser.add_argument(
        "--column-x-offsets",
        type=parse_column_x_offsets,
        help=(
            "Comma-separated horizontal translations in source pixels, one per grid "
            "column. Negative values move frames left. For example: "
            "--column-x-offsets=-11,-11,-9,0"
        ),
    )
    parser.add_argument("--start-index", type=int, default=0)
    parser.add_argument("--pad", type=int, default=0, help="Zero-pad output numbers, for example 2 for 01.")
    parser.add_argument(
        "--output-naming",
        choices=("indexed", "base-mouth-eye", "travel-expressions"),
        default="indexed",
        help=(
            "Output naming scheme. 'base-mouth-eye' maps 4x4 rows to mouth "
            "indices and columns to eye indices, for example base_m0_e0. "
            "'travel-expressions' maps the 3x3 travel sheet to its canonical "
            "travel_wink ... travel_peace names."
        ),
    )
    parser.add_argument("--preview-dir", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="Do not remove existing PREFIX*.jpg/png files before writing new ones.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.quality < 1 or args.quality > 100:
        raise SystemExit("--quality must be between 1 and 100")
    if args.directions < 1:
        raise SystemExit("--directions must be 1 or greater")
    if args.cell_inset < 0:
        raise SystemExit("--cell-inset must be 0 or greater")
    if args.row_top_mask < 0:
        raise SystemExit("--row-top-mask must be 0 or greater")
    if args.column_side_mask < 0:
        raise SystemExit("--column-side-mask must be 0 or greater")
    if args.column_x_offsets is not None and len(args.column_x_offsets) != args.grid.cols:
        raise SystemExit(
            f"--column-x-offsets requires {args.grid.cols} values for a "
            f"{args.grid.rows}x{args.grid.cols} grid"
        )
    if args.min_run_length < 1:
        raise SystemExit("--min-run-length must be 1 or greater")
    if args.pad < 0:
        raise SystemExit("--pad must be 0 or greater")
    if args.output_naming == "base-mouth-eye":
        if args.grid != GridSpec(4, 4) or args.directions != 16:
            raise SystemExit("base-mouth-eye naming requires --grid 4x4 --directions 16")
        if args.start_index != 0 or args.pad != 0:
            raise SystemExit("base-mouth-eye naming does not use --start-index or --pad")
    if args.output_naming == "travel-expressions":
        if args.grid != GridSpec(3, 3) or args.directions != 9:
            raise SystemExit("travel-expressions naming requires --grid 3x3 --directions 9")
        if args.start_index != 0 or args.pad != 0:
            raise SystemExit("travel-expressions naming does not use --start-index or --pad")

    specs = args.sheet or default_sheet_specs()
    target_size = args.size or TARGET_SIZES[args.target]
    out_dir = args.out_dir.expanduser().resolve()
    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)

    for spec in specs:
        spec = SheetSpec(spec.path.expanduser().resolve(), spec.prefix)
        outputs = split_sheet(
            spec=spec,
            out_dir=out_dir,
            grid=args.grid,
            output_count=args.directions,
            source_crop_size=args.crop_size,
            target_size=target_size,
            output_format=args.format,
            quality=args.quality,
            matte=args.matte,
            pixel_threshold=args.pixel_threshold,
            run_threshold=args.run_threshold,
            min_run_length=args.min_run_length,
            layout=args.layout,
            cell_inset=args.cell_inset,
            row_top_mask=args.row_top_mask,
            column_side_mask=args.column_side_mask,
            column_x_offsets=args.column_x_offsets,
            start_index=args.start_index,
            pad=args.pad,
            output_naming=args.output_naming,
            clean=not args.no_clean,
            dry_run=args.dry_run,
        )
        if args.preview_dir and not args.dry_run:
            write_preview(outputs, args.preview_dir / preview_name(spec.prefix))

    print("done")


if __name__ == "__main__":
    main()
