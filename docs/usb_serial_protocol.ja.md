# USB Serial Protocol

この文書は、CoreS3 / StopWatch / AtomS3R Chatbot ファームウェアの
USB CDC / USB Serial interface を説明します。
Android 側で USB Host API からデバイスの USB port を開くクライアント向けの仕様です。

既存の Wi-Fi API はそのまま残ります。

- WebSocket: `ws://<stack-chan-ip>:8080/`
- HTTP capture: `POST http://<stack-chan-ip>/capture`。カメラ搭載の CoreS3 でだけ成功します。

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
| `0x04` capture request | client to device | JSON request payload。カメラ非搭載機種では失敗します |
| `0x05` capture image chunk | device to client | JPEG bytes。CoreS3 capture 成功時のみ |
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
{"type":"audio.speaker_test","requestId":"spk_001","durationMs":450}
{"type":"audio.mic_test","requestId":"mic_001","durationMs":600}
{"type":"streetpass.profile.get","requestId":"sp_profile_001"}
{"type":"streetpass.encounters.get","requestId":"sp_enc_001","sinceRecordId":0,"limit":30}
```

ファームウェアからは次のような JSON event を送ります。

```json
{"type":"affection.state","affection":720,"mood":10,"confusion":0,"levelIndex":4}
{"type":"interaction.event","event":"camera_button","phase":"pressed","source":"device"}
```

CoreS3とStopWatchの0.5.0では、同じJSON transportで体験モード、タイムキーパー、
ポモドーロ設定、画面OFF前通知も扱います。

```json
{"type":"experience.mode.changed","version":1,"mode":"timekeeper","previousMode":"conversation","revision":1}
{"type":"timekeeper.event","version":1,"activity":"stopwatch","event":"lap","state":"running","lapIndex":1}
{"type":"timekeeper.pomodoro.config.get","version":1,"requestId":"pomodoro-001"}
{"type":"device.communication.suspending","version":1,"reason":"display_off"}
```

全フィールド、`bootId`／`eventId`、読み上げ結果、再送条件は
[タイムキーパー・体験モード通信仕様](timekeeper_protocol.ja.md)を参照してください。

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

吹き出し対応機では、各文節PCMの直前にSCU1 type `0x01`で
`display.speech_bubble.cue`を送れます。詳しい順序と制限は
[発話吹き出しプロトコル v1](speech_bubble_protocol.ja.md)を参照してください。

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

## 歩数JSON

StopWatchではSCU1 type `0x01`で歩数スナップショットを要求できます。

```json
{"type":"steps.get","requestId":"steps-001"}
```

接続直後および要求時は`steps.snapshot`、歩数変化時は`steps.update`を返します。
CoreS3／AtomS3Rは`steps.error`を返します。フィールド、日付区切り、通知間隔は
[歩数カウンター／同期仕様](step_counter_protocol.ja.md)を参照してください。

## Capture

USB Serial capture は JSON と image chunk で扱います。カメラ搭載の CoreS3 でだけ成功します。
StopWatch / AtomS3R Chatbot では `camera_not_ready` などのエラーになります。

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

## Audio Diagnostics

音声まわりの疎通確認用に、JSON command と HTTP endpoint の両方で診断を実行できます。

speaker test:

```json
{"type":"audio.speaker_test","requestId":"spk_001","durationMs":450}
```

response:

```json
{"type":"audio.speaker_test","requestId":"spk_001","ok":true,"durationMs":450}
```

mic test:

```json
{"type":"audio.mic_test","requestId":"mic_001","durationMs":600}
```

response には `peak`, `rms`, `dc`, `clipCount`, `chunks`, `underruns` などの測定値が入ります。
HTTP では `/speaker-test` と `/mic-test` でも同じ用途の確認ができます。

playback diagnostics:

```json
{"type":"audio.playback_diag","requestId":"playback-001"}
```

同じ`type`と`requestId`のresponseを返します。主なフィールドは次のとおりです。

| フィールド群 | 内容 |
| --- | --- |
| `state`, `draining`, `playbackStarted`, `speakerEnabled` | 現在の再生状態 |
| `rxAvailable`, `rxCapacity`, `maxBufferedBytes` | PCM受信バッファ使用量 |
| `pcmFramesReceived`, `pcmBytesReceived`, `pcmBytesAccepted`, `pcmBytesDropped` | PCM受信・採用・破棄件数 |
| `rxOverflowEvents`, `underflowResets`, `speakerQueueFullEvents`, `playRawFailEvents` | overflow、underflow、speaker queueの異常件数 |
| `playbackPcmReceivedCursor`, `playbackPcmAcceptedCursor`, `playbackPcmDequeuedCursor` | 再生stream内の累積byte位置 |
| `speechBubbleActive`, `speechBubbleVisible` | 吹き出しprotocol／表示状態 |
| `voicePerf` | 発話中のloop、顔描画、音声、WebSocket／HTTP処理時間 |

このコマンドは状態を変更せず、WebSocketとUSB Serialの両方で利用できます。

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
