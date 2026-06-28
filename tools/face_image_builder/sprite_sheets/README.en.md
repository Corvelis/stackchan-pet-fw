# Local Sprite Sheet Staging

[Japanese](README.md)

Use this directory as a local input area for sprite sheets generated with image generation AI before passing them to `build_faces_from_sprite_sheet/split_firmware_sheet.py` or `build_faces.py`.

Generated images should not be committed, so actual image and video files under this directory are ignored by `.gitignore`. Only the directory structure and README files are tracked.

## Recommended Layout

```text
tools/face_image_builder/sprite_sheets/
  base_faces/
    sprite_sheet.png
  guruguru/
    sheet.png
    blink.png
  petting/
    petting.png
  dizzy/
    dizzy_contact.png
```

## Split Examples

Base face 6x6:

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py tools/face_image_builder/sprite_sheets/base_faces/sprite_sheet.png --out output_faces_local --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces_local data_local --target cores3 --format jpg --clean
```

Guruguru 5x5:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/guruguru/sheet.png:dir --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

Guruguru blink 5x5:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/guruguru/blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

Petting 3x3:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/petting/petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

Dizzy 4x4:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet tools/face_image_builder/sprite_sheets/dizzy/dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

Change `--target` to `cores3`, `stopwatch`, or `atoms3r`.
Use `data/`, `data_stopwatch/`, or `data_atoms3r/` for public release assets. Use `data_local/`, `data_stopwatch_local/`, or `data_atoms3r_local/` for local replacement assets.

## Output Names

| Directory | Input example | Output prefix | Output |
| --- | --- | --- | --- |
| `guruguru/` | `sheet.png` | `dir` | `dir0.jpg` ... `dir16.jpg` |
| `guruguru/` | `blink.png` | `blink` | `blink0.jpg` ... `blink16.jpg` |
| `petting/` | `petting.png` | `pet_anim_` | `pet_anim_0.jpg` ... `pet_anim_8.jpg` |
| `dizzy/` | `dizzy_contact.png` | `dizzy_` | `dizzy_01.jpg` ... `dizzy_15.jpg` |

Always inspect the split result with `--preview-dir previews`.
If top or side fragments from neighboring cells appear, add `--layout even --cell-inset 4` to the split command.
