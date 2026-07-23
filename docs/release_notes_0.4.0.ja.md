# Stack-chan Multi-Device Controller 0.4.0

[English](release_notes_0.4.0.md) | [日本語](release_notes_0.4.0.ja.md)

0.4.0では、CoreS3、StopWatch、AtomS3R Chatbotの顔表示を、旧静止顔中心の構成から
manifest付きの顔画像v2アニメーションへ移行しました。通常の会話、口パク、まばたき、
なでなで、ぐるぐる方向、混乱とその復帰を、機種別に検証した画像セットで表示します。

## ダウンロードするファイル

| デバイス | 既存環境の更新 | 初回導入・顔画像v2への完全移行 |
| --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | `stackchan_cores3_firmware.bin` | `stackchan_cores3_factory.bin` |
| StopWatch | `stackchan_stopwatch_firmware.bin` | `stackchan_stopwatch_factory.bin` |
| AtomS3R Chatbot | `stackchan_atoms3r_firmware.bin` | `stackchan_atoms3r_factory.bin` |

- `firmware.bin`だけの更新では、Wi-Fi設定やLittleFS画像などを保持します。
- `factory.bin`はfirmwareと顔画像v2をまとめて導入しますが、本体内の設定を初期化します。
- `SHA256SUMS`でダウンロードを検証し、同梱の`LICENSE`、`THIRD_PARTY_NOTICES.md`も保存してください。
- 詳しい書き込み方法は[バイナリ版インストール手順](install_binary.ja.md)を参照してください。

## 顔画像v2

- CoreS3／AtomS3Rは65枚、StopWatchは57枚のJPGと`face_assets.json`を使います。
- 通常表示、発話時の口パク、まばたきは`base_m0_e0..base_m3_e3`で行います。
- なでなでは16コマ、方向顔は機種別17／9コマ、中央blinkは1コマ、混乱は15コマです。
- 好感度、認証、熱、低電力、撮影状態は、専用の旧静止顔へ切り替わりません。
- 熱保護、低電力動作、好感度計算、カメラ撮影などの機能自体は継続します。

起動時は画像構成を検証し、`animated`、`transition`、`legacy`、`emergency`のいずれかを
選びます。新旧画像をフレーム単位で混ぜることはありません。詳しくは
[顔レンダラーv2設計](face_renderer_v2.ja.md)を参照してください。

## リアクションの変更

- なでなでが3秒未満で終わると不満、3秒以上続くと喜びの終了リアクションを表示します。
- ぐるぐるモードは機種別の方向顔と中央blinkを使います。
- 振動・混乱後は、基本顔の閉じ目から通常目へ段階的に戻ります。
- 温度`Warm`やLow Powerが有効でも、顔画像v2を旧静止顔へ置き換えません。

## CoreS3カメラ

M5Unifiedの内部I2CとGC0308カメラのSCCB競合を解消しました。撮影後は内部I2Cを復元し、
HTTP `POST /capture`とUSB Serial `capture.request`から640x480のJPEGを取得できます。
PSRAMを使ったRGB565取り込みと、USB／HTTPの分割送信により、大きいJPEGでも通信処理を
長時間占有しないようにしています。

## CoreS3のサーボと音声

- 起動時はサーボの現在位置を採用し、初期化だけを理由に首を動かしません。
- 発話中はサーボを停止し、Listening中の自動うなずきを無効にしました。
- なでなで、振動・混乱、サーボ動作中と動作直後はマイク取り込みを一時停止し、機械音による音声認識の誤反応を抑えます。
- pan／tiltを同時起動せず1軸ずつ動かし、電源電流の急増によるUSB再接続やWi-Fi不通を抑えます。
- なでなで／振動時のサーボ反応は維持しつつ、なでなでで過度に上を向かないよう上向き量を制限しました。

## 通信と診断

- 画面をオフにすると音声状態とアプリ接続を終了し、Wi-Fi、HTTP、WebSocket、USB Serialを停止します。画面をオンにするとWi-Fi再接続を開始するため、クライアント側も再接続してください。
- StreetPass BLEは画面オフ中も低頻度で継続します。StopWatchはCPU周波数を下げ、短いlight sleepも使用します。
- `audio.playback_diag`で、音声バッファ、PCM受信・破棄、underflow、speaker queue、吹き出し、発話中の処理時間を読み取れます。
- HTTP `/status`に詳細な`mic`、`voicePerf`、`streetpass`と`currentState`／`audioState`を追加しました。診断クライアントは未知フィールドを無視してください。

## StopWatch歩数カウンター

StopWatchはIMUで歩数を計測し、日本時間の午前4時区切りで当日を含む最大30日分を保存します。
設定画面の`Steps`で確認でき、1000歩ごとに好感度を`+3`します。接続クライアントには
`steps.snapshot`／`steps.update`を通知します。CoreS3とAtomS3Rは対象外です。詳細は
[歩数カウンター／同期仕様](step_counter_protocol.ja.md)を参照してください。

## 既存環境からの更新とダウングレード

firmware-only更新では端末のLittleFSを保持します。すでに顔画像v2がある場合は`animated`、
旧基本5枚だけがある場合は限定的な`legacy` fallbackで起動します。0.4.0の推奨表示へ完全に
移行する場合は、対象機種のfactory imageまたはv2のLittleFSを書き込んでください。

顔画像v2を導入した端末を`v0.3.1`以前へ戻す場合、firmwareだけでなく、そのバージョンに
対応するfactory imageまたはLittleFSも復元してください。詳細は
[顔画像v2移行ガイド](face_asset_migration.ja.md)を参照してください。

旧`face`と`face_mode`コマンドは、1〜2マイナーリリースの間、意味ベースの互換入力として
維持します。`good_*`、`bad_*`、`photo_*`などの旧画像は0.4.0の配布物へ戻しません。

## 自分のキャラクター画像を使う

[顔画像ビルダー](../tools/face_image_builder/README.md)に、参考画像から基本顔・なでなで・方向・
中央blink・混乱のスプライトシートを作るプロンプト、分割CLI、サンプル、3機種向けの
リサイズ・命名変換・manifest生成手順があります。

作業途中はGit管理外の`face_assets_v2_work/`、実機確認用は`data_local*`を使い、検証済みの
完全なv2セット単位で差し替えてください。

## その他

- WebSocket／USB Serial共通の発話吹き出しプロトコルを追加しました。
- StopWatch本体側の歩数カウンターと状態通知を追加しました。
- 3機種の通常／classic、合計6環境の再現可能な配布ビルドを整備しました。
- 同梱ランタイム画像、スプライトシートサンプル、画像生成用参考画像は、
  ファームウェアソースと同じ[MIT License](../LICENSE)で利用できます。
