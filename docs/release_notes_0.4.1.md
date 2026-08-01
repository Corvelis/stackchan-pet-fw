# Stack-chan Multi-Device Controller 0.4.1

[English](release_notes_0.4.1.md) | [日本語](release_notes_0.4.1.ja.md)

Version 0.4.1 lets M5Stack StopWatch request photo capture and front/rear lens
changes from a connected compatible phone app. It also improves camera and
microphone touch handling and Japan Standard Time／NTP synchronization for the
clock and StreetPass.

## Files to download

| Device | Update an existing install | First install or recovery |
| --- | --- | --- |
| CoreS3 + Stack-chan | `stackchan_cores3_firmware.bin` | `stackchan_cores3_factory.bin` |
| StopWatch | `stackchan_stopwatch_firmware.bin` | `stackchan_stopwatch_factory.bin` |
| AtomS3R Chatbot | `stackchan_atoms3r_firmware.bin` | `stackchan_atoms3r_factory.bin` |

- A firmware-only update preserves Wi-Fi, StreetPass, and LittleFS image data.
- A factory image installs firmware and face asset v2 together, but resets
  settings stored on the device.
- Version 0.4.0 and 0.4.1 use the same face asset v2 set, so a device already on
  0.4.0 can use a firmware-only update.
- Verify downloads with `SHA256SUMS`, and keep the bundled `LICENSE` and
  `THIRD_PARTY_NOTICES.md`.
- See the [Binary Installation Guide](install_binary.md) for flashing steps.

## StopWatch phone-camera remote

This feature does not add a camera to the StopWatch. When a compatible phone app
opens its camera screen and reports readiness with `phone_camera.state`, a camera
overlay appears at the lower-right of the normal StopWatch screen.

- Short-press the camera overlay: request a photo from the phone.
- Hold for about 0.8 seconds: switch front/rear lenses when the app reports both.
- `IN`／`OUT`: the current lens reported by the app.
- Gray state: waiting for capture or lens-change completion.
- Green state and short haptic pulse: success.
- Red state and longer haptic pulse: failure or timeout.

The overlay and its controls remain unavailable until an app reports readiness.
Clients can detect support through `phone_camera.remote_shutter.v1` and
`phone_camera.remote_lens.v1` in `device.info.capabilities`.

## Transport and safe state management

WebSocket and USB Serial use the same JSON. Only the transport that sends
`ready:true` receives requests, and ownership does not move to another transport
while an operation is pending.

- Request IDs combine a boot-session token with a monotonically increasing sequence.
- Results are accepted only when operation, transport, and request ID all match.
- Capture and lens changes are mutually exclusive.
- Capture times out after 30 seconds; lens changes time out after 10 seconds.
- Success or failure feedback returns to ready after about 0.9 seconds.
- `ready:false`, owner-transport disconnection, or display-off clears pending state.

See the [Phone Camera Remote Protocol](phone_camera_remote_protocol.md) for the
complete JSON fields and examples.

## Camera and microphone touch controls

Camera and microphone overlays now use shared gesture tracking. The firmware
tracks touch origin, press duration, maximum travel, and release position so a
hold or drag release cannot accidentally fire a short-press action.

- The StopWatch microphone overlay moves from the lower-right to the lower-left.
- The StopWatch lower-right is assigned to phone-camera controls.
- Camera and microphone touch targets are larger on StopWatch and CoreS3.
- The StopWatch camera target is excluded from petting detection.
- Any partial gesture is discarded when the display is off or interactions are not ready.

See the controls table in the [Device Guide](devices.md#controls) for each target.

## Japan Standard Time and NTP

The firmware stores Unix time as UTC and uses `Asia/Tokyo` (UTC+9) for display
and step-counter day boundaries.

- Boot-time RTC restore immediately updates the system clock.
- `streetpass.time.set` immediately updates both the system clock and RTC.
- NTP is accepted only after an actual server-response callback.
- Every Wi-Fi connection requests a fresh NTP synchronization.
- A missing response retries after 10 seconds; successful sync refreshes every six hours.
- Corrected UTC is written back to the RTC, and correction seconds are logged.

This prevents an already-valid RTC-backed system clock from being mistaken for a
new NTP response. The StopWatch clock and its 04:00 step-counter rollover use the
same Japan Standard Time basis. See [StreetPass Time](streetpass_protocol.md#time).

## Compatibility and update notes

- Phone-camera remote control is StopWatch-only. It is independent of the CoreS3
  on-device camera, HTTP `POST /capture`, and USB Serial `capture.request`.
- Capture requires a phone app that implements the `phone_camera.*` protocol.
- CoreS3 and AtomS3R Chatbot do not advertise the phone-camera capabilities.
- Flashing a factory image resets stored Wi-Fi, StreetPass, servo-home, and other settings.
- To downgrade a face-v2 device to `v0.3.1` or earlier, restore the matching
  factory image or LittleFS image as well as firmware.

## Related documents

- [Phone Camera Remote Protocol](phone_camera_remote_protocol.md)
- [StreetPass Protocol](streetpass_protocol.md)
- [Device Guide](devices.md)
- [Binary Installation Guide](install_binary.md)
- [0.4.0 Face Asset v2 Migration Release Notes](release_notes_0.4.0.md)
