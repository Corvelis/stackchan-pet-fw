# Stack-chan Multi-Device Controller

[日本語](README.md) | [English](README.en.md)

CoreS3 + ｽﾀｯｸﾁｬﾝ、M5Stack StopWatch、AtomS3R Chatbot向けの共通ファームウェアです。
顔アニメーション、音声ストリーミング、なでなで／ふりふり／ぐるぐる反応、StreetPass、
HTTP・WebSocket・USB Serial接続を提供します。CoreS3ではサーボとカメラも利用できます。

このリポジトリはデバイス本体側のファームウェアです。外部クライアントの実装は含みません。

## 最新リリース: v0.4.0

### 顔表示・リアクション

- 3機種の顔表示を、4×4口パク／瞬き、なでなで、方向、クラクラに対応した顔画像v2へ移行。
- 配布画像をCoreS3／AtomS3Rは65 JPG、StopWatchは57 JPGに整理し、`good_*`、`bad_*`、
  `photo_*`、好感度・温度・低電力別の旧静止顔を削除。
- 旧画像だけが残る端末向けに限定fallbackを維持し、温度・低電力・撮影状態では顔画像を切り替えない構成へ変更。
- なでなでが3秒未満なら不満、3秒以上なら喜びを表示し、クラクラ後は閉じ目から通常顔へ段階的に復帰。
- 画像を使わないclassic顔を機種別のsource buildとして追加。program描画の口パク、瞬き、吹き出しに対応。

### デバイス機能・安定性

- CoreS3カメラを640×480 JPEGへ変更し、I2C競合とUSB／HTTP転送を修正。
- CoreS3の起動時サーボ、発話中の不要動作、機械音によるマイク再反応、上向き過ぎるなでなで動作を修正。
- StopWatchに歩数計、午前4時区切りの30日履歴、好感度報酬、アプリ同期を追加。
- 画面OFF時にWi-Fi／HTTP／WebSocket／USB Serialを休止し、StreetPass BLEは低頻度で継続。

### 通信・画像作成・配布

- WebSocket／USB Serial共通の発話吹き出しprotocolと、音声・マイク・StreetPassなどの診断情報を追加。
- 基本顔／なでなで用4×4スプライトシートの作成・分割・変換・manifest生成・検証toolとsampleを追加。
- 3機種の画像顔／classic顔をCIでbuildし、各機種の更新用`firmware`と初回導入用`factory`バイナリを生成。

詳細は[0.4.0リリースノート](docs/release_notes_0.4.0.ja.md)、全versionの履歴は
[日本語CHANGELOG](CHANGELOG.ja.md)を参照してください。

## 対応デバイス

| デバイス | 追加hardware | 画像顔env | classic顔env | 画像directory |
| --- | --- | --- | --- | --- |
| CoreS3 + ｽﾀｯｸﾁｬﾝ | Stack-chan互換サーボ | `m5stack-cores3` | `m5stack-cores3-classic` | `data/` |
| M5Stack StopWatch | なし | `m5stack-stopwatch` | `m5stack-stopwatch-classic` | `data_stopwatch/` |
| AtomS3R Chatbot | Atomic Echo Base | `m5stack-atoms3r-chatbot` | `m5stack-atoms3r-chatbot-classic` | `data_atoms3r/` |

機種ごとの操作、書き込み方法、非対応機能は
[対応デバイス別ガイド](docs/devices.ja.md)にまとめています。

## 主な機能

- LittleFS上のJPGを使った口パク、瞬き、なでなで、方向、クラクラアニメーション
- WebSocket JSON制御、PCM音声再生、マイク音声stream
- Android直結向けUSB CDC／USB Serial制御
- Wi-Fi STA、SoftAP直接接続、browserからのWi-Fi設定
- HTTP status取得とCoreS3カメラ撮影
- BLEによるStack-chan同士のStreetPass
- なでなで、ふりふり、接続、カメラなどのinteraction event
- 好感度管理と状態overlay
- StopWatchの歩数計、30日履歴、同期
- CoreS3のサーボreaction

## 導入方法

### 配布バイナリを使う

GitHub Releasesでは各機種用に次の2種類を配布します。

- `factory`: 初回導入、画像を含む完全移行、復旧用
- `firmware`: 現在のLittleFS画像を保持した更新用

ファイルの選び方、書き込みaddress、downgrade時の注意は
[バイナリ版インストール手順](docs/install_binary.ja.md)を参照してください。

### ソースから書き込む

PlatformIO CoreまたはVS CodeのPlatformIO拡張を使用します。

```sh
cp src/config_private.example.h src/config_private.h
pio run -e <env>
pio run -e <env> -t upload
pio run -e <env> -t uploadfs
```

`<env>`は対応デバイス表の環境名へ置き換えてください。通常の顔画像は同梱済みです。
Wi-Fiは起動後の設定画面から登録でき、`src/config_private.h`はGit管理外です。

## 顔表示

| 構成 | 用途 | 特徴 |
| --- | --- | --- |
| 画像顔 | 通常配布・推奨 | 口パク、瞬き、なでなで、方向、クラクラの画像animation |
| classic顔 | source build専用 | 白い目と口のprogram描画、口パク、瞬き、吹き出し |
| legacy fallback | firmware-only移行用 | 端末に残る旧基本5画像だけを限定利用 |

classic顔は実行時設定ではなく、対応する`*-classic` envを別途buildします。
自分のキャラクターへ差し替える場合は、Git管理外の`data_local*`で確認してから配布用画像へ反映してください。

- [顔画像ビルダー](tools/face_image_builder/README.md)
- [顔レンダラーv2設計](docs/face_renderer_v2.ja.md)
- [顔画像v2移行ガイド](docs/face_asset_migration.ja.md)
- [顔画像ファイル棚卸し](docs/face_image_inventory.ja.md)
- [顔画像group使用仕様](docs/face_image_usage_analysis.ja.md)

## 本体操作

なでなで、ふりふり、ぐるぐる、画面OFF、設定画面の操作方法は機種ごとに異なります。
操作表は[対応デバイス別ガイド](docs/devices.ja.md#操作)を参照してください。

## 接続口

| 接続 | 主な用途 |
| --- | --- |
| HTTP | status、Wi-Fi設定、CoreS3カメラ、設定page |
| WebSocket | JSON制御、PCM再生、マイク送信、interaction event |
| USB Serial | Android直結時の制御、音声、画像転送 |
| BLE | StreetPass profile交換 |

吹き出しはクライアントがTTS PCMの文節に対応するcueを送った場合に表示します。
通信仕様の詳細は次の文書を参照してください。

- [USB Serial protocol](docs/usb_serial_protocol.ja.md)
- [発話吹き出しprotocol](docs/speech_bubble_protocol.ja.md)
- [StreetPass protocol](docs/streetpass_protocol.ja.md)
- [好感度API](docs/device_affection_api.ja.md)
- [StopWatch歩数同期protocol](docs/step_counter_protocol.ja.md)
- [StopWatchスマホカメラ・リモート撮影仕様](docs/phone_camera_remote_protocol.ja.md)

## ドキュメント

| 目的 | 文書 |
| --- | --- |
| 機種別のbuild・操作 | [対応デバイス別ガイド](docs/devices.ja.md) |
| binary導入・復旧 | [バイナリ版インストール手順](docs/install_binary.ja.md) |
| v0.4.0更新・移行 | [0.4.0リリースノート](docs/release_notes_0.4.0.ja.md) |
| 全versionの変更 | [CHANGELOG](CHANGELOG.ja.md) |
| 顔画像の作成 | [顔画像ビルダー](tools/face_image_builder/README.md) |
| 開発時の確認事項 | [CONTRIBUTING](CONTRIBUTING.md#日本語) |

公開文書は日本語版と英語版を対にして管理し、片方だけの追加・更新はCIで検出します。

## 開発と配布

```sh
python3 -m unittest discover -s scripts/tests -p 'test_*.py' -v
python3 scripts/validate_face_assets.py
python3 scripts/check_bilingual_docs.py
bash scripts/build_release_bins.sh all
```

CIでは3機種の画像顔／classic顔、合計6環境をbuildします。配布binary生成時は個人設定を含めず、
build pathがbinaryへ残っていないことも検査します。

## ライセンス

現在のfirmware source、同梱自作画像、sampleは[MIT License](LICENSE)です。
現在のsource treeとv0.4.0配布物には第三者のcharacter素材を含みません。

つくよみちゃん画像を含む過去の配布binaryには現行MITを適用せず、当時のrelease通知と
素材提供元の規約に従ってください。依存関係は[Third-Party Notices](THIRD_PARTY_NOTICES.md)を
参照してください。
