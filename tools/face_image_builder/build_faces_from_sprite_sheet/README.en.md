# Sprite Sheet Splitter CLI

`split_firmware_sheet.py` splits 3x3, 4x4, and 5x5 sheets into canonical face asset v2 frames. The old 6x6 static-face tool has been removed from the current tree; refer to tag `v0.3.1` when it is required.

[日本語](README.md)

## Setup

```sh
python3 -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt
```

## 4x4 base animation

Rows map to mouth indices `m0..m3`; columns map to eye indices `e0..e3`.

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/base_animation_4x4.png:base_ \
  --grid 4x4 --directions 16 --layout even --crop-size auto \
  --row-top-mask 6 --column-side-mask 6 \
  --size 512 --format png --output-naming base-mouth-eye \
  --out-dir face_assets_v2_work/canonical \
  --preview-dir face_assets_v2_work/previews
```

The current bundled base-face sample is aligned without column offsets. Use `--column-x-offsets` only when preview inspection shows that another generated sheet needs column alignment correction.

## 4x4 petting animation

Frames are exported row-major as `pet_anim_0..15`.

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/petting_4x4.png:pet_anim_ \
  --grid 4x4 --directions 16 --layout even --crop-size auto \
  --row-top-mask 6 --column-side-mask 6 \
  --size 512 --format png \
  --out-dir face_assets_v2_work/canonical \
  --preview-dir face_assets_v2_work/previews
```

## Guruguru direction and blink

Select the 16 outer directions and center from a 5x5 sheet as `dir0..16` or `blink0..16`.

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/guruguru_5x5.png:dir \
  --grid 5x5 --directions 17 --target cores3 --format jpg \
  --out-dir face_assets_v2_work/canonical
```

Use the same frame order and change the prefix to `blink` for the closed-eye sheet.

## 4x4 dizzy animation

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/dizzy_4x4.png:dizzy_ \
  --grid 4x4 --directions 15 --layout even \
  --start-index 1 --pad 2 --size 512 --format png \
  --out-dir face_assets_v2_work/canonical
```

## 3x3 travel expressions

The row-major cells are exported as `travel_wink`, `travel_sparkle`, `travel_surprised`, `travel_shy`, `travel_delicious`, `travel_mischief`, `travel_teary`, `travel_yawn`, and `travel_peace`. Install them only for the travel-capable CoreS3 and StopWatch targets; do not add them to `data_atoms3r/`.

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/travel_3x3.png:travel_ \
  --grid 3x3 --directions 9 --layout even --crop-size auto \
  --target cores3 --format jpg --quality 82 \
  --output-naming travel-expressions --out-dir data_local

python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py \
  --sheet path/to/travel_3x3.png:travel_ \
  --grid 3x3 --directions 9 --layout even --crop-size auto \
  --target stopwatch --format jpg --quality 82 \
  --output-naming travel-expressions --out-dir data_stopwatch_local
```

After installing the nine expressions, build picker pages in the option order and device layout used by the firmware.

- Page 0: `pet_anim_8`, `pet_anim_10`, `travel_wink`, `travel_sparkle`, `travel_surprised`, `travel_shy`, `travel_delicious`, `travel_peace`
- Page 1: `dizzy_01`, `dizzy_09`, `pet_anim_13`, `pet_anim_14`, `travel_mischief`, `travel_teary`, `travel_yawn`

Generation stops if any source image is missing. The default JPEG quality is 82. Pre-release validation accepts either the complete 11-file Travel addition or no Travel files and rejects a partial group.

```sh
python3 tools/face_image_builder/build_faces_from_sprite_sheet/build_travel_picker_pages.py \
  data_local --target cores3

python3 tools/face_image_builder/build_faces_from_sprite_sheet/build_travel_picker_pages.py \
  data_stopwatch_local --target stopwatch
```

After device checks and `validate_face_assets.py` pass, rerun with `data/` and
`data_stopwatch/` only when promoting the set to release assets. Never add the
`data_local*` directories to Git.

## Boundary cleanup

- `--layout even`: split as equal-sized grid cells.
- `--cell-inset`: inset every crop boundary.
- `--row-top-mask`: mask the top edge of rows after the first.
- `--column-side-mask`: mask both sides of internal column boundaries.
- `--column-x-offsets`: horizontally translate each grid column.
- `--preview-dir`: write preview output for inspection.

Inspect face alignment and every image edge before accepting output. The splitter cleans existing files with the same prefix by default; pass `--no-clean` to keep them.

## Samples

| Directory | Purpose |
| --- | --- |
| `samples/base_animation_4x4/` | 4x4 base animation |
| `samples/petting_4x4/` | 4x4 petting animation |
| `samples/guruguru_dir_5x5/` | 5x5 direction faces |
| `samples/guruguru_blink_5x5/` | 5x5 closed-eye directions |
| `samples/dizzy_4x4/` | 4x4 dizzy animation |

Sample images are available under the
[`MIT License`](../../../LICENSE).
