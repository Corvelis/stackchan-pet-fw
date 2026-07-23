# Sprite Sheet Splitter CLI

`split_firmware_sheet.py` splits 4x4 and 5x5 sheets into canonical face asset v2 frames. The old 6x6 static-face tool has been removed from the current tree; refer to tag `v0.3.1` when it is required.

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

See [`ASSET_LICENSE.md`](../../../ASSET_LICENSE.md) for sample-image terms.
