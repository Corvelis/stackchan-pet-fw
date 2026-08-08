# Stack-chan Multi-Device Controller 0.5.0

[English](release_notes_0.5.0.md) | [日本語](release_notes_0.5.0.ja.md)

Version 0.5.0 adds four experience modes, Timekeeper, and Travel mode to CoreS3 and M5Stack StopWatch. AtomS3R Chatbot keeps its existing Conversation and Guruguru features.

## Changes by Target

| Target | Changes in 0.5.0 |
| --- | --- |
| CoreS3 + Stack-chan | Adds four experience modes, Timekeeper, Travel, Travel assets, and related APIs |
| M5Stack StopWatch | Adds four experience modes, Timekeeper, Travel, Travel assets, and related APIs |
| AtomS3R Chatbot | Updates the release version to 0.5.0 and adds the shared `device.info.bootId`; existing user-visible Conversation/Guruguru behavior, capabilities, and 65 face images remain unchanged |

AtomS3R does not add the four-sector selector, Timekeeper, Travel mode, Travel assets, or new capabilities such as `experience.mode.v1`.

## Highlights

### Four Experience Modes

- Manages Conversation, Guruguru, Timekeeper, and Travel as independent top-level modes.
- Open the selector with a right flick from the CoreS3 left edge or a roughly 0.7-second hold of the StopWatch yellow BtnA.
- Defers a mode change until audio playback finishes.
- Sends `experience.mode.changed` after the actual change.

See [Experience Modes and On-Device Controls](experience_modes.md) for complete controls.

### Timekeeper

- Stopwatch, laps, and elapsed milestones.
- Countdown from 10 seconds through 120 minutes, with 1/3/5/10/30/60/120-minute presets.
- 10/30/60-second time challenge with three difficulties, six ranks, and affection rewards.
- Pomodoro with automatic work/break transitions; defaults to 25/5 minutes and four cycles.
- Target-specific CoreS3 back-touch and StopWatch yellow/blue-button controls.
- Safe pause/abort on display off or mode change.
- Dedicated smile animation for challenge results within 200ms.

### Travel Mode

- Adds nine new and six reused expressions, for 15 total expressions on CoreS3/StopWatch.
- Two picker pages for photo and mood/playful choices.
- Holds the selected face still without lip sync or blink.
- Adds a 3x3 same-character prompt, row-major splitter naming, and target-specific picker generator.

Release image counts are 76 JPGs on CoreS3, 68 on StopWatch, and the unchanged 65 on AtomS3R.

## App Integration

All three targets add a boot-scoped `bootId` to `device.info`.

CoreS3 and StopWatch `device.info` additionally add:

- `experienceMode` / `experienceModeRevision`
- saved `pomodoro` configuration
- `experience.mode.v1`
- `device.communication.suspending.v1`
- `timekeeper.v1`
- `timekeeper.pomodoro.v1`

New JSON messages include:

- `experience.mode.changed`
- `timekeeper.event`
- `timekeeper.announcement.prefetch` / `timekeeper.announcement.result`
- `timekeeper.pomodoro.config.get` / `.set` / `.result`
- `device.communication.suspending`

Countdown finish and full Pomodoro completion remain pending for up to 120 seconds until an app reports the announcement result. See the [Timekeeper and Experience Mode Protocol](timekeeper_protocol.md).

## Display and Existing Features

- Composites Timekeeper UI into the same full-screen canvas as the face to reduce flicker during overlay refresh.
- Hides microphone, camera, and speech-bubble overlays in Timekeeper/Travel so they do not cover dedicated content.
- Suppresses petting and shake in those modes.
- Routes the CoreS3 power-button double-click through the new mode manager when switching Conversation/Guruguru.
- Keeps affection, StreetPass, StopWatch steps, phone-camera remote, and CoreS3 servo/camera features.

## Updating and LittleFS

GitHub Releases provide `firmware` and `factory` for each target.

| Method | Firmware | LittleFS | Travel mode |
| --- | --- | --- | --- |
| `firmware` | Updates to 0.5.0 | Preserves existing data | From 0.4.1, new picker images are absent and controls fall back to available existing faces |
| `factory` | Updates to 0.5.0 | Replaces with 0.5.0 images | All 15 expressions and two picker pages |
| Source `uploadfs` | Unchanged | Writes target `data*` | Adds the complete Travel assets |

The 0.5.0 firmware boots with the 0.4.1 face-v2 LittleFS. Use a factory update or `uploadfs` for the full Travel experience. A factory image rewrites the full flash including saved configuration; record settings before installing it.

See the [Binary Installation Guide](install_binary.md) for flashing details.

## Development and Image Tools

- `travel_3x3_prompt.txt`
- `split_firmware_sheet.py --output-naming travel-expressions`
- `build_travel_picker_pages.py`
- Tests for Travel naming, 3x3 grid, picker dimensions/layout
- All-11-or-none Travel validation; partial release sets fail
- Host C++ state-machine tests for stopwatch, countdown, challenge, and Pomodoro
- Paired Japanese/English experience-control and Timekeeper protocol documents

Ignored `data_local/`, `data_stopwatch_local/`, and `data_atoms3r_local/` remain local-only and are not committed or included in GitHub Releases. Normal and release builds use `data/`, `data_stopwatch/`, and `data_atoms3r/`.
`build_release_bins.sh` rejects a set `STACKCHAN_FACE_DATA_DIR` and unexpected
`dist/` entries such as stale demo videos.

## Compatibility and Limits

- Timekeeper and Travel are available only on CoreS3/StopWatch.
- AtomS3R returns no new fields or capabilities other than `bootId`; it keeps its existing Conversation/Guruguru behavior and 65-image asset set.
- The shared API remains at `protocolVersion: 2`. Detect new features through their versioned capabilities.
- v1 does not let an app change the experience mode; select it on the device.
- Pomodoro work/break duration uses the app API; cycle count uses the device UI.
- Speech-bubble cues are hidden in Timekeeper/Travel.
- Compatible clients should inspect capabilities and ignore unknown JSON messages.
- Legacy `face` and `face_mode` commands remain as semantic compatibility adapters for one or two minor releases.
- When downgrading, restore the factory image or LittleFS that matches the older firmware as well as the firmware itself. Leaving only face asset v2 installed may omit images required by older firmware.

## Release Checks

- Build all six image/classic environments across three targets.
- Validate 76 CoreS3, 68 StopWatch, and 65 AtomS3R images.
- Run host automation including the Timekeeper state machine.
- Test Travel splitting, picker placement, and complete-set validation.
- Check paired Japanese/English documentation and local absolute paths.
- Manually verify mode selection, Timekeeper, and Travel controls on CoreS3/StopWatch.

## Related Documents

- [Experience Modes and On-Device Controls](experience_modes.md)
- [Timekeeper and Experience Mode Protocol](timekeeper_protocol.md)
- [Face Image Builder](../tools/face_image_builder/README.en.md)
- [Face Image File Inventory](face_image_inventory.md)
- [Binary Installation Guide](install_binary.md)
