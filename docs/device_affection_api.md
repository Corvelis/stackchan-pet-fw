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
  "source": "client",
  "timestamp": 1778750000,
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
| `source` | no | string | Accepted for metadata; currently not required by firmware logic. |
| `timestamp` | no | integer | Accepted for metadata; currently not required by firmware logic. |
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
| `shake` | `-5` | `-10` | `+35` | `4500 ms` |

Processing notes:

- Duplicate `id` values are ignored.
- `confidence` and `intensity` are multiplied into the weighted delta.
- Positive affection deltas are dampened as `affection` approaches the high end.
- `negative_talk` is ignored when `confidence < 0.85`.
- Final values are clamped to the configured state ranges.
- If a visible state change is applied, the firmware broadcasts
  `affection.state`.

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
  "affection": 642,
  "mood": 35,
  "confusion": 0,
  "seq": 128,
  "level": "nakayoshi"
}
```

`requestId` is optional. If provided, the firmware echoes it in the state
response.

Level values:

| Range | Level |
| ---: | --- |
| `0..199` | `cautious` |
| `200..399` | `sulky` |
| `400..599` | `normal` |
| `600..799` | `nakayoshi` |
| `800..1000` | `daisuki` |

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

## Unsupported Messages

The current firmware logs unsupported or malformed affection messages to serial.
It does not currently emit a structured `affection.error` response.
