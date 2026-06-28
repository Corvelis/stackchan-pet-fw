# Stack-chan Face Image Builder

[Japanese](README.md)

This directory contains tools for building face image sets for Stack-chan pet firmware.
It covers the workflow from AI-generated sprite sheets to the 106 JPG firmware
assets used by each device.

## Workflow

1. Use the prompt and related files in `generate_sprite_sheet/` to generate a 6x6 sprite sheet with an image generation AI.
2. Use the prompts in `animation_prompts/` to generate additional guruguru, guruguru blink, petting, and dizzy sheets.
3. Use the CLI in `build_faces_from_sprite_sheet/` to split the base and animation sheets.
4. Standardize the generated files to `.jpg` and install them into the target device's firmware image directory.
5. Upload the LittleFS image with PlatformIO.

Use separate output directories for normal release images and local replacement images.

| Purpose | CoreS3 | StopWatch | AtomS3R |
| --- | --- | --- | --- |
| Normal release set | `data/` | `data_stopwatch/` | `data_atoms3r/` |
| Local replacement set | `data_local/` | `data_stopwatch_local/` | `data_atoms3r_local/` |

GitHub Release assets use the normal release directories. Local replacement directories are not committed.

## Quick Start

Install Python dependencies.

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

The face alignment model is downloaded automatically on the first run.

Generate the 48 base faces into a working directory, then install them into
CoreS3 `data/` as firmware-ready JPG files.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces data --target cores3 --format jpg --clean
```

For StopWatch or AtomS3R Chatbot, change `--output-size`, `--target`, and the
output directory.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_stopwatch --detect-grid --clean --output-size 386
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_stopwatch data_stopwatch --target stopwatch --format jpg --clean

python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_atoms3r --detect-grid --clean --output-size 128
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_atoms3r data_atoms3r --target atoms3r --format jpg --clean
```

StopWatch and AtomS3R Chatbot image directories can also be generated from the
existing CoreS3 `data/` directory.

```bash
python3 scripts/generate_device_assets.py all
```

Generate one target at a time:

```bash
python3 scripts/generate_device_assets.py stopwatch
python3 scripts/generate_device_assets.py atoms3r
```

Use `--source` when generating from another CoreS3 image directory.

```bash
python3 scripts/generate_device_assets.py all --source path/to/core-data
```

Use `generate_local_face_assets.py` to rebuild the three ignored local replacement
directories from a backup source.

```bash
python3 scripts/generate_local_face_assets.py path/to/backup-data all
```

Use `prepare_firmware_assets.py` when an existing firmware image directory should
share the same image size and physical file format. It converts only top-level
firmware face, pet, and dizzy frames, and leaves source materials such as WebP
files in place.

```bash
python3 tools/face_image_builder/prepare_firmware_assets.py data_local --target cores3 --format jpg --in-place
python3 tools/face_image_builder/prepare_firmware_assets.py data_stopwatch_local --target stopwatch --format jpg --in-place
python3 tools/face_image_builder/prepare_firmware_assets.py data_atoms3r_local --target atoms3r --format jpg --in-place
```

Use `build_faces_from_sprite_sheet/split_firmware_sheet.py` to split sprite sheets or contact sheets into
firmware-ready files using the same separator approach as `M5-guruguru`.
Use `--format jpg` for firmware assets. QOI is not used.

5x5 guruguru `dir` / `blink` sheets:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet sheet.png:dir --sheet blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

3x3 petting animation sheet:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

First 15 frames from a 4x4 dizzy/contact sheet for this firmware:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

Upload LittleFS. Replace `<env>` with the target PlatformIO environment.

```bash
pio run -e <env> -t uploadfs
```

Select another image directory at PlatformIO build time when flashing a local set.

```bash
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
```

## Directory Layout

```text
tools/face_image_builder/
  README.md
  README.en.md
  prepare_firmware_assets.py
  sprite_sheets/
    README.md
    base_faces/
    guruguru/
    petting/
    dizzy/
  generate_sprite_sheet/
    README.md
    README.en.md
    sprite_sheet_prompt.txt
    grid_template_6x6.png
    animation_prompts/
      README.md
      guruguru_5x5_prompt.txt
      guruguru_blink_5x5_prompt.txt
      petting_3x3_prompt.txt
      dizzy_4x4_prompt.txt
    references/
  build_faces_from_sprite_sheet/
    README.md
    README.en.md
    build_faces.py
    split_firmware_sheet.py
    download_model.py
    requirements.txt
    face_mapping.json
    face_copy_rules.json
    models/
    samples/
      base_faces_6x6/
      guruguru_dir_5x5/
      guruguru_blink_5x5/
      petting_3x3/
      dizzy_4x4/
```

## Details

Refer to `generate_sprite_sheet/README.en.md` for the image generation workflow in English.

Refer to `build_faces_from_sprite_sheet/README.en.md` for the sprite sheet splitting CLI in English.
