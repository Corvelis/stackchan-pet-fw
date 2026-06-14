#!/usr/bin/env python3
"""Build Stack-chan pet firmware face assets from a sprite sheet."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from collections import Counter
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable

try:
    from PIL import Image, ImageDraw, UnidentifiedImageError
except ImportError as exc:  # pragma: no cover - exercised only without Pillow.
    Image = None
    UnidentifiedImageError = OSError
    PIL_IMPORT_ERROR = exc
else:
    PIL_IMPORT_ERROR = None


BASE_MARGIN_IMAGE_SIZE = 2048
DEFAULT_COLS = 6
DEFAULT_ROWS = 6
DEFAULT_OUTPUT_SIZE = 240
EXPECTED_CELL_COUNT = 36
SUPPORTED_SUFFIXES = {".png", ".jpg", ".jpeg"}
DEFAULT_PAD_COLOR = (0, 0, 0)

TOOL_DIR = Path(__file__).resolve().parent
DEFAULT_MAPPING_JSON = TOOL_DIR / "face_mapping.json"
DEFAULT_COPY_JSON = TOOL_DIR / "face_copy_rules.json"
DEFAULT_FACE_MODEL = TOOL_DIR / "models" / "anime_face_detect_v1.4_n.onnx"
MODEL_DOWNLOAD_COMMAND = (
    "python tools/face_image_builder/build_faces_from_sprite_sheet/download_model.py"
)

DEFAULT_FACE_MAPPING = [
    "idle.png",
    "listen.png",
    "talk_0.png",
    "talk_1.png",
    "blink.png",
    "smile.png",
    "good_0.png",
    "good_1.png",
    "good_blink.png",
    "idle_guarded_0.png",
    "talk_guarded_1.png",
    "blink_guarded_0.png",
    "idle_attached_0.png",
    "talk_attached_1.png",
    "blink_attached_0.png",
    "pet_attached_0.png",
    "pet_attached_1.png",
    "pet_blink_attached_0.png",
    "pet_guarded_0.png",
    "pet_guarded_1.png",
    "nadenade_0.png",
    "nadenade_1.png",
    "furifuri_0.png",
    "furifuri_1.png",
    "photo_0.png",
    "photo_1.png",
    "photo_blink.png",
    "tired_0.png",
    "tired_talk.png",
    "tired_blink.png",
    "photo_master_0.png",
    "photo_master_1.png",
    "bad_0.png",
    "bad_1.png",
    "exhausted_0.png",
    "exhausted_talk.png",
]

DEFAULT_COPY_RULES = {
    "photo_blink_talk.png": "photo_blink.png",
    "talk_guarded_0.png": "idle_guarded_0.png",
    "talk_attached_0.png": "idle_attached_0.png",
    "pet_blink_guarded_0.png": "pet_guarded_0.png",
    "shake_guarded_0.png": "furifuri_0.png",
    "shake_guarded_1.png": "furifuri_1.png",
    "shake_attached_0.png": "furifuri_0.png",
    "shake_attached_1.png": "furifuri_1.png",
    "exhausted_blink.png": "tired_blink.png",
    "low_power_0.png": "exhausted_0.png",
    "low_power_talk.png": "exhausted_talk.png",
    "low_power_blink.png": "tired_blink.png",
}


class BuildFacesError(Exception):
    """Raised for user-facing CLI validation errors."""


@dataclass(frozen=True)
class CellPlan:
    index: int
    row: int
    col: int
    filename: str
    box: tuple[int, int, int, int]


@dataclass(frozen=True)
class CopyRule:
    dest: str
    source: str


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build Stack-chan pet firmware face PNGs from a sprite sheet."
    )
    parser.add_argument("input", type=Path, help="Input square sprite sheet PNG/JPG/JPEG.")
    parser.add_argument("--out", type=Path, default=Path("data"), help="Output data directory.")
    parser.add_argument(
        "--install-to",
        type=Path,
        help="Install generated 48 PNG files directly to a firmware data directory.",
    )
    parser.add_argument(
        "--backup-existing",
        action="store_true",
        help="Backup existing PNG files before writing to the output/install directory.",
    )
    parser.add_argument(
        "--backup-dir",
        type=Path,
        default=Path("backups"),
        help="Directory for --backup-existing snapshots.",
    )
    parser.add_argument(
        "--margin",
        type=float,
        default=0,
        help="Outer margin in pixels at 2048px input size. Scaled for other sizes.",
    )
    parser.add_argument("--cols", type=int, default=DEFAULT_COLS, help="Sprite sheet columns.")
    parser.add_argument("--rows", type=int, default=DEFAULT_ROWS, help="Sprite sheet rows.")
    parser.add_argument(
        "--output-size",
        type=int,
        default=DEFAULT_OUTPUT_SIZE,
        help="Output face PNG size in pixels.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print crop plan without writing files.")
    parser.add_argument(
        "--detect-grid",
        action="store_true",
        help="Detect grid lines and crop the inside of each detected cell.",
    )
    parser.add_argument(
        "--grid-line-padding",
        type=int,
        default=12,
        help="Extra pixels to trim inside detected grid lines.",
    )
    parser.add_argument(
        "--cell-scale",
        type=float,
        default=0.92,
        help="Scale for the fixed square crop size. Use less than 1.0 to crop inside cells.",
    )
    parser.add_argument(
        "--align-face-crop",
        action="store_true",
        default=True,
        help="With --detect-grid and fixed cell crop, move crop boxes to align detected face centers.",
    )
    parser.add_argument(
        "--no-align-face-crop",
        action="store_false",
        dest="align_face_crop",
        help="Disable face-center crop alignment.",
    )
    parser.add_argument(
        "--face-align-max-shift",
        type=int,
        default=28,
        help="Maximum source-image pixels each crop box may move for face-center alignment.",
    )
    parser.add_argument(
        "--normalize-face-size",
        action="store_true",
        default=True,
        help="Resize each cropped cell before final output so detected face heights match.",
    )
    parser.add_argument(
        "--no-normalize-face-size",
        action="store_false",
        dest="normalize_face_size",
        help="Disable detected face size normalization.",
    )
    parser.add_argument(
        "--pad-color",
        default="0,0,0",
        help="RGB color used for padding, for example 0,0,0 or #000000.",
    )
    parser.add_argument(
        "--face-score-threshold",
        type=float,
        default=0.2,
        help="Minimum anime face detection score used for alignment.",
    )
    parser.add_argument(
        "--debug-detections",
        action="store_true",
        help="Write debug_face_detections.png with detected face rectangles.",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Delete existing PNG files in the output directory before generation.",
    )
    parser.add_argument(
        "--mapping-json",
        type=Path,
        help="Override the 36-cell filename mapping with a JSON file.",
    )
    parser.add_argument(
        "--copy-json",
        type=Path,
        help="Override copy completion rules with a JSON file.",
    )
    return parser.parse_args(argv)


def require_pillow() -> None:
    if PIL_IMPORT_ERROR is not None:
        raise BuildFacesError(
            "Pillow is required. Install dependencies with: "
            "python3 -m pip install -r "
            "tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt"
        )


def load_json(path: Path) -> Any:
    try:
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)
    except OSError as exc:
        raise BuildFacesError(f"failed to read JSON file {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise BuildFacesError(f"invalid JSON in {path}: {exc}") from exc


def validate_png_filename(name: Any, source: str) -> str:
    if not isinstance(name, str) or not name:
        raise BuildFacesError(f"{source}: filename must be a non-empty string")
    if name != Path(name).name or "/" in name or "\\" in name:
        raise BuildFacesError(f"{source}: filename must not contain a path: {name!r}")
    if Path(name).suffix.lower() != ".png":
        raise BuildFacesError(f"{source}: filename must end with .png: {name!r}")
    return name


def normalize_mapping(data: Any, expected_count: int, source: str) -> list[str]:
    if isinstance(data, dict):
        for key in ("mapping", "files", "faces"):
            if key in data:
                data = data[key]
                break
        else:
            try:
                ordered_keys = sorted(data, key=lambda value: int(value))
            except (TypeError, ValueError) as exc:
                raise BuildFacesError(
                    f"{source}: mapping JSON must be a list, a mapping/files/faces object, "
                    "or an object with numeric keys"
                ) from exc
            data = [data[key] for key in ordered_keys]

    if not isinstance(data, list):
        raise BuildFacesError(f"{source}: mapping JSON must resolve to a list")

    mapping = [validate_png_filename(name, source) for name in data]
    if len(mapping) != expected_count:
        raise BuildFacesError(
            f"{source}: expected {expected_count} filenames, got {len(mapping)}"
        )
    duplicates = sorted(name for name, count in Counter(mapping).items() if count > 1)
    if duplicates:
        raise BuildFacesError(f"{source}: duplicate mapping filenames: {', '.join(duplicates)}")
    return mapping


def normalize_copy_rules(data: Any, source: str) -> list[CopyRule]:
    if isinstance(data, dict):
        for key in ("copy_rules", "rules", "copies"):
            if key in data:
                data = data[key]
                break

    rules: list[CopyRule] = []
    if isinstance(data, dict):
        for dest, rule_source in data.items():
            rules.append(
                CopyRule(
                    dest=validate_png_filename(dest, source),
                    source=validate_png_filename(rule_source, source),
                )
            )
    elif isinstance(data, list):
        for index, item in enumerate(data, start=1):
            item_source = f"{source}[{index}]"
            if isinstance(item, dict):
                dest = item.get("dest", item.get("to"))
                rule_source = item.get("source", item.get("from"))
            elif isinstance(item, list) and len(item) == 2:
                dest, rule_source = item
            else:
                raise BuildFacesError(
                    f"{item_source}: copy rule must be an object or [dest, source] pair"
                )
            rules.append(
                CopyRule(
                    dest=validate_png_filename(dest, item_source),
                    source=validate_png_filename(rule_source, item_source),
                )
            )
    else:
        raise BuildFacesError(f"{source}: copy JSON must resolve to an object or list")

    destination_counts = Counter(rule.dest for rule in rules)
    duplicates = sorted(name for name, count in destination_counts.items() if count > 1)
    if duplicates:
        raise BuildFacesError(f"{source}: duplicate copy destinations: {', '.join(duplicates)}")
    return rules


def load_mapping(path: Path | None, expected_count: int) -> list[str]:
    if path is not None:
        return normalize_mapping(load_json(path), expected_count, str(path))
    if DEFAULT_MAPPING_JSON.exists():
        return normalize_mapping(
            load_json(DEFAULT_MAPPING_JSON),
            expected_count,
            str(DEFAULT_MAPPING_JSON),
        )
    return normalize_mapping(DEFAULT_FACE_MAPPING, expected_count, "built-in mapping")


def load_copy_rules(path: Path | None) -> list[CopyRule]:
    if path is not None:
        return normalize_copy_rules(load_json(path), str(path))
    if DEFAULT_COPY_JSON.exists():
        return normalize_copy_rules(load_json(DEFAULT_COPY_JSON), str(DEFAULT_COPY_JSON))
    return normalize_copy_rules(DEFAULT_COPY_RULES, "built-in copy rules")


def parse_rgb_color(value: str) -> tuple[int, int, int]:
    raw = value.strip()
    if raw.startswith("#"):
        hex_color = raw[1:]
        if len(hex_color) != 6:
            raise BuildFacesError("--pad-color hex value must be in #RRGGBB format")
        try:
            return (
                int(hex_color[0:2], 16),
                int(hex_color[2:4], 16),
                int(hex_color[4:6], 16),
            )
        except ValueError as exc:
            raise BuildFacesError(f"invalid --pad-color value: {value}") from exc

    parts = [part.strip() for part in raw.split(",")]
    if len(parts) != 3:
        raise BuildFacesError("--pad-color must be R,G,B or #RRGGBB")
    try:
        rgb = tuple(int(part) for part in parts)
    except ValueError as exc:
        raise BuildFacesError(f"invalid --pad-color value: {value}") from exc
    if any(channel < 0 or channel > 255 for channel in rgb):
        raise BuildFacesError("--pad-color channels must be between 0 and 255")
    return rgb


def validate_grid(cols: int, rows: int, mapping_count: int) -> None:
    if cols <= 0 or rows <= 0:
        raise BuildFacesError("--cols and --rows must be positive integers")
    if cols * rows != EXPECTED_CELL_COUNT:
        raise BuildFacesError(
            f"--cols x --rows must be {EXPECTED_CELL_COUNT}; got {cols} x {rows} = {cols * rows}"
        )
    if cols * rows != mapping_count:
        raise BuildFacesError(
            f"grid has {cols * rows} cells but mapping has {mapping_count} filenames"
        )


def scaled_margin_pixels(image_size: int, margin: float) -> float:
    if margin < 0:
        raise BuildFacesError("--margin must be zero or greater")
    scaled = margin * image_size / BASE_MARGIN_IMAGE_SIZE
    if scaled * 2 >= image_size:
        raise BuildFacesError(
            f"--margin {margin:g} is too large for {image_size}x{image_size} input"
        )
    return scaled


def build_crop_boxes(
    image_size: int,
    cols: int = DEFAULT_COLS,
    rows: int = DEFAULT_ROWS,
    margin: float = 0,
) -> list[tuple[int, int, int, int]]:
    """Return row-major crop boxes for a square image."""
    if image_size <= 0:
        raise BuildFacesError("image size must be positive")
    if cols <= 0 or rows <= 0:
        raise BuildFacesError("cols and rows must be positive")

    scaled_margin = scaled_margin_pixels(image_size, margin)
    usable_size = image_size - (scaled_margin * 2)
    x_edges = [
        int(round(scaled_margin + usable_size * col / cols))
        for col in range(cols + 1)
    ]
    y_edges = [
        int(round(scaled_margin + usable_size * row / rows))
        for row in range(rows + 1)
    ]

    boxes: list[tuple[int, int, int, int]] = []
    for row in range(rows):
        for col in range(cols):
            box = (x_edges[col], y_edges[row], x_edges[col + 1], y_edges[row + 1])
            if box[2] <= box[0] or box[3] <= box[1]:
                raise BuildFacesError(f"invalid crop box for row {row + 1}, col {col + 1}: {box}")
            boxes.append(box)
    return boxes


def is_grid_pixel(rgb: tuple[int, int, int]) -> bool:
    red, green, blue = rgb
    brightness = (red + green + blue) / 3
    spread = max(rgb) - min(rgb)
    if spread <= 14 and 80 <= brightness <= 255:
        return True
    return False


def is_grid_pixel_relaxed(rgb: tuple[int, int, int]) -> bool:
    red, green, blue = rgb
    brightness = (red + green + blue) / 3
    spread = max(rgb) - min(rgb)
    if is_grid_pixel(rgb):
        return True
    if spread <= 10 and 25 <= brightness <= 95:
        return True
    return False


def line_scores(image: Any, axis: str, relaxed: bool = False) -> list[float]:
    rgb_image = image.convert("RGB")
    width, height = rgb_image.size
    pixels = rgb_image.load()
    scores: list[float] = []
    grid_pixel = is_grid_pixel_relaxed if relaxed else is_grid_pixel

    if axis == "x":
        for x in range(width):
            hits = 0
            for y in range(height):
                if grid_pixel(pixels[x, y]):
                    hits += 1
            scores.append(hits / height)
    elif axis == "y":
        for y in range(height):
            hits = 0
            for x in range(width):
                if grid_pixel(pixels[x, y]):
                    hits += 1
            scores.append(hits / width)
    else:
        raise BuildFacesError(f"unsupported grid score axis: {axis}")
    return scores


def score_clusters(scores: list[float], threshold: float) -> list[tuple[int, int, float]]:
    clusters: list[tuple[int, int, float]] = []
    start: int | None = None
    cluster_scores: list[tuple[int, float]] = []

    for index, score in enumerate(scores):
        if score >= threshold:
            if start is None:
                start = index
                cluster_scores = []
            cluster_scores.append((index, score))
        elif start is not None:
            _peak_index, peak_score = max(cluster_scores, key=lambda item: item[1])
            clusters.append((start, index - 1, peak_score))
            start = None
            cluster_scores = []

    if start is not None:
        _peak_index, peak_score = max(cluster_scores, key=lambda item: item[1])
        clusters.append((start, len(scores) - 1, peak_score))

    return clusters


def merge_nearby_clusters(
    clusters: list[tuple[int, int, float]],
    max_gap: int,
) -> list[tuple[int, int, float]]:
    if not clusters:
        return []

    merged: list[tuple[int, int, float]] = []
    current_start, current_end, current_score = clusters[0]
    for start, end, score in clusters[1:]:
        if start - current_end <= max_gap + 1:
            current_end = max(current_end, end)
            current_score = max(current_score, score)
        else:
            merged.append((current_start, current_end, current_score))
            current_start, current_end, current_score = start, end, score
    merged.append((current_start, current_end, current_score))
    return merged


def detect_grid_line_clusters(
    image: Any,
    axis: str,
    expected_count: int,
    axis_name: str,
) -> list[tuple[int, int, float]]:
    best_clusters: list[tuple[int, int, float]] = []
    for relaxed in (False, True):
        scores = line_scores(image, axis, relaxed)
        max_gap = max(3, len(scores) // 512)
        for threshold in (0.55, 0.50, 0.45, 0.40, 0.35):
            clusters = merge_nearby_clusters(score_clusters(scores, threshold), max_gap)
            if len(clusters) > len(best_clusters):
                best_clusters = clusters
            if len(clusters) >= expected_count:
                return clusters

    raise BuildFacesError(
        f"failed to detect {expected_count} {axis_name} grid lines; "
        f"detected {len(best_clusters)}"
    )


def choose_grid_lines(
    clusters: list[tuple[int, int, float]],
    expected_count: int,
    image_size: int,
    axis_name: str,
) -> list[tuple[int, int]]:
    if len(clusters) < expected_count:
        raise BuildFacesError(
            f"failed to detect {expected_count} {axis_name} grid lines; "
            f"detected {len(clusters)}"
        )

    max_line_width = max(12, image_size // 80)
    candidates = [
        cluster
        for cluster in clusters
        if cluster[1] - cluster[0] + 1 <= max_line_width
    ]
    if len(candidates) < expected_count:
        candidates = clusters

    expected_positions = [
        image_size * index / (expected_count - 1)
        for index in range(expected_count)
    ]
    chosen: list[tuple[int, int, float]] = []
    remaining = candidates[:]
    for expected in expected_positions:
        cluster = min(
            remaining,
            key=lambda item: (abs(((item[0] + item[1]) / 2) - expected), -item[2]),
        )
        chosen.append(cluster)
        remaining.remove(cluster)

    chosen.sort(key=lambda item: item[0])
    centers = [(start + end) / 2 for start, end, _score in chosen]
    min_spacing = image_size / (expected_count - 1) * 0.55
    for left, right in zip(centers, centers[1:]):
        if right - left < min_spacing:
            raise BuildFacesError(
                f"detected {axis_name} grid lines are too close together; "
                "try uniform cropping or a clearer grid template"
            )

    return [(start, end) for start, end, _score in chosen]


def regularize_grid_lines_if_needed(
    lines: list[tuple[int, int]],
    expected_count: int,
    image_size: int,
) -> list[tuple[int, int]]:
    centers = [(start + end) / 2 for start, end in lines]
    if len(centers) != expected_count:
        return lines

    spacings = [right - left for left, right in zip(centers, centers[1:])]
    if not spacings or min(spacings) <= 0:
        return lines

    ideal_spacing = (centers[-1] - centers[0]) / (expected_count - 1)
    if ideal_spacing <= 0:
        return lines

    edge_tolerance = image_size * 0.03
    if centers[0] > edge_tolerance or centers[-1] < image_size - edge_tolerance:
        return lines

    max_deviation = max(
        abs(center - (centers[0] + ideal_spacing * index))
        for index, center in enumerate(centers)
    )
    spacing_ratio = max(spacings) / min(spacings)
    should_regularize = spacing_ratio > 1.45 or max_deviation > ideal_spacing * 0.24
    if not should_regularize:
        return lines

    regularized: list[tuple[int, int]] = []
    for index in range(expected_count):
        center = int(round(centers[0] + ideal_spacing * index))
        center = max(0, min(image_size - 1, center))
        regularized.append((center, center))
    return regularized


def build_grid_detected_crop_boxes(
    image: Any,
    cols: int,
    rows: int,
    padding: int,
    cell_scale: float = 1.0,
    align_face_crop: bool = False,
    face_score_threshold: float = 0.2,
    face_align_max_shift: int = 28,
    filenames: list[str] | None = None,
) -> list[tuple[int, int, int, int]]:
    if padding < 0:
        raise BuildFacesError("--grid-line-padding must be zero or greater")
    if cell_scale <= 0:
        raise BuildFacesError("--cell-scale must be greater than zero")

    width, height = image.size
    vertical_clusters = detect_grid_line_clusters(image, "x", cols + 1, "vertical")
    horizontal_clusters = detect_grid_line_clusters(image, "y", rows + 1, "horizontal")
    vertical_lines = choose_grid_lines(vertical_clusters, cols + 1, width, "vertical")
    horizontal_lines = choose_grid_lines(horizontal_clusters, rows + 1, height, "horizontal")
    vertical_lines = regularize_grid_lines_if_needed(vertical_lines, cols + 1, width)
    horizontal_lines = regularize_grid_lines_if_needed(horizontal_lines, rows + 1, height)

    raw_boxes: list[tuple[int, int, int, int]] = []
    for row in range(rows):
        for col in range(cols):
            left = vertical_lines[col][1] + 1 + padding
            top = horizontal_lines[row][1] + 1 + padding
            right = vertical_lines[col + 1][0] - padding
            bottom = horizontal_lines[row + 1][0] - padding
            if right <= left or bottom <= top:
                raise BuildFacesError(
                    f"invalid detected grid crop box for row {row + 1}, "
                    f"col {col + 1}: ({left}, {top}, {right}, {bottom})"
                )
            raw_boxes.append((left, top, right, bottom))

    widths = sorted(right - left for left, _top, right, _bottom in raw_boxes)
    heights = sorted(bottom - top for _left, top, _right, bottom in raw_boxes)
    median_width = widths[len(widths) // 2]
    median_height = heights[len(heights) // 2]
    crop_size = int(round(min(median_width, median_height) * cell_scale))
    if crop_size <= 0:
        raise BuildFacesError(
            f"invalid fixed crop size {crop_size}; check --cell-scale"
        )

    boxes: list[tuple[int, int, int, int]] = []
    half_size = crop_size / 2
    for index, (left, top, right, bottom) in enumerate(raw_boxes, start=1):
        center_x = (left + right) / 2
        center_y = (top + bottom) / 2
        fixed_left = int(round(center_x - half_size))
        fixed_top = int(round(center_y - half_size))
        fixed_right = fixed_left + crop_size
        fixed_bottom = fixed_top + crop_size
        if fixed_left < 0:
            fixed_left = 0
            fixed_right = crop_size
        if fixed_top < 0:
            fixed_top = 0
            fixed_bottom = crop_size
        if fixed_right > width:
            fixed_right = width
            fixed_left = width - crop_size
        if fixed_bottom > height:
            fixed_bottom = height
            fixed_top = height - crop_size
        if fixed_left < 0 or fixed_top < 0 or fixed_right > width or fixed_bottom > height:
            raise BuildFacesError(
                f"fixed crop for cell {index} exceeds image bounds; "
                "try a smaller --cell-scale"
            )
        boxes.append((fixed_left, fixed_top, fixed_right, fixed_bottom))
    if align_face_crop:
        boxes = align_crop_boxes_to_face_centers(
            image,
            boxes,
            raw_boxes,
            face_score_threshold,
            face_align_max_shift,
            filenames,
        )
    return boxes


def build_cell_plans(
    mapping: list[str],
    boxes: list[tuple[int, int, int, int]],
    cols: int,
) -> list[CellPlan]:
    return [
        CellPlan(
            index=index + 1,
            row=index // cols + 1,
            col=index % cols + 1,
            filename=filename,
            box=boxes[index],
        )
        for index, filename in enumerate(mapping)
    ]


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(maximum, value))


def open_source_image(path: Path) -> Any:
    require_pillow()
    if path.suffix.lower() not in SUPPORTED_SUFFIXES:
        raise BuildFacesError(
            f"unsupported input image extension {path.suffix!r}; use PNG, JPG, or JPEG"
        )
    try:
        image = Image.open(path)
        image.load()
    except FileNotFoundError as exc:
        raise BuildFacesError(f"input image not found: {path}") from exc
    except (OSError, UnidentifiedImageError) as exc:
        raise BuildFacesError(f"failed to read input image {path}: {exc}") from exc

    width, height = image.size
    if width != height:
        raise BuildFacesError(f"input image must be square; got {width}x{height}")
    return image


def high_quality_resample() -> int:
    if hasattr(Image, "Resampling"):
        return Image.Resampling.LANCZOS
    return Image.LANCZOS


def flatten_to_rgb(image: Any) -> Any:
    rgba = image.convert("RGBA")
    background = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
    background.alpha_composite(rgba)
    return background.convert("RGB")


def finalize_output_image(image: Any) -> Any:
    return flatten_to_rgb(image)


def median(values: list[float]) -> float:
    if not values:
        raise BuildFacesError("cannot calculate median of an empty list")
    sorted_values = sorted(values)
    middle = len(sorted_values) // 2
    if len(sorted_values) % 2 == 1:
        return sorted_values[middle]
    return (sorted_values[middle - 1] + sorted_values[middle]) / 2


def percentile(values: list[float], ratio: float) -> float:
    if not values:
        raise BuildFacesError("cannot calculate percentile of an empty list")
    if ratio <= 0:
        return min(values)
    if ratio >= 1:
        return max(values)
    sorted_values = sorted(values)
    index = (len(sorted_values) - 1) * ratio
    lower = int(index)
    upper = min(lower + 1, len(sorted_values) - 1)
    fraction = index - lower
    return sorted_values[lower] * (1 - fraction) + sorted_values[upper] * fraction


def require_onnxruntime() -> tuple[Any, Any]:
    try:
        import numpy as np
        import onnxruntime as ort
    except ImportError as exc:
        raise BuildFacesError(
            "onnxruntime and numpy are required for face detection options. "
            "Install dependencies with: "
            "python3 -m pip install -r "
            "tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt"
        ) from exc
    return np, ort


def iou(box_a: tuple[float, float, float, float], box_b: tuple[float, float, float, float]) -> float:
    left = max(box_a[0], box_b[0])
    top = max(box_a[1], box_b[1])
    right = min(box_a[2], box_b[2])
    bottom = min(box_a[3], box_b[3])
    if right <= left or bottom <= top:
        return 0.0

    intersection = (right - left) * (bottom - top)
    area_a = (box_a[2] - box_a[0]) * (box_a[3] - box_a[1])
    area_b = (box_b[2] - box_b[0]) * (box_b[3] - box_b[1])
    union = area_a + area_b - intersection
    return intersection / union if union > 0 else 0.0


def nms(
    detections: list[tuple[float, float, float, float, float]],
    iou_threshold: float,
) -> list[tuple[float, float, float, float, float]]:
    selected: list[tuple[float, float, float, float, float]] = []
    for detection in sorted(detections, key=lambda item: item[4], reverse=True):
        box = detection[:4]
        if all(iou(box, chosen[:4]) < iou_threshold for chosen in selected):
            selected.append(detection)
    return selected


def anime_face_session() -> Any:
    if not hasattr(anime_face_session, "_session"):
        _np, ort = require_onnxruntime()
        ensure_face_model()
        anime_face_session._session = ort.InferenceSession(  # type: ignore[attr-defined]
            str(DEFAULT_FACE_MODEL),
            providers=["CPUExecutionProvider"],
        )
    return anime_face_session._session  # type: ignore[attr-defined]


def ensure_face_model() -> None:
    if DEFAULT_FACE_MODEL.exists():
        return

    try:
        from download_model import MODEL_URL, download

        print(f"Downloading anime face detection model to {DEFAULT_FACE_MODEL}")
        download(MODEL_URL, DEFAULT_FACE_MODEL, force=False)
    except Exception as exc:
        raise BuildFacesError(
            "anime face detection model is missing and automatic download failed: "
            f"{DEFAULT_FACE_MODEL}\n"
            f"Run manually: {MODEL_DOWNLOAD_COMMAND}\n"
            f"Reason: {exc}"
        ) from exc


def detect_face_bbox_anime(
    image: Any,
    score_threshold: float,
) -> tuple[float, float, float, float, float] | None:
    np, _ort = require_onnxruntime()
    rgb_image = image.convert("RGB")
    width, height = rgb_image.size
    input_size = 640
    scale = min(input_size / width, input_size / height)
    resized_width = max(1, int(round(width * scale)))
    resized_height = max(1, int(round(height * scale)))
    pad_x = (input_size - resized_width) // 2
    pad_y = (input_size - resized_height) // 2

    canvas = Image.new("RGB", (input_size, input_size), (114, 114, 114))
    resized = rgb_image.resize((resized_width, resized_height), high_quality_resample())
    canvas.paste(resized, (pad_x, pad_y))

    array = np.asarray(canvas).astype("float32") / 255.0
    array = np.transpose(array, (2, 0, 1))[None]
    session = anime_face_session()
    input_name = session.get_inputs()[0].name
    output = session.run(None, {input_name: array})[0][0]
    if output.shape[0] == 5:
        output = output.T

    detections: list[tuple[float, float, float, float, float]] = []
    for center_x, center_y, box_width, box_height, score in output:
        score = float(score)
        if score < score_threshold:
            continue
        left = (float(center_x) - float(box_width) / 2 - pad_x) / scale
        top = (float(center_y) - float(box_height) / 2 - pad_y) / scale
        right = (float(center_x) + float(box_width) / 2 - pad_x) / scale
        bottom = (float(center_y) + float(box_height) / 2 - pad_y) / scale
        left = max(0.0, min(float(width), left))
        top = max(0.0, min(float(height), top))
        right = max(0.0, min(float(width), right))
        bottom = max(0.0, min(float(height), bottom))
        if right <= left or bottom <= top:
            continue
        detections.append((left, top, right, bottom, score))

    selected = nms(detections, iou_threshold=0.5)
    if not selected:
        return None

    left, top, right, bottom, score = selected[0]
    return (left, top, right - left, bottom - top, score)


def align_crop_boxes_to_face_centers(
    source_image: Any,
    crop_boxes: list[tuple[int, int, int, int]],
    bounds_boxes: list[tuple[int, int, int, int]],
    face_score_threshold: float,
    max_shift: int,
    filenames: list[str] | None = None,
) -> list[tuple[int, int, int, int]]:
    if max_shift < 0:
        raise BuildFacesError("--face-align-max-shift must be zero or greater")
    if len(crop_boxes) != len(bounds_boxes):
        raise BuildFacesError("crop boxes and bounds boxes must have the same length")
    if filenames is not None and len(filenames) != len(crop_boxes):
        raise BuildFacesError("filenames and crop boxes must have the same length")

    detections: list[tuple[str, tuple[int, int, int, int], tuple[int, int, int, int], tuple[float, float, float, float, float] | None]] = []
    centers_x: list[float] = []
    tops_y: list[float] = []
    group_centers_x: dict[str, list[float]] = {}
    group_tops_y: dict[str, list[float]] = {}
    for index, (crop_box, bounds_box) in enumerate(zip(crop_boxes, bounds_boxes)):
        filename = filenames[index] if filenames is not None else f"cell_{index + 1}.png"
        group = face_size_group_key(filename)
        crop = flatten_to_rgb(source_image.crop(crop_box))
        face = detect_face_bbox_anime(crop, face_score_threshold)
        detections.append((group, crop_box, bounds_box, face))
        if face is None:
            continue
        face_x, face_y, face_width, face_height, _score = face
        center_x = face_x + face_width / 2
        centers_x.append(center_x)
        tops_y.append(face_y)
        group_centers_x.setdefault(group, []).append(center_x)
        group_tops_y.setdefault(group, []).append(face_y)

    if not centers_x or not tops_y:
        print("Face crop alignment skipped: no face detections")
        return crop_boxes

    target_x = median(centers_x)
    target_y = percentile(tops_y, 0.25)
    group_shift_x = {
        group: clamp(int(round(median(values) - target_x)), -max_shift, max_shift)
        for group, values in group_centers_x.items()
        if values
    }
    group_shift_y = {
        group: clamp(int(round(percentile(values, 0.25) - target_y)), -max_shift, max_shift)
        for group, values in group_tops_y.items()
        if values
    }
    aligned: list[tuple[int, int, int, int]] = []
    aligned_count = 0
    for group, crop_box, bounds_box, face in detections:
        left, top, right, bottom = crop_box
        bound_left, bound_top, bound_right, bound_bottom = bounds_box
        crop_width = right - left
        crop_height = bottom - top
        new_left = left
        new_top = top

        if face is not None:
            new_left = left + group_shift_x.get(group, 0)
            new_top = top + group_shift_y.get(group, 0)
            aligned_count += 1

        min_left = bound_left
        max_left = bound_right - crop_width
        min_top = bound_top
        max_top = bound_bottom - crop_height
        if max_left < min_left or max_top < min_top:
            aligned.append(crop_box)
            continue

        new_left = clamp(new_left, min_left, max_left)
        new_top = clamp(new_top, min_top, max_top)
        aligned.append((new_left, new_top, new_left + crop_width, new_top + crop_height))

    print(f"Face crop alignment: {aligned_count}/{len(crop_boxes)} cells")
    return aligned


def write_detection_debug_image(
    cropped_cells: list[tuple[str, Any, tuple[float, float, float, float, float] | None]],
    path: Path,
) -> None:
    cols = DEFAULT_COLS
    cell_size = 240
    label_height = 34
    rows = (len(cropped_cells) + cols - 1) // cols
    debug = Image.new("RGB", (cols * cell_size, rows * (cell_size + label_height)), (255, 255, 255))
    draw = ImageDraw.Draw(debug)

    for index, (filename, cell, face) in enumerate(cropped_cells):
        row = index // cols
        col = index % cols
        x0 = col * cell_size
        y0 = row * (cell_size + label_height)
        shown = cell.resize((cell_size, cell_size), high_quality_resample())
        debug.paste(shown, (x0, y0))
        draw.rectangle((x0, y0, x0 + cell_size - 1, y0 + cell_size - 1), outline=(180, 180, 180))
        label = filename
        if face is None:
            label += " no face"
            draw.text((x0 + 4, y0 + cell_size + 4), label, fill=(180, 0, 0))
            continue

        face_x, face_y, face_width, face_height, score = face
        scale_x = cell_size / cell.width
        scale_y = cell_size / cell.height
        left = x0 + face_x * scale_x
        top = y0 + face_y * scale_y
        right = left + face_width * scale_x
        bottom = top + face_height * scale_y
        center_x = left + (right - left) / 2
        center_y = top + (bottom - top) / 2
        draw.rectangle((left, top, right, bottom), outline=(255, 0, 0), width=2)
        draw.line((center_x - 6, center_y, center_x + 6, center_y), fill=(0, 80, 255), width=2)
        draw.line((center_x, center_y - 6, center_x, center_y + 6), fill=(0, 80, 255), width=2)
        draw.text((x0 + 4, y0 + cell_size + 4), f"{filename} {score:.2f}", fill=(0, 0, 0))

    debug.save(path, format="PNG")


def paste_clipped(canvas: Any, image: Any, left: int, top: int) -> None:
    canvas_width, canvas_height = canvas.size
    image_width, image_height = image.size
    src_left = max(0, -left)
    src_top = max(0, -top)
    src_right = min(image_width, canvas_width - left)
    src_bottom = min(image_height, canvas_height - top)
    if src_right <= src_left or src_bottom <= src_top:
        return
    canvas.paste(
        image.crop((src_left, src_top, src_right, src_bottom)),
        (max(0, left), max(0, top)),
    )


def face_size_group_key(filename: str) -> str:
    stem = Path(filename).stem
    if stem in {"idle", "listen", "talk_0", "talk_1", "blink", "smile"}:
        return "neutral"
    if stem.startswith("good"):
        return "good"
    if stem.startswith("bad"):
        return "bad"
    if stem.startswith("photo_master"):
        return "photo_master"
    if stem.startswith("photo"):
        return "photo"
    if stem.startswith("nadenade"):
        return "nadenade"
    if stem.startswith("furifuri") or stem.startswith("shake"):
        return "furifuri"
    if "attached" in stem:
        return "attached"
    if "guarded" in stem:
        return "guarded"
    if stem.startswith("tired"):
        return "tired"
    if stem.startswith("exhausted") or stem.startswith("low_power"):
        return "low_power"
    return stem


def normalize_cropped_face_sizes(
    cropped_cells: list[tuple[str, Any]],
    face_score_threshold: float,
    pad_color: tuple[int, int, int],
    debug_detections_path: Path | None = None,
) -> list[tuple[str, Any]]:
    detections: list[tuple[str, Any, tuple[float, float, float, float, float] | None]] = []
    face_heights: list[float] = []
    group_heights: dict[str, list[float]] = {}
    group_centers_x: dict[str, list[float]] = {}
    group_tops_y: dict[str, list[float]] = {}
    centers_x: list[float] = []
    tops_y: list[float] = []

    for filename, image in cropped_cells:
        rgb_image = flatten_to_rgb(image)
        face = detect_face_bbox_anime(rgb_image, face_score_threshold)
        detections.append((filename, rgb_image, face))
        if face is None:
            continue
        face_x, face_y, face_width, face_height, _score = face
        group = face_size_group_key(filename)
        face_heights.append(face_height)
        center_x = face_x + face_width / 2
        group_heights.setdefault(group, []).append(face_height)
        group_centers_x.setdefault(group, []).append(center_x)
        group_tops_y.setdefault(group, []).append(face_y)
        centers_x.append(center_x)
        tops_y.append(face_y)

    if debug_detections_path is not None:
        write_detection_debug_image(detections, debug_detections_path)

    if not face_heights or not centers_x or not tops_y:
        print("Face size normalization skipped: no face detections")
        return [(filename, image) for filename, image, _face in detections]

    target_height = median(face_heights)
    target_center_x = median(centers_x)
    target_top_y = percentile(tops_y, 0.25)
    group_scales = {
        group: target_height / median(heights)
        for group, heights in group_heights.items()
        if heights and median(heights) > 0
    }
    group_current_centers_x = {
        group: median(values)
        for group, values in group_centers_x.items()
        if values
    }
    group_current_tops_y = {
        group: percentile(values, 0.25)
        for group, values in group_tops_y.items()
        if values
    }

    normalized: list[tuple[str, Any]] = []
    normalized_count = 0
    for filename, image, face in detections:
        if face is None:
            normalized.append((filename, image))
            continue

        face_x, face_y, face_width, face_height, _score = face
        if face_height <= 0:
            normalized.append((filename, image))
            continue

        group = face_size_group_key(filename)
        scale = group_scales.get(group, target_height / face_height)
        image_width, image_height = image.size
        scaled_width = max(1, int(round(image_width * scale)))
        scaled_height = max(1, int(round(image_height * scale)))
        scaled = image.resize((scaled_width, scaled_height), high_quality_resample())

        current_group_center_x = group_current_centers_x.get(group, face_x + face_width / 2)
        current_group_top_y = group_current_tops_y.get(group, face_y)
        paste_left = int(round(target_center_x - current_group_center_x * scale))
        paste_top = int(round(target_top_y - current_group_top_y * scale))

        canvas = Image.new("RGB", image.size, pad_color)
        paste_clipped(canvas, scaled, paste_left, paste_top)
        normalized.append((filename, canvas))
        normalized_count += 1

    print(
        f"Face size normalization: {normalized_count}/{len(cropped_cells)} cells, "
        f"target face height={target_height:.1f}px"
    )
    return normalized


def clean_output_dir(out_dir: Path) -> int:
    if not out_dir.exists():
        return 0
    deleted = 0
    for path in out_dir.glob("*.png"):
        path.unlink()
        deleted += 1
    return deleted


def backup_existing_pngs(out_dir: Path, backup_root: Path) -> Path | None:
    if not out_dir.exists():
        return None

    existing_pngs = sorted(out_dir.glob("*.png"))
    if not existing_pngs:
        return None

    backup_path = backup_root / ("data_faces_" + datetime.now().strftime("%Y%m%d_%H%M%S"))
    backup_path.mkdir(parents=True, exist_ok=False)
    for path in existing_pngs:
        shutil.copy2(path, backup_path / path.name)
    return backup_path


def save_generated_faces(
    source_image: Any,
    plans: list[CellPlan],
    out_dir: Path,
    output_size: int,
    normalize_face_size: bool = True,
    face_score_threshold: float = 0.2,
    pad_color: tuple[int, int, int] = DEFAULT_PAD_COLOR,
    debug_detections: bool = False,
) -> list[tuple[str, Any]]:
    generated: list[tuple[str, Any]] = []
    cropped_cells: list[tuple[str, Any]] = [
        (plan.filename, flatten_to_rgb(source_image.crop(plan.box)))
        for plan in plans
    ]
    if normalize_face_size:
        cropped_cells = normalize_cropped_face_sizes(
            cropped_cells,
            face_score_threshold,
            pad_color,
            out_dir / "debug_face_detections.png" if debug_detections else None,
        )
    elif debug_detections:
        debug_cells = [
            (
                filename,
                image,
                detect_face_bbox_anime(image, face_score_threshold),
            )
            for filename, image in cropped_cells
        ]
        write_detection_debug_image(debug_cells, out_dir / "debug_face_detections.png")

    for filename, cropped_cell in cropped_cells:
        face_image = cropped_cell.resize(
            (output_size, output_size),
            high_quality_resample(),
        )
        face_image = finalize_output_image(face_image)
        face_image.save(out_dir / filename, format="PNG")
        generated.append((filename, face_image))
    return generated


def apply_copy_rules(copy_rules: Iterable[CopyRule], out_dir: Path) -> None:
    for rule in copy_rules:
        source_path = out_dir / rule.source
        dest_path = out_dir / rule.dest
        if not source_path.exists():
            raise BuildFacesError(
                f"copy source does not exist for {rule.dest}: {rule.source}"
            )
        shutil.copy2(source_path, dest_path)


def validate_outputs(expected_names: list[str], out_dir: Path, output_size: int) -> None:
    require_pillow()
    missing = [name for name in expected_names if not (out_dir / name).exists()]
    if missing:
        raise BuildFacesError("missing output files: " + ", ".join(missing))

    bad_images: list[str] = []
    for name in expected_names:
        path = out_dir / name
        try:
            with Image.open(path) as image:
                if image.format != "PNG" or image.size != (output_size, output_size):
                    bad_images.append(f"{name} ({image.format}, {image.size[0]}x{image.size[1]})")
        except (OSError, UnidentifiedImageError) as exc:
            bad_images.append(f"{name} ({exc})")

    if bad_images:
        raise BuildFacesError("invalid output images: " + ", ".join(bad_images))


def print_dry_run(
    input_path: Path,
    image_size: int,
    out_dir: Path,
    margin: float,
    plans: list[CellPlan],
    copy_rules: list[CopyRule],
    detect_grid: bool,
) -> None:
    print(f"Input: {input_path} ({image_size}x{image_size})")
    print(f"Output directory: {out_dir}")
    if detect_grid:
        print("Crop mode: detected grid lines")
    else:
        scaled_margin = scaled_margin_pixels(image_size, margin)
        print("Crop mode: uniform grid")
        print(f"Scaled margin: {scaled_margin:.3f}px (from --margin {margin:g})")
    print("Crop plan:")
    for plan in plans:
        left, top, right, bottom = plan.box
        print(
            f"  {plan.index:02d} row={plan.row} col={plan.col} "
            f"box=({left},{top},{right},{bottom}) -> {plan.filename}"
        )
    if copy_rules:
        print("Copy completion rules:")
        for rule in copy_rules:
            print(f"  {rule.dest} <- {rule.source}")
    print("Dry run complete; no files written.")


def run(args: argparse.Namespace) -> int:
    mapping = load_mapping(args.mapping_json, EXPECTED_CELL_COUNT)
    copy_rules = load_copy_rules(args.copy_json)
    pad_color = parse_rgb_color(args.pad_color)
    validate_grid(args.cols, args.rows, len(mapping))
    output_dir = args.install_to if args.install_to is not None else args.out

    source_image = open_source_image(args.input)
    image_size = source_image.size[0]
    if args.detect_grid:
        boxes = build_grid_detected_crop_boxes(
            source_image,
            args.cols,
            args.rows,
            args.grid_line_padding,
            args.cell_scale,
            args.align_face_crop,
            args.face_score_threshold,
            args.face_align_max_shift,
            mapping,
        )
    else:
        boxes = build_crop_boxes(image_size, args.cols, args.rows, args.margin)
    plans = build_cell_plans(mapping, boxes, args.cols)

    if args.dry_run:
        print_dry_run(
            args.input,
            image_size,
            output_dir,
            args.margin,
            plans,
            copy_rules,
            args.detect_grid,
        )
        return 0

    if args.backup_existing:
        backup_path = backup_existing_pngs(output_dir, args.backup_dir)
        if backup_path is not None:
            print(f"Backed up existing PNG files to {backup_path}")

    output_dir.mkdir(parents=True, exist_ok=True)
    if args.clean:
        deleted = clean_output_dir(output_dir)
        print(f"Deleted {deleted} existing PNG files from {output_dir}")

    generated_faces = save_generated_faces(
        source_image,
        plans,
        output_dir,
        args.output_size,
        args.normalize_face_size,
        args.face_score_threshold,
        pad_color,
        args.debug_detections,
    )
    if len(generated_faces) != EXPECTED_CELL_COUNT:
        raise BuildFacesError(
            f"expected {EXPECTED_CELL_COUNT} generated faces, got {len(generated_faces)}"
        )

    expected_names = list(dict.fromkeys(mapping + [rule.dest for rule in copy_rules]))
    apply_copy_rules(copy_rules, output_dir)
    validate_outputs(expected_names, output_dir, args.output_size)

    print(f"Generated {len(expected_names)} face PNG files in {output_dir}")
    return 0


def main(argv: list[str] | None = None) -> int:
    try:
        return run(parse_args(argv))
    except BuildFacesError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
