# Stack-chan Multi-Device Controller

[日本語の詳細](README.ja.md) | [English](README.en.md)

CoreS3 + ｽﾀｯｸﾁｬﾝ、M5Stack StopWatch、AtomS3R Chatbot向けの共通ファームウェアです。顔画像v2アニメーション、音声ストリーミング、吹き出し、なでなで／ふりふり／ぐるぐる反応、StreetPass、HTTP・WebSocket・USB Serial接続を提供します。

## 最新リリース: v0.4.0

### 顔表示・リアクション

- 3機種の顔表示を、口パク・瞬き・なでなで・ぐるぐる・クラクラに対応した顔画像v2へ移行しました。
- `good_*`、`bad_*`、`photo_*`などの旧静止顔を配布画像から削除し、既存端末向けの限定フォールバックだけを残しました。
- なでなでが3秒未満で終わると不満、3秒以上続くと喜びの終了リアクションを表示します。
- クラクラ後は閉じ目から通常顔へ段階的に復帰し、`Warm`やLow Powerでも旧静止顔へ切り替わりません。

### デバイス機能・安定性

- CoreS3の640x480カメラ撮影と、サーボ・マイク・音声対話の安定性を改善しました。
- StopWatchにIMUベースの歩数カウンターを追加し、午前4時区切りの30日履歴、1000歩ごとの好感度加算、アプリへの同期・更新通知に対応しました。
- 画面OFF時はWi-Fi、HTTP、WebSocket、USB Serialを停止し、画面ON時にWi-Fi再接続を開始します。StreetPass BLEは低頻度で継続します。

### 通信・画像作成・配布

- WebSocket／USB Serial共通の吹き出しプロトコルと、音声・マイク・StreetPassなどの状態診断を追加しました。
- 基本顔となでなで用の4x4スプライトシート作成プロンプト、画像分割、リサイズ、命名、manifest生成・検証ツールを追加しました。
- 3機種それぞれに、更新用`firmware`と初回導入・完全移行用`factory`バイナリを用意します。

変更内容と移行時の注意点は[0.4.0リリースノート](docs/release_notes_0.4.0.ja.md)、全変更履歴は[CHANGELOG.md](CHANGELOG.md)を参照してください。

## クイックスタート

```sh
pio run -e m5stack-cores3 -t upload
pio run -e m5stack-cores3 -t uploadfs

pio run -e m5stack-stopwatch -t upload
pio run -e m5stack-stopwatch -t uploadfs

pio run -e m5stack-atoms3r-chatbot -t upload
pio run -e m5stack-atoms3r-chatbot -t uploadfs
```

デバイス別の操作と設定は[README.ja.md](README.ja.md)、ビルド済みバイナリからの導入は[バイナリ版インストール手順](docs/install_binary.ja.md)を参照してください。

## 顔画像v2

配布用の`data/`、`data_stopwatch/`、`data_atoms3r/`は、旧静止顔を含まない`animated` v2構成です。旧画像だけを持つ端末はfirmware-only更新時に限定的なfallbackを利用できます。

- [顔レンダラーv2設計](docs/face_renderer_v2.ja.md)
- [顔画像v2移行ガイド](docs/face_asset_migration.ja.md)
- [顔画像生成ツール](tools/face_image_builder/README.md)
- [0.4.0リリースノート](docs/release_notes_0.4.0.ja.md)
- [StopWatch歩数同期仕様](docs/step_counter_protocol.ja.md)

## 開発と配布

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
bash scripts/build_release_bins.sh all
```

変更時の確認事項は[CONTRIBUTING.md](CONTRIBUTING.md)、変更内容は[CHANGELOG.md](CHANGELOG.md)を参照してください。ファームウェアソースと同梱自作画像は、いずれも[MIT License](LICENSE)です。つくよみちゃん画像を含む旧配布バイナリには現行MITを適用せず、そのリリースに付属する通知と素材提供元の規約に従ってください。
