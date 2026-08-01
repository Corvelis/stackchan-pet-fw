# Stack-chan Multi-Device Controller 0.5.0

[English](release_notes_0.5.0.md) | [日本語](release_notes_0.5.0.ja.md)

0.5.0では、M5Stack StopWatchから接続中の対応スマホアプリへ、写真撮影と
イン／アウトカメラ切替を要求できるようになりました。あわせて、カメラ／マイク表示の
タッチ判定と、StreetPass・時計の日本時間／NTP同期を改善しています。

## ダウンロードするファイル

| デバイス | 既存環境の更新 | 初回導入・復旧 |
| --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `stackchan_cores3_firmware.bin` | `stackchan_cores3_factory.bin` |
| StopWatch | `stackchan_stopwatch_firmware.bin` | `stackchan_stopwatch_factory.bin` |
| AtomS3R Chatbot | `stackchan_atoms3r_firmware.bin` | `stackchan_atoms3r_factory.bin` |

- `firmware.bin`だけの更新では、Wi-Fi設定、StreetPass設定、LittleFS画像などを保持します。
- `factory.bin`はfirmwareと顔画像v2をまとめて導入しますが、本体内の設定を初期化します。
- v0.4.0とv0.5.0の顔画像v2セットは同じため、v0.4.0導入済み端末はfirmware-onlyで更新できます。
- `SHA256SUMS`でダウンロードを検証し、同梱の`LICENSE`、`THIRD_PARTY_NOTICES.md`も保存してください。
- 詳しい書き込み方法は[バイナリ版インストール手順](install_binary.ja.md)を参照してください。

## StopWatchスマホカメラ・リモート撮影

StopWatchにカメラを追加する機能ではありません。対応スマホアプリがカメラ画面を開き、
`phone_camera.state`で準備完了を通知すると、通常画面右下にカメラ表示が現れます。

- カメラ表示を短押し: スマホへ写真撮影を要求。
- 約0.8秒長押し: 対応アプリが両方のレンズを公開している場合にイン／アウトカメラを切替。
- `IN`／`OUT`: アプリが通知した現在のレンズ。
- 灰色表示: 撮影またはレンズ切替の結果待ち。
- 緑／短い振動: 成功。
- 赤／長い振動: 失敗またはtimeout。

アプリが準備完了を通知していない間は、カメラ表示と操作を無効にします。対応機能は
`device.info.capabilities`の`phone_camera.remote_shutter.v1`と
`phone_camera.remote_lens.v1`で判定できます。

## 通信と安全な状態管理

WebSocketとUSB Serialは同じJSONを使用します。`ready:true`を送ったtransportだけを
要求の送信先とし、処理中は別transportへ切り替えません。

- request IDは起動session tokenと単調増加sequenceから生成。
- 処理種別、transport、request IDが一致する結果だけを受理。
- 撮影とレンズ切替は相互排他。
- 撮影は30秒、レンズ切替は10秒でtimeout。
- 成功／失敗表示は約0.9秒後に準備完了表示へ復帰。
- `ready:false`、所有transportの切断、画面OFFでは処理待ちを含む状態を解除。

JSONの全項目と実装例は
[スマホカメラ・リモート撮影仕様](phone_camera_remote_protocol.ja.md)を参照してください。

## カメラ／マイクのタッチ操作

カメラとマイクのタッチ判定を共通gesture管理へ移行しました。タッチ開始位置、押下時間、
最大移動量、終了位置を追跡し、長押しやdragの終了が短押しとして誤発火しないようにしています。

- StopWatchのマイク表示を右下から左下へ移動。
- StopWatch右下をスマホカメラ表示に使用。
- StopWatchとCoreS3のカメラ／マイク操作領域を拡大。
- StopWatchのカメラ操作領域をなでなで判定から除外。
- 画面OFFまたは操作受付前に、途中のgesture状態を破棄。

機種別の配置と操作は[対応デバイス別ガイド](devices.ja.md#操作)を参照してください。

## 日本時間とNTP同期

内部ではUnix timeをUTCで保持し、表示と歩数の日付区切りは`Asia/Tokyo`（UTC+9）を使います。

- 起動時にRTCから復元した時刻をシステム時計へ即時反映。
- `streetpass.time.set`をシステム時計とRTCへ即時反映。
- NTP serverから実際に応答を受けた場合だけ同期成功として採用。
- Wi-Fi接続ごとにNTP再同期を開始。
- 応答がない場合は10秒後にretryし、成功後は6時間ごとに更新。
- 補正したUTCをRTCへ書き戻し、補正秒数をserial logへ出力。

これにより、RTCですでに有効な時刻がある場合に、それを新しいNTP応答と誤認することを防ぎます。
StopWatchの時計表示と歩数の午前4時区切りも同じ日本時間を使用します。詳細は
[StreetPass protocol](streetpass_protocol.ja.md#時刻同期)を参照してください。

## 対応範囲と更新時の注意

- スマホカメラ遠隔操作はStopWatch専用です。CoreS3の本体カメラ、HTTP `POST /capture`、
  USB Serial `capture.request`とは独立しています。
- 撮影には`phone_camera.*` protocolへ対応したスマホアプリが必要です。
- CoreS3とAtomS3R Chatbotはスマホカメラcapabilityを返しません。
- factory imageを書き込むと、本体内のWi-Fi、StreetPass、サーボ原点などの設定が初期化されます。
- 顔画像v2導入済み端末を`v0.3.1`以前へ戻す場合は、firmwareだけでなく対応するfactory image
  またはLittleFSも復元してください。

## 関連文書

- [スマホカメラ・リモート撮影仕様](phone_camera_remote_protocol.ja.md)
- [StreetPass protocol](streetpass_protocol.ja.md)
- [対応デバイス別ガイド](devices.ja.md)
- [バイナリ版インストール手順](install_binary.ja.md)
- [0.4.0顔画像v2移行リリースノート](release_notes_0.4.0.ja.md)
