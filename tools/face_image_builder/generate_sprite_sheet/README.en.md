# Generating Animation Sprite Sheets

This directory contains prompts for face asset v2 sprite sheets. It no longer uses a fixed legacy 6x6 template. Attach an image of the character you are authorized to use, or one of the sample references under `references/`, then apply the prompt for the required animation.

[日本語](README.md)

## Available sheets

| Prompt | Grid | Purpose |
| --- | --- | --- |
| `animation_prompts/base_animation_4x4_prompt.txt` | 4x4 | Four mouth levels by four eye levels |
| `animation_prompts/petting_4x4_prompt.txt` | 4x4 | Petting start, loop, happy, and dissatisfied reactions |
| `animation_prompts/guruguru_5x5_prompt.txt` | 5x5 | Sixteen directions plus center |
| `animation_prompts/guruguru_blink_5x5_prompt.txt` | 5x5 | Closed-eye variants in the same directions |
| `animation_prompts/dizzy_4x4_prompt.txt` | 4x4 | Fifteen dizzy-animation frames |

## Sample references

`references/face_reference_01.png` through `face_reference_04.png` are optional samples for trying the generation workflow. Replace them when generating sheets for your own character.

## Workflow

1. Attach one authorized character reference image or one sample from `references/`.
2. Send the complete relevant prompt file to the image-generation tool.
3. Confirm the requested grid, black background, and fixed composition.
4. Save the sheet and split it with `build_faces_from_sprite_sheet/split_firmware_sheet.py`.
5. Inspect face alignment, every image edge, and neighboring-cell contamination in previews.

Image generation can shift compositions and cell boundaries. For the base sheet, only eyes and mouth should change. For petting, only differences explicitly allowed by the prompt should change. Regenerate badly structured sheets instead of attempting excessive crop correction.

See [`../build_faces_from_sprite_sheet/README.en.md`](../build_faces_from_sprite_sheet/README.en.md) for bundled samples and splitter commands.
