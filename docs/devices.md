# Device Guide

This document explains how to build, flash, and operate the same firmware source
for CoreS3, StopWatch, and AtomS3R Chatbot targets. Shared HTTP, WebSocket, and
USB Serial APIs are documented in the README and API reference files.

## Supported Devices

| Device | PlatformIO env | Image directory | Image size | SoftAP SSID | Main traits |
| --- | --- | --- | --- | --- | --- |
| CoreS3 + Stack-chan | `m5stack-cores3` | `data/` | 240 x 240 | `StackChan-Direct` | Servo, camera, back touch |
| M5Stack StopWatch | `m5stack-stopwatch` | `data_stopwatch/` | 386 x 386 | `StopWatch` | Round display, screen petting, haptic, clock |
| M5Stack AtomS3R Chatbot | `m5stack-atoms3r-chatbot` | `data_atoms3r/` | 128 x 128 | `AtomS3R` | Small display, BtnA operation, Atomic Echo Base audio |

The setup Wi-Fi password is device-specific. The setup page is `http://192.168.4.1/wifi`.

## Classic Face Builds

The firmware can render the classic Stack-chan face procedurally as white eyes
and a mouth on a black background instead of loading character face images.
The existing environments continue to use image faces.

| Device | Classic face env |
| --- | --- |
| CoreS3 + Stack-chan | `m5stack-cores3-classic` |
| M5Stack StopWatch | `m5stack-stopwatch-classic` |
| M5Stack AtomS3R Chatbot | `m5stack-atoms3r-chatbot-classic` |

For example, build or upload the CoreS3 variant with:

```sh
pio run -e m5stack-cores3-classic
pio run -e m5stack-cores3-classic -t upload
```

Classic mode uses display-specific face proportions, blinks, and drives an 8-step
mouth animation at about 6 fps from the PCM mean amplitude. CoreS3 retains its
320 x 240 layout. StopWatch uses the round square canvas while keeping the mouth
above the lower speech bubble, and AtomS3R places the eyes below its top bubble
with thicker features that remain legible at 128 x 128. Speech bubbles and
connection-status overlays remain available. Image-specific petting, guruguru, and dizzy-face
animations and guruguru controls are disabled. Petting still updates affection;
audio playback, CoreS3 servo motion, and other non-face features continue to work.

It uses the existing M5GFX renderer and adds no Avatar library dependency.
Clients can distinguish the build through `device.info.faceRenderer`, whose value
is `classic` or `image`.
Image builds select `animated`, `transition`, `legacy`, or `emergency` at boot.
Inspect the active profile in `faceRendererMode`, its schema in `faceAssetSchema`,
and manifest status in `faceAssetManifestPresent` / `faceAssetManifestValid`.
`legacyFaceFallbackActive` is true only for the five-file fallback. The existing
`faceRenderer` field remains for compatibility. See `docs/face_renderer_v2.md`.

## Feature Differences

| Feature | CoreS3 | StopWatch | AtomS3R Chatbot |
| --- | --- | --- | --- |
| Face display | yes | yes | yes |
| WebSocket / USB Serial | yes | yes | yes |
| Microphone / speaker | yes | yes | Atomic Echo Base |
| Camera capture `/capture` | yes | no | no |
| `camera_button` UI | yes | no | no |
| Phone camera remote shutter | no | yes | no |
| Servo motion | yes | no | no |
| Back-touch petting | yes | no | no |
| Screen-touch petting | no | yes | no |
| Button-hold petting | no | no | yes |
| Petting animation | yes | yes | yes |
| Guruguru face | yes | yes | yes |
| Guruguru input | touch / IMU switchable | touch / IMU switchable | IMU only |
| Guruguru directions | 16 directions + center | 8 directions + center | 16 directions + center |
| Guruguru dizzy animation | yes | yes | yes |
| Haptic feedback | no | yes | no |
| Clock display | no | yes | no |
| Step counter / 30-day history | no | yes | no |
| StreetPass | yes | yes | yes |

`motion` and `pose` commands are accepted on devices without servos, but no
physical motion occurs. `capture.request` and HTTP `POST /capture` fail on
devices without cameras.

Implementation differences:

- CoreS3 builds with `STACKCHAN_HAS_SERVO=1`, `STACKCHAN_HAS_CAMERA=1`, and `STACKCHAN_HAS_BACK_TOUCH=1`, so it uses servo motion, camera capture, and the Stack-chan back touch sensor.
- StopWatch builds with `STACKCHAN_ROUND_DISPLAY=1`, `STACKCHAN_HAS_SCREEN_TOUCH_PETTING=1`, and `STACKCHAN_HAS_HAPTIC=1`, so it uses the round UI, screen-touch petting, haptics, and clock display.
- AtomS3R Chatbot builds with `STACKCHAN_SMALL_DISPLAY=1` and `STACKCHAN_HAS_ATOMIC_ECHO_BASE=1`, so it uses BtnA-only controls and Atomic Echo Base audio.
- StopWatch counts steps from its IMU, rolls activity days over at 04:00 Japan Standard Time, and stores up to 30 days including today. See the [Step Counter And Sync Protocol](step_counter_protocol.md).
- Petting display uses `pet_anim_0..pet_anim_15` on all three targets. Only the physical trigger differs: CoreS3 uses back touch, StopWatch uses center screen touch/drag, and AtomS3R uses BtnA hold.
- Guruguru display uses `dir*`, `blink*`, and `dizzy_*` assets on all three targets. CoreS3 and AtomS3R use `dir0..16` with center frame `blink16`; StopWatch uses the reduced `dir0..8` set with center frame `blink8`.
- Guruguru dizzy spin thresholds are target-specific. CoreS3 uses 32 total steps and 24 biased steps in a 2-second window. StopWatch uses 16 total steps and 12 biased steps. AtomS3R uses 22 total steps and 4 biased steps, and also triggers dizzy when irregular IMU shake changes of at least 0.40G continue for 900ms.
- Image files live in LittleFS as JPGs. At runtime, guruguru direction, blink, and dizzy frames are cached under `STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED` with `M5Canvas`. The canvases call `setPsram(true)`, so they prefer PSRAM allocation.

## Control Assignments By Mode

Here, normal/voice screen means the face screen, not a settings screen. Guruguru
face mode is not shown while a conversation client is connected, while a
settings screen is visible, or while the display is off.

| Device | Normal/voice screen | Guruguru face mode | Settings screens |
| --- | --- | --- | --- |
| CoreS3 + Stack-chan | Power short-press toggles display. Power double-click turns guruguru on. Back touch triggers petting. Edge flick opens settings | Power double-click turns guruguru off. Triple-click or more switches touch/IMU input. Touch input uses screen touch/drag for 16 directions plus center. IMU input uses device tilt; screen hold resets the baseline | Hold Network to switch STA/SoftAP. Servo screen saves/restores home |
| StopWatch | BtnA short-press opens settings. BtnA double-click turns guruguru on. BtnB/power short-press toggles display. Center touch/drag triggers petting. While connected, tapping the lower-left mic overlay toggles mute. When a compatible app reports camera readiness, short-press the lower-right camera overlay to take a phone photo or hold it for about 0.8 seconds to switch front/rear cameras | BtnA double-click turns guruguru off. BtnB double-click switches touch/IMU input. BtnB hold resets the baseline. Touch input uses screen touch/drag for 8 directions plus center. IMU input uses device tilt | `<` / `>` or left/right flick changes pages. Hold Network to switch STA/SoftAP |
| AtomS3R Chatbot | BtnA short-press opens Network. BtnA double-click mutes mic. BtnA hold triggers petting. BtnA triple-click turns guruguru on | BtnA triple-click turns guruguru off. IMU input only. BtnA hold resets the baseline. BtnA short-press still advances pages, and opening Network stops guruguru display. BtnA double-click is ignored | BtnA short-press changes pages. Hold Network to switch STA/SoftAP. Double-click Audio to enter volume adjust mode |

Turning the display off ends the audio state and app connection, then stops
Wi-Fi, HTTP, WebSocket, and USB Serial. Turning it on starts Wi-Fi reconnection,
so clients must reconnect as well. StreetPass BLE continues at a reduced rate.
StopWatch also lowers CPU frequency and uses short light-sleep intervals.

## Common Setup

Use the PlatformIO CLI.

```sh
pio --version
```

On macOS, if `pio` is installed but not on your shell path, it may be available at:

```sh
~/.platformio/penv/bin/pio --version
```

You may create local Wi-Fi defaults. This is optional because Wi-Fi can also be
configured from the device setup page after boot.

```sh
cp src/config_private.example.h src/config_private.h
```

PlatformIO command guide:

| Purpose | Command |
| --- | --- |
| Compile only | `pio run -e <env>` |
| Flash firmware | `pio run -e <env> -t upload` |
| Upload face images and other LittleFS data | `pio run -e <env> -t uploadfs` |
| Open serial logs | `pio device monitor -e <env>` |

For a first-time flash, run `upload` and then `uploadfs`. If only code changed,
`upload` is enough. If only face images changed, `uploadfs` is enough.

## CoreS3 + Stack-chan

### Requirements

- M5Stack CoreS3
- Stack-chan-compatible servo hardware
- USB-C cable that supports data transfer

### Build And Flash

```sh
pio run -e m5stack-cores3
pio run -e m5stack-cores3 -t upload
pio run -e m5stack-cores3 -t uploadfs
pio device monitor -e m5stack-cores3
```

Use `m5stack-cores3-release` for release builds. Release builds enable
`STACKCHAN_RELEASE_BUILD` and do not include personal Wi-Fi values from
`src/config_private.h`.

```sh
pio run -e m5stack-cores3-release
pio run -e m5stack-cores3-release -t buildfs
```

To build GitHub Releases assets for all devices, run this from the repository
root:

```sh
bash scripts/build_release_bins.sh all
```

To build one device, pass `cores3`, `stopwatch`, or `atoms3r`.

### Controls

- Flick from a screen edge: show or hide the settings screen.
- Hold the Network screen: switch between STA and SoftAP.
- Short-press the power button: display on/off.
- Back touch: petting interaction.
- Tap the microphone overlay while a WebSocket or USB Serial client is connected: mute or unmute mic streaming.
- Tap the camera overlay: send a `camera_button` event.
- Servo screen: save or restore servo home.
- Double-click the power button: toggle guruguru face mode.
- Triple-click or more on the power button while guruguru face mode is active: switch between touch input and IMU input.
- Touch the screen while guruguru face mode is active: in touch input mode, select the face direction. In IMU input mode, hold the screen to reset the IMU baseline.

## M5Stack StopWatch

### Requirements

- M5Stack StopWatch
- USB-C cable that supports data transfer

### Build And Flash

```sh
pio run -e m5stack-stopwatch
pio run -e m5stack-stopwatch -t upload
pio run -e m5stack-stopwatch -t uploadfs
pio device monitor -e m5stack-stopwatch
```

StopWatch uploads LittleFS data from `data_stopwatch/` by default. To use a
different image directory, select it explicitly at build time with
`STACKCHAN_FACE_DATA_DIR`.
Release asset generation uses the `m5stack-stopwatch-release` environment.

### Controls

- Short-press BtnA: show or hide the settings screen.
- Double-click BtnA: toggle guruguru face mode.
- Short-press BtnB or the power button: display on/off.
- Double-click BtnB while guruguru face mode is active: switch between touch input and IMU input.
- Hold BtnB while guruguru face mode is active: reset the IMU baseline.
- Briefly hold near the center of the normal face screen, or drag from near the center: petting interaction. This is disabled while the display is off or the settings screen is visible.
- While a WebSocket or USB Serial client is connected, tap the microphone overlay on the lower-left rim of the normal face screen: mute or unmute mic streaming. This tap target is excluded from petting detection.
- Touch or drag the normal face screen while guruguru face mode is active: in touch input mode, this selects one of 8 directions plus center. In IMU input mode, device tilt controls the face direction.
- Tap the `<` / `>` buttons at the top of the settings screen: move to the previous or next settings page.
- Flick left or right on the settings screen: move to the previous or next settings page. A left flick moves forward, and a right flick moves backward.
- Settings page order: Network -> Display -> Audio -> Power -> Steps -> StreetPass -> Network. StopWatch has no Servo page.
- Tap the QR screen: return to the Network screen.
- Network screen: hold to switch between STA and SoftAP. In SoftAP mode, tap `Wi-Fi QR` or `Setup QR` to show the corresponding QR code. When connected in STA mode, tap `Setup QR` to show the setup QR code.
- Display screen: tap `-` / `+` to adjust brightness in 20-point steps. Tap `Screen Off` / `Screen On` to toggle the display.
- Audio screen: tap `-` / `+` to adjust speaker volume in 20-point steps.
- Power screen: tap `Low Power On` / `Low Power Off` to toggle low-power mode.
- Steps screen: view today's count, the 04:00 reset, and stored daily history.
- StreetPass screen: tap `Turn On` / `Turn Off` to toggle StreetPass, and tap `Profile` / `History` to switch between the local profile and latest history view.

StopWatch has no camera or servo. HTTP `/capture`, USB Serial `capture.request`,
servo home calibration, and on-device camera capture are unavailable. A
compatible phone app can advertise readiness and the current lens with
`phone_camera.state`; the lower-right camera overlay then requests a
phone-camera capture. Its `IN` / `OUT` badge shows the selected lens, and
holding it for about 0.8 seconds switches lenses. See the
[Phone Camera Remote Protocol](phone_camera_remote_protocol.md).

## M5Stack AtomS3R Chatbot

### Requirements

- M5Stack AtomS3R
- Atomic Echo Base
- USB-C cable that supports data transfer

### Build And Flash

```sh
pio run -e m5stack-atoms3r-chatbot
pio run -e m5stack-atoms3r-chatbot -t upload
pio run -e m5stack-atoms3r-chatbot -t uploadfs
pio device monitor -e m5stack-atoms3r-chatbot
```

AtomS3R uses the board definition in `boards/m5stack-atoms3r.json` and uploads
LittleFS data from `data_atoms3r/`. To use a different image directory, select
it explicitly at build time with `STACKCHAN_FACE_DATA_DIR`.
Release asset generation uses the `m5stack-atoms3r-chatbot-release` environment.

### Controls

AtomS3R Chatbot has no touch controls. Use BtnA short-press, double-click, and
hold actions. When the display is off, short-press or hold BtnA to turn it on.
The settings page order is Network -> StreetPass -> Audio -> Power -> normal
face screen.

| Screen | BtnA short-press | BtnA double-click | BtnA hold |
| --- | --- | --- | --- |
| Normal face | Open Network | Mute or unmute mic streaming | Petting interaction. The reaction lingers briefly after release |
| Network | Move to StreetPass. If a QR code is shown, return to Network | Show or hide `Setup QR` | Switch between STA and SoftAP |
| StreetPass | Move to Audio | Switch Status / Profile / Latest views | Toggle StreetPass |
| Audio | Move to Power in view mode. Increase volume by 20 in volume adjust mode | Toggle volume adjust mode | Decrease volume by 20 only in volume adjust mode. Continuing to hold repeats about once per second |
| Power | Return to normal face | No action | Toggle low-power mode |

Triple-click BtnA on the normal face screen to toggle guruguru face mode.
AtomS3R guruguru face mode always uses IMU input. While guruguru face mode is
active, BtnA hold resets the IMU baseline instead of triggering petting.

AtomS3R Chatbot has no camera or servo. HTTP `/capture`, USB Serial
`capture.request`, servo home calibration, and camera-overlay operation
are unavailable. It has no touch controls, Display page, or Servo page.

## Wi-Fi Setup

If Wi-Fi is not configured or STA connection fails, each target starts a setup
SoftAP.

| Device | SSID | Password | URL |
| --- | --- | --- | --- |
| CoreS3 | `StackChan-Direct` | `stackchan123` | `http://192.168.4.1/wifi` |
| StopWatch | `StopWatch` | `stopwatch123` | `http://192.168.4.1/wifi` |
| AtomS3R | `AtomS3R` | `atoms3r123` | `http://192.168.4.1/wifi` |

When the device is connected in STA mode, the same setup page is available at
`/wifi` on the IP address shown on the Network screen.

## Face Images

All devices read the same LittleFS paths, but each target uploads from a
different source directory and uses a different image size.

| Device | Upload source | Size |
| --- | --- | --- |
| CoreS3 | `data/` | 240 x 240 |
| StopWatch | `data_stopwatch/` | 386 x 386 |
| AtomS3R | `data_atoms3r/` | 128 x 128 |

Plain `pio run -e <env> -t uploadfs` uses the normal directory shown above. To use
another image directory such as `data_local/`, `data_stopwatch_local/`, or
`data_atoms3r_local/`, select it explicitly at build time:

```sh
STACKCHAN_FACE_DATA_DIR=data_local pio run -e m5stack-cores3 -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_stopwatch_local pio run -e m5stack-stopwatch -t uploadfs
STACKCHAN_FACE_DATA_DIR=data_atoms3r_local pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

Run `scripts/build_release_bins.sh all` without `STACKCHAN_FACE_DATA_DIR` for
GitHub Releases. Release assets include the normal image directories. `*_local`
directories are local replacement sets and are not committed or included in
release assets.

The firmware keeps `.png` paths such as `/idle.png` in its configuration for
compatibility, but runtime image files are standardized to `.jpg`. `FaceController`
falls back to the matching `.jpg` stem when the `.png` path is not present.

## Troubleshooting

- If faces are missing, check that the JPG files exist in the target environment's image directory, then run `pio run -e <env> -t uploadfs`.
- If the Wi-Fi setup page does not open, confirm that your phone or PC is connected to that target's setup SSID.
- WebSocket uses port `8080`; HTTP uses port `80`.
- `/capture` failures on StopWatch and AtomS3R are expected because those devices do not have cameras.
- If AtomS3R audio is not working, confirm that the Atomic Echo Base is attached and that the firmware was built with `m5stack-atoms3r-chatbot`.
