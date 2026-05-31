# Stack-chan StreetPass Specification

StreetPass is the device-side BLE encounter feature for Stack-chan. Nearby
Stack-chan devices exchange a small profile card containing a stable peer ID,
name, and message. The device stores only the latest 30 encounter records;
companion apps fetch records over WebSocket or USB Serial JSON and keep
long-term history.

## Responsibilities

Device:

- Advertise the StreetPass BLE service and expose a GATT server.
- Scan for nearby StreetPass advertisements.
- Read the peer Public Card over GATT.
- Write its own Public Card to the peer when supported.
- Store up to 30 records, unread state, and synced state.
- Store UTC timestamps only when time is known.

Companion app:

- Sync UTC time with `streetpass.time.set`.
- Read or edit the local profile with `streetpass.profile.get/set`.
- Fetch encounters with `streetpass.encounters.get`.
- Persist unlimited history outside the device.
- Mark records as synced/read with `streetpass.encounters.ack`.

## BLE Service

| Item | UUID |
| --- | --- |
| Service | `9f4d1100-9a7b-4f3b-9d7f-4e5c2f1f5a10` |
| Info | `9f4d1101-9a7b-4f3b-9d7f-4e5c2f1f5a10` |
| PublicCard | `9f4d1102-9a7b-4f3b-9d7f-4e5c2f1f5a10` |
| EncounterWrite | `9f4d1103-9a7b-4f3b-9d7f-4e5c2f1f5a10` |

Advertisement:

- Always includes the StreetPass manufacturer data in the primary
  advertisement. To keep the advertisement small, the Service UUID is exposed
  by the GATT service and is not the primary advertisement detection path.
- Uses local name `STC-<cardSeq>`.
- Includes manufacturer data with `peerToken` and `nameHash`.
- `cardSeq` is included in the local name and Public Card, and identifies
  profile updates.
- `nameHash` is used to decide whether to bypass the GATT retry cooldown. A
  message-only change keeps the cooldown; a name change can trigger an earlier
  re-fetch.
- `peerToken` is a stable 32-bit value derived from `profileId`. To avoid
  simultaneous Stack-chan-to-Stack-chan outbound GATT connections, only the
  device with `own peerToken < peer peerToken` initiates as the GATT client.
  The other device remains available as the GATT server.
- If `peerToken` values are equal, the BLE address string is used as a
  secondary tie-breaker. This should be rare because normal devices have
  different `profileId` values.

Public Card is UTF-8 JSON:

```json
{
  "v": 1,
  "profileId": "stc-a1b2c3d4",
  "cardSeq": 3,
  "name": "Stack-chan",
  "message": "また会ったね",
  "source": "stackchan"
}
```

Limits:

- `name`: UTF-8, max 32 bytes.
- `message`: UTF-8, max 80 bytes.

## Encounter Conditions

Current implementation values:

- scan duration: 5 seconds
- idle scan interval: 12.5 seconds
- busy scan interval: 60 seconds
- BLE scan interval/window: 80 / 80
- BLE advertising interval: 160 - 320 units
- BLE power: `ESP_PWR_LVL_P3`
- observe minimum: 1.5 seconds
- observe count: at least 2
- RSSI threshold: `rssiMax >= -75 dBm`
- same BLE address GATT retry cooldown: 3 hours

If the manufacturer data `nameHash` changes, the candidate cooldown is reset
even when the BLE address is unchanged. If only the message changes, `cardSeq`
changes but the 3-hour retry cooldown remains in effect.

BLE processing is mutually exclusive during GATT. While an inbound GATT
connection is active, scanning and outbound GATT are stopped. Before starting an
outbound GATT connection, advertising is paused. Advertising and scanning resume
after the connection finishes.
Inbound GATT callbacks only copy received data into a buffer; record storage and
UI updates run later on the main loop.

## Audio Coexistence

Audio interaction has priority. StreetPass scan, GATT, and advertisement stop
while:

- the high-level state is `Listening` and the microphone is streaming,
- the device is `Speaking`,
- playback is draining.

If the microphone is turned off and `micStreaming=false`, StreetPass can run
even while the high-level state remains `Listening`.
The main loop processes WebSocket, HTTP, and USB Serial before StreetPass GATT
work so audio interaction commands are not blocked by a new StreetPass exchange.
GATT writes received while audio is busy are stored for later processing.

## Device Storage

The device stores at most 30 records. When a new record exceeds capacity, it
evicts in this order:

1. oldest `synced=true` record,
2. oldest `unread=false` record,
3. oldest record.

Record merge rules:

- `profileId` is the stable peer key.
- If time is known, records merge for the same peer until 24 hours have elapsed
  since the existing record's `seenUnix`.
- If at least 24 hours have elapsed since the previous record, the same peer
  creates a new record.
- If time is unknown, the latest card from the same peer overwrites the existing
  record.
- If `name`, `message`, or `cardSeq` changes within 24 hours, the same record is
  updated and marked unread/unsynced.

## Time

The firmware stores time internally as UTC. The device UI does not show
encounter times.

Time sources:

- app-provided `streetpass.time.set`,
- NTP while connected in STA Wi-Fi mode,
- CoreS3 RTC restored at boot.

Current implementation values:

- minimum valid Unix time: `1700000000`
- NTP retry interval: 10 seconds
- NTP refresh interval: 6 hours

`timeQuality` values:

- `exact`: recorded while time was known,
- `estimated`: estimated from the current boot's time anchor,
- `unknown`: no usable time.

## JSON API

WebSocket and USB Serial use the same JSON payloads.

- WebSocket: JSON text frame to `ws://<stackchan-ip>:8080/`.
- USB Serial: `921600 baud`, preferably SCU1 type `0x01` JSON frame.
- Raw JSON line plus `\n` is accepted for diagnostics.

## Defaults

Initial profile:

- `enabled`: `true`
- `shareProfile`: `true`
- `name`: `Stack-chan`
- `message`: `Konnichiwa`
- `cardSeq`: `1`
- `profileId`: generated on first boot and stored in Preferences

Storage limits:

- encounters: 30 records
- `name`: UTF-8 32 bytes
- `message`: UTF-8 80 bytes
- `peerKey`: UTF-8 48 bytes

### Ping

```json
{"type":"ping","requestId":"req-ping"}
```

Response:

```json
{"type":"pong","requestId":"req-ping","timestampMs":123456}
```

### Profile

```json
{"type":"streetpass.profile.get","requestId":"req-1"}
```

```json
{
  "type": "streetpass.profile.set",
  "requestId": "req-2",
  "enabled": true,
  "shareProfile": true,
  "name": "Stack-chan",
  "message": "また会ったね"
}
```

### Encounters

```json
{
  "type": "streetpass.encounters.get",
  "requestId": "req-3",
  "sinceRecordId": 1000,
  "limit": 30
}
```

`sinceRecordId` returns records with a larger `recordId`.

```json
{
  "type": "streetpass.encounters.ack",
  "requestId": "req-4",
  "recordIds": [1042, 1043],
  "markSynced": true,
  "markRead": true
}
```

ACK updates `synced` and `unread`; it does not delete records.

```json
{
  "type": "streetpass.encounters.delete",
  "requestId": "req-5",
  "recordIds": [1042]
}
```

### Time

```json
{
  "type": "streetpass.time.set",
  "requestId": "req-6",
  "unixTime": 1780123456,
  "timezone": "UTC"
}
```

Firmware-side grouping uses elapsed UTC seconds, not local calendar days.

## Device UI

- The normal face screen shows an envelope icon when unread records exist.
- Opening the StreetPass page marks records as read.
- The `Pass` tab shows On/Off, profile, and history.
- History is newest first and paged 3 records at a time.
- Records show synced state, name, and message.
- Records can be deleted one at a time.
- Japanese UTF-8 is displayed; half-width katakana is normalized to full-width
  katakana for display only.

## Status API

HTTP `/status` includes StreetPass state under the `streetpass` object.

Main fields:

- `enabled`
- `shareProfile`
- `capacity`
- `storedCount`
- `unreadCount`
- `unsyncedCount`
- `droppedCount`
- `timeSynced`
- `timezone`
- `bleReady`
- `scanActive`
- `advertising`
- `exchangeInProgress`
- `paused`
- `pauseReason`

When `paused=true`, `pauseReason` is one of `listening`, `speaking`, or `playback`.

## Non-Published Test Implementations

A BLE fake peer was used during development. That implementation is not part of
the public firmware release. The public repository should contain the Stack-chan
device-side implementation and the external API contract documented here.
