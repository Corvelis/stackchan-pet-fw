# Stack-chan Multi-Device Controller 0.4.0

[English](release_notes_0.4.0.md) | [日本語](release_notes_0.4.0.ja.md)

Version 0.4.0 moves face rendering on CoreS3, StopWatch, and AtomS3R Chatbot
from the legacy static-face layout to manifest-backed face asset v2 animation.
Normal conversation, lip sync, blinking, petting, guruguru direction, dizziness,
and recovery now use complete target-validated animation sets.

## Files to download

| Device | Update an existing install | First install or complete face v2 migration |
| --- | --- | --- |
| CoreS3 + Stack-chan | `stackchan_cores3_firmware.bin` | `stackchan_cores3_factory.bin` |
| StopWatch | `stackchan_stopwatch_firmware.bin` | `stackchan_stopwatch_factory.bin` |
| AtomS3R Chatbot | `stackchan_atoms3r_firmware.bin` | `stackchan_atoms3r_factory.bin` |

- A firmware-only update preserves Wi-Fi settings and LittleFS images.
- A factory image installs firmware and face asset v2 together, but resets
  settings stored on the device.
- Verify downloads with `SHA256SUMS`, and keep the bundled `LICENSE` and
  `THIRD_PARTY_NOTICES.md`.
- See the [Binary Installation Guide](install_binary.md) for flashing steps.

## Face asset v2

- CoreS3/AtomS3R use 65 JPG files; StopWatch uses 57. Every set includes
  `face_assets.json`.
- Normal display, lip sync, and blinking use `base_m0_e0..base_m3_e3`.
- Petting uses 16 frames, direction uses 17 or 9 target-specific frames, center
  blink uses one frame, and dizziness uses 15 frames.
- Affection, authentication, thermal, low-power, and camera state no longer
  select dedicated legacy static faces.
- Thermal protection, low-power behavior, affection calculations, and camera
  capture remain active.

Boot validates the installed set and selects `animated`, `transition`, `legacy`,
or `emergency`. It never mixes new and legacy frames within one profile. See
[Face Renderer v2 Design](face_renderer_v2.md) for the complete rules.

## Reaction changes

- Petting that ends before 3 seconds uses the dissatisfied after-reaction;
  petting that lasts at least 3 seconds uses the happy reaction.
- Guruguru mode uses target-specific direction faces and center blink.
- Shake/dizzy recovery opens the base face from closed eyes back to normal.
- Thermal `Warm` and Low Power no longer replace face asset v2 with a legacy
  static face.

## CoreS3 camera

The firmware resolves the SCCB conflict between the GC0308 camera and
M5Unified's internal I2C bus, then restores internal I2C after capture. JPEG
capture is available through HTTP `POST /capture` and USB Serial
`capture.request` at 640x480. RGB565 capture uses PSRAM, and bounded USB/HTTP
chunks keep a large JPEG from monopolizing transport processing.

## CoreS3 servo and audio

- Startup adopts the servos' current physical pose without moving the neck merely for initialization.
- Servo output is held during speech, and automatic listening nods are disabled.
- Microphone capture pauses during petting, shake/dizzy, servo movement, and mechanical settling to avoid speech-recognition retriggers from servo noise.
- Pan and tilt start one axis at a time to reduce supply-current spikes, USB resets, and stale Wi-Fi connections.
- Petting and shake servo reactions remain enabled, while the upward petting range is limited.

## Communication and diagnostics

- Turning the display off ends audio/app sessions and stops Wi-Fi, HTTP, WebSocket, and USB Serial. Display-on starts Wi-Fi reconnection, so clients must reconnect too.
- StreetPass BLE continues at a reduced rate while the display is off. StopWatch also lowers CPU frequency and uses short light-sleep intervals.
- `audio.playback_diag` reports audio buffering, PCM receive/drop counts, underflow, speaker queue, speech-bubble state, and speech processing timings.
- HTTP `/status` now includes detailed `mic`, `voicePerf`, and `streetpass` objects plus `currentState`/`audioState`. Diagnostic clients should ignore unknown fields.

## StopWatch step counter

StopWatch counts steps from its IMU, uses 04:00 Japan Standard Time as the
activity-day boundary, and stores up to 30 days including today. The `Steps`
screen displays the history, every 1,000 steps adds `+3` affection, and clients
receive `steps.snapshot`/`steps.update`. CoreS3 and AtomS3R are not supported.
See the [Step Counter And Sync Protocol](step_counter_protocol.md).

## Updating and downgrading

A firmware-only update preserves LittleFS. A device with face asset v2 selects
`animated`; a device with only the old five-file minimum can use the limited
`legacy` fallback. Install the target factory image or v2 LittleFS to complete
the recommended 0.4.0 face migration.

When downgrading a v2 device to `v0.3.1` or earlier, restore the matching factory
image or LittleFS as well as firmware. See the
[Face Asset v2 Migration Guide](face_asset_migration.md).

The old `face` and `face_mode` commands remain as semantic compatibility inputs
for one or two minor releases. Legacy `good_*`, `bad_*`, and `photo_*` images do
not return to the 0.4.0 release assets.

## Using your own character images

The [Face Image Builder](../tools/face_image_builder/README.en.md) provides
reference-image prompts for base, petting, direction, center-blink, and dizzy
sprite sheets; a splitter; public samples; and three-target resize, naming, and
manifest generation.

Keep intermediate files under ignored `face_assets_v2_work/`, test complete sets
through `data_local*`, and replace assets only as a complete validated v2 set.

## Other additions

- Shared WebSocket and USB Serial speech-bubble protocol.
- StopWatch device-side step counter and status reporting.
- Reproducible release builds for all three normal and three classic targets.
- Bundled runtime images, sprite-sheet samples, and image-generation references
  use the same [MIT License](../LICENSE) as the firmware source.
