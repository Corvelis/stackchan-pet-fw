# Animation Generation Prompts

These are the canonical face asset v2 prompts. Supply your own authorized character reference or use an optional sample from `../references/`.

[日本語](README.md)

| Prompt | Grid | Output names | Sample |
| --- | --- | --- | --- |
| `base_animation_4x4_prompt.txt` | 4x4 | `base_m0_e0` ... `base_m3_e3` | `samples/base_animation_4x4/` |
| `petting_4x4_prompt.txt` | 4x4 | `pet_anim_0` ... `pet_anim_15` | `samples/petting_4x4/` |
| `guruguru_5x5_prompt.txt` | 5x5 | `dir0` ... `dir16` | `samples/guruguru_dir_5x5/` |
| `guruguru_blink_5x5_prompt.txt` | 5x5 | `blink0` ... `blink16` | `samples/guruguru_blink_5x5/` |
| `dizzy_4x4_prompt.txt` | 4x4 | `dizzy_01` ... `dizzy_15` | `samples/dizzy_4x4/` |

See [`../../build_faces_from_sprite_sheet/README.en.md`](../../build_faces_from_sprite_sheet/README.en.md) for splitting instructions. Do not blindly apply sample-specific alignment offsets to other generated sheets.
