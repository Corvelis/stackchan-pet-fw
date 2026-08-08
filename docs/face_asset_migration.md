# Face Asset v2 Migration Guide

[English](face_asset_migration.md) | [日本語](face_asset_migration.ja.md)

## Current stage

The release directories `data/`, `data_stopwatch/`, and `data_atoms3r/` now use face asset v2 without legacy static faces. Each contains `face_assets.json`, so normal builds select the `animated` renderer.

The complete legacy set and old 6x6/3x3 generators remain available from tag `v0.3.1`. Current releases and factory images do not include them.

Version 0.5.0 adds nine Travel expressions and two picker pages to the CoreS3
and StopWatch release assets. The boot manifest keeps its backward-compatible
required counts of 65/57/65. Release directories contain 76 CoreS3, 68
StopWatch, and 65 AtomS3R images. Pre-release validation accepts either all 11
Travel files or none; a partial Travel set is invalid.

## Update behavior

| Operation | LittleFS | Face renderer |
| --- | --- | --- |
| Firmware bin only | Preserved | `animated` for v2, `transition` for the 16-frame migration set, `legacy` for the old five-file minimum, otherwise `emergency` |
| v2 `uploadfs` | Replaced with v2 | `animated` |
| v2 factory image | Firmware and v2 assets installed together | `animated` |
| Old firmware only after v2 | v2 remains | Firmware from v0.3.1 or earlier may not find required legacy files |

Legacy files do not need to remain in the current repository for firmware-only fallback. Firmware updates do not erase images already stored in a device's LittleFS, and boot selects one complete renderer profile. New and old frames are never mixed within a profile.

A firmware-bin-only update from 0.4.1 to 0.5.0 boots CoreS3/StopWatch with the
existing face-v2 LittleFS, but it does not install the new Travel images or
picker. Install the 0.5.0 factory image or run `uploadfs` for the target to add
the complete Travel experience.

## Downgrading

When returning a v2 device to `v0.3.1` or earlier, restore the matching factory image or LittleFS image as well. A firmware-bin-only downgrade is unsupported.

## Custom images

To create a new character set, follow the prompts, splitter, samples, and
three-target generation workflow in the
[Face Image Builder](../tools/face_image_builder/README.en.md). Keep intermediate
work in `face_assets_v2_work/` and complete device-test sets in ignored
`data_local/`, `data_stopwatch_local/`, or `data_atoms3r_local/`. Put local
five-file fallback fixtures under `legacy_face_assets_local/`.

To convert complete 129-image transition sets, place `data/`, `data_stopwatch/`,
and `data_atoms3r/` below `legacy_face_assets_local/`, then copy their tested
JPEG files into v2 without recompression with:

```sh
python3 scripts/promote_transition_face_assets.py all \
  --source-root legacy_face_assets_local --replace
```

## Release checks

1. Run `python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v`.
2. Run `python3 scripts/validate_face_assets.py`; confirm `mode=v2` and image counts of 76/68/65.
3. Build all three normal and three classic environments.
4. Run `bash scripts/build_release_bins.sh all` to generate firmware, factory images, and SHA256 checksums.
5. Boot factory images on all targets and confirm `device.info.faceRendererMode=animated`.
6. Confirm firmware-only updates preserve settings and LittleFS.
7. Test the old five-file and image-free boot paths when required.
8. Include downgrade guidance and the legacy API window in release notes.

Validation permits either all 11 Travel files or none. The 0.5.0 release
`data/` and `data_stopwatch/` directories must contain the complete set.

An invalid manifest selects `emergency`; inspect `device.info.faceAssetError` and `faceAssetMissing` for diagnostics.
