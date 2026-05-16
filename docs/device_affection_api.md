# Stack-chan Device Affection API

[English](device_affection_api.md) | [日本語](device_affection_api.ja.md)

This document describes the affection-related WebSocket API exposed by the
Stack-chan firmware. It is a device-side reference: it documents what messages
the firmware accepts, how it stores state, and what responses it may send.

External client implementation details, detection logic, UI design, and
conversation handling are intentionally out of scope.

## Transport

WebSocket endpoint:

```text
ws://<stack-chan-ip>:8080/
```

Text frames are UTF-8 JSON. Binary frames are reserved for raw PCM audio and are
not used for affection messages.

HTTP `/status` also exposes the current affection values as part of the device
status, but WebSocket JSON is the main control path.

Immediately after a WebSocket client connects, the firmware always sends the
latest `affection.state`. On reconnect, the firmware resets short-term `mood`
and `confusion` to `0` before sending state. If startup/reunion reactions are
enabled, the firmware sends `interaction.event` `session_start` after that state
message.

## Device-Owned State

The firmware owns and persists the canonical affection state.

| Field | Type | Range | Persistence | Meaning |
| --- | --- | ---: | --- | --- |
| `affection` | integer | `0..1000` | persistent | Long-term affection. `500` is the default normal value. |
| `mood` | integer | `-100..100` | temporary | Short-term mood. |
| `confusion` | integer | `0..100` | temporary | Short-term confusion/shake state. |
| `seq` | integer | monotonic | persistent or semi-persistent | Incremented when visible affection state changes. |

The firmware applies event weights, confidence, intensity, cooldowns, duplicate
suppression, clamping, persistence, and display updates.

`mood` and `confusion` are short-term state. `confusion` decays toward `0` by
`10` about every 10 seconds, while `mood` decays toward `0` by `2` about every
10 seconds. During TTS playback or playback-buffer draining, natural decay and
the resulting `affection.state` broadcasts are paused to prioritize smooth audio
playback. When short-term state changes through natural decay or reconnect
reset, `seq` increments and the firmware broadcasts `affection.state`.

`seq` is the primary ordering signal for state freshness. `timestampMs` is the
device uptime from `millis()`; it is not a Unix timestamp. Use it for logs,
debugging, and short-window `repeat` throttling, not for ordering across device
restarts.

Conversation style uses five levels, while face images use three visual tiers.

| `levelIndex` | `level` | `styleId` | `visualTier` |
| ---: | --- | --- | --- |
| 1 | `cautious` | `cautious` | `guarded` |
| 2 | `sulky` | `sulky` | `guarded` |
| 3 | `normal` | `normal` | `normal` |
| 4 | `nakayoshi` | `friendly` | `attached` |
| 5 | `daisuki` | `attached` | `attached` |

Level changes use hysteresis so `levelIndex` does not flap near boundaries.

## Visual Behavior

The firmware renders a left-side vertical heart meter. Fill height represents
`affection`; color also changes with affection.

Reference colors:

```text
0    #1A1014
250  #5A2638
500  #E66A9A
750  #FF4F93
1000 #FF2D55
```

When affection changes, the display may show a temporary delta such as `+8` or
`-3` next to the heart meter.

## Message: Affection Event

Inbound JSON:

```json
{
  "type": "affection.event",
  "id": "client-20260514-000001",
  "event": "praise",
  "confidence": 0.92,
  "intensity": 0.8,
  "source": "phone",
  "requestId": "client-req-0001"
}
```

Fields:

| Field | Required | Type | Firmware behavior |
| --- | --- | --- | --- |
| `type` | yes | string | Must be `affection.event`. |
| `id` | no | string | Used for duplicate suppression when present. |
| `event` | yes | string | Must match a supported event name. |
| `confidence` | no | number | Clamped to `0.0..1.0`; default `1.0`. |
| `intensity` | no | number | Clamped to `0.0..1.0`; default `1.0`. |
| `source` | no | string | Accepts `phone`, `app`, `device`, or `debug`; `app` is normalized to `phone`. |
| `requestId` | no | string | Included in `affection.state` responses when applicable. |

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

Current event weights:

| Event | Affection weight | Mood weight | Confusion weight | Cooldown |
| --- | ---: | ---: | ---: | ---: |
| `talk` | `+2` | `+5` | `0` | `2500 ms` |
| `positive_talk` | `+6` | `+10` | `0` | `5000 ms` |
| `negative_talk` | `-10` | `-20` | `0` | `8000 ms` |
| `praise` | `+10` | `+15` | `0` | `6000 ms` |
| `thanks` | `+6` | `+10` | `0` | `5000 ms` |
| `apology` | `+6` | `+20` | `0` | `6000 ms` |
| `master_seen` | `+2` | `+3` | `0` | `30000 ms` |
| `session_start` | `0` | `+5` | `0` | `30000 ms` |
| `petting` | `+9` | `+20` | `0` | `5000 ms` |
| `shake` | `-5` | `-10` | `+100` | `4500 ms` |

Processing notes:

- Duplicate `id` values are ignored.
- `confidence` and `intensity` are multiplied into the weighted delta.
- Positive affection deltas are dampened as `affection` approaches the high end.
- `negative_talk` is ignored when `confidence < 0.85`.
- Final values are clamped to the configured state ranges.
- If a visible state change is applied, the firmware broadcasts
  `affection.state`.
- Unknown `event` values are ignored and logged to serial.

## Message: Get State

Inbound JSON:

```json
{
  "type": "affection.get",
  "requestId": "client-req-0002"
}
```

Outbound JSON:

```json
{
  "type": "affection.state",
  "requestId": "client-req-0002",
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

`requestId` is optional. If provided, the firmware echoes it in the state
response. The firmware always responds to `affection.get`, even when the state
has not changed.

Level values:

| Range | Level |
| ---: | --- |
| `0..199` | `cautious` |
| `200..399` | `sulky` |
| `400..599` | `normal` |
| `600..799` | `nakayoshi` |
| `800..1000` | `daisuki` |

## Message: Interaction Event

Outbound JSON:

```json
{
  "type": "interaction.event",
  "event": "petting",
  "phase": "start",
  "source": "device",
  "seq": 128,
  "timestampMs": 345700,
  "affection": 650,
  "mood": 55,
  "confusion": 0,
  "level": "nakayoshi",
  "levelIndex": 4,
  "visualTier": "attached",
  "styleId": "friendly"
}
```

`interaction.event` reports physical or fixed device-side interactions. `seq`
and the state fields in the payload are the affection state snapshot at the time
the event message is sent.

Event values:

```text
petting
shake
session_start
level_up
level_down
```

Phase values:

| Phase | Meaning |
| --- | --- |
| `start` | Start of a continuous interaction. |
| `repeat` | Continued or repeated detection. Clients usually throttle TTS here. |
| `end` | End of a continuous interaction. |
| `instant` | One-shot events such as `session_start`, `level_up`, and `level_down`. |

`interaction.event.source` is always `device`. For `level_up` / `level_down`,
the firmware broadcasts `affection.state` first, then sends the interaction
event.

## TTS and Connection Modes

When at least one WebSocket client is connected, the firmware is in connected
mode. In connected mode, the device does not play template TTS by itself; it
only sends interaction events and handles face/motion changes. The phone
generates TTS and streams PCM back over the existing binary WebSocket path.

When no WebSocket client is connected, the device can be considered standalone.
Local voice/effect playback for standalone mode is a future extension.

## Compatibility and Fallbacks

Clients should tolerate older `affection.state` payloads. If `levelIndex`,
`visualTier`, `styleId`, or `timestampMs` is missing, fall back to normal
behavior. Unknown `event`, `phase`, `styleId`, and `visualTier` values should be
ignored or treated as normal fallback values without crashing.

## Message: Reset

Inbound JSON:

```json
{
  "type": "affection.reset",
  "value": 500,
  "requestId": "client-req-0003"
}
```

Firmware behavior:

- `affection` is set to `value`, clamped to `0..1000`.
- `mood` is reset to `0`.
- `confusion` is reset to `0`.
- `seq` is incremented.
- The state is saved immediately.
- `affection.state` is broadcast when a WebSocket client is connected.

The current firmware ignores `mode` and `reason` if they are present.

## Message: Debug Adjust

Inbound JSON:

```json
{
  "type": "affection.debug_adjust",
  "delta": 25,
  "requestId": "client-req-0004"
}
```

Firmware behavior:

- `delta` is applied to `affection`.
- The final value is clamped to `0..1000`.
- `mood` and `confusion` are not changed by this command.
- `affection.state` is broadcast when a WebSocket client is connected.

This command is intended for tuning and diagnostics.

## Message: Debug Set

Inbound JSON:

```json
{
  "type": "affection.debug_set",
  "levelIndex": 5,
  "mood": 40,
  "confusion": 0,
  "persist": false,
  "requestId": "client-req-0005"
}
```

Firmware behavior:

- Directly switches the affection state for debug UIs.
- `levelIndex` is clamped to `1..5`; the firmware sets `affection` to the
  representative value for that level.
- `affection` or `value` can set the raw affection value, clamped to `0..1000`.
- If both `levelIndex` and `affection` are present, `levelIndex` wins.
- `mood` is clamped to `-100..100`; `confusion` is clamped to `0..100`.
- `persist` is optional. When omitted or `false`, the debug state is not kept
  after reboot. When `true`, the state is saved immediately.
- If state changes, `seq` is incremented and `affection.state` is broadcast.
- If `levelIndex` changes, the firmware sends `interaction.event` `level_up` or
  `level_down` after `affection.state`.

Representative values:

| `levelIndex` | `affection` |
| ---: | ---: |
| 1 | 100 |
| 2 | 300 |
| 3 | 500 |
| 4 | 700 |
| 5 | 900 |

## Unsupported Messages

The current firmware logs unsupported or malformed affection messages to serial.
It does not currently emit a structured `affection.error` response.
