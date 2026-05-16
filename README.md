# Stack-chan CoreS3 Controller

[English](README.md) | [日本語](README.ja.md)

Firmware for an M5Stack CoreS3 based Stack-chan. It provides face rendering,
servo motion, microphone/speaker streaming, camera capture, touch/shake
reactions, and network interfaces for external control.

This repository contains only the Stack-chan firmware side. It documents the
HTTP and WebSocket interfaces exposed by the device; external client
implementations are out of scope.

## Features

- M5Stack CoreS3 firmware built with PlatformIO and Arduino.
- LittleFS-based face image rendering.
- WebSocket JSON control channel.
- WebSocket binary PCM audio channel.
- HTTP camera capture and status endpoints.
- STA Wi-Fi mode and SoftAP direct-connection mode.
- Local petting and shake reactions.
- Persistent affection state controlled through WebSocket events.
- Device-side interaction events for petting, shake, session start, and level changes.
- On-device settings screen for network, display, audio, and power controls.
- Battery, microphone, affection, thermal, and low-power visual overlays.

## Hardware

- M5Stack CoreS3
- Stack-chan compatible servo hardware
- PlatformIO development environment

The PlatformIO environment is defined as `m5stack-cores3` in `platformio.ini`.

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
- `src/`: firmware source code.
- `data/`: local LittleFS data directory. Runtime PNGs are ignored by Git.
- `docs/device_affection_api.md`: detailed device-side affection API notes.

Optional:

- `.vscode/extensions.json`: editor recommendation only.

Ignored:

- `.pio/`: build output and downloaded dependencies.
- `.DS_Store`: macOS metadata.
- `src/config_private.h`: local Wi-Fi credentials.
- `assets/`: local source/reference image assets.
- `data/*.png`: runtime face images derived from third-party character materials.

## Setup

1. Install PlatformIO and confirm that `pio --version` works.
2. Copy the private config template:

```sh
cp src/config_private.example.h src/config_private.h
```

3. Edit `src/config_private.h`:

```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
```

4. Prepare the face PNG files listed below under `data/`.
5. Connect the CoreS3 by USB.
6. Build and upload the firmware:

```sh
pio run --target upload
```

7. Upload the LittleFS image data:

```sh
pio run --target uploadfs
```

8. Open the serial monitor:

```sh
pio device monitor
```

The device prints the Wi-Fi mode and IP address on boot.

## Face Images

The firmware loads face images from LittleFS using the paths in `src/config.h`.
Place 240 x 240 PNG files with these exact names under `data/`.
Folders under `assets/` are only source/reference work areas and are not read
directly by the firmware. Rename/export the runtime images to the filenames
below, place them directly under `data/`, then run `pio run --target uploadfs`
to write them to LittleFS.

| File | Expected image | Example source from Tsukuyomi-chan standing material |
| --- | --- | --- |
| `idle.png` | Default idle face, mouth closed. | `01-01a 基本-目ふんわり-口閉じ.png` |
| `listen.png` | Listening/ready face, mouth closed. | `01-02a 基本-目ぱっちり-口閉じ.png` |
| `talk_0.png` | Normal speaking frame, mouth closed. | `01-01a 基本-目ふんわり-口閉じ.png` |
| `talk_1.png` | Normal speaking frame, mouth open. | `01-01b 基本-目ふんわり-口開け.png` |
| `blink.png` | Normal blink frame. | `01-01c 基本-目ふんわり-目閉じ.png` |
| `smile.png` | Idle smile frame used only in the normal affection tier. | `01-03ac 基本-にっこり-口閉じ.png` |
| `good_0.png` | Positive/master-recognized face, mouth closed. | `02-04a 喜び・称賛-口閉じ.png` |
| `good_1.png` | Positive/master-recognized face, mouth open. | `02-04b 喜び・称賛-口開け.png` |
| `good_blink.png` | Positive/master-recognized blink frame. | `02-04c 喜び・称賛-目閉じ.png` |
| `bad_0.png` | Negative/not-master face, mouth closed. | `10-03ac 覚悟-口閉じ.png` |
| `bad_1.png` | Negative/not-master face, mouth open. | `10-03b 覚悟-口開け.png` |
| `photo_0.png` | Camera/photo mode face, mouth closed. | `09-01a 祈り-口閉じ.png` |
| `photo_1.png` | Camera/photo mode face, mouth open. | `09-01b 祈り-口開け.png` |
| `photo_blink.png` | Camera/photo mode blink frame, mouth closed. | `09-01c 祈り-目閉じ・口閉じ.png` |
| `photo_blink_talk.png` | Camera/photo mode blink frame, mouth open. | `09-01d 祈り-目閉じ・口開け.png` |
| `photo_master_0.png` | Master-photo mode face, mouth closed. | `02-02ac ほんわか-手を組む-口閉じ.png` |
| `photo_master_1.png` | Master-photo mode face, mouth open. | `02-02b ほんわか-手を組む-口開け.png` |
| `nadenade_0.png` | Petting/nadenade face, mouth closed. | `02-01ac ほんわか-基本ポーズ-口閉じ.png` |
| `nadenade_1.png` | Petting/nadenade face, mouth open. | `02-01b ほんわか-基本ポーズ-口開け.png` |
| `furifuri_0.png` | Shake/furifuri face, mouth closed. | `07-01ac 大変です！-口閉じ.png` |
| `furifuri_1.png` | Shake/furifuri face, mouth open. | `07-01b 大変です！-口開け.png` |

Mouth-open files ending in `_1.png` are used during speaking animation. The
matching `_0.png` files are used for the closed-mouth frame and for the base
face in that mode.

Additional optional face files can be added for richer visual states. The
firmware falls back to the base faces above if these files are missing.
`talk_guarded_0.png` and `talk_attached_0.png` are only needed when the
closed-mouth speaking frame should differ from the idle face. If they are
missing, the firmware uses `idle_guarded_0.png` / `idle_attached_0.png` as the
closed-mouth speaking frame.

| File group | Files |
| --- | --- |
| Guarded affection tier | `idle_guarded_0.png`, `blink_guarded_0.png`, `talk_guarded_0.png`, `talk_guarded_1.png` |
| Attached affection tier | `idle_attached_0.png`, `blink_attached_0.png`, `talk_attached_0.png`, `talk_attached_1.png` |
| Guarded petting | `pet_guarded_0.png`, `pet_guarded_1.png`, `pet_blink_guarded_0.png` |
| Attached petting | `pet_attached_0.png`, `pet_attached_1.png`, `pet_blink_attached_0.png` |
| Guarded shake | `shake_guarded_0.png`, `shake_guarded_1.png` |
| Attached shake | `shake_attached_0.png`, `shake_attached_1.png` |
| Thermal and low power | `tired_0.png`, `tired_talk.png`, `tired_blink.png`, `exhausted_0.png`, `exhausted_talk.png`, `exhausted_blink.png`, `low_power_0.png`, `low_power_talk.png`, `low_power_blink.png` |

The local image set used during development was prepared from
"つくよみちゃん万能立ち絵素材（花兎*様）". These image files are not included in
this repository. Download the original material from the official distribution
page, follow its terms, export the required 240 x 240 PNGs, and place them in
`data/`.

Credit for the local image set:

```text
フリー素材キャラクター「つくよみちゃん」
https://tyc.rei-yumesaki.net/
つくよみちゃん万能立ち絵素材（花兎*様）
https://tyc.rei-yumesaki.net/material/illust/
```

## Network Modes

The firmware supports two network modes:

- STA mode: connects to the Wi-Fi credentials in `src/config_private.h`.
- SoftAP mode: starts its own access point.

Default SoftAP settings:

```text
SSID: StackChan-Direct
Password: stackchan123
IP: 192.168.4.1
```

Hold the touch screen while the info screen is visible to switch between STA and
SoftAP. The selected mode is saved in device preferences and applied after
restart.

## On-Device Controls

- Flick from an edge of the touch screen to show or hide the settings screen.
- Use the settings tabs for Network, Display, Audio, and Power.
- Hold the Network settings page to switch between STA and SoftAP.
- Use Display settings to adjust brightness, turn the screen off, or enable low
  power mode.
- Use Audio settings to adjust speaker volume.
- Use Power settings to check thermal state, battery state, and low-power
  suggestion.
- Press the power button to toggle the display.
- When a WebSocket client is connected, tap the microphone overlay on the right
  side of the face screen to mute or unmute mic streaming.

## Connection Points

### HTTP

Base URL:

```text
http://<stack-chan-ip>
```

Endpoints:

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/status` | Returns device status as JSON. |
| `POST` | `/capture` | Captures a JPEG image from the camera. |
| `OPTIONS` | `/status`, `/capture` | CORS preflight support. |

Example `/status` response:

```json
{
  "cameraReady": false,
  "networkMode": "STA",
  "wsClientConnected": true,
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
  "lowPowerMode": false,
  "thermalLevel": "Normal",
  "chipTempC": 48.5,
  "pmicTempC": 42.0,
  "batteryLevel": 87,
  "charging": false,
  "wifiConnected": true,
  "ip": "192.168.1.10"
}
```

### WebSocket

Endpoint:

```text
ws://<stack-chan-ip>:8080/
```

Text frames are UTF-8 JSON commands. Binary frames are signed 16-bit PCM audio:
the client can send playback audio to the device speaker, and the device sends
microphone audio while it is in listening state and mic streaming is not muted.
Muting the microphone stops only the outgoing microphone audio stream; it does
not disconnect the WebSocket client.

Audio settings:

```text
Sample rate: 16000 Hz
Channels: 1
Format: signed 16-bit PCM
Recommended chunk: 20 ms input, 60 ms playback
```

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

Set face mode:

```json
{ "type": "face_mode", "value": "photo" }
```

Allowed values: `normal`, `photo`, `photo_master`, `nadenade`, `pet`,
`furifuri`, `shake`.

Show a specific face image:

```json
{ "type": "face", "value": "idle" }
```

Common values: `idle`, `listen`, `talk_0`, `talk_1`, `bad_0`, `bad_1`,
`good_0`, `good_1`, `good_blink`, `photo_0`, `photo_1`, `photo_blink`,
`photo_blink_talk`, `photo_master_0`, `photo_master_1`, `nadenade_0`,
`nadenade_1`, `furifuri_0`, `furifuri_1`, `blink`, `smile`, and the optional
guarded, attached, thermal, and low-power face names listed in the Face Images
section.

Set motion:

```json
{ "type": "motion", "name": "center" }
```

Allowed names: `center`, `look_left`, `look_right`, `look_away`, `not_master`,
`nod`, `small_nod`, `small_bounce`, `lean_forward`, `wobble`, `shy_nod`,
`thinking`.

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

The device also broadcasts `interaction.event` messages for physical and
device-side events such as `petting`, `shake`, `session_start`, `level_up`, and
`level_down`.

Short-term state behavior:

- Immediately after a WebSocket connection, the device resets `mood` and
  `confusion` to `0`, sends `affection.state`, then sends the `session_start`
  `interaction.event`.
- `mood` decays toward `0` by `2` every 10 seconds. `confusion` decays toward
  `0` by `10` every 10 seconds.
- During TTS playback and playback-buffer draining, natural decay and
  decay-driven `affection.state` broadcasts are paused to prioritize smooth
  audio playback.
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

- If the screen says Wi-Fi is not configured, create `src/config_private.h` and
  set `WIFI_SSID` / `WIFI_PASSWORD`.
- If faces are missing, make sure all required PNG files are in `data/` and run
  `pio run --target uploadfs`.
- If HTTP works but WebSocket does not, check that the client connects to port
  `8080`, not port `80`.
- If SoftAP mode is active, connect the phone, PC, or other client device to `StackChan-Direct` and
  use `192.168.4.1` as the device IP.

## License Notes

Firmware source code in this repository is licensed under the MIT License. See
`LICENSE`.

Direct third-party library license notes are listed in `THIRD_PARTY_NOTICES.md`.

This repository does not include third-party character image files. Prepare
runtime images locally and follow the terms of the material you use.
