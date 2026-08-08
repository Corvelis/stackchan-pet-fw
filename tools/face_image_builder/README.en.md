# Stack-chan Face Image Builder

This directory contains the tools and samples used to create face asset v2 animation frames from sprite sheets. The current release does not use the old 6x6 static-face workflow, old 3x3 petting workflow, or separate static faces for affection, thermal, low-power, and camera states. Refer to tag `v0.3.1` if the legacy workflow is required.

[日本語](README.md)

## Face asset v2 layout

| Group | Canonical source | Output |
| --- | --- | --- |
| Base animation | 4x4, mouth rows and eye columns | `base_m0_e0.jpg` ... `base_m3_e3.jpg` |
| Petting | 4x4, row-major | `pet_anim_0.jpg` ... `pet_anim_15.jpg` |
| Guruguru direction | 17 cells selected from 5x5 | `dir0.jpg` ... `dir16.jpg` |
| Center blink | Center cell from 5x5 | `blink16.jpg` on CoreS3/AtomS3R, `blink8.jpg` on StopWatch |
| Dizzy | 15 cells from 4x4 | `dizzy_01.jpg` ... `dizzy_15.jpg` |
| Travel expressions | 3x3, row-major | `travel_wink.jpg` ... `travel_peace.jpg` |

The required face asset v2 set contains 65 images on CoreS3 and AtomS3R and 57 on StopWatch. Travel-capable CoreS3 and StopWatch builds add nine expressions and two picker pages.

## Recommended workflow

1. Use a prompt in [`generate_sprite_sheet/animation_prompts/`](generate_sprite_sheet/animation_prompts/) with your own character reference to create each sheet.
2. Split sheets into the CoreS3-sized canonical source with [`split_firmware_sheet.py`](build_faces_from_sprite_sheet/split_firmware_sheet.py).
3. Run `generate_v2_face_assets.py` to resize, map target filenames, and write manifests for all three devices.
4. Validate and test through ignored `data_local*` or `face_assets_v2_work/` directories.
5. Replace the repository `data*` directory as a whole only when promoting reviewed release assets.

For the 3x3 travel sheet, split and resize directly for CoreS3 and StopWatch, then run `build_travel_picker_pages.py` to create each device's picker layout. See [`build_faces_from_sprite_sheet/README.en.md`](build_faces_from_sprite_sheet/README.en.md) for the frame order and commands.

```sh
python3 -m pip install -r tools/face_image_builder/build_faces_from_sprite_sheet/requirements.txt

python3 scripts/generate_v2_face_assets.py \
  face_assets_v2_work/canonical all --replace

python3 scripts/validate_face_assets.py \
  face_assets_v2_work/generated/cores3 --target cores3 --mode v2
```

To convert complete 129-image transition directories without recompressing
their tested JPEG files, place `data/`, `data_stopwatch/`, and `data_atoms3r/`
below `legacy_face_assets_local/`, then run:

```sh
python3 scripts/promote_transition_face_assets.py all \
  --source-root legacy_face_assets_local --replace
```

## Hardware testing

```sh
STACKCHAN_FACE_DATA_DIR=face_assets_v2_work/generated/cores3 \
  pio run -e m5stack-cores3 -t uploadfs

STACKCHAN_FACE_DATA_DIR=face_assets_v2_work/generated/stopwatch \
  pio run -e m5stack-stopwatch -t uploadfs

STACKCHAN_FACE_DATA_DIR=face_assets_v2_work/generated/atoms3r \
  pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

Confirm that `device.info.faceRendererMode` reports `animated` after boot.

## Directory layout

```text
face_image_builder/
  face_assets_v2.schema.json
  prepare_firmware_assets.py
  generate_sprite_sheet/
    animation_prompts/
  build_faces_from_sprite_sheet/
    split_firmware_sheet.py
    build_travel_picker_pages.py
    requirements.txt
    samples/
      base_animation_4x4/
      petting_4x4/
      guruguru_dir_5x5/
      guruguru_blink_5x5/
      dizzy_4x4/
```

Sample images are available under the
[`MIT License`](../../LICENSE).
