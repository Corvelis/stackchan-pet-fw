#!/usr/bin/env python3
"""Download the anime face detection model used by build_faces.py."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
from urllib.error import HTTPError, URLError
from urllib.request import urlopen


TOOL_DIR = Path(__file__).resolve().parent
DEFAULT_MODEL_PATH = TOOL_DIR / "models" / "anime_face_detect_v1.4_n.onnx"
MODEL_URL = (
    "https://huggingface.co/deepghs/anime_face_detection/resolve/main/"
    "face_detect_v1.4_n/model.onnx"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download the anime face detection ONNX model."
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=DEFAULT_MODEL_PATH,
        help="Destination ONNX path.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Download even if the destination file already exists.",
    )
    return parser.parse_args()


def download(url: str, out_path: Path, force: bool) -> None:
    if out_path.exists() and not force:
        print(f"Model already exists: {out_path}")
        return

    out_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = out_path.with_suffix(out_path.suffix + ".tmp")
    try:
        with urlopen(url, timeout=60) as response, temp_path.open("wb") as handle:
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                handle.write(chunk)
        temp_path.replace(out_path)
    except (HTTPError, URLError, TimeoutError, OSError) as exc:
        if temp_path.exists():
            temp_path.unlink()
        raise RuntimeError(f"failed to download model from {url}: {exc}") from exc

    print(f"Downloaded model: {out_path}")


def main() -> int:
    args = parse_args()
    try:
        download(MODEL_URL, args.out, args.force)
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
