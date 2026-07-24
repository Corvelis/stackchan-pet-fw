# 変更履歴

[日本語](CHANGELOG.ja.md) | [English](CHANGELOG.md)

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
