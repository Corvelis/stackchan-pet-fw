# Face Renderer v2 Design

[English](face_renderer_v2.md) | [日本語](face_renderer_v2.ja.md)

Face asset v2 renders normal conversation and reactions with animation frames rather than legacy static expressions. The release runtime directories have completed the v2 cutover.

## Principles

- Normal display, mouth animation, and blinking use only `base_m*_e*`.
- Petting, direction, and dizzy reactions use complete, independent animation sets.
- Affection, authentication, thermal, low-power, and camera state do not select legacy static faces.
- Thermal protection, low-power behavior, affection calculations, and camera operations remain active.
- Caches are performance optimizations. When allocation fails, the same v2 frame is decoded directly from LittleFS.
- Boot selects one renderer profile; new and legacy frames are never mixed frame-by-frame.

## Renderer selection at boot

1. Valid `face_assets.json` and every required image: `animated`.
2. No manifest and a complete 16-frame migration `voice_m*_e*` set: `transition`.
3. No manifest and complete `idle`, `listen`, `talk_0`, `talk_1`, and `blink`: `legacy`.
4. Otherwise: image-free `emergency`.

An invalid present manifest selects `emergency`; missing v2 frames are not filled with legacy images. Classic builds always select `classic`.

## v2 image profiles

| Group | CoreS3 | StopWatch | AtomS3R | Filenames |
| --- | ---: | ---: | ---: | --- |
| Base | 16 | 16 | 16 | `base_m0_e0.jpg` ... `base_m3_e3.jpg` |
| Petting | 16 | 16 | 16 | `pet_anim_0.jpg` ... `pet_anim_15.jpg` |
| Direction | 17 | 9 | 17 | `dir0.jpg` ... |
| Center blink | 1 | 1 | 1 | `blink16.jpg` on CoreS3/AtomS3R, `blink8.jpg` on StopWatch |
| Dizzy | 15 | 15 | 15 | `dizzy_01.jpg` ... `dizzy_15.jpg` |
| Required total | 65 | 57 | 65 | |
| Travel still expressions | 9 | 9 | none | `travel_wink.jpg` ... `travel_peace.jpg` |
| Travel picker pages | 2 | 2 | none | `travel_picker_page_0.jpg`, `travel_picker_page_1.jpg` |
| 0.5.0 release total | 76 | 68 | 65 | |

`base_m{mouth}_e{eye}` is four mouth levels by four eye levels. Petting frames use row-major ordering. StopWatch maps the canonical 17 directions to eight directions plus center.
The 11 Travel files are an additional CoreS3/StopWatch set and are not part of the boot manifest's required 65/57 images. This keeps a firmware-only update bootable with an older LittleFS and lets it use whichever Travel faces are present. The 0.5.0 release `data*` directories contain the complete 11-file addition.

## Manifest and validation

The manifest declares `schemaVersion=2`, `renderer=animated`, target, canvas size, and every required group. The machine-readable schema is `tools/face_image_builder/face_assets_v2.schema.json`.

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
```

## Creating your own face images

The [Face Image Builder](../tools/face_image_builder/README.en.md) provides
reference-image prompts for base, petting, direction, center-blink, and dizzy
sprite sheets, plus Travel expressions; 3x3/4x4/5x5 splitting; Travel picker
generation; public samples; and target-specific resizing, naming, and manifest
generation.

Keep intermediate work in ignored `face_assets_v2_work/` and complete test sets
in `data_local/`, `data_stopwatch_local/`, or `data_atoms3r_local/`. Never mix
individual replacement frames into an existing profile; replace a complete v2
set only after it passes validation.

## State and reactions

- Base state: idle, listening, speaking
- Animation: petting, shake recovery, guruguru direction, dizzy
- Travel: keep the selected `travel_*` or reused expression still and stop normal lip sync/blink
- Timekeeper result: reuse `pet_anim_0,8,9,10,9,8,0` as a dedicated smile
- Status overlays: battery, microphone, camera, thermal, low power
- Other overlays: speech bubble, clock, step count, affection delta, Timekeeper UI

Shake recovery opens the eyes through `e3 → e2 → e1 → e0`. It never exposes legacy `idle` or `blink`, including while a microphone client is connected.

Petting that ends before 3 seconds uses the dissatisfied `pet_anim_12..15`
reaction. Petting that lasts at least 3 seconds uses the happy
`pet_anim_8..11` reaction.

Timekeeper and Travel suppress microphone/camera overlays and speech bubbles to
avoid competing for the same screen area. Timekeeper UI is a frame overlay
composited into the full-screen canvas and pushed with the face, so an
overlay-only refresh does not expose a black intermediate frame. StopWatch also
moves the affection-delta label downward while Timekeeper is active.

## Compatibility window

`face` and `face_mode` remain as semantic compatibility adapters for one or two minor releases. The legacy renderer covers only the old five-file minimum. Current releases do not restore `good_*`, `bad_*`, `photo_*`, tier, thermal, or low-power images.
