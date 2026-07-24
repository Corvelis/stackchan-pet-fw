# Stack-chan Multi-Device Controller

[日本語](README.md) | [English](README.en.md)

Firmware for an M5Stack CoreS3 based Stack-chan, M5Stack StopWatch, and M5Stack
AtomS3R Chatbot. It provides face rendering, microphone/speaker streaming,
petting/shake reactions, guruguru mode, StreetPass, and HTTP / WebSocket / USB
Serial control interfaces. On CoreS3, it also provides servo motion and camera
capture.

This repository contains only the device firmware side. It documents the
HTTP, WebSocket, and USB Serial interfaces exposed by the device; external client
implementations are out of scope.

## Latest Release: v0.4.0

- Migrated all three targets to face image v2 with lip-sync, blink, petting, guruguru, and dizzy animations.
- Removed legacy static release faces such as `good_*`, `bad_*`, and `photo_*`, while retaining a limited fallback for existing devices.
- Improved CoreS3 640×480 camera capture and servo, microphone, and voice-session stability.
- Added a StopWatch step counter, 30-day history with a 04:00 rollover, affection rewards, and app synchronization.
- Added a shared WebSocket／USB Serial speech-bubble protocol and expanded diagnostics.
- Added tools for generating, splitting, converting, and validating 4×4 base-face and petting sprite sheets.
- Prepared update `firmware` and first-install／complete-migration `factory` binaries for all three targets.

See the [0.4.0 Release Notes](docs/release_notes_0.4.0.md) for details and the
[English Changelog](CHANGELOG.md) for the complete version history.

## Features

- CoreS3 / StopWatch / AtomS3R Chatbot firmware built with PlatformIO and Arduino.
- LittleFS-based face image rendering sized per target device.
- WebSocket JSON control channel.
- WebSocket binary PCM playback and microphone streaming.
- USB CDC / USB Serial control channel for Android direct USB connections.
- HTTP status endpoints and CoreS3 camera capture.
- CoreS3 on-face camera button event for requesting client-side capture handling.
- BLE StreetPass-style exchange between Stack-chan devices.
- StreetPass profile, latest 30 encounter records, unread notification, and sync API for companion apps.
- STA Wi-Fi mode, SoftAP direct-connection mode, and USB Serial mode.
- Browser-based Wi-Fi setup with SSID scanning and multiple saved networks.
- Guruguru mode with face direction tracking, blink frames, and dizzy animation through device-specific IMU, touch, or button controls.
- Local petting and shake reactions. The physical interaction differs by device.
- Persistent affection state controlled through WebSocket events.
- Device-side interaction events for petting, shake, camera button, session start, and level changes.
- On-device settings for network, display, audio, power, StreetPass, and StopWatch Steps.
- StopWatch device-side step counting, 04:00 day rollover, 30-day history, and client sync.
- Battery, microphone, affection, thermal, and low-power visual overlays.

## Hardware

- PlatformIO development environment
- Target device:

| Device | Additional hardware |
| --- | --- |
| M5Stack CoreS3 + Stack-chan | Stack-chan-compatible servo hardware |
| M5Stack StopWatch | none |
| M5Stack AtomS3R Chatbot | Atomic Echo Base |

PlatformIO environments:

| Device | Image face env | Classic face env | LittleFS image directory |
| --- | --- | --- | --- |
| CoreS3 + Stack-chan | `m5stack-cores3` | `m5stack-cores3-classic` | `data/` |
| StopWatch | `m5stack-stopwatch` | `m5stack-stopwatch-classic` | `data_stopwatch/` |
| AtomS3R Chatbot | `m5stack-atoms3r-chatbot` | `m5stack-atoms3r-chatbot-classic` | `data_atoms3r/` |

See the [Device Guide](docs/devices.md) for target-specific build commands,
controls, and unsupported features.

The regular `firmware` and `factory` binaries on GitHub Releases use image
faces. A classic face is not a runtime setting; build the target's separate
`*-classic` environment from source. It needs no face-image LittleFS and keeps
procedural eyes and mouth, blinking, eight-step lip sync, speech bubbles, and
status overlays. Image-specific petting, guruguru, and dizzy-face animations
and guruguru controls are disabled.

## Install PlatformIO

Use either the VS Code PlatformIO extension or PlatformIO Core CLI.

Check that the `pio` command is available:

```sh
pio --version
```

If `pio` is not found but PlatformIO is installed on macOS, it may be available
at:

```sh
~/.platformio/penv/bin/pio --version
```

## Repository Contents

Required:

- `platformio.ini`: PlatformIO build configuration.
- `boards/`: board definitions not bundled with PlatformIO.
- `src/`: firmware source code.
- `src/*Controller.{h,cpp}`: feature controllers for display, audio, communication, and sensors.
- `CONTRIBUTING.md`: build checks and commit guidance for changes.
- `data/`: CoreS3 LittleFS face images. Default JPG files are included.
- `data_stopwatch/`: StopWatch LittleFS face images.
- `data_atoms3r/`: AtomS3R Chatbot LittleFS face images.
- `docs/devices.md`: device-specific build and operation guide.
- `docs/device_affection_api.md`: detailed device-side affection API notes.
- `docs/usb_serial_protocol.md`: USB Serial frame protocol notes for app clients.
- `docs/step_counter_protocol.md`: StopWatch step-counter and sync JSON protocol.
- `docs/speech_bubble_protocol.md`: TTS speech-bubble protocol details.
- `docs/streetpass_protocol.md`: StreetPass BLE and JSON API details.
- `docs/release_notes_0.4.0.md`: 0.4.0 update, migration, and downgrade notes.
- `CHANGELOG.md`: complete English version history.

Optional:

- `.vscode/extensions.json`: editor recommendation only.
- `tools/face_image_builder/`: tools for building face image sets from AI-generated sprite sheets.

Ignored:

- `.pio/`: build output and downloaded dependencies.
- `dist/`: generated firmware and factory images for GitHub Releases.
- `.DS_Store`: macOS metadata.
- `src/config_private.h`: local Wi-Fi credentials.
- `assets/`: local source/reference image assets.
- `data_local/`, `data_stopwatch_local/`, and `data_atoms3r_local/`: personal complete image sets.
- `face_assets_v2_work/`: sprite-sheet splitting, conversion, and review workspace.
- `legacy_face_assets_local/`: legacy migration and fallback-test fixtures.

Speech bubbles work with both image and classic faces. They are not subtitles
generated automatically by the firmware. A connected client first checks for
`display.speech_bubble.v1`, then sends `display.speech_bubble.cue` immediately
before the corresponding TTS PCM segment. WebSocket and USB Serial use the same
JSON. See the [Speech Bubble Protocol](docs/speech_bubble_protocol.md) for
ordering, limits, layout, and clearing behavior.

## Setup

1. Install PlatformIO and confirm that `pio --version` works.
2. Copy the private config template:

```sh
cp src/config_private.example.h src/config_private.h
```

3. Optionally edit `src/config_private.h`.
   You can also leave it unconfigured and register Wi-Fi from the browser setup
   page after boot:

```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
```

4. The bundled face asset v2 set is ready to use. Only when replacing the
   character, use the [Face Image Builder](tools/face_image_builder/README.en.md)
   to create a complete v2 set and `face_assets.json`, then place it in an
   ignored `data_local*` directory.
5. Connect the target device by USB.
6. Optionally compile without flashing. Replace `<env>` with the target
   environment:

```sh
pio run -e <env>
```

7. Upload the firmware:

```sh
pio run -e <env> -t upload
```

8. Upload the LittleFS image data:

```sh
pio run -e <env> -t uploadfs
```

9. Open the serial monitor:

```sh
pio device monitor -e <env>
```

The device prints the Wi-Fi mode and IP address on boot.
For a first-time flash, run `upload` and then `uploadfs`. If only code changed,
`upload` is enough. If only face images changed, `uploadfs` is enough.

For ready-to-flash `.bin` installation, see the [Binary Installation Guide](docs/install_binary.md).
Release builds should define `STACKCHAN_RELEASE_BUILD` so `src/config_private.h` local Wi-Fi credentials are not included.
Generate device-specific GitHub Releases assets into `dist/` with:
Do not set `STACKCHAN_FACE_DATA_DIR` for release assets; they use the normal image directories.

```sh
bash scripts/build_release_bins.sh all
```

The generated filenames match the release asset table in the
[Binary Installation Guide](docs/install_binary.md).

## Face Images

> [!IMPORTANT]
> Release assets have completed the face asset v2 cutover and select the
> `animated` renderer. Legacy static faces are not shipped. Firmware-only
> updates can still use an old five-file set already present on a device as a limited fallback.

See the [0.4.0 Release Notes](docs/release_notes_0.4.0.md) for the change summary
and update guidance for existing installations. See the
[English Changelog](CHANGELOG.md) for the complete version history.

The firmware loads JPG frames from LittleFS. See [Face Renderer v2](docs/face_renderer_v2.md)
for naming, profiles, and renderer selection, and the
[Face Asset v2 Migration Guide](docs/face_asset_migration.md) for update behavior.

| Device | Upload source | Size | JPG files | Center direction |
| --- | --- | ---: | ---: | --- |
| CoreS3 + Stack-chan | `data/` | 240x240 | 65 | `dir16` / `blink16` |
| StopWatch | `data_stopwatch/` | 386x386 | 57 | `dir8` / `blink8` |
| AtomS3R Chatbot | `data_atoms3r/` | 128x128 | 65 | `dir16` / `blink16` |

Each directory contains 16 base frames, 16 petting frames, direction faces, one
center blink, 15 dizzy frames, and `face_assets.json`. Base frames are named
`base_m0_e0..base_m3_e3`, with mouth rows and eye columns.

Plain `uploadfs` uses the release directory shown above. Put personal images in
ignored `data_local*` directories and select one explicitly:

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_atoms3r_local pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

To replace the character, create complete 4x4 base and petting sheets plus
direction, center-blink, and dizzy sets. The
[Face Image Builder](tools/face_image_builder/README.en.md) provides prompts for
creating sprite sheets from a reference image, the splitter, bundled samples,
and target-specific resize, naming, and manifest generation. Generate and
validate under `face_assets_v2_work/` first, then move only a complete,
device-tested set into `data_local*` or the release `data*` directories.

Affection, authentication, thermal, low-power, and camera state do not replace
the base face. Their calculations and protective behavior remain active, with
relevant information shown through overlays.

Bundled images and samples use the same [`MIT License`](LICENSE) as the
firmware source.

## Network Modes

The firmware supports two network modes:

- STA mode: connects to saved Wi-Fi settings or credentials in `src/config_private.h`.
- SoftAP mode: starts its own access point.

Default SoftAP settings:

| Device | SSID | Password | IP |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `192.168.4.1` |
| StopWatch | `StopWatch` | `stopwatch123` | `192.168.4.1` |
| AtomS3R | `AtomS3R` | `atoms3r123` | `192.168.4.1` |

On the Network screen, hold the CoreS3 / StopWatch touch screen or hold AtomS3R
Chatbot BtnA to switch between STA and SoftAP. CoreS3 / StopWatch restart after
the switch; AtomS3R Chatbot switches the connection in place. The selected mode
is saved in device preferences.

## Wi-Fi Setup Page

If STA mode starts with no configured Wi-Fi credentials, the device
automatically falls back to SoftAP mode. Connect a phone or PC to the target
device's SoftAP SSID, then open this URL in a browser:

```text
http://192.168.4.1/wifi
```

The setup page can scan nearby SSIDs, save an SSID/password, edit or delete
saved networks, and change connection priority. Saved networks are tried from
top to bottom. When the device is already connected in STA mode, open `/wifi`
on the IP address shown on the Network screen to use the same setup page.

The on-device Network screen can show QR codes for Wi-Fi setup.

- SoftAP mode: use `Wi-Fi QR` to connect to the target device's SoftAP, then use
  `Setup QR` to open `http://192.168.4.1/wifi`.
- STA connected: use `Setup QR` to open `http://<device-ip>/wifi`. The phone or
  PC must be on the same Wi-Fi network as the device.
- Tap the QR screen to return to the Network screen.

## On-Device Controls

- On-device controls differ by target. See the [Device Guide](docs/devices.md) for details.
- On CoreS3, flick from an edge of the touch screen to show or hide the settings screen.
- Use the settings tabs for Network, Display, Audio, Servo, and Pwr.
- The `Servo` tab is for servo-equipped CoreS3 builds. It is not shown on StopWatch or AtomS3R Chatbot.
- If Wi-Fi is not configured, the device starts the target-specific setup Wi-Fi network.
  Connect a phone or PC to it, then open
  `http://192.168.4.1/wifi` to configure Wi-Fi.
- On the Network settings page, hold the CoreS3 / StopWatch touch screen or hold AtomS3R Chatbot BtnA to switch between STA and SoftAP.
- Use Display settings to adjust brightness or turn the screen off. AtomS3R Chatbot does not have a Display page.
- Use Audio settings to adjust speaker volume. On AtomS3R Chatbot, double-click BtnA on the Audio screen to enter volume adjust mode, then short-press to increase volume or hold to decrease volume.
- Use Servo settings to save the CoreS3 face reference position or return to the saved position.
- Use Power settings to check thermal state, battery state, and low-power mode.
- On StopWatch, use Steps settings to view today's count and stored daily history.
- Low-power mode caps display brightness and reduces idle face updates and nodding motion.
  Audio playback and lip-sync during speech continue.
- On CoreS3 / StopWatch, press the power button to toggle the display.
- While the screen is off, petting and shake interactions are disabled.
- Turning the display off ends the audio state and suspends Wi-Fi, HTTP, WebSocket, and USB Serial app communication. Turning it on starts Wi-Fi reconnection, so clients must reconnect as well. StreetPass BLE continues on a reduced schedule while the display is off.
- On CoreS3 / StopWatch, when a WebSocket or USB Serial client is connected, tap the microphone
  overlay (right side on CoreS3, lower-right rim on StopWatch) to mute or unmute mic streaming.
- On AtomS3R Chatbot, double-click BtnA on the normal face screen to mute or unmute mic streaming.
- On CoreS3, when a WebSocket or USB Serial client is connected, tap the camera overlay above the
  microphone overlay to send a `camera_button` event. The device does not send
  image data over WebSocket; network clients should call HTTP `POST /capture`,
  and USB Serial clients should send a `capture.request` message.
  StopWatch and AtomS3R Chatbot do not have cameras, so this operation and image capture are unavailable.

## Connection Points

### HTTP

Base URL:

```text
http://<stack-chan-ip>
```

Endpoints:

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/status` | Returns device status as JSON. `charging` is `true` while charging or externally powered over USB/VBUS. |
| `POST` | `/capture` | Captures a JPEG image from the CoreS3 camera. Fails on devices without cameras. |
| `GET`, `POST` | `/speaker-test` | Plays a short diagnostic tone for StopWatch / AtomS3R Chatbot speaker checks. |
| `GET`, `POST` | `/mic-test` | Returns microphone diagnostic metrics as JSON. |
| `OPTIONS` | `/status`, `/capture`, `/speaker-test`, `/mic-test` | CORS preflight support. |

Example `/status` response:

```json
{
  "cameraReady": false,
  "networkMode": "STA",
  "wsClientConnected": true,
  "usbSerialClientConnected": false,
  "affection": 500,
  "mood": 0,
  "confusion": 0,
  "affectionSeq": 1,
  "affectionLevel": "normal",
  "levelIndex": 3,
  "visualTier": "normal",
  "styleId": "normal",
  "timestampMs": 345678,
  "freeHeap": 123456,
  "freePsram": 1234567,
  "displayOn": true,
  "brightness": 160,
  "volume": 180,
  "micMuted": false,
  "micStreaming": true,
  "mic": {
    "enabled": true,
    "captureActive": true,
    "queuedPackets": 1,
    "queueCapacity": 8,
    "sentChunks": 120,
    "droppedChunks": 0,
    "captureUnderruns": 0,
    "queueOverflows": 0,
    "sendFails": 0,
    "lastPeak": 820
  },
  "voicePerf": {
    "active": false,
    "sessionSeq": 3,
    "loopCount": 0,
    "maxLoopGapMs": 0,
    "maxFaceFrameGapMs": 0
  },
  "lowPowerMode": false,
  "thermalLevel": "Normal",
  "chipTempC": 48.5,
  "pmicTempC": 42.0,
  "batteryLevel": 87,
  "charging": false,
  "streetpass": {
    "enabled": true,
    "bleReady": true,
    "scanActive": false,
    "advertising": true,
    "exchangeInProgress": false,
    "paused": false,
    "pauseReason": ""
  },
  "currentState": "listening",
  "audioState": "listening",
  "wifiConnected": true,
  "ip": "192.168.1.10"
}
```

This example shows representative fields. `mic` also includes per-transport
counters, recent processing timestamps, and gain/filter settings; `voicePerf`
includes maximum loop, rendering, audio, and transport timings during speech;
and `streetpass` includes persistent state and BLE exchange details. Diagnostic
clients should ignore unknown fields.

### WebSocket

Endpoint:

```text
ws://<stack-chan-ip>:8080/
```

Text frames are UTF-8 JSON commands. Client-to-device binary frames are signed
16-bit PCM audio for speaker playback. Device-to-client microphone binary frames
use a 16-byte header followed by signed 16-bit PCM while the device is in
listening state and mic streaming is not muted. While the remote VAD is inactive,
the device may pause microphone frame transmission during local silence; it keeps
the WebSocket connected and resumes with a short pre-roll when local input rises.
Muting the microphone stops only the outgoing microphone audio stream; it does
not disconnect the WebSocket client.

Audio settings:

```text
Sample rate: 16000 Hz
Channels: 1
Format: signed 16-bit PCM
Recommended chunk: 40 ms microphone input, 60 ms playback
Microphone packet: "MIC1" magic, uint32 little-endian seq, uint32 little-endian timestampMs, uint16 little-endian sampleCount, uint16 flags, PCM payload
Microphone flags: bit 0 = stream segment start
```

### USB Serial

The firmware also accepts the same command/event model over USB CDC / USB
Serial. This path is intended for Android devices that open the device USB port
with Android USB Host APIs such as `usb-serial-for-android`.

The device supports two USB Serial input forms:

- Newline-delimited raw JSON for simple diagnostics, for example
  `{"type":"ping","id":"phone_001"}\n`.
- Binary SCU1 frames for JSON, TTS PCM, microphone PCM, and capture image data.

SCU1 frame layout:

```text
magic     4 bytes  "SCU1"
version   1 byte   0x01
type      1 byte
flags     1 byte
reserved  1 byte   0x00
seq       4 bytes  little-endian
length    4 bytes  little-endian
payload   length bytes
crc32     4 bytes  little-endian
```

CRC32 uses the standard IEEE polynomial and covers `version` through `payload`;
the `magic` bytes and final CRC field are not included.

Frame types:

| Type | Direction | Payload |
| --- | --- | --- |
| `0x01` JSON | both | UTF-8 JSON command/event |
| `0x02` TTS PCM | client to device | raw signed 16-bit little-endian PCM, 16 kHz mono |
| `0x03` MIC PCM | device to client | existing `MIC1` microphone packet |
| `0x04` capture request | client to device | JSON request payload |
| `0x05` capture image chunk | device to client | JPEG bytes |
| `0x08` ping | client to device | optional JSON payload |
| `0x09` pong | device to client | JSON payload |

The same JSON commands listed below are accepted inside SCU1 type `0x01`.
For TTS playback, send `{"type":"state","value":"speaking"}`, then one or more
type `0x02` PCM frames, then `{"type":"state","value":"idle"}`. Playback starts
after the prebuffer threshold is reached or when `idle` drains the remaining
buffer.

USB Serial clients must treat the USB stream as mixed binary data. Development
builds may print diagnostic text on the same CDC port, so clients should scan
for the `SCU1` magic and resynchronize instead of assuming every byte belongs to
a frame. See `docs/usb_serial_protocol.md` for the complete protocol and Android
client notes.

## StreetPass

StreetPass uses BLE to exchange a Stack-chan profile card with nearby devices.
The device stores only the latest 30 encounter records; companion apps should
fetch those records over WebSocket or USB Serial JSON and keep long-term history
on the app side.

Device UI:

- The normal face screen shows an envelope icon when unread encounters exist.
- The `Pass` settings tab shows StreetPass On/Off, the local profile, and history.
- History is shown newest first and can be deleted one record at a time.
- Japanese names and messages are displayed as UTF-8; half-width katakana is normalized to full-width katakana for display.

Audio coexistence:

- BLE scan, GATT, and advertisement stop while the device is listening with the microphone streaming, speaking, or draining playback.
- If the microphone is turned off and `micStreaming=false`, StreetPass can run even while the high-level state is `Listening`.
- Display-off mode stops Wi-Fi, HTTP, WebSocket, and USB Serial app communication. Only StreetPass BLE continues with longer scan/exchange intervals, and Wi-Fi reconnection starts after display-on.

Companion app APIs:

- `streetpass.time.set`: sync UTC time.
- `streetpass.profile.get` / `streetpass.profile.set`: read or edit the local profile.
- `streetpass.encounters.get`: fetch encounter records.
- `streetpass.encounters.ack`: mark records as synced or read.
- `streetpass.encounters.delete`: delete specific device-side records.

See `docs/streetpass_protocol.md` for the full BLE and JSON API specification.

## WebSocket JSON Commands

Set the high-level state:

```json
{ "type": "state", "value": "listening" }
```

Allowed values: `idle`, `listening`, `speaking`.

Set recognition result:

```json
{ "type": "auth", "result": "master" }
```

Allowed values: `master`, `not_master`, `unknown`, `none`.

Set voice activity:

```json
{ "type": "vad", "active": true }
```

Set a semantic face mode:

```json
{ "type": "face_mode", "value": "normal" }
```

Allowed values: `normal`, `photo`, `photo_master`, `nadenade`, `pet`,
`furifuri`, `shake`.

New integrations should use semantic values such as `normal`, `pet`, and
`shake`. Under v2, `photo` and `photo_master` remain accepted only as
compatibility inputs for old clients and do not select the old camera-specific
images. `nadenade` / `pet` starts the
petting animation, while `furifuri` / `shake` starts the dizzy animation.

Legacy `face` command compatibility:

```json
{ "type": "face", "value": "idle" }
```

`face` is retained for old clients. In v2 it does not select an individual
legacy image; it redraws the base animation for the active semantic state. The
five-file legacy fallback accepts only `idle`, `listen`, `talk_0`, `talk_1`, and
`blink` as direct names. New integrations should send semantic state through
`state`, `vad`, `pet`, and `face_mode` instead.

Set motion:

```json
{ "type": "motion", "name": "center" }
```

Allowed names: `center`, `look_left`, `look_right`, `look_away`, `not_master`,
`nod`, `small_nod`, `small_bounce`, `lean_forward`, `wobble`, `shy_nod`,
`thinking`.

StopWatch and AtomS3R Chatbot accept these commands, but they do not have servos,
so no physical motion occurs.

Set servo pose directly:

```json
{ "type": "pose", "pan": 90, "tilt": 82 }
```

Default servo limits are:

```text
pan: 45..135
tilt: 60..120
```

Set petting state:

```json
{ "type": "pet", "active": true }
```

`nadenade` is accepted as an alias for `pet`.

Inspect audio playback state:

```json
{ "type": "audio.playback_diag", "requestId": "playback-001" }
```

This is available over WebSocket and USB Serial. It reports buffering, PCM
receive/drop counts, underflow, speaker queue, speech-bubble state, and speech
processing timings. See [USB Serial Protocol](docs/usb_serial_protocol.md#audio-diagnostics).

## Affection Commands

The device owns the canonical affection state. It accepts event messages and
applies weighting, cooldown, duplicate suppression, persistence, and display
updates on the firmware side.

Inbound event example:

```json
{
  "type": "affection.event",
  "id": "client-000001",
  "event": "praise",
  "confidence": 0.92,
  "intensity": 0.8,
  "source": "phone",
  "requestId": "req-000001"
}
```

Supported event names:

```text
talk
positive_talk
negative_talk
praise
thanks
apology
master_seen
session_start
petting
shake
```

State request:

```json
{ "type": "affection.get", "requestId": "req-000002" }
```

Device response:

```json
{
  "type": "affection.state",
  "requestId": "req-000002",
  "seq": 128,
  "timestampMs": 345678,
  "affection": 642,
  "mood": 35,
  "confusion": 0,
  "level": "nakayoshi",
  "levelIndex": 4,
  "visualTier": "attached",
  "styleId": "friendly"
}
```

Device identity:

```json
{ "type": "device.info.get", "requestId": "req-device-001" }
```

The device returns a stable NVS-backed `deviceId` in `device.info`.

Character-scoped sync:

```json
{ "type": "affection.sync.state", "requestId": "req-sync-001", "characterId": "char_shared_001" }
{ "type": "affection.sync.apply", "requestId": "req-sync-002", "characterId": "char_shared_001", "affection": 680 }
```

The device stores `syncedBaseAffection` per `characterId` and returns
`unsyncedDelta` from `affection.sync.state`.

The device also broadcasts `interaction.event` messages for physical and
device-side events such as `petting`, `shake`, `camera_button`, `session_start`,
`level_up`, and `level_down`. `camera_button` is sent by CoreS3 only while a
WebSocket or USB Serial client is connected, uses phase `pressed`, and is locked
until the next client text/binary response or a 30-second timeout.

Short-term state behavior:

- Immediately after a WebSocket connection, the device resets `mood` and
  `confusion` to `0`, sends `affection.state`, then sends the `session_start`
  `interaction.event`.
- `mood` decays toward `0` by `2` every 10 seconds. `confusion` decays toward
  `0` by `10` every 10 seconds.
- During TTS playback and playback-buffer draining, natural decay and
  decay-driven `affection.state` broadcasts are paused to prioritize smooth
  audio playback.
- Conversation-driven affection changes update the internal state and WebSocket
  response immediately, while the on-screen heart meter and delta indicator are
  refreshed after TTS playback.
- Shake detection raises `confusion` to `100`.

Reset request:

```json
{ "type": "affection.reset", "value": 500 }
```

Debug adjust request:

```json
{ "type": "affection.debug_adjust", "delta": 10 }
```

Debug set request:

```json
{ "type": "affection.debug_set", "levelIndex": 5, "mood": 40, "persist": false }
```

See `docs/device_affection_api.md` for the detailed device-side affection API.

## Troubleshooting

- If Wi-Fi is not configured, connect to the target device's SoftAP SSID and open
  `http://192.168.4.1/wifi` to register an SSID/password. When building from
  source, you can also set `WIFI_SSID` / `WIFI_PASSWORD` in `src/config_private.h`.
- If faces are missing, make sure all required JPG files are in the target device's image directory and run
  `pio run -e <env> -t uploadfs`.
- If HTTP works but WebSocket does not, check that the client connects to port
  `8080`, not port `80`.
- If SoftAP mode is active, connect the phone, PC, or other client device to
  the target device's SoftAP SSID and use `192.168.4.1` as the device IP. The Wi-Fi setup
  page is `/wifi`.
- `/capture` failures on StopWatch and AtomS3R Chatbot are expected because those devices do not have cameras.
- If USB Serial ping/pong times out, check that the client can resynchronize on
  the `SCU1` magic and ignores diagnostic text printed before or between frames.

## License Notes

Firmware source code, bundled runtime images, sprite-sheet samples, and
image-generation references in this repository are all licensed under the
same MIT License. See `LICENSE`.

Direct third-party library license notes are listed in `THIRD_PARTY_NOTICES.md`.

The current source tree and v0.4.0 release assets do not include third-party
character source material. The MIT License permits copying, modification,
redistribution, sale, and commercial use. Redistributions of an image or a
substantial portion of it must include the copyright and permission notices
from `LICENSE`. Verify the rights and terms for any replacement images.

This MIT grant does not relicense third-party images embedded in historical
release binaries. A historical binary containing Tsukuyomi-chan images remains
subject to the `THIRD_PARTY_NOTICES.md` bundled with that release and the
original material provider's terms. Ailog's tracked original images from
`0.3.0` onward use the MIT License; third-party images do not.
