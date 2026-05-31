# USB Serial Protocol

This document describes the USB CDC / USB Serial interface used by the
Stack-chan CoreS3 firmware. It is intended for Android clients that open the
CoreS3 USB port with Android USB Host APIs.

The existing Wi-Fi APIs remain available:

- WebSocket: `ws://<stack-chan-ip>:8080/`
- HTTP capture: `POST http://<stack-chan-ip>/capture`

USB Serial is an additional transport. It does not replace STA, SoftAP,
WebSocket, or HTTP.

## Startup

The firmware starts USB CDC on boot with:

```text
baud: 921600
data bits: 8
parity: none
stop bits: 1
flow control: none
```

USB CDC does not use the baud rate as a physical UART clock, but the firmware
and companion apps should use `921600` consistently so the same USB session can
carry audio, camera, JSON, and StreetPass traffic without reconnecting at a
different baud rate. Configure the port as 8N1 with no flow control.

After opening the port, the client should wait briefly, drain any startup text,
then send a ping. The client should not depend on DTR/RTS for protocol state.

## Raw JSON Line Mode

For diagnostics, the firmware accepts newline-delimited UTF-8 JSON:

```json
{"type":"ping","id":"phone_001"}
```

The line must end with `\n`. The response is also newline-delimited JSON:

```json
{"type":"pong","id":"phone_001","timestampMs":12345}
```

Raw JSON is useful for initial bring-up, but large binary payloads must use SCU1
frames.

## SCU1 Frame

Binary traffic uses an SCU1 frame:

```text
magic     4 bytes  "SCU1"
version   1 byte   0x01
type      1 byte
flags     1 byte
reserved  1 byte   0x00
seq       4 bytes  little-endian
length    4 bytes  little-endian
payload   length bytes
crc32     4 bytes  little-endian
```

CRC32 is the standard IEEE CRC32 used by zlib. It covers:

```text
version, type, flags, reserved, seq, length, payload
```

The `SCU1` magic bytes and final CRC field are excluded.

The current maximum payload size is `8192` bytes. TTS PCM chunks of `4096` bytes
are recommended.

## Frame Types

| Type | Direction | Payload |
| --- | --- | --- |
| `0x01` JSON | both | UTF-8 JSON command/event |
| `0x02` TTS PCM | client to device | raw signed 16-bit little-endian PCM, 16 kHz mono |
| `0x03` MIC PCM | device to client | existing `MIC1` packet |
| `0x04` capture request | client to device | JSON request payload |
| `0x05` capture image chunk | device to client | JPEG bytes |
| `0x06` ACK | reserved | currently unused |
| `0x07` ERROR | device to client | JSON error payload |
| `0x08` PING | client to device | optional JSON payload |
| `0x09` PONG | device to client | JSON payload |

## Ping

The firmware accepts either:

- SCU1 type `0x08`
- SCU1 type `0x01` with `{"type":"ping","id":"..."}`
- raw JSON line `{"type":"ping","id":"..."}\n`

For SCU1 type `0x08`, the firmware responds with type `0x09`. For JSON ping,
the firmware responds in the same framed or line mode.

## JSON Commands

SCU1 type `0x01` carries the same JSON command/event payloads used by the
WebSocket API. StreetPass commands use this same JSON transport.

```json
{"type":"state","value":"listening"}
{"type":"state","value":"speaking"}
{"type":"state","value":"idle"}
{"type":"vad","active":false}
{"type":"auth","result":"unknown"}
{"type":"motion","name":"center"}
{"type":"affection.event","id":"phone_001","event":"petting","source":"phone","intensity":1.0}
{"type":"affection.debug_set","requestId":"phone_002","affection":700,"persist":false}
{"type":"streetpass.profile.get","requestId":"sp_profile_001"}
{"type":"streetpass.encounters.get","requestId":"sp_enc_001","sinceRecordId":0,"limit":30}
```

The firmware sends JSON events such as:

```json
{"type":"affection.state","affection":720,"mood":10,"confusion":0,"levelIndex":4}
{"type":"interaction.event","event":"camera_button","phase":"pressed","source":"device"}
```

## TTS PCM

Client-to-device TTS uses SCU1 type `0x02`.

```text
format: signed 16-bit little-endian PCM
sample rate: 16000 Hz
channels: mono
recommended chunk: 4096 bytes
```

Recommended sequence:

1. Send SCU1 JSON `{"type":"state","value":"speaking"}`.
2. Send one or more type `0x02` PCM frames.
3. Send SCU1 JSON `{"type":"state","value":"idle"}`.

Playback starts after the firmware prebuffer is reached. Sending `idle` at the
end drains any remaining short buffer.

## Microphone PCM

When the device is in `listening` state and mic streaming is not muted, the
firmware can send SCU1 type `0x03` frames. The payload is the existing `MIC1`
packet:

```text
magic       4 bytes  "MIC1"
seq         uint32 little-endian
timestampMs uint32 little-endian
sampleCount uint16 little-endian
flags       uint16 little-endian
payload     signed 16-bit little-endian PCM
```

`flags` bit 0 marks the start of a stream segment. The PCM is 16 kHz mono.

## Capture

USB Serial capture uses JSON plus image chunks:

Client request:

```json
{"type":"capture.request","id":"cap_001","format":"jpeg","maxWidth":640,"maxHeight":480}
```

Device response:

```json
{"type":"capture.start","id":"cap_001","contentType":"image/jpeg","length":123456}
```

The JPEG bytes follow as one or more SCU1 type `0x05` frames. Completion is sent
as JSON:

```json
{"type":"capture.end","id":"cap_001","ok":true}
```

On error:

```json
{"type":"capture.end","id":"cap_001","ok":false,"error":"camera_not_ready"}
```

## Receiver Requirements

The USB CDC stream can contain diagnostic text in development builds. Clients
must parse it as a byte stream and scan for the `SCU1` magic. If CRC fails, skip
bytes until the next magic sequence and resynchronize.

For raw JSON line mode, ignore non-JSON diagnostic lines and only parse lines
that start with `{`.

Recommended Android behavior:

- Open the port.
- Wait briefly and drain pending bytes.
- Send SCU1 ping or raw JSON ping.
- Keep the reader running continuously.
- Do not block writes waiting for ACK on every PCM frame.
- Use backpressure or small delays if the device reports CRC errors under heavy
  PCM transfer.
