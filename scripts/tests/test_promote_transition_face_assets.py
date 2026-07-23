from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts import promote_transition_face_assets as promoter
from scripts import validate_face_assets as validator


class PromoteTransitionFaceAssetsTest(unittest.TestCase):
    def write_transition_set(self, directory: Path, target) -> None:
        directory.mkdir(parents=True)
        for index, name in enumerate(sorted(validator.expected_transition_files())):
            color = (index % 255, (index * 3) % 255, (index * 7) % 255)
            Image.new("RGB", (target.width, target.height), color).save(
                directory / name,
                "JPEG",
                quality=80,
            )

    def test_promotes_complete_set_without_recompressing_base_frame(self) -> None:
        target = validator.TARGETS["atoms3r"]
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / target.directory
            self.write_transition_set(source, target)
            expected_bytes = (source / "voice_m0_e0.jpg").read_bytes()

            output = promoter.promote_target(root, root / "out", target, False)

            self.assertEqual(expected_bytes, (output / "base_m0_e0.jpg").read_bytes())
            result = validator.validate_directory(output, target, "v2")
            self.assertTrue(result.ok, result.errors)
            manifest = json.loads((output / validator.MANIFEST_NAME).read_text())
            self.assertEqual("scripts/promote_transition_face_assets.py", manifest["generatedBy"])

    def test_stopwatch_maps_eight_directions_and_center(self) -> None:
        target = validator.TARGETS["stopwatch"]
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / target.directory
            self.write_transition_set(source, target)

            output = promoter.promote_target(root, root / "out", target, False)

            for output_index, source_index in enumerate(promoter.STOPWATCH_DIRECTION_SOURCES):
                self.assertEqual(
                    (source / f"dir{source_index}.jpg").read_bytes(),
                    (output / f"dir{output_index}.jpg").read_bytes(),
                )
            self.assertEqual(
                (source / "blink16.jpg").read_bytes(),
                (output / "blink8.jpg").read_bytes(),
            )


if __name__ == "__main__":
    unittest.main()
