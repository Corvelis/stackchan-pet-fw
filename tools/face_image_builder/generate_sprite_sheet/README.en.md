# Stack-chan Face Sprite Sheet Generation Assets

[Japanese](README.md)

This directory contains assets for generating a 6x6 face sprite sheet for Stack-chan pet firmware with image generation AI tools such as ChatGPT Image or Gemini.

The actual splitting step that creates the 48 firmware PNG files lives in `tools/face_image_builder/build_faces_from_sprite_sheet/`.

## Files

```text
tools/face_image_builder/generate_sprite_sheet/
  README.md
  README.en.md
  sprite_sheet_prompt.md
  grid_template_6x6.png
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
  the full text of sprite_sheet_prompt.md
```

`grid_template_6x6.png` is Image A in the prompt. It is the fixed 6x6 grid template.

Image B should be one face image for the character you want to generate. The images in `references/` are only examples used while testing this repository.

You do not need to use the images in `references/`. For your own character, choose one mostly front-facing image where the hairstyle, eyes, outfit, and accessories are easy to see.

`sprite_sheet_prompt.md` is the completed prompt. Paste it as-is into the image generation AI.

## Image Generation Steps

Use ChatGPT Image 2.0 / gpt-image-2.0 or a Gemini + nano banana style image generation UI to create the sprite sheet from Image A, Image B, and the prompt.

You can do this from a smartphone app chat UI. No dedicated API or script is required.

Steps:

1. Open ChatGPT or Gemini and switch to a state where it can generate images.
2. Attach `grid_template_6x6.png`.
3. Attach one face image of your choice.
4. Paste the full text of `sprite_sheet_prompt.md` into the message field.
5. Start generation.
6. Save the generated square 6x6 sprite sheet image.
7. Pass the saved image to `build_faces.py` to split it into the 48 firmware PNG files.

```bash
python tools/face_image_builder/build_faces_from_sprite_sheet/build_faces.py sprite_sheet.png --install-to data --backup-existing --detect-grid --clean
```

## Sample Generation Environments

The sample images in `tools/face_image_builder/build_faces_from_sprite_sheet/samples/` were generated with the following setups.

| Sample | Generation environment |
| --- | --- |
| `sprite_sheet_sample_01.png` | Gemini Pro + nano banana 2 |
| `sprite_sheet_sample_02.png` | Gemini Pro + nano banana 2 |
| `sprite_sheet_sample_03.png` | GPT-5.5 highest intelligence + ChatGPT Image 2.0 |
| `sprite_sheet_sample_04.png` | GPT-5.5 highest intelligence + ChatGPT Image 2.0 |

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
