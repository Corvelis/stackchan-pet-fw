# Timekeeper and Experience Mode Protocol v1

[English](timekeeper_protocol.md) | [日本語](timekeeper_protocol.ja.md)

This document specifies CoreS3 and M5Stack StopWatch experience-mode events, Timekeeper events, announcement requests, Pomodoro configuration, and the pre-display-off notification. WebSocket and USB Serial carry the same JSON messages. AtomS3R Chatbot does not implement this protocol.

## Capability Discovery

Send `device.info.get` after connecting and use these messages only when the required capability is present.

```json
{
  "type": "device.info",
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "experienceMode": "conversation",
  "experienceModeRevision": 0,
  "pomodoro": {
    "workDurationMs": 1500000,
    "breakDurationMs": 300000,
    "totalCycles": 4,
    "configRevision": 1
  },
  "capabilities": [
    "device.info",
    "experience.mode.v1",
    "device.communication.suspending.v1",
    "timekeeper.v1",
    "timekeeper.pomodoro.v1"
  ]
}
```

- `deviceId` is the stable device identifier persisted in NVS.
- `bootId` changes on every boot. Deduplicate events with `deviceId`, `bootId`, and `eventId` together.
- `experienceMode` is `conversation`, `guruguru`, `timekeeper`, or `travel`.
- `experienceModeRevision` increments after each mode change in the current boot.
- `pomodoro` describes the saved next-session configuration.

The shared `protocolVersion` remains `2` for compatibility with existing APIs. Detect these additions through the versioned capabilities above.

## Experience Mode Change

Modes are selected on the device. After a change, the firmware sends:

```json
{
  "type": "experience.mode.changed",
  "version": 1,
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "mode": "travel",
  "previousMode": "conversation",
  "revision": 1
}
```

When a choice is made during audio playback, this event is sent only after playback ends and the mode actually changes. v1 does not define a client-to-device mode-change command.

## `timekeeper.event`

The firmware sends this when state changes or a milestone is reached.

```json
{
  "type": "timekeeper.event",
  "version": 1,
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "eventId": "b7e31c9a4d20f011-e12",
  "sessionId": "cd-3",
  "activity": "countdown",
  "event": "finished",
  "state": "finished",
  "ageMs": 0,
  "elapsedMs": 300000,
  "remainingMs": 0,
  "durationMs": 300000,
  "announcement": {
    "key": "countdown.finished",
    "importance": "critical",
    "maxAgeMs": 120000,
    "delivery": "until_result"
  }
}
```

### Common Fields

| Field | Meaning |
| --- | --- |
| `eventId` | Event identifier unique within the current boot |
| `sessionId` | Session identifier prefixed with `sw-`, `cd-`, `challenge-`, or `pomodoro-` |
| `activity` | `stopwatch`, `countdown`, `ten_second_challenge`, or `pomodoro` |
| `event` | `started`, `paused`, `resumed`, `reset`, `lap`, `milestone`, `finished`, `result`, `aborted`, `transition`, or `completed` |
| `state` | `ready`, `running`, `paused`, `finished`, `aborted`, or `completed` |
| `ageMs` | Time from event creation to transmission |
| `elapsedMs` | Session or current-phase elapsed time |
| `reason` | `display_off` or `mode_changed` for automatic suspension. Events with a reason are not announcement candidates |

### Activity-Specific Fields

| Activity / Event | Additional fields |
| --- | --- |
| `countdown` | `remainingMs`, `durationMs` |
| `lap` | `lapIndex`, `lapDurationMs`, optional `previousLapDurationMs`, `lapDeltaMs`, `isBestLap` |
| `ten_second_challenge` | `targetMs`, `difficulty` |
| challenge `result` | `signedErrorMs`, `absoluteErrorMs`, `rank`, `affectionDelta` |
| `pomodoro` | `phase`, optional `transition`, `cycleIndex`, `totalCycles`, `remainingCycles`, `isFinalCycle`, `workDurationMs`, `breakDurationMs`, optional `phaseDurationMs`, `remainingMs`, `configRevision` |
| named `milestone` | `milestone`: `remaining_30_seconds`, `remaining_10_seconds`, `work_half`, `work_remaining_5_minutes`, `work_finishing_soon`, `break_remaining_1_minute`, or `break_finishing_soon` |

Countdown halfway, remaining-five-minute, and remaining-one-minute events, plus Stopwatch elapsed milestones, use `event:milestone` and time fields but may omit the `milestone` string.

Pomodoro `transition` is `work_to_break` or `break_to_work`; `phase` is `work` or `break`. The final completed event omits the phase.

## Announcement Requests

An app that speaks Timekeeper events should act only on events containing `announcement`.

| Field | Values |
| --- | --- |
| `key` | Stable content-selection key, such as `stopwatch.lap` or `pomodoro.transition.work_to_break` |
| `importance` | `normal`, `high`, or `critical` |
| `maxAgeMs` | Deadline relative to event creation for starting playback |
| `delivery` | `best_effort` or `until_result` |

`best_effort` has no resend guarantee. Countdown finish and full Pomodoro completion use `critical` and `until_result`. The device retains those events for up to 120 seconds. After a disconnect, a reconnecting client can send `device.info.get` to receive one resend with the same `eventId`.

### Prefetch

For a challenge result within 200ms, the device may send this while its smile animation is playing:

```json
{
  "type": "timekeeper.announcement.prefetch",
  "version": 1,
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "eventId": "b7e31c9a4d20f011-e13",
  "sessionId": "challenge-4",
  "activity": "ten_second_challenge",
  "event": "result",
  "ageMs": 50,
  "elapsedMs": 10042,
  "targetMs": 10000,
  "difficulty": "medium",
  "absoluteErrorMs": 42,
  "rank": "amazing",
  "affectionDelta": 11,
  "announcement": {
    "key": "ten_second_challenge.result",
    "importance": "high",
    "maxAgeMs": 30000,
    "delivery": "best_effort",
    "playbackGate": "matching_timekeeper_event"
  }
}
```

Use prefetch only to prepare audio. Do not play it until the matching `timekeeper.event` with the same `eventId` arrives, as required by `playbackGate`.

### Announcement Result

A client receiving `delivery:until_result` returns:

```json
{
  "type": "timekeeper.announcement.result",
  "version": 1,
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "eventId": "b7e31c9a4d20f011-e12",
  "result": "sent"
}
```

`result` is `queued`, `sent`, `suppressed`, `expired`, or `failed`. `queued` is intermediate. The other four are terminal and release the retained event. Results with mismatched identifiers are ignored.

## Pomodoro Configuration

### Get

```json
{"type":"timekeeper.pomodoro.config.get","version":1,"requestId":"pomodoro-get-001"}
```

### Set

```json
{
  "type": "timekeeper.pomodoro.config.set",
  "version": 1,
  "requestId": "pomodoro-set-001",
  "workDurationMs": 1500000,
  "breakDurationMs": 300000
}
```

Work accepts 60000–7200000ms and break accepts 60000–3600000ms. Both must be multiples of 60000ms. `requestId` is required for Set. Cycle count is selected on the device from 1 through 12 and is not changed by this v1 API.

### Result

```json
{
  "type": "timekeeper.pomodoro.config.result",
  "version": 1,
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "requestId": "pomodoro-set-001",
  "result": "saved",
  "appliesTo": "next_start",
  "workDurationMs": 1500000,
  "breakDurationMs": 300000,
  "configRevision": 2
}
```

- `result` is `current` for Get, `saved` for a successful Set, or `rejected`.
- `appliesTo` is `next_start` while idle or `next_session` while running/paused.
- `reason` is `unsupported_version`, `invalid_request`, `duration_out_of_range`, or `storage_failed`.

## Pre-Suspend Notification

On display off, the device first pauses or aborts Timekeeper and then sends this before communication stops:

```json
{
  "type": "device.communication.suspending",
  "version": 1,
  "deviceId": "stackchan_8f3a21",
  "bootId": "b7e31c9a4d20f011",
  "sequence": 1,
  "reason": "display_off"
}
```

This is a notification and the device does not wait for an ACK. After a short bounded flush window, it suspends WebSocket, USB Serial, HTTP, and Wi-Fi. Reconnect after display on and restart synchronization with `device.info.get`.

## Existing APIs While a Mode Is Active

- `display.speech_bubble.*` is intentionally hidden in Timekeeper and Travel. Any existing bubble is cleared.
- `state:listening` is ignored outside Conversation.
- `state:speaking` is ignored in Guruguru and Travel.
- `experienceModeRevision` and event order are scoped to one boot. Always include `bootId` when comparing across reconnects or restarts.
