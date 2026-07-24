# StopWatch 歩数カウンター／同期プロトコル

[English](step_counter_protocol.md) | [日本語](step_counter_protocol.ja.md)

この仕様は、M5Stack StopWatch ビルドの本体内歩数カウンターと、WebSocket／USB Serial
クライアントへ送る歩数JSONを説明します。CoreS3とAtomS3R Chatbotは歩数計測に対応しません。

## 本体側の動作

- StopWatchのIMUで歩行らしい連続振動を検出し、本体側で歩数を保持します。医療・競技用の計測器ではありません。
- 日付の区切りは日本時間（UTC+9）の午前4時です。時刻未同期中は日付を確定できません。
- 当日を含む最大30日分をNVSへ保存します。変化が10歩以上、または前回保存から60秒以上で保存します。
- `Steps`設定ページで当日歩数、午前4時区切り、保存済み履歴を確認できます。
- 当日歩数が1000歩増えるごとに好感度を`+3`します。日をまたぐと当日の到達段階をリセットします。

## 対応判定

`device.info`の`capabilities`に`steps.sync`があれば、この仕様に対応しています。
0.4.0ではStopWatchだけがこれを返します。

## 通信

JSONは、WebSocket text frameまたはUSB SerialのSCU1 JSON frame（type `0x01`）で送受信します。
接続直後に本体から`steps.snapshot`を送ります。クライアントから明示的に取得する場合は次を送ります。

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

`requestId`は要求に指定された場合だけ返ります。`generatedAt`と`dayStartUnix`はUTCのUnix秒で、
時刻がまだ有効でない場合は`0`です。`history`は新しい日から順に並びます。
`activityDay`は本体内部の日付キーなので、表示には`localDate`を使用してください。

### `steps.update`

接続中に歩数が変化すると、10歩以上の差分、60秒経過、日付変更、または値の巻き戻り時に
当日レコードを通知します。

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

`sequence`は起動中に単調増加します。再起動をまたいだ新旧判定には`activityDay`／`localDate`と
`steps`を併用してください。

### 非対応機種

CoreS3またはAtomS3Rへ`steps.get`を送ると次を返します。

```json
{
  "type": "steps.error",
  "schemaVersion": 1,
  "requestId": "steps-001",
  "error": "steps_not_supported"
}
```
