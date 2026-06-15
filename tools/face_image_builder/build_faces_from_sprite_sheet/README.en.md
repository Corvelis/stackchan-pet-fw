# Stack-chan Face Image Splitter CLI

[Japanese](README.md)

`build_faces.py` is a CLI tool that builds 240x240 PNG face images for the Stack-chan pet firmware `data/` directory from a square 6x6 face sprite sheet.

It crops 36 cells from the sprite sheet, then applies copy completion rules for the additional files referenced by the firmware. The final output is 48 PNG files.

Instructions for creating a sprite sheet are in `tools/face_image_builder/generate_sprite_sheet/`.

This directory is for converting an already generated sprite sheet into firmware-ready face images.

## First Command To Use

The execution environment is assumed to be macOS, Windows, or Ubuntu with Python 3.

For the first run, install dependencies.

```bash
python -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

Use the following command to install an AI-generated sprite sheet into `data/` for firmware use.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

This command crops the 36 cells, aligns face positions, normalizes face sizes, applies copy completion rules, and writes the 48 firmware PNG files into `data/`.

With the `--backup-existing` option, existing `data/*.png` files are copied to `backups/data_faces_YYYYMMDD_HHMMSS/` before writing new files.

Note: this tool uses an AI model for face alignment. The model file is downloaded automatically on the first run of `build_faces.py`.

Upload the result to LittleFS.

```bash
pio run -e m5stack-cores3 -t uploadfs
```

Upload the firmware itself when needed.

```bash
pio run -e m5stack-cores3 -t upload
```

## Check Output Before Installing

Use `--out` if you want to inspect the generated files before writing to `data/`.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
```

If the output looks good, run the `--install-to data` command above.

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

`samples/` contains example sprite sheets that can be used as CLI inputs. Public samples are 01 to 04.

```text
samples/
  sprite_sheet_sample_01.png
  sprite_sheet_sample_02.png
  sprite_sheet_sample_03.png
  sprite_sheet_sample_04.png
```

Run the CLI with a sample:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py tools/face_image_builder/build_faces_from_sprite_sheet/samples/sprite_sheet_sample_01.png --out output_faces --detect-grid --clean
```

## Main Options

| Option | Default | Description |
| --- | --- | --- |
| `--out` | `data` | Working output directory |
| `--install-to` | none | Install the 48 files directly into a firmware data directory |
| `--backup-existing` | none | Backup existing PNG files before writing |
| `--backup-dir` | `backups` | Backup directory |
| `--detect-grid` | none | Detect grid lines and crop cells |
| `--grid-line-padding` | `12` | Extra pixels to trim inside detected grid lines |
| `--cell-scale` | `0.92` | Fixed crop size scale |
| `--output-size` | `240` | Output PNG size |
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
