# Animation Sprite Sheet Prompts

[Japanese](README.md)

This directory contains base prompts for generating additional animation images for Stack-chan pet firmware with image generation AI.

Do not commit generated images or private character reference images here. Put them in ignored local folders such as `data_local/`, `data_stopwatch_local/`, `data_atoms3r_local/`, or in an external backup. This directory should contain prompt text only.

## Files

```text
animation_prompts/
  README.md
  README.en.md
  guruguru_5x5_prompt.txt
  guruguru_blink_5x5_prompt.txt
  petting_3x3_prompt.txt
  dizzy_4x4_prompt.txt
```

## Prompt Usage

- `guruguru_5x5_prompt.txt`: 5x5 sheet for guruguru tracking images, split into `dir0..dir16`.
- `guruguru_blink_5x5_prompt.txt`: 5x5 sheet for guruguru blink images, split into `blink0..blink16`.
- `petting_3x3_prompt.txt`: 3x3 sheet for petting images, split into `pet_anim_0..pet_anim_8`.
- `dizzy_4x4_prompt.txt`: 4x4 sheet for guruguru dizzy images, split into `dizzy_01..dizzy_15`.

| Prompt | Generated image | Firmware use | Split files |
| --- | --- | --- | --- |
| `guruguru_5x5_prompt.txt` | 5x5 open-eye face direction sheet. | Changes face direction in guruguru mode based on IMU or touch position. | `dir0.jpg` ... `dir16.jpg` |
| `guruguru_blink_5x5_prompt.txt` | 5x5 closed-eye face direction sheet. | Blink frames in guruguru mode. Use the same direction order as `dir`. | `blink0.jpg` ... `blink16.jpg` |
| `petting_3x3_prompt.txt` | 3x3 petting reaction animation sheet. | Facial animation during petting input. | `pet_anim_0.jpg` ... `pet_anim_8.jpg` |
| `dizzy_4x4_prompt.txt` | 4x4 dizzy animation sheet. | Loop and recovery frames after entering the guruguru dizzy state. | `dizzy_01.jpg` ... `dizzy_15.jpg` |

## Sample Images

Public samples generated with these prompts are stored under the splitter CLI `samples/` directory.
Use them as input references for `split_firmware_sheet.py`. Before splitting a new generated image, compare it with the sample to confirm the layout is the same.
The current additional animation samples were generated with the ChatGPT smartphone app.

| Prompt | Sample image | What to check |
| --- | --- | --- |
| `guruguru_5x5_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/guruguru_dir_5x5/sprite_sheet_sample_01.png` | 5x5, open-eye direction variants, center cell faces forward |
| `guruguru_blink_5x5_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/guruguru_blink_5x5/sprite_sheet_sample_01.png` | 5x5, same direction order as `guruguru`, closed-eye blink variants |
| `petting_3x3_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/petting_3x3/sprite_sheet_sample_01.png` | 3x3, petting reactions ordered from top-left to bottom-right |
| `dizzy_4x4_prompt.txt` | `tools/face_image_builder/build_faces_from_sprite_sheet/samples/dizzy_4x4/sprite_sheet_sample_01.png` | 4x4, dizzy loop and recovery frames ordered from top-left |

## Generation Flow

Attach one mostly front-facing face image for the character, then paste the full text of the `.txt` prompt for the target animation.
Save the generated result as one black-background image without text, numbers, grid lines, or borders.

When saving generated sheets inside this project, use `tools/face_image_builder/sprite_sheets/`.
Actual image files under that directory are ignored by Git, so private character images are not committed.

Recommended file names:

```text
tools/face_image_builder/sprite_sheets/
  guruguru/
    sheet.png
    blink.png
  petting/
    petting.png
  dizzy/
    dizzy_contact.png
```

## Splitting

Split generated sheets into firmware JPEG files with `tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py`.

5x5 guruguru:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet sheet.png:dir --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

5x5 guruguru blink:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet blink.png:blink --directions 17 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

3x3 petting:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet petting.png:pet_anim_ --grid 3x3 --directions 9 --layout even --crop-size auto --cell-inset 4 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

4x4 dizzy:

```bash
python3 tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py --sheet dizzy_contact.png:dizzy_ --grid 4x4 --directions 15 --layout even --start-index 1 --pad 2 --target stopwatch --format jpg --out-dir data_stopwatch_local --preview-dir previews
```

`--target` controls output size. `cores3` is 240x240, `stopwatch` is 386x386, and `atoms3r` is 128x128.
Use `data/`, `data_stopwatch/`, or `data_atoms3r/` for public release assets. Use `data_local/`, `data_stopwatch_local/`, or `data_atoms3r_local/` for local replacement assets.

If fragments from neighboring cells appear, add `--layout even --cell-inset 4`.
Increase it slightly, for example to `--cell-inset 8`, if fragments remain.
