# Stack-chan Face Sprite Sheet Generation Assets

[Japanese](README.md)

This directory describes the assets and steps for generating a 6x6 face sprite sheet for Stack-chan pet firmware with image generation AI tools such as ChatGPT Image or Gemini.

The splitting step that creates the 48 base face files is handled separately in
`tools/face_image_builder/build_faces_from_sprite_sheet/`. Final firmware files
are standardized to JPG with `prepare_firmware_assets.py`.

## Files

```text
tools/face_image_builder/generate_sprite_sheet/
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
    face_reference_01.png
    face_reference_02.png
    face_reference_03.png
    face_reference_04.png
```

## Inputs

Pass these three inputs to the image generation AI.

```text
Image A:
  grid_template_6x6.png

Image B:
  one face image of your choice

Prompt:
  the full text of sprite_sheet_prompt.txt
```

`grid_template_6x6.png` is the fixed 6x6 grid template.

Image B should be one face image for the character you want to generate. The images in `references/` are sample reference images used while testing this repository.
Choose one mostly front-facing image where the hairstyle, eyes, outfit, and accessories are easy to see.

`sprite_sheet_prompt.txt` is the prompt I used. Edit it as needed.

For guruguru, petting, and dizzy animation sheets, use the prompt files under
`animation_prompts/`. Split generated sheets with
`tools/face_image_builder/build_faces_from_sprite_sheet/split_firmware_sheet.py`.

## Image Generation Steps

Use the ChatGPT or Gemini app to create a sprite sheet from Image A, Image B, and the prompt.
You can also use an API, but a smartphone app chat UI is enough.

Steps:

1. Open a ChatGPT or Gemini chat where image generation is available.
2. Attach `grid_template_6x6.png`.
3. Attach one face image of your choice.
4. Paste the full text of `sprite_sheet_prompt.txt` into the message field.
5. Start generation.
6. Save the generated square 6x6 sprite sheet image.
7. Pass the saved image to `build_faces.py` to split it into the 48 base face files.
8. Convert the result into target-device JPG files with `prepare_firmware_assets.py`.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --out output_faces --detect-grid --clean
python3 tools/face_image_builder/prepare_firmware_assets.py output_faces data --target cores3 --format jpg --clean
```

Use `tools/face_image_builder/sprite_sheets/` when you want a local in-project
staging area for generated sprite sheets. Actual image files under that folder
are ignored by Git.

## Sample Generation Environments

The sample images in `tools/face_image_builder/build_faces_from_sprite_sheet/samples/base_faces_6x6/` were generated with the following setups.

| Sample | Generation environment |
| --- | --- |
| `base_faces_6x6/sprite_sheet_sample_01.png` | Gemini Pro + nano banana 2 |
| `base_faces_6x6/sprite_sheet_sample_02.png` | Gemini Pro + nano banana 2 |
| `base_faces_6x6/sprite_sheet_sample_03.png` | GPT-5.5 highest intelligence + ChatGPT Image 2.0 |
| `base_faces_6x6/sprite_sheet_sample_04.png` | GPT-5.5 highest intelligence + ChatGPT Image 2.0 |

Gemini was tested as a free user. ChatGPT was tested as a paid user. Both were tested from smartphone app chat UIs.

## Generation Notes

The prompt strongly asks the model to keep Image A's grid and insert faces into the existing cells instead of creating a new grid.

If the generated result has any of the following issues, regenerate with the same assets and prompt.

```text
- It is not 6x6.
- Grid lines are missing or have changed thickness.
- Row or column sizes are inconsistent.
- Face size or position differs too much between cells.
- A one-eye-closed expression appears.
- Extra decorations appear in the background or grid.
```

Small grid-line irregularities and face-position shifts can usually be corrected by `build_faces.py --detect-grid`. If the cell structure is badly broken, regenerate the sprite sheet before splitting.
