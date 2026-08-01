# StopWatch スマホカメラ・リモート撮影仕様

## 対象

この機能は M5Stack StopWatch 専用です。StopWatch にカメラを追加する機能ではなく、
接続中のスマホアプリへ撮影要求を送ります。

`device.info.capabilities` に次の値がある場合に利用できます。

```json
[
  "phone_camera.remote_shutter.v1",
  "phone_camera.remote_lens.v1"
]
```

CoreS3 の `camera_button`、HTTP `POST /capture`、USB Serial
`capture.request` とは独立した機能です。

## Transport

WebSocket と USB Serial JSON の両方で同じメッセージを使用できます。

アプリが `ready:true` を送った transport が撮影要求の送信先になります。
ファームウェアは WebSocket と USB Serial へ同じ撮影要求を同時送信しません。

撮影待ち中は transport を固定します。別 transport から届いた
`ready:true` と撮影結果は無視します。

## 準備状態

アプリはカメラ画面を開き、撮影可能になった後で送ります。

```json
{
  "type": "phone_camera.state",
  "version": 1,
  "available": true,
  "ready": true,
  "lens": "back",
  "supportedLenses": ["front", "back"]
}
```

`lens`はアプリが現在使用しているカメラで、`front`はインカメラ、`back`は
アウトカメラです。StopWatchはこの値を正として、カメラ表示に`IN`または
`OUT`を表示します。`supportedLenses`に両方が含まれる場合だけ長押し切替を
有効にします。

カメラ画面を閉じた場合、権限がなくなった場合、または撮影不能になった場合は
同じ transport から送ります。

```json
{"type":"phone_camera.state","version":1,"available":false,"ready":false}
```

`ready:false`、所有 transport の切断、StopWatch の画面 OFF では、撮影待ちを含む
リモート撮影状態を解除します。自動的に別 transport へ切り替えません。

## 撮影要求

撮影可能なときに StopWatch の右下カメラボタンを押すと、`ready:true` を送った
transport だけへ次の要求を送ります。

```json
{
  "type": "phone_camera.shutter.request",
  "version": 1,
  "requestId": "pcam-a31f9270-00000017",
  "mode": "photo"
}
```

`requestId` は起動セッションと単調増加 sequence から生成します。アプリは同じ
`requestId` を重複処理しないでください。

## カメラ切替

撮影可能で撮影処理中ではないとき、右下カメラボタンを約0.8秒長押しすると、
現在と反対のカメラを明示して、準備通知を送ったtransportだけへ要求します。

```json
{
  "type": "phone_camera.lens.set.request",
  "version": 1,
  "requestId": "lens-a31f9270-00000018",
  "lens": "front"
}
```

成功:

```json
{
  "type": "phone_camera.lens.set.result",
  "version": 1,
  "requestId": "lens-a31f9270-00000018",
  "status": "applied",
  "lens": "front"
}
```

失敗:

```json
{
  "type": "phone_camera.lens.set.result",
  "version": 1,
  "requestId": "lens-a31f9270-00000018",
  "status": "camera_unavailable",
  "lens": "back"
}
```

撮影結果と同様に、撮影待ち状態、同一transport、同一`requestId`がすべて
一致する結果だけを受理します。`applied`かつ返信された`lens`が要求値と
一致した場合だけ成功です。レンズ切替は10秒でタイムアウトします。

撮影とレンズ切替は相互排他です。どちらかの要求が処理中の間は、もう一方の
要求を開始しません。アプリは適用済みのレンズを保存し、以後の撮影と
再接続後の`phone_camera.state`へ反映してください。

## 撮影結果

成功:

```json
{
  "type": "phone_camera.shutter.result",
  "version": 1,
  "requestId": "pcam-a31f9270-00000017",
  "status": "captured"
}
```

失敗:

```json
{
  "type": "phone_camera.shutter.result",
  "version": 1,
  "requestId": "pcam-a31f9270-00000017",
  "status": "capture_failed",
  "error": "camera_not_ready"
}
```

ファームウェアが結果を受理する条件は次のすべてです。

- 撮影待ち状態である
- `requestId` が現在の撮影要求と一致する
- 結果を受信した transport が撮影要求を送った transport と一致する
- `status` が空ではない（`captured` の場合は成功、それ以外は失敗）

それ以外の text、binary、PCM、異なる `requestId`、別 transport の結果では
撮影待ちを解除しません。

結果が 30 秒以内に届かなければ失敗表示へ移行します。成功または失敗表示は
約 0.9 秒後に撮影可能状態へ戻ります。ただし、その間に `ready:false` または
切断を受けた場合は未準備状態へ移行します。
