# Stack-chan Multi-Device Controller

[日本語](README.md) | [English](README.en.md)

Shared firmware for CoreS3 + Stack-chan, M5Stack StopWatch, and AtomS3R Chatbot.
It provides face animation, audio streaming, petting／shake／guruguru reactions,
StreetPass, and HTTP, WebSocket, and USB Serial connections. CoreS3 also supports
servos and camera capture.

This repository contains the device firmware. External client implementations are
out of scope.

## Latest Release: v0.5.0

### StopWatch Phone Camera

- Sends shutter and front/rear lens requests to a compatible phone app over WebSocket or USB Serial.
- Short-press the lower-right camera overlay to capture; hold it for about 0.8 seconds to switch lenses.
- Shows the current lens as `IN`／`OUT` and reports pending, success, and failure through the display and haptics.
- Validates transport, request ID, and operation while handling timeouts, disconnects, and display-off safely.

### Touch Controls and Time Sync

- Uses shared gesture tracking for camera and microphone overlays to prevent long-press and drag misfires.
- Moves the StopWatch microphone overlay to the lower-left and enlarges StopWatch／CoreS3 touch targets.
- Standardizes time on `Asia/Tokyo` and immediately applies RTC, app, and NTP updates to the system clock.
- Confirms NTP responses and supports Wi-Fi reconnect sync, retry, and six-hour refreshes.

### Protocol and Distribution

- Adds `phone_camera.remote_shutter.v1` and `phone_camera.remote_lens.v1` capabilities.
- Publishes the phone-camera remote protocol in Japanese and English.
- Distributes update `firmware` and first-install `factory` binaries for all three targets.

See the [0.5.0 Release Notes](docs/release_notes_0.5.0.md) for details and the
[English Changelog](CHANGELOG.md) for the complete version history.

## Supported Devices

| Device | Additional hardware | Image-face env | Classic-face env | Image directory |
| --- | --- | --- | --- | --- |
| CoreS3 + Stack-chan | Stack-chan-compatible servos | `m5stack-cores3` | `m5stack-cores3-classic` | `data/` |
| M5Stack StopWatch | none | `m5stack-stopwatch` | `m5stack-stopwatch-classic` | `data_stopwatch/` |
| AtomS3R Chatbot | Atomic Echo Base | `m5stack-atoms3r-chatbot` | `m5stack-atoms3r-chatbot-classic` | `data_atoms3r/` |

See the [Device Guide](docs/devices.md) for target-specific controls, flashing
instructions, and unsupported features.

## Main Features

- LittleFS JPG lip-sync, blink, petting, direction, and dizzy animations
- WebSocket JSON control, PCM playback, and microphone streaming
- USB CDC／USB Serial control for direct Android connections
- Wi-Fi STA, direct SoftAP, and browser-based Wi-Fi setup
- HTTP status and CoreS3 camera capture
- BLE StreetPass between Stack-chan devices
- Interaction events for petting, shake, connection, camera, and other actions
- Affection management and status overlays
- StopWatch step counter, 30-day history, and synchronization
- StopWatch shutter and lens requests to a compatible phone app
- CoreS3 servo reactions

## Installation

### Use Release Binaries

GitHub Releases provide two files for each target:

- `factory`: first installation, complete image migration, or recovery
- `firmware`: firmware update that preserves the current LittleFS images

See the [Binary Installation Guide](docs/install_binary.md) for file selection,
flash addresses, and downgrade precautions.

### Build from Source

Use PlatformIO Core or the PlatformIO extension for VS Code.

```sh
cp src/config_private.example.h src/config_private.h
pio run -e <env>
pio run -e <env> -t upload
pio run -e <env> -t uploadfs
```

Replace `<env>` with an environment from the supported-device table. Regular face
images are already included. Wi-Fi can be configured from the device setup page,
and Git ignores `src/config_private.h`.

## Face Rendering

| Configuration | Purpose | Features |
| --- | --- | --- |
| Image face | Regular release; recommended | Image-based lip-sync, blink, petting, direction, and dizzy animations |
| Classic face | Source build only | Procedural white eyes and mouth, lip-sync, blink, and speech bubbles |
| Legacy fallback | Firmware-only migration | Limited use of five old base images already on the device |

Classic face is a separate `*-classic` build environment, not a runtime setting.
When replacing the character, validate a complete set in a Git-ignored
`data_local*` directory before promoting it into release assets.

- [Face Image Builder](tools/face_image_builder/README.en.md)
- [Face Renderer v2 Design](docs/face_renderer_v2.md)
- [Face Asset v2 Migration Guide](docs/face_asset_migration.md)
- [Face Image File Inventory](docs/face_image_inventory.md)
- [Face Image Group Usage](docs/face_image_usage_analysis.md)

## On-Device Controls

Petting, shake, guruguru, display-off, and settings controls differ by target.
See the controls table in the [Device Guide](docs/devices.md#controls).

## Connection Points

| Connection | Main uses |
| --- | --- |
| HTTP | status, Wi-Fi setup, CoreS3 camera, settings page |
| WebSocket | JSON control, PCM playback, microphone stream, interaction events |
| USB Serial | direct Android control, audio, and image transfer |
| BLE | StreetPass profile exchange |

Speech bubbles appear when a client sends cues aligned with TTS PCM segments.
See the protocol documents for details.

- [USB Serial Protocol](docs/usb_serial_protocol.md)
- [Speech Bubble Protocol](docs/speech_bubble_protocol.md)
- [StreetPass Protocol](docs/streetpass_protocol.md)
- [Affection API](docs/device_affection_api.md)
- [StopWatch Step Sync Protocol](docs/step_counter_protocol.md)
- [StopWatch Phone Camera Remote Protocol](docs/phone_camera_remote_protocol.md)

## Documentation

| Purpose | Document |
| --- | --- |
| Target builds and controls | [Device Guide](docs/devices.md) |
| Binary installation and recovery | [Binary Installation Guide](docs/install_binary.md) |
| v0.5.0 update | [0.5.0 Release Notes](docs/release_notes_0.5.0.md) |
| v0.4.0 face asset v2 migration | [0.4.0 Release Notes](docs/release_notes_0.4.0.md) |
| Complete version history | [Changelog](CHANGELOG.md) |
| Face image creation | [Face Image Builder](tools/face_image_builder/README.en.md) |
| Development checks | [Contributing](CONTRIBUTING.md#english) |

Public documents are maintained as Japanese／English pairs. CI rejects additions
or updates that change only one side.

## Development and Release

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
python3 scripts/check_bilingual_docs.py
bash scripts/build_release_bins.sh all
```

CI builds image-face and classic-face environments for all three targets. Release
binary generation excludes personal settings and checks that local build paths
are not embedded in binaries.

## License

The current firmware source, bundled original images, and samples use the
[MIT License](LICENSE). The current source tree and v0.5.0 release assets contain
no third-party character material.

Historical release binaries containing Tsukuyomi-chan images are not relicensed
under the current MIT License. Follow their original release notices and material
provider terms. See [Third-Party Notices](THIRD_PARTY_NOTICES.md) for dependencies.
