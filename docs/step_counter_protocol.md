# StopWatch Step Counter And Sync Protocol

[English](step_counter_protocol.md) | [日本語](step_counter_protocol.ja.md)

This document describes the device-side step counter in the M5Stack StopWatch
build and the step JSON sent to WebSocket or USB Serial clients. CoreS3 and
AtomS3R Chatbot do not count steps.

## Device behavior

- The StopWatch IMU detects continuous walking-like motion and stores the count on the device. It is not a medical or competition-grade instrument.
- A day rolls over at 04:00 Japan Standard Time (UTC+9). The activity date cannot be finalized before the clock is synchronized.
- Up to 30 days, including today, are stored in NVS. Changes are saved after 10 steps or 60 seconds.
- The `Steps` settings page shows today's count, the 04:00 reset, and stored history.
- Every 1,000 steps reached during an activity day adds `+3` affection. Milestone progress resets for the next activity day.

## Capability detection

This protocol is available when `device.info.capabilities` contains `steps.sync`.
Since 0.4.1, only the StopWatch returns this capability.

## Transport

Send and receive the JSON as a WebSocket text frame or as an SCU1 USB Serial
JSON frame (type `0x01`). The device sends `steps.snapshot` just after a client
connects. To request one explicitly, send:

```json
{"type":"steps.get","requestId":"steps-001"}
```

### `steps.snapshot`

```json
{
  "type": "steps.snapshot",
  "schemaVersion": 1,
  "requestId": "steps-001",
  "deviceId": "stopwatch_8f3a21",
  "sequence": 12,
  "generatedAt": 1784662800,
  "resetHour": 4,
  "timezoneOffsetMinutes": 540,
  "currentActivityDay": 20656,
  "todaySteps": 3210,
  "history": [
    {
      "activityDay": 20656,
      "localDate": "2026-07-22",
      "dayStartUnix": 1784660400,
      "steps": 3210
    }
  ]
}
```

`requestId` is present only when supplied by a request. `generatedAt` and
`dayStartUnix` are UTC Unix seconds and are `0` when time is not valid.
`history` is newest first. `activityDay` is an internal date key; use
`localDate` for presentation.

### `steps.update`

While connected, the device sends the current activity-day record after a
10-step change, after 60 seconds with a change, on an activity-day change, or
when the value moves backward.

```json
{
  "type": "steps.update",
  "schemaVersion": 1,
  "deviceId": "stopwatch_8f3a21",
  "sequence": 13,
  "generatedAt": 1784662860,
  "activityDay": 20656,
  "localDate": "2026-07-22",
  "dayStartUnix": 1784660400,
  "steps": 3221
}
```

`sequence` increases monotonically during one boot. Across restarts, use
`activityDay`/`localDate` together with `steps` when reconciling state.

### Unsupported devices

CoreS3 and AtomS3R return this response to `steps.get`:

```json
{
  "type": "steps.error",
  "schemaVersion": 1,
  "requestId": "steps-001",
  "error": "steps_not_supported"
}
```
