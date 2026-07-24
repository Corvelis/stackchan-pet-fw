from __future__ import annotations

import json
import struct
import tempfile
import unittest
from pathlib import Path

from scripts import validate_face_assets as validator


def write_test_jpeg(path: Path, width: int, height: int) -> None:
    """Write the smallest JPEG-shaped fixture needed by the header validator."""
    sof_payload = (
        bytes([8])
        + struct.pack(">HH", height, width)
        + bytes([3, 1, 0x11, 0, 2, 0x11, 0, 3, 0x11, 0])
    )
    path.write_bytes(
        b"\xff\xd8"
        + b"\xff\xc0"
        + struct.pack(">H", len(sof_payload) + 2)
        + sof_payload
        + b"\xff\xd9"
    )


def populate(directory: Path, names: set[str], width: int, height: int) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for name in names:
        write_test_jpeg(directory / name, width, height)


class ValidateFaceAssetsTest(unittest.TestCase):
    def test_complete_transition_set_passes(self) -> None:
        target = validator.TARGETS["atoms3r"]
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "data_atoms3r"
            populate(
                directory,
                validator.expected_transition_files(),
                target.width,
                target.height,
            )

            result = validator.validate_directory(directory, target, "auto")

        self.assertTrue(result.ok, result.errors)
        self.assertEqual(result.mode, "transition")
        self.assertEqual(result.checked_files, 129)
        self.assertTrue(result.warnings)

    def test_transition_set_rejects_a_missing_image(self) -> None:
        target = validator.TARGETS["cores3"]
        names = validator.expected_transition_files()
        names.remove("talk_1.jpg")
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "data"
            populate(directory, names, target.width, target.height)

            result = validator.validate_directory(directory, target, "transition")

        self.assertFalse(result.ok)
        self.assertTrue(any("talk_1.jpg" in error for error in result.errors))

    def test_complete_v2_set_and_manifest_pass(self) -> None:
        target = validator.TARGETS["stopwatch"]
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "data_stopwatch"
            populate(
                directory,
                validator.expected_v2_files(target),
                target.width,
                target.height,
            )
            (directory / validator.MANIFEST_NAME).write_text(
                json.dumps(validator.build_v2_manifest(target)),
                encoding="utf-8",
            )

            result = validator.validate_directory(directory, target, "auto")

        self.assertTrue(result.ok, result.errors)
        self.assertEqual(result.mode, "v2")
        self.assertEqual(result.checked_files, 57)
        self.assertFalse(result.warnings)

    def test_partial_v2_does_not_fall_back_to_transition(self) -> None:
        target = validator.TARGETS["cores3"]
        names = validator.expected_v2_files(target)
        names.remove("base_m3_e3.jpg")
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "data"
            populate(directory, names, target.width, target.height)
            (directory / validator.MANIFEST_NAME).write_text(
                json.dumps(validator.build_v2_manifest(target)),
                encoding="utf-8",
            )

            result = validator.validate_directory(directory, target, "auto")

        self.assertEqual(result.mode, "v2")
        self.assertFalse(result.ok)
        self.assertTrue(any("base_m3_e3.jpg" in error for error in result.errors))

    def test_v2_manifest_rejects_wrong_target(self) -> None:
        target = validator.TARGETS["atoms3r"]
        manifest = validator.build_v2_manifest(target)
        manifest["target"] = "cores3"
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "data_atoms3r"
            populate(
                directory,
                validator.expected_v2_files(target),
                target.width,
                target.height,
            )
            (directory / validator.MANIFEST_NAME).write_text(
                json.dumps(manifest),
                encoding="utf-8",
            )

            result = validator.validate_directory(directory, target, "v2")

        self.assertFalse(result.ok)
        self.assertTrue(any("manifest.target" in error for error in result.errors))

    def test_legacy_minimal_allows_additional_old_images(self) -> None:
        target = validator.TARGETS["cores3"]
        names = validator.expected_legacy_minimal_files() | {"good_0.jpg"}
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "legacy"
            populate(directory, names, target.width, target.height)

            result = validator.validate_directory(directory, target, "legacy-minimal")

        self.assertTrue(result.ok, result.errors)
        self.assertEqual(result.checked_files, 5)

    def test_wrong_image_dimensions_fail(self) -> None:
        target = validator.TARGETS["stopwatch"]
        names = validator.expected_legacy_minimal_files()
        with tempfile.TemporaryDirectory() as temp_dir:
            directory = Path(temp_dir) / "legacy"
            populate(directory, names, target.width, target.height)
            write_test_jpeg(directory / "blink.jpg", 240, 240)

            result = validator.validate_directory(directory, target, "legacy-minimal")

        self.assertFalse(result.ok)
        self.assertTrue(any("expected 386x386" in error for error in result.errors))


if __name__ == "__main__":
    unittest.main()
