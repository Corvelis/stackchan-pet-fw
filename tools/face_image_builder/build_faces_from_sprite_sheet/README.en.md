# Stack-chan Face Image Splitter CLI

[Japanese](README.md)

`build_faces.py` is a CLI tool that crops the 48 base face images for
Stack-chan pet firmware from a square 6x6 face sprite sheet. The default output
size is 240x240 for CoreS3.

It crops 36 cells from the sprite sheet, then applies copy completion rules for
the additional files referenced by the firmware. `build_faces.py` writes PNG
files. Use `prepare_firmware_assets.py` to install them into firmware image
directories as JPG files.

Instructions for creating a sprite sheet are in `tools/face_image_builder/generate_sprite_sheet/`.

This directory is for converting an already generated sprite sheet into firmware-ready face images.

Use `build_faces.py` for the 6x6 base face sheet. Use `split_firmware_sheet.py`
for additional animation sheets such as guruguru, petting, and dizzy frames.

## First Command To Use

The execution environment is assumed to be macOS, Windows, or Ubuntu with Python 3.

For the first run, install dependencies.

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

Use these commands to split an AI-generated sprite sheet and install the result
into CoreS3 `data/` as firmware-ready JPG files.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces data --target cores3 --format jpg --clean
```

These commands crop the 36 cells, align face positions, normalize face sizes,
apply copy completion rules, and write the 48 base faces into `data/` as JPG.

With `--backup-existing` and `--install-to`, existing `*.png` files are copied
to `backups/data_faces_YYYYMMDD_HHMMSS/` before writing new files. For the final
JPG workflow, write to a working directory and install with `prepare_firmware_assets.py`.

For StopWatch or AtomS3R Chatbot, change `--output-size` and the installation
target.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_stopwatch --detect-grid --clean --output-size 386
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_stopwatch data_stopwatch --target stopwatch --format jpg --clean

python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces_atoms3r --detect-grid --clean --output-size 128
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_atoms3r data_atoms3r --target atoms3r --format jpg --clean
```

Use `split_firmware_sheet.py` for additional animation sheets.

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/guruguru/sheet.png:dir --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/petting/petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/dizzy/dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

## Additional Animation Splitting

`split_firmware_sheet.py` splits one sprite sheet into the numbered JPG files read by the firmware.
`--sheet` uses the `INPUT_IMAGE:OUTPUT_PREFIX` form. You can pass it more than once to split multiple sheets with the same settings.

| Use | Input sheet | Main options | Output files |
| --- | --- | --- | --- |
| Guruguru direction | 5x5 | `--sheet sheet.png:dir --directions 17` | `dir0.jpg` ... `dir16.jpg` |
| Guruguru blink | 5x5 | `--sheet blink.png:blink --directions 17` | `blink0.jpg` ... `blink16.jpg` |
| Petting | 3x3 | `--sheet petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even` | `pet_anim_0.jpg` ... `pet_anim_8.jpg` |
| Dizzy | 4x4 | `--sheet dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2` | `dizzy_01.jpg` ... `dizzy_15.jpg` |

The 5x5 guruguru sheets contain 25 cells, but the firmware currently uses 17 directions.
StopWatch plays an 8-direction-equivalent flow internally, but the image files are still prepared as `dir0..dir16` and `blink0..blink16` to keep the asset set aligned with the other devices.

If fragments from neighboring cells appear, first inspect the output written by `--preview-dir previews`.
Try these adjustments in order.

```bash
# Split by an even grid and crop inside each cell boundary.
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews --layout even --cell-inset 4

# Increase the inset if neighboring cell fragments are still visible.
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews --layout even --cell-inset 8
```

Use the default `--layout auto` when black-background boundary detection is stable.
For AI-generated sheets with slightly uneven grids, compressed lower rows, or neighboring-cell edges, `--layout even --cell-inset 4` is usually more stable.

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

Note: this tool uses an AI model for face alignment. The model file is downloaded automatically on the first run of `build_faces.py`.

Upload the result to LittleFS.

```bash
pio run -e <env> -t uploadfs
```

Upload the firmware itself when needed.

```bash
pio run -e <env> -t upload
```

## Check Output Before Installing

Use `--out` if you want to inspect the generated files before writing to a device image directory.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
```

If the output looks good, install it into the target device image directory with
`prepare_firmware_assets.py`.

## With a Virtual Environment

macOS / Ubuntu:

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

Windows PowerShell:

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

## Common Adjustments

If grid lines or cell-edge artifacts remain:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --grid-line-padding 14 --clean
```

If faces are cropped too tightly or have too much margin:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --cell-scale 0.94 --clean
```

If you want to change the padding color:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --pad-color 0,0,0 --clean
```

## Debugging

Print the crop plan without writing files.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --dry-run
```

Write a debug image with detected face rectangles.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --debug-detections --clean
```

This writes `debug_face_detections.png` into the output directory.

## Samples

`samples/` contains example sprite sheets that can be used as CLI inputs.
Use the following five-purpose directory layout.

```text
samples/
  base_faces_6x6/
    sprite_sheet_sample_01.png
    sprite_sheet_sample_02.png
    sprite_sheet_sample_03.png
    sprite_sheet_sample_04.png
  guruguru_dir_5x5/
  guruguru_blink_5x5/
  petting_3x3/
  dizzy_4x4/
```

Existing public samples 01 to 04 are base face 6x6 sheets, so they belong under
`base_faces_6x6/`.
Additional animation samples are input examples generated with the prompts in
`generate_sprite_sheet/animation_prompts/`. These additional animation samples were generated with the ChatGPT smartphone app.

Directory purposes:

| Directory | Purpose |
| --- | --- |
| `base_faces_6x6/` | 6x6 sheets split by `build_faces.py` into the 48 base face files |
| `guruguru_dir_5x5/` | 5x5 sheets split by `split_firmware_sheet.py` into `dir0..dir16` |
| `guruguru_blink_5x5/` | 5x5 sheets split by `split_firmware_sheet.py` into `blink0..blink16` |
| `petting_3x3/` | 3x3 sheets split by `split_firmware_sheet.py` into `pet_anim_0..pet_anim_8` |
| `dizzy_4x4/` | 4x4 sheets split by `split_firmware_sheet.py` into `dizzy_01..dizzy_15` |

Additional animation sample to prompt mapping:

| Sample | Source prompt |
| --- | --- |
| `guruguru_dir_5x5/sprite_sheet_sample_01.png` | `generate_sprite_sheet/animation_prompts/guruguru_5x5_prompt.txt` |
| `guruguru_blink_5x5/sprite_sheet_sample_01.png` | `generate_sprite_sheet/animation_prompts/guruguru_blink_5x5_prompt.txt` |
| `petting_3x3/sprite_sheet_sample_01.png` | `generate_sprite_sheet/animation_prompts/petting_3x3_prompt.txt` |
| `dizzy_4x4/sprite_sheet_sample_01.png` | `generate_sprite_sheet/animation_prompts/dizzy_4x4_prompt.txt` |

Run the CLI with a sample:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py tools/face_image_builder/build_faces_from_sprite_sheet/samples/base_faces_6x6/sprite_sheet_sample_01.png --out output_faces --detect-grid --clean
```

## Main Options

| Option | Default | Description |
| --- | --- | --- |
| `--out` | `data` | Working output directory |
| `--install-to` | none | Install the 48 PNG files directly into a directory |
| `--backup-existing` | none | Backup existing PNG files before writing |
| `--backup-dir` | `backups` | Backup directory |
| `--detect-grid` | none | Detect grid lines and crop cells |
| `--grid-line-padding` | `12` | Extra pixels to trim inside detected grid lines |
| `--cell-scale` | `0.92` | Fixed crop size scale |
| `--output-size` | `240` | Output image size |
| `--pad-color` | `0,0,0` | Padding color used during face-size normalization |
| `--clean` | none | Delete existing PNG files in the output directory before generation |
| `--dry-run` | none | Print the crop plan without writing files |
| `--debug-detections` | none | Write a face-detection debug image |
| `--mapping-json` | `face_mapping.json` | Override the 36-cell filename mapping |
| `--copy-json` | `face_copy_rules.json` | Override copy completion rules |

Usually, use `--detect-grid --clean`. Adjust `--grid-line-padding` and `--cell-scale` only when the generated sheet needs it.

## 36-Cell Order

Cells are read in row-major order: left to right, then top to bottom.

| # | File name |
| --- | --- |
| 1 | `idle.png` |
| 2 | `listen.png` |
| 3 | `talk_0.png` |
| 4 | `talk_1.png` |
| 5 | `blink.png` |
| 6 | `smile.png` |
| 7 | `good_0.png` |
| 8 | `good_1.png` |
| 9 | `good_blink.png` |
| 10 | `idle_guarded_0.png` |
| 11 | `talk_guarded_1.png` |
| 12 | `blink_guarded_0.png` |
| 13 | `idle_attached_0.png` |
| 14 | `talk_attached_1.png` |
| 15 | `blink_attached_0.png` |
| 16 | `pet_attached_0.png` |
| 17 | `pet_attached_1.png` |
| 18 | `pet_blink_attached_0.png` |
| 19 | `pet_guarded_0.png` |
| 20 | `pet_guarded_1.png` |
| 21 | `nadenade_0.png` |
| 22 | `nadenade_1.png` |
| 23 | `furifuri_0.png` |
| 24 | `furifuri_1.png` |
| 25 | `photo_0.png` |
| 26 | `photo_1.png` |
| 27 | `photo_blink.png` |
| 28 | `tired_0.png` |
| 29 | `tired_talk.png` |
| 30 | `tired_blink.png` |
| 31 | `photo_master_0.png` |
| 32 | `photo_master_1.png` |
| 33 | `bad_0.png` |
| 34 | `bad_1.png` |
| 35 | `exhausted_0.png` |
| 36 | `exhausted_talk.png` |

## Copy Completion Rules

Some firmware-referenced face files are not direct cells in the 36-cell sprite sheet. They are completed by copying existing PNG files.

| Destination | Source |
| --- | --- |
| `photo_blink_talk.png` | `photo_blink.png` |
| `talk_guarded_0.png` | `idle_guarded_0.png` |
| `talk_attached_0.png` | `idle_attached_0.png` |
| `pet_blink_guarded_0.png` | `pet_guarded_0.png` |
| `shake_guarded_0.png` | `furifuri_0.png` |
| `shake_guarded_1.png` | `furifuri_1.png` |
| `shake_attached_0.png` | `furifuri_0.png` |
| `shake_attached_1.png` | `furifuri_1.png` |
| `exhausted_blink.png` | `tired_blink.png` |
| `low_power_0.png` | `exhausted_0.png` |
| `low_power_talk.png` | `exhausted_talk.png` |
| `low_power_blink.png` | `tired_blink.png` |

## Model and License

Face alignment uses the `face_detect_v1.4_n` ONNX model from `deepghs/anime_face_detection`.

- Source: https://huggingface.co/deepghs/anime_face_detection
- License: MIT

The model is usually downloaded automatically by `build_faces.py` on the first run. Run `download_model.py` only when you want to fetch it ahead of time.
