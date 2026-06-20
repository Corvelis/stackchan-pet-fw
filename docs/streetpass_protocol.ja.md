# Stack-chan StreetPass 仕様

この文書は、Stack-chan 本体側のすれ違い通信機能を説明します。
BLE で近くの Stack-chan を検出し、相手の名前とメッセージを交換します。
本体に保存する履歴は直近 30 件までで、長期保存や時刻表示はスマホなどの外部アプリが担当します。

## 機能概要

- BLE advertisement で近くの相手を見つけます。
- 一定時間・一定強度で見えている相手に GATT 接続します。
- GATT で相手の Public Card を読み取り、自分の Public Card も相手へ書き込みます。
- 本体には最大 30 件の履歴を保存します。
- 通常画面には未読通知の封筒アイコンだけを表示します。
- 本体設定画面の StreetPass ページで、プロフィールと履歴を確認できます。
- WebSocket / USB Serial の JSON API からプロフィール編集、履歴取得、ACK、時刻同期ができます。

## 役割分担

本体側:

- 自分の `profileId`, `name`, `message`, `cardSeq` を保持します。
- BLE advertisement と GATT server を出します。
- BLE scan と GATT client で相手の Public Card を取得します。
- 直近 30 件の履歴、未読状態、スマホ同期済み状態を保存します。
- 時刻が分かる場合だけ UTC の `seenUnix` を保存します。

外部アプリ側:

- 接続時に `streetpass.time.set` で本体へ UTC 時刻を同期します。
- `streetpass.profile.get/set` で本体プロフィールを確認・編集します。
- `streetpass.encounters.get` で本体履歴を取得して、アプリ側に長期保存します。
- 保存後に `streetpass.encounters.ack` を返し、本体側の `synced` / `read` を更新します。
- `timeQuality == "unknown"` の履歴は、時刻不明として扱います。

## BLE 仕様

初期実装では次の 128-bit UUID を使います。

| Item | UUID |
| --- | --- |
| Service | `9f4d1100-9a7b-4f3b-9d7f-4e5c2f1f5a10` |
| Info | `9f4d1101-9a7b-4f3b-9d7f-4e5c2f1f5a10` |
| PublicCard | `9f4d1102-9a7b-4f3b-9d7f-4e5c2f1f5a10` |
| EncounterWrite | `9f4d1103-9a7b-4f3b-9d7f-4e5c2f1f5a10` |

Advertisement:

- primary advertisement には manufacturer data を必ず含めます。広告サイズを抑えるため、Service UUID は GATT service 側で公開し、広告判定の主経路には使いません。
- local name は `STC-<cardSeq>` 形式です。
- manufacturer data に `peerToken` と `nameHash` を含めます。
- `cardSeq` は local name と Public Card に含まれ、プロフィール更新検出用に使います。
- `nameHash` は GATT 再試行クールダウンの解除判定に使います。メッセージだけの変更ではクールダウンを維持し、名前変更時は再取得しやすくします。
- `peerToken` は `profileId` から作る 32-bit の安定値です。ｽﾀｯｸﾁｬﾝ同士が同時に接続開始しないよう、`自分の peerToken < 相手の peerToken` の側だけが GATT client として取りに行きます。相手側は GATT server として受けます。
- `peerToken` が同値の場合は BLE address 文字列で二次比較します。通常は `profileId` が異なるため、この分岐に入ることはほぼありません。

Public Card は UTF-8 JSON です。

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

フィールド:

- `v`: protocol version。現在は `1`。
- `profileId`: 同じ相手を識別するための ID。初回起動時に生成し、Preferences に保存します。
- `cardSeq`: プロフィール変更時に増える sequence。
- `name`: 表示名。UTF-8 で最大 48 bytes。
- `message`: ひとことメッセージ。UTF-8 で最大 80 bytes。
- `source`: 任意の送信元識別子。

## すれ違い成立条件

BLE の距離は RSSI と観測時間で近似します。

現在の実装値:

- scan duration: 5 秒
- idle scan interval: 12.5 秒
- busy scan interval: 60 秒
- BLE scan interval/window: 80 / 80
- BLE advertising interval: 160 - 320 units
- BLE power: `ESP_PWR_LVL_P3`
- observe minimum: 1.5 秒
- observe count: 2 回以上
- RSSI threshold: `rssiMax >= -75 dBm`
- 同じ BLE address への GATT 再試行抑制: 3 時間

ただし、manufacturer data の `nameHash` が変わった場合は、同じ BLE address でも候補を更新し、再試行抑制を解除します。
メッセージだけが変わった場合は、`cardSeq` が変わっても再試行抑制を維持します。
これにより、名前変更は早めに取りに行き、メッセージ変更だけでは 3 時間クールダウンを尊重します。

GATT 接続中は BLE 処理を相互排他にします。相手から GATT 接続されている間は scan と outbound GATT を止め、自分から GATT 接続を開始する前は advertise を一時停止します。接続終了後に advertise と scan を再開します。
受信 GATT callback では保存・画面更新を直接行わず、受信内容だけをバッファに積み、メインループ側で処理します。

GATT 失敗時は候補止まりで、履歴には保存しません。

## 音声対話との共存

音声対話を優先します。

次の場合、StreetPass は scan / GATT / advertisement を停止します。

- `Listening` 状態でマイクストリーミング中
- `Speaking` 状態
- スピーカー再生バッファを drain している間

マイクを OFF にして `micStreaming=false` になった場合は、`Listening` 状態でも StreetPass を再開します。
再開時は次回 scan を即時化します。
通常ループでは WebSocket / HTTP / USB Serial の処理を StreetPass GATT より先に処理し、音声対話のコマンド処理を優先します。音声 busy 中に届いた GATT write は保存処理を後回しにします。

画面 OFF 時は、画面更新やタッチ処理などを抑えつつ、通信・時刻同期・StreetPass・最低限のサーバ処理だけを動かします。

## 本体保存

本体は最大 30 件を保存します。31 件目が入る場合は、次の優先度で古いものから削除します。

1. `synced=true` の最古
2. `unread=false` の最古
3. それでも空きがなければ最古

`unread` と `synced` は別の状態です。

- `unread`: 通常画面の封筒アイコン用です。StreetPass ページを開いたとき、または API ACK で消えます。
- `synced`: スマホなど外部ストレージへ保存済みかを表します。API ACK で更新します。

履歴のマージ規則:

- 同じ `profileId` を同じ相手として扱います。
- 時刻同期済みの場合、同じ相手かつ前回履歴の `seenUnix` から 24 時間未満なら 1 件にまとめます。
- 前回履歴から 24 時間以上経過した後に同じ相手とすれ違った場合は別履歴にします。
- 時刻不明の場合、同じ相手の履歴は最新内容で上書きします。
- 24 時間未満でも相手の `name`, `message`, `cardSeq` が変わった場合は、同じ履歴を更新し、未読・未同期に戻します。

## 時刻同期

本体内部の基準は UTC です。UI には時刻を表示しません。

時刻の取得元:

- 外部アプリからの `streetpass.time.set`
- STA Wi-Fi 接続中の NTP
- RTC が使えるデバイスでは、RTC からの起動時復元

本体は時刻同期時に RTC へ UTC を書き込みます。再起動後は RTC から時刻を復元できる場合があります。
常時 NTP を取り続けるのではなく、接続中に一定間隔で補正します。

現在の実装値:

- 有効な時刻として扱う最小 Unix time: `1700000000`
- NTP retry interval: 10 秒
- NTP refresh interval: 6 時間

`timeQuality`:

- `exact`: 時刻同期済みの状態で記録した
- `estimated`: 同じ起動中の時刻アンカーから推定した
- `unknown`: 時刻不明

保存フィールド:

- `bootId`
- `firstSeenMs`
- `lastSeenMs`
- `seenCount`
- `rssiMax`
- `seenUnix`
- `timeQuality`

## JSON API

WebSocket JSON と USB Serial JSON frame は同じメッセージを受け付けます。

通信経路:

- WebSocket: `ws://<stackchan-ip>:8080/` に JSON 文字列を送ります。
- USB Serial: `921600 baud` を使います。
- USB Serial の推奨形式は既存 USB プロトコルの `SCU1` JSON frame です。
- デバッグ互換として、JSON 1行 + `\n` も受け付けます。

`requestId` を含む request には、response に同じ `requestId` を含めます。

## デフォルト設定

初期プロフィール:

- `enabled`: `true`
- `shareProfile`: `true`
- `name`: CoreS3 は `Stack-chan`、StopWatch は `StopWatch`、AtomS3R Chatbot は `ｽﾀｯｸﾁｬﾝ ミニマル`
- `message`: `Konnichiwa`
- `cardSeq`: `1`
- `profileId`: 初回起動時に生成して Preferences に保存

保存上限:

- encounters: 30 件
- `name`: UTF-8 48 bytes
- `message`: UTF-8 80 bytes
- `peerKey`: UTF-8 48 bytes

### Ping

```json
{"type":"ping","requestId":"req-ping"}
```

応答:

```json
{"type":"pong","requestId":"req-ping","timestampMs":123456}
```

### Profile Get

```json
{"type":"streetpass.profile.get","requestId":"req-1"}
```

応答:

```json
{
  "type": "streetpass.profile",
  "requestId": "req-1",
  "enabled": true,
  "shareProfile": true,
  "profileId": "stc-a1b2c3d4",
  "name": "Stack-chan",
  "message": "また会ったね",
  "cardSeq": 3
}
```

### Profile Set

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

応答は更新後の `streetpass.profile` です。

### Encounters Get

```json
{
  "type": "streetpass.encounters.get",
  "requestId": "req-3",
  "sinceRecordId": 1000,
  "limit": 30
}
```

`sinceRecordId` より大きい `recordId` の履歴を返します。

応答:

```json
{
  "type": "streetpass.encounters",
  "requestId": "req-3",
  "capacity": 30,
  "storedCount": 18,
  "unsyncedCount": 5,
  "records": [
    {
      "recordId": 1042,
      "peerKey": "stc-peer-001",
      "name": "Stack-chan",
      "message": "また会ったね",
      "cardSeq": 3,
      "unread": true,
      "synced": false,
      "seenCount": 2,
      "rssiMax": -54,
      "seenUnix": 1780123456,
      "timeQuality": "exact"
    }
  ]
}
```

### Encounters ACK

```json
{
  "type": "streetpass.encounters.ack",
  "requestId": "req-4",
  "recordIds": [1042, 1043],
  "markSynced": true,
  "markRead": true
}
```

応答:

```json
{
  "type": "streetpass.encounters.ack",
  "requestId": "req-4",
  "updatedCount": 2
}
```

ACK では削除しません。`synced` / `unread` だけを更新します。

### Encounters Delete

本体側の履歴を個別に削除します。テストや履歴整理に使います。
削除後は BLE 候補キャッシュもクリアし、同じ相手との再取得を試しやすくします。

```json
{
  "type": "streetpass.encounters.delete",
  "requestId": "req-5",
  "recordIds": [1042]
}
```

応答:

```json
{
  "type": "streetpass.encounters.delete",
  "requestId": "req-5",
  "deletedCount": 1
}
```

### Time Get

```json
{"type":"streetpass.time.get","requestId":"req-6"}
```

### Time Set

```json
{
  "type": "streetpass.time.set",
  "requestId": "req-7",
  "unixTime": 1780123456,
  "timezone": "UTC"
}
```

`timezone` は保存しますが、履歴の別件判定は UTC の経過秒数で行います。

### Notifications Clear

```json
{"type":"streetpass.notifications.clear","requestId":"req-8"}
```

### Debug Inject

BLE なしで UI/API/保存をテストするための診断コマンドです。

```json
{
  "type": "streetpass.debug.inject",
  "requestId": "req-9",
  "peerKey": "test-peer-001",
  "name": "テスト相手",
  "message": "こんにちは",
  "rssi": -55
}
```

## USB Serial の載せ方

StreetPass API は、既存 USB Serial protocol の SCU1 JSON frame に載せます。
WebSocket では同じ JSON payload をそのまま text frame として送ります。

SCU1 JSON frame:

- magic: ASCII `SCU1`
- version: `0x01`
- type: `0x01` JSON
- flags: `0x00`
- reserved: `0x00`
- seq: little-endian uint32
- length: little-endian uint32
- payload: UTF-8 JSON
- crc32: little-endian uint32。header の version 以降 12 bytes と payload に対する CRC32 です。

詳細は [USB Serial Protocol](usb_serial_protocol.ja.md) を参照してください。

## 本体 UI

通常画面:

- `unreadCount > 0` のとき封筒アイコンを表示します。
- 同期状態は通常画面には表示しません。

StreetPass ページ:

- ページを開いた時点で既読化します。
- `Turn On` / `Turn Off` で StreetPass を切り替えます。
- `Profile` で自分の名前とメッセージを確認します。
- `History` で履歴を新しい順に表示します。
- 履歴は 3 件ずつページ切り替えできます。
- 各履歴には同期状態、名前、メッセージを表示します。
- 履歴は個別に削除できます。
- 時刻、RSSI、recordId、詳細な遭遇回数は表示しません。

表示や操作方法はデバイスごとに異なります。CoreS3 はタッチ UI、StopWatch は丸画面 UI、
AtomS3R Chatbot は BtnA 中心の小画面 UI で操作します。

日本語表示:

- UTF-8 の日本語名・メッセージを表示します。
- 半角カタカナは表示時に全角カタカナへ正規化します。

## Status API

HTTP `/status` の `streetpass` object には、同期状態と BLE 状態を含みます。

主なフィールド:

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

`paused=true` の場合、`pauseReason` は `listening`, `speaking`, `playback` のいずれかです。
