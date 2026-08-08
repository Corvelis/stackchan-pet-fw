from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = (
    ROOT
    / "tools"
    / "face_image_builder"
    / "build_faces_from_sprite_sheet"
    / "split_firmware_sheet.py"
)
TRAVEL_PICKER_SCRIPT = (
    ROOT
    / "tools"
    / "face_image_builder"
    / "build_faces_from_sprite_sheet"
    / "build_travel_picker_pages.py"
)


class SplitFirmwareSheetTest(unittest.TestCase):
    def test_travel_picker_pages_match_target_dimensions_and_slot_layouts(self) -> None:
        source_names = (
            "pet_anim_8.jpg",
            "pet_anim_10.jpg",
            "travel_wink.jpg",
            "travel_sparkle.jpg",
            "travel_surprised.jpg",
            "travel_shy.jpg",
            "travel_delicious.jpg",
            "travel_peace.jpg",
            "dizzy_01.jpg",
            "dizzy_09.jpg",
            "pet_anim_13.jpg",
            "pet_anim_14.jpg",
            "travel_mischief.jpg",
            "travel_teary.jpg",
            "travel_yawn.jpg",
        )
        targets = {
            "cores3": ((320, 240), (44, 62)),
            "stopwatch": ((386, 386), (193, 52)),
        }

        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            assets = work / "assets"
            assets.mkdir()
            for index, name in enumerate(source_names):
                Image.new("RGB", (48, 48), (80 + index, 120, 160)).save(assets / name)

            for target, (expected_size, first_center) in targets.items():
                output = work / target
                result = subprocess.run(
                    [
                        sys.executable,
                        str(TRAVEL_PICKER_SCRIPT),
                        str(assets),
                        "--target",
                        target,
                        "--output-dir",
                        str(output),
                        "--quality",
                        "100",
                    ],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(
                    {path.name for path in output.iterdir()},
                    {"travel_picker_page_0.jpg", "travel_picker_page_1.jpg"},
                )
                with Image.open(output / "travel_picker_page_0.jpg") as page:
                    self.assertEqual(page.size, expected_size)
                    pixel = page.getpixel(first_center)
                    self.assertGreater(pixel[0], 70)
                    self.assertGreater(pixel[1], 110)
                    self.assertGreater(pixel[2], 150)

    def test_tracked_v2_samples_split_into_sixteen_frames(self) -> None:
        samples = (
            (
                ROOT
                / "tools"
                / "face_image_builder"
                / "build_faces_from_sprite_sheet"
                / "samples"
                / "base_animation_4x4"
                / "sprite_sheet_sample_01.jpg",
                "base_",
                [
                    "--output-naming",
                    "base-mouth-eye",
                    "--column-x-offsets=-11,-11,-9,0",
                ],
                {f"base_m{mouth}_e{eye}.png" for mouth in range(4) for eye in range(4)},
            ),
            (
                ROOT
                / "tools"
                / "face_image_builder"
                / "build_faces_from_sprite_sheet"
                / "samples"
                / "petting_4x4"
                / "sprite_sheet_sample_01.jpg",
                "pet_anim_",
                [],
                {f"pet_anim_{index}.png" for index in range(16)},
            ),
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            for sample_index, (sheet, prefix, naming_args, expected_names) in enumerate(samples):
                output = work / str(sample_index)
                result = subprocess.run(
                    [
                        sys.executable,
                        str(SCRIPT),
                        "--sheet",
                        f"{sheet}:{prefix}",
                        "--grid",
                        "4x4",
                        "--directions",
                        "16",
                        "--layout",
                        "even",
                        "--crop-size",
                        "auto",
                        "--row-top-mask",
                        "6",
                        "--column-side-mask",
                        "6",
                        "--size",
                        "32",
                        "--format",
                        "png",
                        *naming_args,
                        "--out-dir",
                        str(output),
                    ],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual({path.name for path in output.iterdir()}, expected_names)

    def test_base_animation_names_follow_mouth_rows_and_eye_columns(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            sheet = work / "base_animation.png"
            output = work / "canonical"
            preview = work / "previews"

            image = Image.new("RGB", (160, 160), "black")
            draw = ImageDraw.Draw(image)
            colors: dict[tuple[int, int], tuple[int, int, int]] = {}
            for mouth in range(4):
                for eye in range(4):
                    color = (40 + mouth * 45, 40 + eye * 45, 80)
                    colors[(mouth, eye)] = color
                    draw.rectangle(
                        (eye * 40, mouth * 40, (eye + 1) * 40 - 1, (mouth + 1) * 40 - 1),
                        fill=color,
                    )
            image.save(sheet)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sheet",
                    f"{sheet}:base_",
                    "--grid",
                    "4x4",
                    "--directions",
                    "16",
                    "--layout",
                    "even",
                    "--crop-size",
                    "auto",
                    "--size",
                    "32",
                    "--format",
                    "png",
                    "--output-naming",
                    "base-mouth-eye",
                    "--out-dir",
                    str(output),
                    "--preview-dir",
                    str(preview),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            expected_names = {
                f"base_m{mouth}_e{eye}.png"
                for mouth in range(4)
                for eye in range(4)
            }
            self.assertEqual({path.name for path in output.iterdir()}, expected_names)
            for mouth in range(4):
                for eye in range(4):
                    with Image.open(output / f"base_m{mouth}_e{eye}.png") as frame:
                        self.assertEqual(frame.getpixel((16, 16)), colors[(mouth, eye)])
            self.assertTrue((preview / "base_preview.jpg").is_file())

    def test_base_animation_naming_rejects_the_wrong_grid(self) -> None:
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--grid",
                "3x3",
                "--directions",
                "9",
                "--output-naming",
                "base-mouth-eye",
                "--dry-run",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires --grid 4x4 --directions 16", result.stderr)

    def test_petting_animation_is_exported_in_row_major_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            sheet = work / "petting_4x4.png"
            output = work / "canonical"

            image = Image.new("RGB", (80, 80), "black")
            draw = ImageDraw.Draw(image)
            colors: list[tuple[int, int, int]] = []
            for index in range(16):
                row, col = divmod(index, 4)
                color = (32 + index * 10, 48 + index * 6, 96)
                colors.append(color)
                draw.rectangle(
                    (col * 20, row * 20, (col + 1) * 20 - 1, (row + 1) * 20 - 1),
                    fill=color,
                )
            image.save(sheet)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sheet",
                    f"{sheet}:pet_anim_",
                    "--grid",
                    "4x4",
                    "--directions",
                    "16",
                    "--layout",
                    "even",
                    "--crop-size",
                    "auto",
                    "--size",
                    "20",
                    "--format",
                    "png",
                    "--out-dir",
                    str(output),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                {path.name for path in output.iterdir()},
                {f"pet_anim_{index}.png" for index in range(16)},
            )
            for index, color in enumerate(colors):
                with Image.open(output / f"pet_anim_{index}.png") as frame:
                    self.assertEqual(frame.getpixel((10, 10)), color)

    def test_travel_sheet_uses_canonical_expression_names_in_row_major_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            sheet = work / "travel_3x3.png"
            output = work / "canonical"

            image = Image.new("RGB", (90, 90), "black")
            draw = ImageDraw.Draw(image)
            colors: list[tuple[int, int, int]] = []
            for index in range(9):
                row, col = divmod(index, 3)
                color = (32 + index * 18, 48 + index * 12, 96)
                colors.append(color)
                draw.rectangle(
                    (col * 30, row * 30, (col + 1) * 30 - 1, (row + 1) * 30 - 1),
                    fill=color,
                )
            image.save(sheet)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sheet",
                    f"{sheet}:travel_",
                    "--grid",
                    "3x3",
                    "--directions",
                    "9",
                    "--layout",
                    "even",
                    "--crop-size",
                    "auto",
                    "--size",
                    "30",
                    "--format",
                    "png",
                    "--output-naming",
                    "travel-expressions",
                    "--out-dir",
                    str(output),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            names = (
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
            self.assertEqual(
                {path.name for path in output.iterdir()},
                {f"{name}.png" for name in names},
            )
            for index, name in enumerate(names):
                with Image.open(output / f"{name}.png") as frame:
                    self.assertEqual(frame.getpixel((15, 15)), colors[index])

    def test_travel_expression_naming_rejects_the_wrong_grid(self) -> None:
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--grid",
                "4x4",
                "--directions",
                "16",
                "--output-naming",
                "travel-expressions",
                "--dry-run",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires --grid 3x3 --directions 9", result.stderr)

    def test_row_top_mask_removes_previous_row_fragment_without_zooming(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            sheet = work / "row_fragments.png"
            output = work / "frames"

            image = Image.new("RGB", (20, 40), "black")
            draw = ImageDraw.Draw(image)
            draw.rectangle((0, 0, 19, 2), fill="red")
            draw.rectangle((4, 8, 15, 15), fill="blue")
            draw.rectangle((0, 20, 19, 22), fill="red")
            draw.rectangle((4, 28, 15, 35), fill="blue")
            image.save(sheet)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sheet",
                    f"{sheet}:frame_",
                    "--grid",
                    "2x1",
                    "--directions",
                    "2",
                    "--layout",
                    "even",
                    "--crop-size",
                    "auto",
                    "--size",
                    "20",
                    "--format",
                    "png",
                    "--row-top-mask",
                    "3",
                    "--out-dir",
                    str(output),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            with Image.open(output / "frame_0.png") as first:
                self.assertGreater(first.getpixel((10, 1))[0], 240)
                self.assertGreater(first.getpixel((10, 10))[2], 240)
            with Image.open(output / "frame_1.png") as second:
                self.assertEqual(second.getpixel((10, 1)), (0, 0, 0))
                self.assertGreater(second.getpixel((10, 10))[2], 240)

    def test_column_side_mask_removes_internal_boundary_fragments(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            sheet = work / "column_fragments.png"
            output = work / "frames"

            image = Image.new("RGB", (40, 20), "black")
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 4, 15, 15), fill="blue")
            draw.rectangle((17, 0, 22, 19), fill="red")
            draw.rectangle((24, 4, 31, 15), fill="blue")
            image.save(sheet)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sheet",
                    f"{sheet}:frame_",
                    "--grid",
                    "1x2",
                    "--directions",
                    "2",
                    "--layout",
                    "even",
                    "--crop-size",
                    "auto",
                    "--size",
                    "20",
                    "--format",
                    "png",
                    "--column-side-mask",
                    "3",
                    "--out-dir",
                    str(output),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            with Image.open(output / "frame_0.png") as first:
                self.assertEqual(first.getpixel((19, 10)), (0, 0, 0))
                self.assertGreater(first.getpixel((10, 10))[2], 240)
            with Image.open(output / "frame_1.png") as second:
                self.assertEqual(second.getpixel((0, 10)), (0, 0, 0))
                self.assertGreater(second.getpixel((10, 10))[2], 240)

    def test_column_x_offsets_translate_without_wrapping(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            work = Path(temp_dir)
            sheet = work / "column_offsets.png"
            output = work / "frames"

            image = Image.new("RGB", (40, 20), "black")
            draw = ImageDraw.Draw(image)
            draw.rectangle((8, 6, 12, 14), fill="blue")
            draw.rectangle((28, 6, 32, 14), fill="blue")
            image.save(sheet)

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--sheet",
                    f"{sheet}:frame_",
                    "--grid",
                    "1x2",
                    "--directions",
                    "2",
                    "--layout",
                    "even",
                    "--crop-size",
                    "auto",
                    "--size",
                    "20",
                    "--format",
                    "png",
                    "--column-x-offsets=-2,3",
                    "--out-dir",
                    str(output),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

            with Image.open(output / "frame_0.png") as first:
                self.assertGreater(first.getpixel((8, 10))[2], 240)
                self.assertEqual(first.getpixel((12, 10)), (0, 0, 0))
                self.assertEqual(first.getpixel((19, 10)), (0, 0, 0))
            with Image.open(output / "frame_1.png") as second:
                self.assertGreater(second.getpixel((13, 10))[2], 240)
                self.assertEqual(second.getpixel((8, 10)), (0, 0, 0))
                self.assertEqual(second.getpixel((0, 10)), (0, 0, 0))


if __name__ == "__main__":
    unittest.main()
