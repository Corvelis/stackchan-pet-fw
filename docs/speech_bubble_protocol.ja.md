# 発話吹き出しプロトコル v1

CoreS3 / StopWatch / AtomS3R Chatbot ファームウェアは、TTS PCMに対応する短い字幕を顔画面へ表示できます。WebSocketとUSB Serialのどちらでも同じJSONを使います。

## 対応確認

接続後に`device.info.get`を送り、`capabilities`に`display.speech_bubble.v1`がある場合だけ吹き出しJSONを送ってください。

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

非対応ファームウェアには従来どおりPCMだけを送ります。

## 送信順序

1. `{"type":"state","value":"speaking"}`
2. 1文節目の`display.speech_bubble.cue`
3. 1文節目のPCMバイナリ
4. 次の文節の`cue`とPCMを同じ順序で繰り返す
5. `display.speech_bubble.end`
6. `{"type":"state","value":"idle"}`

`cue`と直後のPCMは同じtransportから順番を保って送ってください。WebSocketでは`cue`をtext frame、PCMをbinary frameで送ります。USB Serialでは`cue`をSCU1 type `0x01`、PCMをtype `0x02`に載せます。

## cue

```json
{
  "type": "display.speech_bubble.cue",
  "version": 1,
  "sequenceId": "tts_123456",
  "segmentIndex": 0,
  "text": "こんにちは。今日はいい天気ですね。",
  "pcmBytes": 38400,
  "sampleRate": 16000,
  "holdMs": 800
}
```

- `sequenceId`: 1回の発話を識別するUTF-8文字列。最大64 bytes。
- `segmentIndex`: 同じ発話内で単調増加させます。
- `text`: 表示するUTF-8文字列。最大512 bytes。改行を使用できます。
- `pcmBytes`: この`cue`直後に送る文節PCMの合計byte数。正の偶数、最大8 MiB。
- `sampleRate`: `16000`のみ対応します。
- `holdMs`: v1の`cue`では使用しません。終了後の保持時間は`end.holdMs`で指定します。

ファームウェアはPCM受信量から各文節の開始位置を記録し、対応する音声が再生キューへ入ると吹き出しを表示します。スピーカー内部キューの分だけ、字幕が実際の音より少し先行する場合があります。

`state:speaking`の反映直前に`cue`が到着した場合、ファームウェアは`audio_not_speaking`を返しながら同じcueを最大500 ms保持します。送信側は従来どおり`state:speaking`を再送し、同一`sequenceId` / `segmentIndex`のcueを1回再送できます。保持済みcueと再送cueは重複表示せず合流します。

## end

```json
{
  "type": "display.speech_bubble.end",
  "version": 1,
  "sequenceId": "tts_123456",
  "holdMs": 800
}
```

`holdMs`は`0..5000` msです。上限を超えた値は5000 msに丸めます。`end`受信時には消去せず、PCMドレイン完了後に保持時間を開始します。`end`が届かなかった場合は800 msを使います。

## cancel

```json
{
  "type": "display.speech_bubble.cancel",
  "version": 1,
  "sequenceId": "tts_123456"
}
```

吹き出しを即時消去します。音声自体は停止しないため、音声も中止する場合は既存のstate/cancel処理を併用してください。

## エラー

成功時のACKは送りません。不正なコマンドだけ、受信したtransportへ次のJSONを返します。

```json
{
  "type": "display.speech_bubble.error",
  "version": 1,
  "sequenceId": "tts_123456",
  "segmentIndex": 0,
  "error": "unsupported_sample_rate"
}
```

代表的なエラーは`unsupported_version`、`audio_not_speaking`、`text_too_long`、`invalid_pcm_bytes`、`unsupported_sample_rate`、`segment_index_out_of_order`、`cue_queue_full`です。エラーになってもPCM再生経路は停止しません。

## 表示と安全動作

| Device | 吹き出し領域 | Font | 行数 |
| --- | ---: | ---: | ---: |
| CoreS3 | 288 x 56 px（画面下端から2 px） | 日本語16 px | 最大2行 |
| StopWatch | 315 x 86 px | 日本語12 pxを1.75倍描画 | 最大3行 |
| AtomS3R | 124 x 40 px（画面上部） | 日本語12 px | 最大2行 |

領域外の文字はUTF-8文字境界を保ったまま末尾を`...`にします。吹き出しは顔画像に合成するため、口・目・ハート・バッテリーなどのアニメーション更新後も再描画されます。

WebSocket/USB切断、表示OFF、`cancel`、別発話への切り替えで即時消去します。また、通信切断を検出できない場合に備え、PCM受信・再生カーソルが15秒間進まなければ自動消去します。

CoreS3とStopWatchのタイムキーパー／旅モードでは専用表示を優先するため、
`display.speech_bubble.*`を受信しても吹き出しを表示しません。モードへ入る時点で既存の
吹き出しも消去します。タイムキーパーの読み上げ連携では
[タイムキーパー・体験モード通信仕様](timekeeper_protocol.ja.md)の`announcement`を使い、
PCMは再生しても吹き出しcueは送らないでください。

AtomS3Rは画面が小さいため、口のアニメーションを隠さないよう吹き出しを画面上部へ配置します。

吹き出し用PSRAM spriteは初回表示時だけ確保し、文節切り替えでは再利用します。使用量はCoreS3約32 KiB、StopWatch約53 KiB、AtomS3R約10 KiBです。

WebSocket受信タスクは吹き出し状態だけを短時間更新し、PSRAM spriteの生成と画面転送はメインループで行います。PCM受信側は吹き出しロックを待たないため、字幕描画が音声パケット受信やスピーカー再生タスクを停止させることはありません。描画に失敗したcueは破棄せず再試行します。
