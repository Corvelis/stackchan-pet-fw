# Stack-chan Device Affection API

[English](device_affection_api.md) | [日本語](device_affection_api.ja.md)

この文書は、スタックチャン本体ファームウェアが公開する好感度関連の WebSocket API を説明します。
本体側のリファレンスとして、ファームウェアが受け付けるメッセージ、保持する状態、
返す可能性のあるレスポンスだけを記載します。

外部クライアント側の実装方法、検出ロジック、UI 設計、会話処理は対象外です。

## 通信

WebSocket エンドポイント:

```text
ws://<stack-chan-ip>:8080/
```

text frame は UTF-8 JSON です。binary frame は raw PCM 音声用で、好感度メッセージには使いません。

HTTP `/status` でも本体状態の一部として現在の好感度値を確認できますが、
制御の主経路は WebSocket JSON です。

WebSocket クライアントが接続した直後、ファームウェアは最新の
`affection.state` を必ず送信します。再接続時は短期状態の `mood` と
`confusion` を `0` に戻してから state を送信します。起動時/再接続時の定型反応を使う場合は、
その後に `interaction.event` の `session_start` を送信します。

## 本体が保持する状態

好感度状態の正本はファームウェア側が保持し、永続化します。

| Field | Type | Range | Persistence | Meaning |
| --- | --- | ---: | --- | --- |
| `affection` | integer | `0..1000` | persistent | 長期的な好感度。`500` が通常の初期値です。 |
| `mood` | integer | `-100..100` | temporary | 短期的な気分。 |
| `confusion` | integer | `0..100` | temporary | 短期的な混乱/ふりふり状態。 |
| `seq` | integer | monotonic | persistent or semi-persistent | 表示上の好感度状態が変わるたびに増えます。 |

ファームウェアはイベント重み、confidence、intensity、クールダウン、重複抑制、
範囲丸め、永続化、表示更新を本体側で処理します。

`mood` と `confusion` は短期状態です。`confusion` は約 10 秒ごとに `10` ずつ
`0` へ戻り、`mood` は約 10 秒ごとに `2` ずつ `0` へ戻ります。
TTS 再生中または再生バッファのドレイン中は、音声再生を優先するため自然減衰と
それに伴う `affection.state` broadcast は一時停止します。
短期状態が自然減衰または再接続リセットで変化した場合も `seq` は増え、
ファームウェアは `affection.state` を broadcast します。

`seq` は状態の新旧判定に使う主軸です。`timestampMs` は本体起動後の `millis()` で、
絶対時刻ではありません。ログ、デバッグ、短時間の `repeat` 間引き補助に使い、
再起動をまたいだ順序比較には使わないでください。

会話スタイルは 5 段階、画像表示は 3 段階です。

| `levelIndex` | `level` | `styleId` | `visualTier` |
| ---: | --- | --- | --- |
| 1 | `cautious` | `cautious` | `guarded` |
| 2 | `sulky` | `sulky` | `guarded` |
| 3 | `normal` | `normal` | `normal` |
| 4 | `nakayoshi` | `friendly` | `attached` |
| 5 | `daisuki` | `attached` | `attached` |

レベル境界にはヒステリシスがあり、境界付近で頻繁に `levelIndex` が上下しないようにします。

## 表示動作

ファームウェアは画面左側に縦型のハートメーターを表示します。
塗りつぶし量が `affection` を表し、色も好感度に応じて変わります。

参考色:

```text
0    #1A1014
250  #5A2638
500  #E66A9A
750  #FF4F93
1000 #FF2D55
```

好感度が変化すると、ハートメーター付近に `+8` や `-3` のような一時的な差分表示が出ます。

## Message: Affection Event

受信 JSON:

```json
{
  "type": "affection.event",
  "id": "client-20260514-000001",
  "event": "praise",
  "confidence": 0.92,
  "intensity": 0.8,
  "source": "phone",
  "requestId": "client-req-0001"
}
```

Fields:

| Field | Required | Type | Firmware behavior |
| --- | --- | --- | --- |
| `type` | yes | string | `affection.event` である必要があります。 |
| `id` | no | string | 指定されている場合、重複抑制に使います。 |
| `event` | yes | string | 対応イベント名のいずれか。 |
| `confidence` | no | number | `0.0..1.0` に丸めます。未指定時は `1.0`。 |
| `intensity` | no | number | `0.0..1.0` に丸めます。未指定時は `1.0`。 |
| `source` | no | string | `phone`, `app`, `device`, `debug` を受け付けます。`app` は `phone` として正規化します。 |
| `requestId` | no | string | 該当する `affection.state` レスポンスに含めます。 |

対応イベント名:

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

現在のイベント重み:

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
| `shake` | `-5` | `-10` | `+100` | `4500 ms` |

処理メモ:

- 重複する `id` は無視します。
- `confidence` と `intensity` は重み付き delta に乗算されます。
- 正の affection delta は、`affection` が高いほど減衰します。
- `negative_talk` は `confidence < 0.85` の場合は無視します。
- 最終値は設定された範囲に丸めます。
- 表示上の状態変更が適用された場合、ファームウェアは `affection.state` を broadcast します。
- 未知の `event` は無視し、シリアルログに記録します。

## Message: Get State

受信 JSON:

```json
{
  "type": "affection.get",
  "requestId": "client-req-0002"
}
```

送信 JSON:

```json
{
  "type": "affection.state",
  "requestId": "client-req-0002",
  "seq": 128,
  "timestampMs": 345678,
  "affection": 642,
  "mood": 35,
  "confusion": 0,
  "level": "nakayoshi",
  "levelIndex": 4,
  "visualTier": "attached",
  "styleId": "friendly"
}
```

`requestId` は任意です。指定されている場合、ファームウェアは state レスポンスに同じ値を含めます。
`affection.get` に対しては、状態が変わっていなくても必ず `affection.state` を返します。

Level values:

| Range | Level |
| ---: | --- |
| `0..199` | `cautious` |
| `200..399` | `sulky` |
| `400..599` | `normal` |
| `600..799` | `nakayoshi` |
| `800..1000` | `daisuki` |

## Message: Interaction Event

送信 JSON:

```json
{
  "type": "interaction.event",
  "event": "petting",
  "phase": "start",
  "source": "device",
  "seq": 128,
  "timestampMs": 345700,
  "affection": 650,
  "mood": 55,
  "confusion": 0,
  "level": "nakayoshi",
  "levelIndex": 4,
  "visualTier": "attached",
  "styleId": "friendly"
}
```

`interaction.event` は本体で検知した物理/定型イベントを通知します。`seq` と payload 内の
状態値は、イベントメッセージ送信時点の好感度 state snapshot です。

Event values:

```text
petting
shake
session_start
level_up
level_down
```

Phase values:

| Phase | Meaning |
| --- | --- |
| `start` | 継続イベントの開始。 |
| `repeat` | 継続中または再検知。アプリ側は通常 TTS を間引きます。 |
| `end` | 継続イベントの終了。 |
| `instant` | `session_start`, `level_up`, `level_down` などの単発イベント。 |

`interaction.event.source` は `device` 固定です。`level_up` / `level_down` は、
先に `affection.state` を broadcast してから送信します。

## TTS と接続モード

WebSocket クライアントが 1 つ以上接続している場合は connected mode とします。
connected mode では、本体はテンプレート TTS を鳴らさず、物理イベント通知、表情、
モーションのみを担当します。TTS はスマホ側が生成し、既存の binary PCM 経路で本体へ送ります。

WebSocket クライアントが 0 の場合は standalone mode とみなせます。standalone mode での
内蔵音声/効果音再生は将来拡張です。

## 互換と fallback

アプリ側は旧形式の `affection.state` でも動くように、`levelIndex`, `visualTier`,
`styleId`, `timestampMs` がない場合は `normal` 相当に fallback してください。
未知の `event`, `phase`, `styleId`, `visualTier` は無視または `normal` 相当に fallback し、
クラッシュしないようにしてください。

## Message: Reset

受信 JSON:

```json
{
  "type": "affection.reset",
  "value": 500,
  "requestId": "client-req-0003"
}
```

ファームウェアの動作:

- `affection` を `value` に設定し、`0..1000` に丸めます。
- `mood` を `0` に戻します。
- `confusion` を `0` に戻します。
- `seq` を増やします。
- 状態を即時保存します。
- WebSocket クライアントが接続されている場合、`affection.state` を broadcast します。

現在のファームウェアは、`mode` や `reason` が含まれていても無視します。

## Message: Debug Adjust

受信 JSON:

```json
{
  "type": "affection.debug_adjust",
  "delta": 25,
  "requestId": "client-req-0004"
}
```

ファームウェアの動作:

- `delta` を `affection` に適用します。
- 最終値を `0..1000` に丸めます。
- このコマンドでは `mood` と `confusion` は変更しません。
- WebSocket クライアントが接続されている場合、`affection.state` を broadcast します。

このコマンドは調整と診断用です。

## Message: Debug Set

受信 JSON:

```json
{
  "type": "affection.debug_set",
  "levelIndex": 5,
  "mood": 40,
  "confusion": 0,
  "persist": false,
  "requestId": "client-req-0005"
}
```

ファームウェアの動作:

- デバッグUI向けに、好感度状態を直接切り替えます。
- `levelIndex` は `1..5` に丸め、指定されたレベルの代表値へ `affection` を設定します。
- `affection` または `value` を指定すると、生値を `0..1000` に丸めて設定します。
- `levelIndex` と `affection` の両方がある場合は、`levelIndex` を優先します。
- `mood` は `-100..100`、`confusion` は `0..100` に丸めます。
- `persist` は任意です。未指定または `false` の場合、再起動後には残りません。`true` の場合は即時保存します。
- 状態が変わった場合は `seq` を増やし、`affection.state` を broadcast します。
- `levelIndex` が変わった場合は、`affection.state` の後に `level_up` または `level_down` の `interaction.event` を送ります。

`levelIndex` の代表値:

| `levelIndex` | `affection` |
| ---: | ---: |
| 1 | 100 |
| 2 | 300 |
| 3 | 500 |
| 4 | 700 |
| 5 | 900 |

## 未対応メッセージ

現在のファームウェアは、未対応または不正な好感度メッセージをシリアルログに出力します。
構造化された `affection.error` レスポンスは現時点では送信しません。
