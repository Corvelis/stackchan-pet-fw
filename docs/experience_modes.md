# Experience Modes and On-Device Controls

[English](experience_modes.md) | [日本語](experience_modes.ja.md)

CoreS3 and M5Stack StopWatch image-face builds provide four experience modes: Conversation, Guruguru, Timekeeper, and Travel. AtomS3R Chatbot keeps its existing Conversation and Guruguru controls; it does not provide the four-sector selector, Timekeeper, or Travel mode.

## Selecting a Mode

### CoreS3

1. Flick right from the left edge to open the mode selector.
2. Tap Conversation at the top, Guruguru on the right, Timekeeper at the bottom, or Travel on the left.
3. The selector closes after the choice.

A left flick from the right edge opens settings. That opposite-edge gesture closes the mode selector when it is open; a right flick from the left edge closes settings when settings are open.

### StopWatch

1. Hold the yellow BtnA for about 0.7 seconds.
2. On the round four-sector selector, tap Conversation at the top, Guruguru on the right, Timekeeper at the bottom, or Travel on the left.
3. Release BtnA to close the selector.

If audio is playing or its buffer is draining, the selected change waits until playback finishes. The device starts in Conversation mode and does not persist the selected mode across a reboot.

## Mode Behavior

| Mode | Purpose | Display and input |
| --- | --- | --- |
| Conversation | Voice conversation and regular reactions | Uses lip sync, blink, petting, shake, microphone/camera overlays, and speech bubbles |
| Guruguru | Move the face with touch or IMU input | Uses direction faces and the dizzy animation |
| Timekeeper | Stopwatch, timers, and challenge | Owns the screen with dedicated UI; suppresses petting, shake, microphone/camera overlays, and speech bubbles |
| Travel | Hold a photo-friendly expression | Keeps a selected still face; suppresses normal lip sync, blink, petting, shake, microphone/camera overlays, and speech bubbles |

`state:listening` is ignored outside Conversation. `state:speaking` is also ignored in Guruguru and Travel. Timekeeper can play app-generated announcements, but it does not show their speech bubbles.

## Timekeeper

Use the top tabs, or horizontal flicks while stopped, to select Stopwatch, Timer, or Challenge. Timer contains Countdown and Pomodoro. Tab changes and activity flicks are disabled while a measurement is running.

### Shared Controls

| Action | CoreS3 | StopWatch |
| --- | --- | --- |
| Start/pause/resume | Short back-touch tap | Short press of yellow BtnA |
| Lap | Tap the background outside controls while Stopwatch is running | Short press of blue BtnB while running |
| Reset after pause or finish | Tap the background outside controls | Short press of blue BtnB |
| Open mode selector | Flick right from the left edge | Hold yellow BtnA for about 0.7 seconds |

In Timekeeper mode, StopWatch reserves BtnB for lap/reset and never uses it to turn the display off. Use the power button or settings to turn the display off.

### Stopwatch

- Sends a milestone event at one minute and every five minutes afterward.
- Lap events include the split duration, delta from the previous lap, and whether the lap is the best so far.

### Countdown

- Defaults to five minutes.
- `-` and `+` adjust one minute at a time. The internal range is 10 seconds through 120 minutes.
- Tap the time panel to choose 1, 3, 5, 10, 30, 60, or 120 minutes.
- Emits applicable halfway, five-minute, one-minute, 30-second, and 10-second milestones.

### Pomodoro

- Defaults to 25 minutes of work, five minutes of break, and four cycles.
- `-` and `+` select 1 through 12 cycles.
- A client changes work and break durations through `timekeeper.pomodoro.config.*`. Work accepts 1–120 minutes and break accepts 1–60 minutes, both in whole minutes.
- Work and break phases transition automatically. Completion follows the final work phase.
- A configuration change made during an active session applies to the next session.

### Time Challenge

- Target duration is 10, 30, or 60 seconds.
- Difficulty is Low, Medium, or High.
- Start the challenge, then perform the start action again when the target time has elapsed.
- Results are ranked `perfect`, `amazing`, `excellent`, `close`, `near`, or `try_again`.
- Affection increases according to target duration, difficulty, and error. An error of at most 200ms triggers a dedicated smile animation.

### Suspension and Display Off

- Leaving Timekeeper pauses a running Stopwatch, Countdown, or Pomodoro with reason `mode_changed`.
- The same action aborts a running challenge.
- Display off pauses regular measurement, aborts a challenge with reason `display_off`, and then suspends app communication.

See the [Timekeeper and Experience Mode Protocol](timekeeper_protocol.md) for app integration.

## Travel Mode

Entering or leaving Travel resets to the normal face. A picked expression remains still until another expression is selected or it is reset.

### Expressions

| Page | Expressions |
| --- | --- |
| Photo | Heart, Smile, Wink, Sparkle, Surprised, Shy, Delicious, Peace |
| Mood/playful | Dizzy, Wobbly, Sulky, Angry, Mischief, Teary, Yawn |

### Controls

| Action | CoreS3 | StopWatch |
| --- | --- | --- |
| Open/close picker | Short back-touch tap | Single press of yellow BtnA |
| Change page | Flick left or right in the picker | Flick left or right in the picker |
| Select expression | Tap a thumbnail | Tap a thumbnail |
| Reset to normal | Tap Normal in the picker, or double-tap the face screen | Tap the picker center, or press BtnA two or more times |

Travel assets exist only in CoreS3 `data/` and StopWatch `data_stopwatch/`. A firmware-only update from 0.4.1 preserves the existing LittleFS, so the new picker pages are absent. In that case, opening the picker advances through available existing expressions instead. To use all 15 expressions and the picker, install the 0.5.0 `factory` binary or upload the target LittleFS with `uploadfs`.

See the [Face Image Builder](../tools/face_image_builder/README.en.md) and [Face Image File Inventory](face_image_inventory.md) for generation, splitting, and placement.
