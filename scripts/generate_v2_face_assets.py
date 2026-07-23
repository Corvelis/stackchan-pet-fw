#!/usr/bin/env python3
"""Generate target-specific v2 face directories from canonical animation frames.

The command intentionally writes to ``face_assets_v2_work/`` by default. It
does not install files into the repository's runtime ``data*`` directories.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

try:
    from PIL import Image, ImageOps
except ImportError:  # pragma: no cover - depends on the local tool setup.
    Image = None  # type: ignore[assignment]
    ImageOps = None  # type: ignore[assignment]

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


DEFAULT_OUTPUT_ROOT = ROOT / "face_assets_v2_work" / "generated"
SOURCE_SUFFIXES = (".png", ".webp", ".jpg", ".jpeg", ".bmp")
STOPWATCH_DIRECTION_SOURCES = (*range(0, 16, 2), 16)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate CoreS3, StopWatch, and AtomS3R v2 JPG sets from canonical "
            "17-direction animation frames."
        ),
    )
    parser.add_argument(
        "source",
        type=Path,
        help=(
            "Directory containing base_m0_e0..base_m3_e3, pet_anim_0..15, "
            "dir0..16, blink16, and dizzy_01..15 source images."
        ),
    )
    parser.add_argument(
        "targets",
        nargs="*",
        choices=(*TARGETS.keys(), "all"),
        default=["all"],
        help="Targets to generate. Defaults to all.",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=DEFAULT_OUTPUT_ROOT,
        help="Parent directory for generated target folders.",
    )
    parser.add_argument("--quality", type=int, default=82, help="JPEG quality. Defaults to 82.")
    parser.add_argument(
        "--replace",
        action="store_true",
        help="Replace an existing generated target directory after the new set validates.",
    )
    return parser.parse_args()


def expand_targets(targets: list[str]) -> list[TargetSpec]:
    if not targets or "all" in targets:
        return list(TARGETS.values())
    return [TARGETS[name] for name in targets]


def canonical_source_names() -> set[str]:
    return expected_v2_files(TARGETS["cores3"])


def collect_sources(source_dir: Path) -> dict[str, Path]:
    if not source_dir.is_dir():
        raise RuntimeError(f"source directory does not exist: {source_dir}")

    selected: dict[str, Path] = {}
    missing: list[str] = []
    for output_name in sorted(canonical_source_names()):
        stem = Path(output_name).stem
        candidates = [source_dir / f"{stem}{suffix}" for suffix in SOURCE_SUFFIXES]
        source = next((candidate for candidate in candidates if candidate.is_file()), None)
        if source is None:
            missing.append(stem)
        else:
            selected[stem] = source
    if missing:
        raise RuntimeError("missing canonical source frames: " + ", ".join(missing))
    return selected


def source_stem_for_output(target: TargetSpec, output_name: str) -> str:
    stem = Path(output_name).stem
    if target.name != "stopwatch":
        return stem
    if stem.startswith("dir"):
        output_index = int(stem.removeprefix("dir"))
        return f"dir{STOPWATCH_DIRECTION_SOURCES[output_index]}"
    if stem == "blink8":
        return "blink16"
    return stem


def prepare_image(source: Path, size: tuple[int, int]) -> Image.Image:
    if Image is None or ImageOps is None:  # pragma: no cover - checked by main.
        raise RuntimeError("Pillow is not available")
    with Image.open(source) as opened:
        image = ImageOps.exif_transpose(opened).convert("RGBA")
    if image.size != size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    background = Image.new("RGBA", image.size, (0, 0, 0, 255))
    return Image.alpha_composite(background, image).convert("RGB")


def write_target(
    sources: dict[str, Path],
    target: TargetSpec,
    output_root: Path,
    quality: int,
    replace: bool,
) -> Path:
    destination = output_root / target.name
    if destination.exists() and not replace:
        raise RuntimeError(f"output already exists (pass --replace): {destination}")

    output_root.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{target.name}-", dir=output_root))
    try:
        for output_name in sorted(expected_v2_files(target)):
            source_stem = source_stem_for_output(target, output_name)
            source = sources[source_stem]
            image = prepare_image(source, (target.width, target.height))
            image.save(
                staging / output_name,
                "JPEG",
                quality=quality,
                optimize=True,
                subsampling=0,
            )

        manifest = build_v2_manifest(target)
        manifest["generatedBy"] = "scripts/generate_v2_face_assets.py"
        (staging / MANIFEST_NAME).write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        validation = validate_directory(staging, target, "v2")
        if not validation.ok:
            raise RuntimeError("generated set failed validation: " + "; ".join(validation.errors))

        if destination.exists():
            shutil.rmtree(destination)
        staging.replace(destination)
        print(
            f"{target.name}: wrote {validation.checked_files} images and {MANIFEST_NAME} "
            f"to {destination}"
        )
        return destination
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise


def main() -> int:
    args = parse_args()
    if Image is None or ImageOps is None:
        raise SystemExit(
            "Pillow is required. Install it with: "
            "python -m pip install -r "
            "tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt"
        )
    if not 1 <= args.quality <= 100:
        raise SystemExit("--quality must be between 1 and 100")

    source_dir = args.source.expanduser().resolve()
    output_root = args.output_root.expanduser().resolve()
    if output_root in {ROOT, source_dir}:
        raise SystemExit("--output-root must not be the repository root or source directory")

    protected = {(ROOT / spec.directory).resolve() for spec in TARGETS.values()}
    if output_root in protected or any(parent in protected for parent in output_root.parents):
        raise SystemExit("refusing to generate inside a runtime data directory")

    try:
        sources = collect_sources(source_dir)
        for target in expand_targets(args.targets):
            write_target(sources, target, output_root, args.quality, args.replace)
    except RuntimeError as exc:
        raise SystemExit(str(exc)) from exc
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
