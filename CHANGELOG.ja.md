# 変更履歴

[日本語](CHANGELOG.ja.md) | [English](CHANGELOG.md)

## [0.5.0](docs/release_notes_0.5.0.ja.md) - 2026-08-08

### 追加

- CoreS3／StopWatchの会話、ぐるぐる、タイムキーパー、旅モードと4分割選択画面。
- ストップウォッチ、ラップ、カウントダウン、時間当てチャレンジ、ポモドーロ。
- タイムキーパーイベント、読み上げprefetch／結果、重要通知再送、ポモドーロ設定protocol。
- 旅モード用9表情、2ページピッカー、3×3生成プロンプト、分割／ピッカー生成ツール。
- 全3機種の`device.info`に、起動境界を識別する`bootId`。
- CoreS3／StopWatchの`device.info`に、現在モード、ポモドーロ設定、4つのversion付きcapability。
- 体験モード操作仕様とタイムキーパー通信仕様の日本語／英語文書。

### 変更

- タイムキーパー／旅モードでは専用表示を優先し、なでなで、ふりふり、マイク／カメラ表示、吹き出しを抑止。
- タイムキーパーUIとoverlayをフル画面canvasへまとめ、画面更新時のちらつきを低減。
- CoreS3電源ダブルクリックのぐるぐる切替を新しい体験モード管理へ統合。
- CoreS3配布画像を76 JPG、StopWatchを68 JPGへ更新。AtomS3Rは65 JPGを維持。
- 旅モード追加画像を11枚全部または0枚として検証し、部分構成を拒否。
- AtomS3Rのユーザー向け機能、capability、顔画像構成は、versionと`bootId`以外を従来どおり維持。

### 移行時の注意

- 0.4.1からのfirmware-only更新は既存LittleFSを保持するため、完全な旅モードには0.5.0 factoryまたは`uploadfs`が必要。
- `data_local*`は引き続きローカル確認専用で、Git管理とGitHub Releasesには含めない。

## [0.4.1](docs/release_notes_0.4.1.ja.md) - 2026-08-01

### 追加

- StopWatchから対応スマホアプリへ撮影を要求する、WebSocket／USB Serial共通のスマホカメラ・リモート撮影protocol。
- イン／アウトカメラ切替要求、現在レンズの`IN`／`OUT`表示、処理中・成功・失敗の画面表示と振動feedback。
- transport所有権、session単位のrequest ID、応答照合、相互排他、timeoutを管理するスマホカメラ状態controller。
- `phone_camera.remote_shutter.v1`と`phone_camera.remote_lens.v1` capability。
- スマホカメラ遠隔操作仕様の日本語版／英語版。

### 変更

- StopWatchのマイク表示を右下から左下へ移動し、右下をスマホカメラ操作に使用。
- StopWatch／CoreS3のカメラ・マイク操作を、開始位置、押下時間、移動量、終了位置を追跡する共通gesture判定へ移行。
- StopWatch／CoreS3のoverlay button操作領域を拡大し、カメラ領域をなでなで判定から除外。
- StreetPassと時計のtimezoneを`Asia/Tokyo`へ統一。
- RTC、アプリ、NTPの時刻をシステム時計へ即時反映し、補正後のUTCをRTCへ書き戻すよう変更。
- NTPを応答callbackで確定し、Wi-Fi再接続時の再同期、10秒retry、6時間更新に対応。

### 修正

- 長押しやdragの終了時にカメラ撮影／マイク切替が短押しとして誤発火する問題を修正。
- 既存の有効なシステム時刻を、新しいNTP応答と誤認する可能性を解消。
- RTC復元または`streetpass.time.set`後にシステム時計と表示が古いまま残る問題を修正。

## [0.4.0](docs/release_notes_0.4.0.ja.md) - 2026-07-25

### 追加

- CoreS3、StopWatch、AtomS3R向けのアニメーション顔画像v2プロファイルとmanifest。
- 4×4の基本口パク／まばたきアニメーションと、16コマのなでなでリアクション。
- 起動時の`animated`、`transition`、`legacy`、`classic`、画像不要の`emergency`レンダラー選択。
- 顔画像の厳密な検証、schema、変換ツール、単体テスト、CI検証。
- `device.info`の顔レンダラー診断情報。
- WebSocketとUSB Serialで共通の発話吹き出しプロトコル。
- プログラム描画のまばたき、8段階の口パク、発話吹き出し、画像に依存しない状態overlayを備えた、3機種別のclassic顔ソースビルド環境。
- 読み取り専用の`audio.playback_diag` JSONコマンドと、`/status`のマイク、音声性能、StreetPass診断情報。
- StopWatch専用の歩数計、30日履歴、`steps.sync`状態通知、好感度報酬。
- 再現可能な6環境のCIビルドと、機種別のrelease／factory image生成。
- 4×4の基本顔／なでなでスプライトシート用プロンプトとサンプル。

### 変更

- 配布画像を顔画像v2だけに整理。CoreS3／AtomS3Rは65 JPG、StopWatchは57 JPG。
- 好感度、認証、温度、低電力、カメラ状態による個別静止顔の選択を廃止。
- 温度保護、低電力動作、好感度計算、カメラ機能は引き続き動作し、必要な状態はoverlayで表示。
- なでなでが3秒未満なら不満、3秒以上なら喜びの終了リアクションを表示。
- 顔キャッシュ確保を必須条件から最適化へ変更し、v2画像をLittleFSから直接decode可能に変更。
- 再現可能なPlatformIOビルドのため、直接依存関係を固定。
- CoreS3カメラを640×480 JPEG出力とし、USB／HTTP転送chunkを制限。
- 画面OFF時に音声／アプリsessionを終了してWi-Fi、HTTP、WebSocket、USB Serialを休止。画面ON時にWi-Fi再接続を開始し、StreetPass BLEは低頻度で継続。
- 同梱runtime画像、スプライトシートサンプル、画像生成referenceをファームウェアソースと同じMIT Licenseへ統一。
- AILog作成画像へのMIT許諾は、過去の配布バイナリに含まれる第三者画像を再ライセンスしないことを明記。過去画像には当時の通知と素材提供元の規約を適用。

### 修正

- 同梱する基本アニメーションサンプルの分割時の横位置ずれを修正。
- 分割したスプライトframeの下端・側面を含む、隣接cellの混入を除去。
- 温度`Warm`状態や低電力状態がアニメーション顔を置き換えないよう修正。
- マイクclient接続中のふりふり／混乱復帰で、旧idle／blink顔が露出しないよう修正。
- manifest付きv2画像への切り替え後も、ぐるぐる方向顔を表示するよう修正。
- CoreS3用manifest付きv2画像のLittleFS準備時に中央blink画像を保持。
- CoreS3カメラとM5Unified内部I2C busのSCCB競合を解消し、撮影後にbusを復元。
- CoreS3の発話中サーボ動作を停止し、自動的な聞き取りnodを無効化。
- なでなで、ふりふり／混乱、サーボ安定待ち中はマイク取得を一時停止し、機械音による音声認識の再反応を防止。
- 起動時に物理的な現在位置を採用し、移動命令を送らないことでCoreS3のサーボ落下を防止。
- CoreS3のpan／tilt動作を直列化し、電流spike、USB reset、Wi-Fi接続のstale化を抑制。
- なでなで／ふりふり時のサーボ動作を維持しながら、上向きの可動量を制限。

### 削除

- `good_*`、`bad_*`、`photo_*`、好感度tier、温度、低電力の画像を含む、旧48静止顔runtime assets。
- 現行branchから旧6×6静止顔／3×3なでなで生成ツールとサンプルを削除。これらは`v0.3.1`tagから参照可能。

### 移行時の注意

- firmwareのみを更新する場合はLittleFSが保持され、旧基本5画像の完全なsetを限定的な`legacy`レンダラーで利用可能。
- 新しいfactory imageはv2画像を導入し、`animated`を選択。
- v2画像を導入した端末を`v0.3.1`以前へdowngradeする場合は、firmwareに加えて対応するfactory imageまたはLittleFS imageも復元。
- `face`と`face_mode`コマンドは、1〜2 minor releaseの間、意味的な互換adapterとして維持。
