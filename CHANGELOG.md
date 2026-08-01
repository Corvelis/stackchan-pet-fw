# Changelog

[日本語](CHANGELOG.ja.md) | [English](CHANGELOG.md)

## [0.4.1](docs/release_notes_0.4.1.md) - 2026-08-01

### Added

- Shared WebSocket／USB Serial phone-camera remote protocol for requesting a capture from a compatible phone app on StopWatch.
- Front/rear lens requests, `IN`／`OUT` lens display, pending/success/failure UI, and haptic feedback.
- Phone-camera state controller for transport ownership, session-scoped request IDs, response matching, mutual exclusion, and timeouts.
- `phone_camera.remote_shutter.v1` and `phone_camera.remote_lens.v1` capabilities.
- Japanese and English phone-camera remote protocol documents.

### Changed

- Moved the StopWatch microphone overlay from the lower-right to the lower-left and assigned the lower-right to phone-camera controls.
- Replaced StopWatch／CoreS3 camera and microphone taps with shared gesture tracking for start position, duration, travel, and release position.
- Enlarged StopWatch／CoreS3 overlay-button touch targets and excluded the camera target from petting detection.
- Standardized StreetPass and clock timezone handling on `Asia/Tokyo`.
- Applied RTC, app, and NTP time immediately to the system clock and wrote corrected UTC back to the RTC.
- Confirmed NTP through its response callback and added Wi-Fi reconnect sync, 10-second retry, and six-hour refresh handling.

### Fixed

- Prevented a hold or drag release from accidentally firing a short camera or microphone action.
- Prevented an already-valid system clock from being mistaken for a fresh NTP response.
- Prevented the system clock and display from remaining stale after RTC restore or `streetpass.time.set`.

## [0.4.0](docs/release_notes_0.4.0.md) - 2026-07-25

### Added

- Animated face asset v2 profiles and manifests for CoreS3, StopWatch, and AtomS3R.
- Four-by-four base mouth/blink animation and 16-frame petting reactions.
- Boot-time `animated`, `transition`, `legacy`, `classic`, and image-free `emergency` renderer selection.
- Strict face-asset validation, schema, conversion tools, unit tests, and CI validation.
- Face renderer diagnostics in `device.info`.
- Shared WebSocket and USB Serial speech-bubble protocol.
- Three separate source-built classic-face environments with procedural blinking, eight-step lip sync, speech-bubble overlays, and no image-specific reaction animations.
- Read-only `audio.playback_diag` JSON command and expanded `/status` microphone, voice-performance, and StreetPass diagnostics.
- StopWatch-only step counter, 30-day history, `steps.sync` status reporting, and affection rewards.
- Reproducible six-environment CI builds and device-specific release/factory image generation.
- Public 4x4 base and petting sprite-sheet prompts and samples.

### Changed

- Release images now contain only the v2 animation set: 65 JPG files on CoreS3/AtomS3R and 57 on StopWatch.
- Affection, authentication, thermal, low-power, and camera state no longer select separate static face files.
- Thermal protection, low-power behavior, affection calculations, and camera operation remain active and use overlays where applicable.
- Petting shorter than 3 seconds uses the dissatisfied after-reaction; 3 seconds or longer uses the happy reaction.
- Face cache allocation is now an optimization rather than a requirement; v2 frames can be decoded directly from LittleFS.
- Direct dependencies are pinned for reproducible PlatformIO builds.
- CoreS3 camera capture now uses 640x480 JPEG output with bounded USB/HTTP transfer chunks.
- Display-off now ends audio/app sessions and suspends Wi-Fi, HTTP, WebSocket, and USB Serial; display-on starts Wi-Fi reconnection, while StreetPass BLE continues at a reduced rate.
- Bundled runtime images, sprite-sheet samples, and image-generation references use the same MIT License as the firmware source.
- The MIT grant for Ailog-created images does not relicense third-party images embedded in historical release binaries; those remain subject to their original release notices and material-provider terms.

### Fixed

- Corrected horizontal alignment in the bundled base-animation sample split.
- Removed neighboring-cell fragments from split sprite frames, including lower and side edges.
- Prevented thermal `Warm` and low-power state from replacing the animated face.
- Prevented shake/dizzy recovery from exposing legacy idle/blink faces while a microphone client is connected.
- Restored guruguru direction-face rendering after switching the runtime image set to manifest-backed v2.
- Preserved CoreS3's center blink image when preparing manifest-backed v2 assets for LittleFS.
- Resolved the CoreS3 camera SCCB conflict with M5Unified's internal I2C bus and restored that bus after capture.
- Prevented CoreS3 servo movement during speech and disabled automatic listening nods.
- Paused microphone capture during petting, shake/dizzy, and servo settling so mechanical noise does not retrigger speech recognition.
- Prevented the CoreS3 startup servo drop by adopting the current physical pose without issuing a startup movement.
- Serialized CoreS3 pan/tilt movement to avoid current spikes, USB resets, and stale Wi-Fi connections.
- Limited the upward petting pose while preserving servo movement for petting and shake reactions.

### Removed

- Legacy 48-face runtime assets, including `good_*`, `bad_*`, `photo_*`, affection-tier, thermal, and low-power image variants.
- Legacy 6x6 static-face and 3x3 petting generators and samples from the current branch. They remain available in tag `v0.3.1`.

### Migration notes

- Firmware-only updates preserve LittleFS and can use a complete old five-file face set through the limited `legacy` renderer.
- New factory images install v2 and select `animated`.
- To downgrade a device with v2 images to `v0.3.1` or earlier, restore the matching factory or LittleFS image as well as firmware.
- The `face` and `face_mode` commands remain as semantic compatibility adapters for one or two minor releases.
