# Face Image Group Usage Specification

[English](face_image_usage_analysis.md) | [日本語](face_image_usage_analysis.ja.md)

This document describes how each face image v2 group is used at runtime. See tag `v0.3.1` for the pre-migration 129-image layout and legacy static-face analysis.

## Rendering Priority

```text
shake/dizzy recovery > guruguru > petting animation > base animation
```

Affection, authentication, thermal, low-power, and camera states do not switch face image groups. Required state remains active and is shown through overlays where applicable.

## Base Animation

`base_m0_e0..base_m3_e3` form a four-row mouth grid and a four-column eye grid.

- Idle and listening: normally `m0`
- Speaking: `m0..m3` according to volume
- Blink: `e0 → e1 → e2 → e3 → e3 → e2 → e1 → e0`
- Shake recovery: `e3 → e2 → e1 → e0` with the mouth closed

Displaying a closed mouth while a microphone is connected does not require the complete 16-frame cache. If cache allocation fails, the requested v2 image is decoded directly from LittleFS.

## Petting

`pet_anim_0..15` are the 16 row-major frames split from the 4×4 sheet.

- Start: `0 → 1 → 2`
- While being petted: `3 → 4 → 5 → 6 → 7 → 5 → 4 → 6`
- End transition: `2 → 1 → 0`
- End after 3 seconds or longer: happy reaction `8 → 9 → 10 → 11 → 9 → 8 → 0`
- End before 3 seconds: dissatisfied reaction `12 → 13 → 14 → 15 → 12 → 0`

Input is the rear touch sensor on CoreS3, a touch or drag near the center of the StopWatch display, and a BtnA long press on the regular AtomS3R face screen.

## Guruguru and Dizzy

- CoreS3／AtomS3R: `dir0..16` and center `blink16`
- StopWatch: `dir0..8` and center `blink8`
- Dizzy: `dizzy_01..15`

Direction faces follow tilt or touch direction, and `dizzy_*` plays after dizzy motion is detected. When the dizzy sequence ends, the base animation returns from closed eyes to the normal open-eye frame.

## Legacy Compatibility

`transition` supports the 16 manifest-free `voice_m*_e*` files. `legacy` supports only the five legacy base images. Current v2 releases do not contain `idle`, `talk_*`, `good_*`, `bad_*`, `photo_*`, tier, thermal, or low-power face images.
