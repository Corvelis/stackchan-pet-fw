# タイムキーパー・体験モード通信仕様 v1

[English](timekeeper_protocol.md) | [日本語](timekeeper_protocol.ja.md)

この文書は、CoreS3とM5Stack StopWatchの体験モード、タイムキーパーイベント、読み上げ要求、ポモドーロ設定、画面OFF前通知を説明します。WebSocketとUSB SerialのJSONで同じメッセージを使用します。AtomS3R Chatbotはこの仕様に対応しません。

## 対応確認

接続後に`device.info.get`を送り、必要なcapabilityがある場合だけ対応メッセージを使用してください。

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

- `deviceId`: NVSへ保存される端末の安定ID。
- `bootId`: 起動ごとに変わるID。イベントの重複判定では`deviceId`、`bootId`、`eventId`を組み合わせます。
- `experienceMode`: `conversation`、`guruguru`、`timekeeper`、`travel`。
- `experienceModeRevision`: 起動後にモードが変わるたび増えます。
- `pomodoro`: 保存中の次回セッション設定です。

`protocolVersion`は従来の共通APIとの互換のため`2`を維持し、新機能の有無は上記のversion付きcapabilityで判定します。

## 体験モード変更

モードは本体操作で選択します。変更後、接続中のクライアントへ次を送ります。

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

音声再生中に選択した場合、イベントは再生終了後の実際の切替時に送られます。v1にはクライアントからモードを変更するコマンドはありません。

## `timekeeper.event`

状態が変化した時、またはマイルストーンへ到達した時に送ります。

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

### 共通フィールド

| Field | Meaning |
| --- | --- |
| `eventId` | 同一起動内で一意なイベントID |
| `sessionId` | `sw-*`、`cd-*`、`challenge-*`、`pomodoro-*`のセッションID |
| `activity` | `stopwatch`、`countdown`、`ten_second_challenge`、`pomodoro` |
| `event` | `started`、`paused`、`resumed`、`reset`、`lap`、`milestone`、`finished`、`result`、`aborted`、`transition`、`completed` |
| `state` | `ready`、`running`、`paused`、`finished`、`aborted`、`completed` |
| `ageMs` | イベント生成から送信までの経過時間 |
| `elapsedMs` | セッションまたは現在phaseの経過時間 |
| `reason` | 自動中断時の`display_off`または`mode_changed`。存在するイベントは読み上げ対象外 |

### 活動別フィールド

| Activity / Event | Additional fields |
| --- | --- |
| `countdown` | `remainingMs`, `durationMs` |
| `lap` | `lapIndex`, `lapDurationMs`, optional `previousLapDurationMs`, `lapDeltaMs`, `isBestLap` |
| `ten_second_challenge` | `targetMs`, `difficulty` |
| challenge `result` | `signedErrorMs`, `absoluteErrorMs`, `rank`, `affectionDelta` |
| `pomodoro` | `phase`, optional `transition`, `cycleIndex`, `totalCycles`, `remainingCycles`, `isFinalCycle`, `workDurationMs`, `breakDurationMs`, optional `phaseDurationMs`, `remainingMs`, `configRevision` |
| named `milestone` | `milestone`。値は`remaining_30_seconds`、`remaining_10_seconds`、`work_half`、`work_remaining_5_minutes`、`work_finishing_soon`、`break_remaining_1_minute`、`break_finishing_soon` |

カウントダウンの半分、残り5分、残り1分と、ストップウォッチの経過マイルストーンは`event:milestone`と時間フィールドで通知しますが、`milestone`文字列を持たない場合があります。

ポモドーロの`transition`は`work_to_break`または`break_to_work`、`phase`は`work`または`break`です。最終作業の完了イベントではphaseを省略します。

## 読み上げ要求

アプリがタイムキーパーイベントを音声で読み上げる場合、`announcement`を持つイベントだけを対象にします。

| Field | Values |
| --- | --- |
| `key` | 読み上げ内容を選ぶ安定キー。例: `stopwatch.lap`, `pomodoro.transition.work_to_break` |
| `importance` | `normal`, `high`, `critical` |
| `maxAgeMs` | 読み上げを開始できるイベント生成後の期限 |
| `delivery` | `best_effort`または`until_result` |

`best_effort`は再送保証を持ちません。カウントダウン完了とポモドーロ全体完了は`critical`／`until_result`です。本体はこれらを最大120秒保持し、切断後にクライアントが再接続して`device.info.get`を送ると、同じ`eventId`で1回再送できます。

### 事前読み込み

誤差200ms以内のチャレンジ結果では、本体の笑顔アニメーション中に次を先に送る場合があります。

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

prefetchは音声生成の準備だけに使い、`playbackGate`が示す同じ`eventId`の`timekeeper.event`を受け取るまで再生しないでください。

### 読み上げ結果

`delivery:until_result`を受け取ったクライアントは次を返します。

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

`result`は`queued`、`sent`、`suppressed`、`expired`、`failed`です。`queued`は中間状態です。残り4値は終端状態で、本体は保持中イベントを破棄します。識別子が一致しない結果は無視します。

## ポモドーロ設定

### 取得

```json
{"type":"timekeeper.pomodoro.config.get","version":1,"requestId":"pomodoro-get-001"}
```

### 保存

```json
{
  "type": "timekeeper.pomodoro.config.set",
  "version": 1,
  "requestId": "pomodoro-set-001",
  "workDurationMs": 1500000,
  "breakDurationMs": 300000
}
```

作業時間は60000〜7200000ms、休憩時間は60000〜3600000msで、どちらも60000msの倍数です。`requestId`は保存時に必須です。サイクル数は本体UIで1〜12に設定し、このv1 APIでは変更しません。

### 結果

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

- `result`: 取得時は`current`、保存成功は`saved`、失敗は`rejected`。
- `appliesTo`: 待機中は`next_start`、実行中または一時停止中は`next_session`。
- `reason`: `unsupported_version`、`invalid_request`、`duration_out_of_range`、`storage_failed`。

## 画面OFF前通知

画面OFFでは、タイムキーパーを一時停止または中断した後、通信停止前に次を送ります。

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

これは通知でありACKを待ちません。本体は短い送信猶予の後にWebSocket、USB Serial、HTTP、Wi-Fiを休止します。画面ON後は再接続し、`device.info.get`から同期を再開してください。

## モード中の既存API

- タイムキーパーと旅モード中の`display.speech_bubble.*`は画面へ表示せず、既存の吹き出しがあれば消去します。
- 会話以外の`state:listening`は無視します。
- ぐるぐると旅モード中の`state:speaking`は無視します。
- `experienceModeRevision`と各イベントは同一起動内の順序情報です。再起動をまたぐ比較には`bootId`を必ず含めてください。
