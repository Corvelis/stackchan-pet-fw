# Face Image File Inventory

[English](face_image_inventory.md) | [日本語](face_image_inventory.ja.md)

## Summary

- Regular releases use only face image v2 assets.
- CoreS3 and AtomS3R use 65 JPG files; StopWatch uses 57.
- Each directory contains `face_assets.json`, and the complete set is validated at boot.
- Current releases exclude the 48 legacy static faces, legacy directional blink set, and legacy 6×6／3×3 generation assets.
- Devices containing only legacy images can boot through the `legacy` fallback after a firmware-only update. The legacy images do not need to remain in Git.

## Device Directories

| Purpose | CoreS3 + Stack-chan | StopWatch | AtomS3R Chatbot |
| --- | --- | --- | --- |
| Release images | `data/` | `data_stopwatch/` | `data_atoms3r/` |
| Personal replacements | `data_local/` | `data_stopwatch_local/` | `data_atoms3r_local/` |
| Resolution | 240×240 | 386×386 | 128×128 |
| Image count | 65 | 57 | 65 |
| Renderer | `animated` | `animated` | `animated` |

`data_local*`, `legacy_face_assets_local/`, and `face_assets_v2_work/` are excluded from Git.

## v2 File Layout

| Group | CoreS3 | StopWatch | AtomS3R |
| --- | ---: | ---: | ---: |
| `base_m*_e*` | 16 | 16 | 16 |
| `pet_anim_*` | 16 | 16 | 16 |
| `dir*` | 17 | 9 | 17 |
| Center `blink*` | 1 | 1 | 1 |
| `dizzy_*` | 15 | 15 | 15 |

StopWatch uses `dir0..8` and `blink8`. The canonical source frames `dir0,2,4,6,8,10,12,14,16` are converted to `dir0..8`. CoreS3 and AtomS3R use `dir0..16` and `blink16`.

## Build Behavior

Each PlatformIO environment copies its target directory to `.pio/generated_data_*` and creates a LittleFS image. Set `STACKCHAN_FACE_DATA_DIR` only when using another image set.

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
```

Release generation does not set this environment variable and always uses the regular `data*` directories.

## Working Assets

| Location | Purpose | Tracked by Git |
| --- | --- | --- |
| `data*` | Completed release v2 assets | yes |
| `data_local*` | Completed personal v2 assets | no |
| `face_assets_v2_work/` | Assets being generated or validated | no |
| `build_faces_from_sprite_sheet/samples/` | Public sample inputs | yes |
| `animation_prompts/` | Image-generation prompts | yes |

## Validation

```sh
python3 scripts/validate_face_assets.py
```

- Confirm that all three targets report `mode=v2`.
- Confirm that target directories contain no device-unnecessary files other than `.jpg` and `face_assets.json`.
- Confirm that each target uses the correct resolution.
- After changing images, inspect every edge and the animation order on all three targets.
