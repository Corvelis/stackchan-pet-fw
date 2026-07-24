from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover - CI installs Pillow; import-only users may not.
    Image = None

from scripts.generate_v2_face_assets import (
    STOPWATCH_DIRECTION_SOURCES,
    canonical_source_names,
    collect_sources,
    source_stem_for_output,
    write_target,
)
from scripts.validate_face_assets import TARGETS, validate_directory


class GenerateV2FaceAssetsTest(unittest.TestCase):
    def test_canonical_source_has_cores3_profile(self) -> None:
        names = canonical_source_names()
        self.assertEqual(len(names), 65)
        self.assertIn("base_m3_e3.jpg", names)
        self.assertIn("dir16.jpg", names)
        self.assertIn("blink16.jpg", names)

    def test_stopwatch_directions_use_eight_radial_frames_and_center(self) -> None:
        target = TARGETS["stopwatch"]
        mapped = [source_stem_for_output(target, f"dir{index}.jpg") for index in range(9)]
        self.assertEqual(
            mapped,
            [f"dir{index}" for index in STOPWATCH_DIRECTION_SOURCES],
        )

    def test_stopwatch_center_blink_uses_canonical_center(self) -> None:
        target = TARGETS["stopwatch"]
        self.assertEqual(source_stem_for_output(target, "blink8.jpg"), "blink16")

    def test_other_targets_keep_logical_names(self) -> None:
        for target_name in ("cores3", "atoms3r"):
            target = TARGETS[target_name]
            self.assertEqual(source_stem_for_output(target, "dir7.jpg"), "dir7")
            self.assertEqual(source_stem_for_output(target, "blink16.jpg"), "blink16")

    @unittest.skipUnless(Image is not None, "Pillow is required for the generator integration test")
    def test_generated_directory_passes_v2_validation(self) -> None:
        target = TARGETS["atoms3r"]
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source"
            source.mkdir()
            for name in canonical_source_names():
                assert Image is not None
                Image.new("RGB", (8, 8), (20, 40, 60)).save(source / name, "JPEG")

            destination = write_target(
                collect_sources(source),
                target,
                root / "generated",
                quality=82,
                replace=False,
            )
            result = validate_directory(destination, target, "v2")

        self.assertTrue(result.ok, result.errors)
        self.assertEqual(result.checked_files, 65)


if __name__ == "__main__":
    unittest.main()
