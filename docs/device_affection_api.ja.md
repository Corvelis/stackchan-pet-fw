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
  "source": "client",
  "timestamp": 1778750000,
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
| `source` | no | string | メタデータとして受け付けます。現在のファームウェア処理では必須ではありません。 |
| `timestamp` | no | integer | メタデータとして受け付けます。現在のファームウェア処理では必須ではありません。 |
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
| `shake` | `-5` | `-10` | `+35` | `4500 ms` |

処理メモ:

- 重複する `id` は無視します。
- `confidence` と `intensity` は重み付き delta に乗算されます。
- 正の affection delta は、`affection` が高いほど減衰します。
- `negative_talk` は `confidence < 0.85` の場合は無視します。
- 最終値は設定された範囲に丸めます。
- 表示上の状態変更が適用された場合、ファームウェアは `affection.state` を broadcast します。

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
  "affection": 642,
  "mood": 35,
  "confusion": 0,
  "seq": 128,
  "level": "nakayoshi"
}
```

`requestId` は任意です。指定されている場合、ファームウェアは state レスポンスに同じ値を含めます。

Level values:

| Range | Level |
| ---: | --- |
| `0..199` | `cautious` |
| `200..399` | `sulky` |
| `400..599` | `normal` |
| `600..799` | `nakayoshi` |
| `800..1000` | `daisuki` |

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

## 未対応メッセージ

現在のファームウェアは、未対応または不正な好感度メッセージをシリアルログに出力します。
構造化された `affection.error` レスポンスは現時点では送信しません。
