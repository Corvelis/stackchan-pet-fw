# Speech Bubble Protocol v1

The CoreS3, StopWatch, and AtomS3R Chatbot firmware can display short subtitles associated with TTS PCM. WebSocket and USB Serial use the same JSON messages.

## Capability discovery

After connecting, send `device.info.get` and enable subtitles only when `capabilities` contains `display.speech_bubble.v1`.

```json
{
  "type": "device.info",
  "capabilities": ["device.info", "display.speech_bubble.v1"],
  "display": {"width": 320, "height": 240, "shape": "rect"},
  "speechBubble": {
    "version": 1,
    "sampleRate": 16000,
    "maxSequenceIdUtf8Bytes": 64,
    "maxTextUtf8Bytes": 512,
    "maxQueuedCues": 16,
    "maxPcmBytes": 8388608,
    "defaultHoldMs": 800,
    "maxHoldMs": 5000,
    "stallTimeoutMs": 15000,
    "preSpeakingHoldMs": 500
  }
}
```

Continue sending PCM without speech-bubble JSON to older firmware.

## Message order

1. `{"type":"state","value":"speaking"}`
2. `display.speech_bubble.cue` for segment 1
3. PCM binary for segment 1
4. Repeat cue then PCM for each segment
5. `display.speech_bubble.end`
6. `{"type":"state","value":"idle"}`

Keep each cue and its following PCM ordered on the same transport. On WebSocket, cue is a text frame and PCM is a binary frame. On USB Serial, cue uses SCU1 type `0x01` and PCM uses type `0x02`.

## cue

```json
{
  "type": "display.speech_bubble.cue",
  "version": 1,
  "sequenceId": "tts_123456",
  "segmentIndex": 0,
  "text": "Hello. It is a beautiful day.",
  "pcmBytes": 38400,
  "sampleRate": 16000
}
```

- `sequenceId`: UTF-8 identifier for one utterance, up to 64 bytes.
- `segmentIndex`: monotonically increasing within the utterance.
- `text`: UTF-8 display text, up to 512 bytes; newlines are accepted.
- `pcmBytes`: total byte count of the segment PCM immediately following this cue; positive, even, and at most 8 MiB.
- `sampleRate`: only `16000` is supported.

The firmware records segment boundaries from received PCM byte counts and displays a cue when the corresponding audio enters the playback queue. The subtitle may lead physical speaker output slightly because of the internal speaker queue.

If a cue arrives just before `state:speaking` takes effect, the firmware returns `audio_not_speaking` but retains that cue for up to 500 ms. The sender may resend `state:speaking` and retry the same `sequenceId` / `segmentIndex` once. The retained cue and retry are merged without duplicate display.

## end

```json
{
  "type": "display.speech_bubble.end",
  "version": 1,
  "sequenceId": "tts_123456",
  "holdMs": 800
}
```

`holdMs` is clamped to `0..5000` ms. The device starts the hold after PCM draining completes, not when it receives `end`. If `end` is missing, it uses 800 ms.

## cancel

```json
{
  "type": "display.speech_bubble.cancel",
  "version": 1,
  "sequenceId": "tts_123456"
}
```

This immediately clears the bubble. It does not stop audio, so use the existing state/cancel flow when audio must also stop.

## Errors

There is no success ACK. Invalid commands produce an error on the transport that received them.

```json
{
  "type": "display.speech_bubble.error",
  "version": 1,
  "sequenceId": "tts_123456",
  "segmentIndex": 0,
  "error": "unsupported_sample_rate"
}
```

Common errors include `unsupported_version`, `audio_not_speaking`, `text_too_long`, `invalid_pcm_bytes`, `unsupported_sample_rate`, `segment_index_out_of_order`, and `cue_queue_full`. An error does not stop the PCM playback path.

## Layout and safety

| Device | Bubble sprite | Font | Lines |
| --- | ---: | ---: | ---: |
| CoreS3 | 288 x 56 px (2 px above bottom) | Japanese 16 px | up to 2 |
| StopWatch | 315 x 86 px | Japanese 12 px at 1.75x scale | up to 3 |
| AtomS3R | 124 x 40 px (top of screen) | Japanese 12 px | up to 2 |

Overflow text is truncated with `...` at a valid UTF-8 boundary. The bubble is composited after the face and status overlays so it survives mouth, eye, heart, and battery animation redraws.

The device clears the bubble immediately on WebSocket/USB disconnect, display off, cancel, or sequence replacement. It also clears after 15 seconds without PCM receive/playback cursor progress as a fallback for undetected connection loss.

Timekeeper and Travel on CoreS3/StopWatch reserve the display for their own UI.
The firmware intentionally hides incoming `display.speech_bubble.*` commands in
those modes and clears any existing bubble on entry. For Timekeeper speech, use
the `announcement` flow in the
[Timekeeper and Experience Mode Protocol](timekeeper_protocol.md); PCM may play,
but the client should not send bubble cues.

AtomS3R places the bubble at the top of its small display so it does not cover the mouth animation.

The PSRAM sprite is allocated only on first display and reused between segments. It uses about 32 KiB on CoreS3, 53 KiB on StopWatch, and 10 KiB on AtomS3R.

The WebSocket receive task only performs short speech-bubble state updates. PSRAM sprite construction and display transfer run on the main loop. PCM ingestion never waits for the speech-bubble lock, so subtitle rendering cannot stop PCM receipt or the speaker playback task. A cue is retained and retried if rendering fails.
