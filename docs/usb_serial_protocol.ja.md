# USB Serial Protocol

この文書は、Stack-chan CoreS3 ファームウェアの USB CDC / USB Serial interface を説明します。
Android 側で USB Host API から CoreS3 の USB port を開くクライアント向けの仕様です。

既存の Wi-Fi API はそのまま残ります。

- WebSocket: `ws://<stack-chan-ip>:8080/`
- HTTP capture: `POST http://<stack-chan-ip>/capture`

USB Serial は追加 transport であり、STA / SoftAP / WebSocket / HTTP を置き換えません。

## 起動と port 設定

ファームウェアは起動時に USB CDC を開始します。

```text
baud: 921600
data bits: 8
parity: none
stop bits: 1
flow control: none
```

USB CDC では baud rate は物理 UART clock ではありませんが、既存アプリとの接続管理を
単純にするため、ファームウェアとアプリは `921600` に統一します。
8N1、flow control none であることが重要です。

port open 後、クライアントは短く待って startup text を drain し、その後 ping を送ってください。
DTR/RTS は protocol state として扱わないでください。

## Raw JSON Line Mode

診断用に、改行区切り UTF-8 JSON を受け付けます。

```json
{"type":"ping","id":"phone_001"}
```

末尾には `\n` が必要です。レスポンスも改行区切り JSON です。

```json
{"type":"pong","id":"phone_001","timestampMs":12345}
```

raw JSON は初期疎通確認向けです。大きな binary payload は SCU1 frame を使ってください。

## SCU1 Frame

binary traffic は SCU1 frame に載せます。

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

CRC32 は zlib と同じ標準 IEEE CRC32 です。対象は次の範囲です。

```text
version, type, flags, reserved, seq, length, payload
```

`SCU1` magic bytes と最後の CRC field は含めません。

現在の最大 payload size は `8192` bytes です。TTS PCM chunk は `4096` bytes を推奨します。

## Frame Type

| Type | Direction | Payload |
| --- | --- | --- |
| `0x01` JSON | 双方向 | UTF-8 JSON command/event |
| `0x02` TTS PCM | client to device | raw signed 16-bit little-endian PCM, 16 kHz mono |
| `0x03` MIC PCM | device to client | 既存の `MIC1` packet |
| `0x04` capture request | client to device | JSON request payload |
| `0x05` capture image chunk | device to client | JPEG bytes |
| `0x06` ACK | reserved | 現状未使用 |
| `0x07` ERROR | device to client | JSON error payload |
| `0x08` PING | client to device | optional JSON payload |
| `0x09` PONG | device to client | JSON payload |

## Ping

ファームウェアは次の ping を受け付けます。

- SCU1 type `0x08`
- SCU1 type `0x01` の `{"type":"ping","id":"..."}`
- raw JSON line `{"type":"ping","id":"..."}\n`

SCU1 type `0x08` には type `0x09` で返します。JSON ping には、受信した framed / line mode に
合わせて JSON pong を返します。

## JSON Command

SCU1 type `0x01` には、WebSocket API と同じ JSON command/event を載せます。
StreetPass API も同じ JSON transport を使います。

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

ファームウェアからは次のような JSON event を送ります。

```json
{"type":"affection.state","affection":720,"mood":10,"confusion":0,"levelIndex":4}
{"type":"interaction.event","event":"camera_button","phase":"pressed","source":"device"}
```

## TTS PCM

client-to-device の TTS は SCU1 type `0x02` です。

```text
format: signed 16-bit little-endian PCM
sample rate: 16000 Hz
channels: mono
recommended chunk: 4096 bytes
```

推奨シーケンス:

1. SCU1 JSON `{"type":"state","value":"speaking"}` を送る。
2. type `0x02` PCM frame を 1 つ以上送る。
3. SCU1 JSON `{"type":"state","value":"idle"}` を送る。

ファームウェアはプリバッファ到達後に再生を開始します。末尾で `idle` を送ると、
短い残りバッファも drain して再生します。

## Microphone PCM

本体が `listening` 状態で、マイク送信がミュートされていない場合、ファームウェアは
SCU1 type `0x03` frame を送れます。payload は既存の `MIC1` packet です。

```text
magic       4 bytes  "MIC1"
seq         uint32 little-endian
timestampMs uint32 little-endian
sampleCount uint16 little-endian
flags       uint16 little-endian
payload     signed 16-bit little-endian PCM
```

`flags` bit 0 は stream segment start です。PCM は 16 kHz mono です。

## Capture

USB Serial capture は JSON と image chunk で扱います。

client request:

```json
{"type":"capture.request","id":"cap_001","format":"jpeg","maxWidth":640,"maxHeight":480}
```

device response:

```json
{"type":"capture.start","id":"cap_001","contentType":"image/jpeg","length":123456}
```

続けて JPEG bytes を SCU1 type `0x05` frame で分割送信します。完了時は JSON を送ります。

```json
{"type":"capture.end","id":"cap_001","ok":true}
```

エラー時:

```json
{"type":"capture.end","id":"cap_001","ok":false,"error":"camera_not_ready"}
```

## 受信側の注意

開発ビルドでは、同じ USB CDC stream に診断テキストが混ざることがあります。
クライアントは byte stream として処理し、常に `SCU1` magic をスキャンしてください。
CRC が失敗した場合は、次の magic sequence まで読み飛ばして再同期してください。

raw JSON line mode では、JSON ではない診断行を無視し、`{` から始まる行だけを parse してください。

Android 側の推奨動作:

- port を open する。
- 短く待って pending bytes を drain する。
- SCU1 ping または raw JSON ping を送る。
- reader は常時動かす。
- PCM frame ごとに ACK 待ちで block しない。
- PCM 転送中に CRC error が出る場合は、少し間隔を空けるか backpressure を入れる。
