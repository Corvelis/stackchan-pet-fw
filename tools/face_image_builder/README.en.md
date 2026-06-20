# Stack-chan Face Image Builder

[Japanese](README.md)

This directory contains tools for building face image sets for Stack-chan pet firmware.

## Workflow

1. Use the prompt and related files in `generate_sprite_sheet/` to generate a 6x6 sprite sheet with an image generation AI.
2. Use the CLI in `build_faces_from_sprite_sheet/` to split the sprite sheet into 48 PNG files.
3. Install the generated PNG files into the target device's firmware image directory.
4. Upload the LittleFS image with PlatformIO.

## Quick Start

Install Python dependencies.

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

The face alignment model is downloaded automatically on the first run.

Generate and install 48 face PNG files into CoreS3 `data/`.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

For StopWatch or AtomS3R Chatbot, change `--output-size` and the output directory.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data_stopwatch --backup-existing --detect-grid --clean --output-size 386
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data_atoms3r --backup-existing --detect-grid --clean --output-size 128
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

Upload LittleFS. Replace `<env>` with the target PlatformIO environment.

```bash
pio run -e <env> -t uploadfs
```

## Directory Layout

```text
tools/face_image_builder/
  README.md
  README.en.md
  generate_sprite_sheet/
    README.md
    README.en.md
    sprite_sheet_prompt.md
    grid_template_6x6.png
    references/
  build_faces_from_sprite_sheet/
    README.md
    README.en.md
    build_faces.py
    download_model.py
    requirements.txt
    face_mapping.json
    face_copy_rules.json
    models/
    samples/
```

## Details

Refer to `generate_sprite_sheet/README.en.md` for the image generation workflow in English.

Refer to `build_faces_from_sprite_sheet/README.en.md` for the sprite sheet splitting CLI in English.
